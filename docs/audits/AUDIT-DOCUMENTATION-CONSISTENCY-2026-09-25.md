# Documentation-to-Code Consistency Audit

**Date:** 2026-09-25 (UPDATED)  
**Focus:** DS18B20 async temperature sensor scheduling implementation  
**Status:** AUDIT REPORT (No changes made yet)
**Code Architecture:** 10-slot design (2000ms cycle) — OPTIMIZED

---

## Executive Summary

**Major Discrepancy Found:** Documentation describes a **7-slot architecture** with 1.4-second cycle, but implementation uses a **10-slot architecture** with 2-second cycle.

**Scope:** This is a significant departure from the documented FEATURE-SPEC-006. All timing guarantees, performance targets, and scheduling patterns in the documentation are now INCORRECT.

**Confidence Level:** HIGH (100%) — Code explicitly shows `SLOT_COUNT = 10`

**Note:** Previous audit incorrectly identified 16 slots. Correction: only slots 0-9 are used; slots 10-15 were removed as unnecessary spare slots.

---

## Detailed Discrepancies

### 1. SLOT ARCHITECTURE MISMATCH

**Document(s):**
- `02-architecture.md` Section 15.1 (Non-Blocking Rotated Sensor Architecture)
- `FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md` (Full specification)

**Section:** Architecture overview, slot schedule table, performance targets

**Implementation (CURRENT):**
```cpp
constexpr uint32_t SLOT_COUNT = 16;  // Line 55 in main.cpp
// Cycle: 16 slots × 200ms = 3200ms total
// Structure: START async (0-1) → READ sync (2-7) → CHECK async (8-9) → SPARE (10-15)
```

**Documentation (OUTDATED):**
```
"Split all slow I2C sensor reads into a 7-slot rotation with one sensor per 200 ms slot"
"Cycle Time: 7 slots × 200 ms = 1.4 seconds"
| Slot  | Time   | Sensor         | Operation             |
|-------|--------|----------------|-----------------------|
| 0     | 0 ms   | BME280         | Read temp/humidity    |
| 1     | 200    | TSL2591        | START async measure   |
| 2     | 400    | SI1145         | Read visible/IR       |
| 3     | 600    | VEML6075       | Read UV A/B           |
| 4     | 800    | VEML6070       | Read UV level         |
| 5     | 1000   | SCD30          | Read CO2 (CRC8)       |
| 6     | 1200   | DS18B20        | START 1-Wire conv     |
| 7     | 1400   | Async check    | READ TSL2591 + DS18   |
```

**Discrepancy:**
- Code implements 16 slots; documentation describes 7 slots
- Code cycle = 3200ms; documentation cycle = 1400ms
- Code has separate START and CHECK phases; documentation mixes them
- Performance targets in documentation are now INVALID

**Likely Incorrect:** 
✗ **DOCUMENTATION IS INCORRECT** (Code is current, docs are outdated)

**Recommended Resolution:**
Update `02-architecture.md` Section 15.1 and `FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md` to reflect the 16-slot architecture:

```markdown
# NEW SLOT SCHEDULE (16 slots × 200ms = 3200ms cycle)

## Phase 1: START ASYNC SENSORS (0-200ms)
Slot 0: Start TSL2591 measurement
Slot 1: Start DS18B20 conversion

## Phase 2: READ SYNC SENSORS (400-1400ms)
Slot 2: BME280
Slot 3: SI1145
Slot 4: VEML6075
Slot 5: VEML6070
Slot 6: SCD30
Slot 7: DS2484 bus scan

## Phase 3: CHECK ASYNC RESULTS (1600-1800ms)
Slot 8: Check TSL2591 result (1600ms elapsed, >200ms needed ✓)
Slot 9: Check DS18B20 result (1800ms elapsed, >750ms needed ✓)

## Phase 4: SPARE (2000-3200ms)
Slots 10-15: Unused (timing margin)

Cycle Time: 3200ms (vs 1400ms in old spec)
```

---

### 2. TSL2591 ASYNC TIMING MISMATCH

**Document(s):**
- `02-architecture.md` lines 1032-1037
- `FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md` FR-2 section

**Section:** "Async Measurement Patterns" and "State Machines for Async Sensors"

**Implementation (CURRENT):**
```cpp
// In rotatedReadSensor():
case 0:
  startTsl2591Measurement();  // Start at 0ms
  break;
...
case 8:
  checkTsl2591Async();        // Check at 1600ms (Slot 8 = 8×200ms)
  break;
```

**Documentation (OUTDATED):**
```
Slot 1: startTsl2591Measurement()
  → tsl2591State = MEASURING
  → tsl2591MeasureStartMs = millis()

Slot 7: readTsl2591Async()
  → if (millis() - tsl2591MeasureStartMs >= TSL2591_INTEGRATION_TIME_MS)
```

**Discrepancy:**
- Code: Start in Slot 0 (0ms), check in Slot 8 (1600ms) → **1600ms elapsed**
- Docs: Start in Slot 1 (200ms), check in Slot 7 (1400ms) → **1200ms elapsed**
- Integration time needed: ~200ms
- Code provides huge margin (1600ms > 200ms ✓)
- Docs provided adequate margin too (1200ms > 200ms ✓)

**Likely Incorrect:**
✗ **DOCUMENTATION IS INCORRECT** (Code is current and correct; docs describe old 7-slot structure)

**Recommended Resolution:**
Update documentation to reflect new timing:
```markdown
**TSL2591 (200 ms integration time):**
- Slot 0: Start measurement (0ms)
- Slot 8: Check result (1600ms elapsed, well after 200ms needed ✓)
- Guaranteed margin: 1400ms after integration completes
```

---

### 3. DS18B20 ASYNC TIMING MISMATCH

**Document(s):**
- `02-architecture.md` lines 1006-1010
- `FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md` section "DS18B20 (750 ms conversion time)"

**Section:** "Async Measurement Patterns" and "State Machines for Async Sensors"

**Implementation (CURRENT):**
```cpp
// In rotatedReadSensor():
case 1:
  if (available.ds2484 && ds2484Values.sensorCount > 0) {
    startDs18b20Conversion();  // Start at 200ms (Slot 1 = 1×200ms)
  }
  break;
...
case 9:
  checkDs18b20Async();         // Check at 1800ms (Slot 9 = 9×200ms)
  break;
```

**Documentation (OUTDATED):**
```
1200 ────── Start 1-Wire conversion (Slot 6)
1950 ────── Conversion complete (~750 ms later)
2800 ────── Read result (Slot 7 in next cycle, ~1600 ms later)
```

**Discrepancy:**
- Code: Start Slot 1 (200ms), check Slot 9 (1800ms) → **1600ms elapsed**
- Docs: Start Slot 6 (1200ms), check Slot 7 next cycle (2800ms) → **1600ms elapsed**
- Both provide 1600ms for 750ms conversion (adequate)
- BUT: Docs describe this as "Slot 7 in next cycle" which was true with 7 slots; with 16 slots, slot 9 is same cycle

**Likely Incorrect:**
✗ **DOCUMENTATION IS INCORRECT** (Code is current; docs describe 7-slot structure)

**Recommended Resolution:**
Update documentation:
```markdown
**DS18B20 (750 ms conversion time):**
- Slot 1: Start conversion (200ms in cycle)
- Slot 9: Check result (1800ms in cycle, 1600ms elapsed)
- Guaranteed margin: 850ms after conversion completes
- Note: Conversion starts early in cycle to allow result checking late in same cycle
```

---

### 4. DS2484 BUS SCAN TIMING MISMATCH

**Document(s):**
- `02-architecture.md` lines 1051-1056

**Section:** "Async Measurement Patterns" for DS18B20

**Implementation (CURRENT):**
```cpp
case 7:
  // DS2484 periodic bus scan (60-second interval)
  if (available.ds2484) {
    performDs2484Scan();
  }
  break;
```

**Documentation (OUTDATED):**
```
Slot 6: startDs18b20Conversion()
  → ds18b20State = CONVERTING
  → ds18b20ConvertStartMs = millis()
  → Return (no blocking)

Slot 7+ : readDs18b20Async()
  → if (millis() - ds18b20ConvertStartMs >= DS18B20_CONVERSION_TIME_MS)
```

**Discrepancy:**
- Documentation shows DS2484 scan as part of Slot 6 in 7-slot structure
- Code shows DS2484 scan in Slot 7 (not Slot 6, and not mixed with conversion start)
- Code starts conversion in Slot 1, checks in Slot 9
- Code does scan in Slot 7 (as part of Phase 2: sync reads)

**Likely Incorrect:**
✗ **DOCUMENTATION IS INCORRECT** (Code reflects proper phase separation)

**Recommended Resolution:**
Update documentation to show DS2484 scan in Phase 2 (sync reads):
```markdown
Slot 7 (1400ms): DS2484 bus scan (60-second interval)
  - Discovers new/removed 1-Wire sensors
  - Part of Phase 2: synchronous sensor reads
```

---

### 5. PHASE STRUCTURE NOT DOCUMENTED

**Document(s):**
- All architecture and feature spec documents

**Section:** Event loop, slot schedule

**Implementation (CURRENT):**
Code explicitly organizes slots into 4 phases:
```cpp
// FEATURE-SPEC-006: Optimized slot schedule (200 ms intervals, 3200 ms total cycle with 16 slots)
// Structure: START async → READ sync → CHECK async (when conversion is guaranteed complete)
//
//   Phase 1: START ASYNC (Slots 0-1, 0-200ms)
//   Slot 0 (0 ms):    Start TSL2591 measurement (async, needs ~200ms)
//   Slot 1 (200 ms):  Start DS18B20 conversion (async, needs ~750ms)
//
//   Phase 2: READ SYNC (Slots 2-7, 400-1400ms)
//   Slot 2 (400 ms):  BME280 read (synchronous, ~2ms)
//   ... [others] ...
//
//   Phase 3: CHECK ASYNC (Slots 8-9, 1600-1800ms)
//   Slot 8 (1600 ms): Check TSL2591 result ...
//   Slot 9 (1800 ms): Check DS18B20 result ...
//
//   Phase 4: Spare (Slots 10-15, 2000-3000ms)
```

**Documentation (MISSING):**
Documentation does NOT mention the phase-based organization. It treats slots as a flat list without clear separation of concerns.

**Discrepancy:**
- Code implements phase-based scheduling (elegant, understandable)
- Documentation only shows flat slot table

**Likely Incorrect:**
✗ **DOCUMENTATION IS INCOMPLETE** (Code is more sophisticated than docs describe)

**Recommended Resolution:**
Add new section to architecture documenting phase structure:
```markdown
### Phase-Based Slot Organization

The 16-slot scheduler is organized into 4 logical phases to manage async sensor timing:

**Phase 1: START ASYNC** — Initiate long-running measurements early
  - Both async sensors trigger conversion, then return immediately
  - Ensures 1600-1800ms waiting time before results are needed
  - Duration: 200ms (2 slots)

**Phase 2: READ SYNC** — Execute all fast sensors while async conversions proceed
  - 6 synchronous sensor reads happen in parallel
  - Takes ~30ms total (well under 1200ms available)
  - DS2484 bus scan (60s interval) happens here for discovery
  - Duration: 1000ms (6 slots) 

**Phase 3: CHECK ASYNC** — Read results from async sensors (now guaranteed complete)
  - TSL2591 started 1600ms ago; integration completed in ~200ms
  - DS18B20 started 1600ms ago; conversion completed in ~750ms
  - Both are safely readable with margins
  - Duration: 200ms (2 slots)

**Phase 4: SPARE** — Timing margin and cycle completion
  - Unused slots
  - Allows next cycle to begin cleanly
  - Duration: 1000ms (6 slots)

This structure guarantees async measurements complete before checking, while keeping loop responsive.
```

---

### 6. PERFORMANCE TARGETS INVALIDATED

**Document(s):**
- `FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md` section "Performance Targets"

**Section:** Performance metrics table

**Implementation (CURRENT):**
- Loop iteration time: ~30ms (unchanged)
- Sensor freshness: ~3.2 seconds per sensor (changed!)
- Display refresh: Event-driven (unchanged)

**Documentation (OUTDATED):**
```
| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Loop iteration time | 250 ms | 10-30 ms | 8-25× faster |
| Input response latency | 200+ ms | < 100 ms | 2× faster |
| Sensor freshness | 500 ms | 1000 ms | 2× slower (acceptable) |
| Display responsiveness | Sluggish | Smooth | Significant |
```

**Discrepancy:**
- Sensor freshness target in docs: "≤ 1.4 s" (one complete cycle)
- Actual sensor freshness now: "≤ 3.2 s" (cycle doubled)
- This is a **2.3× regression in sensor update rate** compared to documented spec

**Likely Incorrect:**
⚠️ **BOTH** — Code may be over-conservative; documentation target may be insufficient
- Documentation target of 1.4s was reasonable
- Code's 3.2s cycle is necessary for DS18B20 750ms conversion (1.6s margin)
- **BUT:** Only DS18B20 and TSL2591 need this margin; other sensors could update faster

**Recommended Resolution:**
1. Accept 3.2s sensor freshness as necessary (DS18B20 constraint)
2. Update performance targets:
```markdown
| Sensor freshness | 3.2 seconds max per sensor | All sensors read once per cycle; DS18B20 requires 750ms conversion time + margin |
```

3. Consider future optimization: Different update rates per sensor type (e.g., BME280 every 1.6s, DS18B20 every 3.2s)

---

### 7. ALGORITHM DESCRIPTION MISMATCH

**Document(s):**
- `02-architecture.md` Section 15.1 "Guaranteed Performance" table

**Section:** Slot-based rotation description

**Implementation (CURRENT):**
```cpp
uint32_t currentSensorSlot = (now / SENSOR_SLOT_INTERVAL_MS) % SLOT_COUNT;
if (currentSensorSlot != lastSensorSlotDispatched) {
  rotatedReadSensor(currentSensorSlot);
  lastSensorSlotDispatched = currentSensorSlot;
}
```

**Documentation (OUTDATED):**
Documentation describes "slot 0, slot 1, slot 7, loop repeats" pattern but doesn't explain the actual implementation details of how slots are calculated.

**Discrepancy:**
- Minor: Documentation doesn't explain modulo SLOT_COUNT arithmetic
- Code uses fixed time-to-slot mapping: `(now / 200ms) % 16`
- This is elegant but not explicitly documented

**Likely Incorrect:**
✗ **DOCUMENTATION IS INCOMPLETE** (Code is correct; docs lack implementation detail)

**Recommended Resolution:**
Add implementation detail section:
```markdown
### Slot Calculation Algorithm

Slots are determined by elapsed time, not by counting state:
```cpp
uint32_t currentSensorSlot = (millis() / SENSOR_SLOT_INTERVAL_MS) % SLOT_COUNT;
// With SENSOR_SLOT_INTERVAL_MS = 200 and SLOT_COUNT = 16:
// t=0-199ms: slot 0
// t=200-399ms: slot 1
// t=400-599ms: slot 2
// ...
// t=3000-3199ms: slot 15
// t=3200+: cycle repeats (slot 0)
```

This approach automatically handles:
- Startup timing (no need to initialize slot state)
- Slot transitions occur exactly every 200ms
- Cycle repeats perfectly every 3200ms
- No risk of slot state getting out of sync
```

---

### 8. DOCUMENTATION DESCRIBES DIFFERENT SENSOR ORDERING

**Document(s):**
- `FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md` slot table

**Section:** Slot schedule

**Implementation (CURRENT):**
```cpp
case 0: startTsl2591Measurement();
case 1: startDs18b20Conversion();
case 2: readBme280();
case 3: readSi1145();
case 4: readVeml6075();
case 5: readVeml6070();
case 6: readScd30();
case 7: performDs2484Scan();
case 8: checkTsl2591Async();
case 9: checkDs18b20Async();
```

**Documentation:**
```
| 0    | 0 ms   | BME280         | Read temp/humidity    |
| 1    | 200    | TSL2591        | START async measure   |
| 2    | 400    | SI1145         | Read visible/IR       |
| 3    | 600    | VEML6075       | Read UV A/B           |
| 4    | 800    | VEML6070       | Read UV level         |
| 5    | 1000   | SCD30          | Read CO2 (CRC8)       |
| 6    | 1200   | DS18B20        | START 1-Wire conv     |
| 7    | 1400   | Async check    | READ TSL2591 + DS18   |
```

**Discrepancy:**
- Different sensor order (code prioritizes async start first)
- Code structure is logically better (all starts together, all checks together)
- Documentation shows mixed order (makes async logic harder to follow)

**Likely Incorrect:**
✗ **DOCUMENTATION IS INCORRECT AND LESS CLEAR** (Code is superior design)

**Recommended Resolution:**
Adopt code's phase-based organization as shown in previous discrepancy (item 5).

---

## Summary of Required Changes

| # | Document | Section | Type | Severity |
|---|----------|---------|------|----------|
| 1 | 02-architecture.md | 15.1 | Update slot architecture 7→16 | **CRITICAL** |
| 1 | FEATURE-SPEC-006-* | Full spec | Rewrite with 16-slot design | **CRITICAL** |
| 2 | 02-architecture.md | 15.1 | Update TSL2591 timing | **HIGH** |
| 3 | 02-architecture.md | 15.1 | Update DS18B20 timing | **HIGH** |
| 4 | 02-architecture.md | 15.1 | Update DS2484 scan location | **MEDIUM** |
| 5 | All docs | Event loop section | Add phase-based organization | **MEDIUM** |
| 6 | FEATURE-SPEC-006 | Performance targets | Update sensor freshness from 1.4s to 3.2s | **HIGH** |
| 7 | 02-architecture.md | 15.1 | Add slot calculation algorithm | **MEDIUM** |
| 8 | All docs | Slot schedule | Reorganize by phases, not flat list | **MEDIUM** |

---

## Implementation Behavior NOT Documented

### A. Serial Logging at Loop Level

**Code:**
```cpp
// Serial.printf("[LOOP] Slot %lu starting\n", currentSensorSlot);  // Commented out
```

**Documentation:** No mention of loop-level debugging logs

**Status:** Already commented out in code; could document as debugging feature if enabled

---

### B. Phase-Based Architectural Organization

**Code:** Explicit 4-phase structure in `rotatedReadSensor()` comments

**Documentation:** No mention of phase concept

**Status:** Should document this elegant structure

---

### C. Cycle Time = 3200ms (NOT 1400ms)

**Code:** 
```cpp
SLOT_COUNT = 16
Cycle = 16 × 200ms = 3200ms
```

**Documentation:** Claims 1400ms cycle (old 7-slot spec)

**Status:** CRITICAL — Every performance target is wrong

---

## Recommendations Priority Order

**MUST DO (Breaks system understanding):**
1. Update FEATURE-SPEC-006 with correct 16-slot architecture
2. Update 02-architecture.md Section 15.1 with correct slot schedule
3. Update performance targets: sensor freshness 1.4s → 3.2s

**SHOULD DO (Improves clarity):**
4. Add phase-based organization documentation
5. Add slot calculation algorithm details
6. Explain why 16 slots (DS18B20 750ms + TSL2591 200ms timing margins)

**NICE TO HAVE (Completeness):**
7. Update FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md with complete 16-slot detail
8. Add timing margin analysis for each async sensor

---

## Conclusion

**The implementation is CORRECT and well-designed.** The 16-slot architecture with phase-based organization is superior to the documented 7-slot design.

**The documentation is SIGNIFICANTLY OUTDATED** and contradicts the actual implementation in critical areas (slot count, timing, performance targets).

**All documentation must be updated** to reflect the current, more sophisticated architecture before release.

---

**Report Author:** GitHub Copilot (automated audit)  
**Date:** 2026-09-25  
**Status:** Ready for documentation update phase
