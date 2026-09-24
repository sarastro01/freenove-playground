# FEATURE-SPEC-006: Analysis Summary

**Date:** 2026-09-24  
**Task:** Analyze FEATURE-SPEC-006 requirements without implementation  
**Status:** ✅ Analysis Complete (Ready for Implementation Decision)

---

## Requirement Analysis

### User Request Interpretation

The user requested optimization of the system's responsiveness due to blocking sensor reads:

1. **Core Problem:** `readSensors()` blocks for ~250 ms due to TSL2591 integration time
2. **Impact:** Input handling (gamepad/encoder/INA219) is delayed
3. **Solution Approach:** Split sensor reads, keep fast inputs responsive

### Feature Scope

**Architectural Changes:**
- Rotate sensor reads (5 sensors, 200 ms per sensor)
- Async TSL2591 measurement (2-stage)
- Decouple display updates from gamepad input
- Add performance metrics logging

**Performance Goals:**
- Loop iteration: 250 ms → 10-30 ms (8-25× faster)
- Input latency: 200+ ms → < 100 ms (2× faster)
- Sensor freshness: 500 ms → 1000 ms (acceptable trade-off)

---

## Affected System Components

### 1. Existing Requirements (from 01-requirements.md)

**FR-001: Multi-Sensor Data Acquisition** ✓ AFFECTED
- Sensor read frequencies change from 500 ms to 1000 ms (rotation)
- Still acquires all sensor data, just less frequently
- **Impact:** Acceptable (1 Hz refresh still sufficient for slow sensors)

**FR-002: Real-Time Display of Sensor Data** ✓ AFFECTED
- Display refresh rate may need adjustment
- Data freshness up to 1 second old (vs 500 ms)
- **Impact:** Acceptable for climate/light sensors

**FR-003: Circular Pagination Navigation** ✓ AFFECTED (Improvement)
- Display update latency improved to < 100 ms
- Navigation response feels 2× faster
- **Impact:** Positive UX improvement

**FR-004 and beyond:** Unchanged

---

### 2. Domain Entities (from 04-data-model.md)

**Cache Structs** — NO CHANGES
- Bme280Values, Tsl2591Values, Si1145Values remain identical
- Backward compatible

**Input Structs** — NO CHANGES
- GamepadValues, EncoderValues, Ina219Values unchanged
- No new fields required

**NEW Structs:**
```cpp
struct InputStats {          // Per-input timing aggregation
  uint32_t count, minTimeUs, maxTimeUs, sumTimeUs;
};

enum Tsl2591State {          // TSL2591 async measurement state
  TSL_IDLE, TSL_MEASURING, TSL_READY
};
```

**Impact:** +~100 bytes RAM (negligible on ESP32)

---

### 3. Business Rules (from 06-business-rules.md)

**NEW Rules (FEATURE-SPEC-006):**

| Rule | Description | Enforcement |
|------|-------------|-------------|
| BR-SC-005 | Rotated reading: no two sensors in same slot | `currentSensorSlot` cycles 0→1→2→3→4 |
| BR-SC-006 | Sensor freshness: ≥ 1/second per sensor | Rotation ensures 5×200ms=1000ms max |
| BR-SC-007 | TSL2591 async: 200 ms between start→read | State machine with timestamp check |
| BR-SC-008 | Input guarantee: read within 20 ms budget | Separate timing gates, no blocking |
| BR-SC-009 | Display latency: < 100 ms on button press | Deferred render, sensor slot < 100 ms |
| BR-SC-010 | Loop iteration: ≤ 30 ms max per cycle | Budget enforcement, logging |

**Impact:** 6 new business rules requiring validation in tests

---

### 4. API Changes (from 05-api.md)

**REMOVED:**
- ~~`void readSensors()`~~ — Broken up into rotated pattern

**ADDED:**
- `void startTsl2591Measurement()`
- `void readTsl2591Async()`
- `void rotatedReadSensor()`
- `void updateInputStats()`
- `void reportInputStats()`

**MODIFIED:**
- `void loop()` — Sensor gate changed 500 ms → 200 ms
- `void readGamepadAt40Hz()` — Add timing capture
- `void readEncoderAt50Hz()` — Add timing capture
- `void readIna219At50Hz()` — Add timing capture

**Impact:** 5 new functions, 1 removed, 4 modified

---

### 5. UI/Display Components (from 07-ui.md)

**Display Output Changes:**

Before:
```
Page renders immediately on button press (blocking until sensors finish)
```

After:
```
Page renders within < 100 ms on button press (deferred, non-blocking)
```

**Visual Changes:** NONE
- Layout, colors, formatting identical
- Only latency improves

**Serial Console Output Changes:**

**Before:**
```
[No per-sensor timing logged]
[No aggregated input statistics]
```

**After:**
```
BME280 read time: 2157 µs
TSL2591 read time: 895 µs
SI1145 read time: 687 µs
Gamepad Stats (1s): Count=40, MinTime=4800, MaxTime=5234, SumTime=201456
Encoder Stats (1s): Count=50, MinTime=987, MaxTime=1156, SumTime=50123
INA219 Stats (1s): Count=50, MinTime=1100, MaxTime=1289, SumTime=61234
```

**Impact:** Better observability, no display changes

---

### 6. Configuration & Constants

**NEW Constants:**
```cpp
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;      // vs 500 before
constexpr uint32_t TSL2591_INTEGRATION_TIME_MS = 200;   // Measurement duration
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;      // Aggregation window
```

**Impact:** 3 new tunable parameters

---

### 7. Security & Authorization

**Impact:** NONE
- No authentication changes
- No encryption changes
- No access control changes
- No sensitive data exposure

---

### 8. Testing Requirements

**New Test Cases Required:**

1. **Unit Tests (5 tests):**
   - Sensor slot rotation cycle
   - TSL2591 state machine
   - Input stats aggregation
   - Loop iteration time measurement
   - Input stat reset edge case

2. **Integration Tests (3 tests):**
   - Responsive input validation
   - Concurrent operations (multiple read rates)
   - TSL2591 async accuracy

3. **Performance Benchmarks (3 benchmarks):**
   - Loop iteration time baseline
   - Input response latency
   - Sensor read time verification

**Impact:** 11 new test cases, ~50 hours of manual testing expected

---

### 9. Documentation Updates Required

**Files to Create (2):**
- `FEATURE-SPEC-006.md` (root) ✅ CREATED
- `docs/FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md` ✅ CREATED

**Files to Modify (7):**
- `02-architecture.md` — Event loop section
- `06-business-rules.md` — Add BR-SC-005 through BR-SC-010
- `07-ui.md` — Display latency expectations
- `09-configuration.md` — Timing constants table
- `10-testing.md` — New test cases
- `14-reconstruction-plan.md` — Phase 6 implementation steps

**Impact:** 9 documentation files require updates

---

## Architecture Diagram (NEW)

```
BEFORE (Blocking):
┌─────────────────────────────────────┐
│ Main Loop (250 ms per iteration)    │
├─────────────────────────────────────┤
│ Gamepad (5 ms)                      │
│ Display (2 ms)                      │
│ Encoder (1 ms)                      │
│ INA219 (1.2 ms)                     │
│ readSensors() (240 ms) ← BLOCKING   │
│   ├─ BME280 (2 ms)                  │
│   ├─ TSL2591 (200 ms) ← BOTTLENECK  │
│   ├─ SI1145 (0.7 ms)                │
│   └─ VEML* + SCD30 (37 ms)          │
└─────────────────────────────────────┘
Result: ~3.8 Hz loop, sluggish UI
```

```
AFTER (Non-Blocking, Rotated):
┌─────────────────────────────────────┐
│ Main Loop Iteration 1 (20 ms)       │
├─────────────────────────────────────┤
│ Gamepad (5 ms)                      │
│ Display (2 ms)                      │
│ Encoder (1 ms)                      │
│ INA219 (1.2 ms)                     │
│ Sensor Slot: BME280 (2 ms)          │
│ Display refresh (2 ms)              │
├─────────────────────────────────────┤
│ Main Loop Iteration 2 (8 ms)        │
│ Gamepad (5 ms)                      │
│ Sensor Slot: TSL2591 START (0 ms)   │
│ Display refresh (2 ms)              │
├─────────────────────────────────────┤
│ ... continues with rotated pattern  │
└─────────────────────────────────────┘
Result: ~50-100 Hz loop, responsive UI
```

---

## State Machine Diagram (TSL2591 Async)

```
           ┌──────────┐
           │  IDLE    │
           └────┬─────┘
                │
      Slot 1: trigger
      getFullLuminosity()
                │
                ▼
           ┌──────────┐
           │ MEASURING│◄─── Check: elapsed < 200 ms?
           └────┬─────┘     Yes → Stay in MEASURING
                │            No  → Continue to READY
      Wait 200 ms
                │
      Slot 4/5: ready?
                │
                ▼
           ┌──────────┐
           │  READY   │
           └────┬─────┘
                │
      getFullLuminosity()
      (returns data)
                │
                ▼
           Extract values
           Update cache
                │
                ▼
           ┌──────────┐
           │  IDLE    │
           └──────────┘
```

---

## Decision Matrix

### Key Decisions Made in Spec

| Decision | Options | Chosen | Rationale |
|----------|---------|--------|-----------|
| Sensor read frequency | 500 ms vs 1000 ms | **1000 ms** | Rotated reading, acceptable for slow sensors |
| TSL2591 approach | Sync vs Async | **Async** | Eliminates 200 ms blocking |
| Slot interval | 100 ms vs 200 ms | **200 ms** | Balance responsiveness vs measurement time |
| Display update decoupling | Yes vs No | **Yes** | Improves UX, minimal complexity |
| Performance metrics | None vs Detailed | **Detailed** | Better observability, debugging |
| Stat aggregation window | 500 ms vs 1000 ms | **1000 ms** | Matches rotation cycle |

---

## Open Questions for User Input (NOW RESOLVED ✅)

### Critical Decisions ✅

1. **SCD30 Sensor Handling** ✅ RESOLVED
   - **Decision:** Include in rotation (1 second full cycle)
   - **Implementation:** Slot 5 (1000 ms)

2. **1-Wire Temperature (DS18B20)** ✅ RESOLVED
   - **Decision:** Include in rotation with async implementation
   - **Implementation:** Slot 6 start → Slot 7 read (750+ ms later)

3. **1-Wire Async Measurement** ✅ RESOLVED
   - **Decision:** Yes, implement like TSL2591 (start → wait → read)
   - **Pattern:** Same state machine as TSL2591

4. **Display Refresh Rate Adjustment** ✅ RESOLVED
   - **Decision:** Only on value changes + timer updates (INA219 seconds)
   - **Benefit:** Eliminates flicker, only redraws on actual changes

### Implementation Details ✅

5. **Stats Output Verbosity** ✅ RESOLVED
   - **Decision:** Configurable via `#define ENABLE_STATS_REPORTING`
   - **Values:** 1 = enabled, 0 = disabled
   - **Default:** 1 (enabled)

6. **Sensor Configuration** ✅ RESOLVED
   - **Decision:** Make TSL2591 integration time configurable
   - **Variable:** `uint32_t TSL2591_INTEGRATION_TIME_MS = 200`
   - **Also:** DS18B20 conversion time configurable (`DS18B20_CONVERSION_TIME_MS = 750`)

---

## Affected System Statistics

| Metric | Impact |
|--------|--------|
| New global variables | 8 variables (~100 bytes) |
| New functions | 5 functions (~1-2 KB) |
| Modified functions | 4 functions (~50 lines) |
| Removed functions | 1 function |
| New tests required | 11 test cases |
| Documentation updates | 9 files |
| Breaking changes | 1 (readSensors removal) |
| Memory impact | < 1% (ESP32: 327 KB → negligible) |
| Flash impact | < 1% (ESP32: 1.3 MB → ~1-2 KB) |
| Performance gain | 8-25× faster loop |
| Backward compatibility | Minor (API breaking) |

---

## Risk Assessment

### Low Risk
- ✅ No hardware changes
- ✅ No sensor value changes (same read logic)
- ✅ No display changes
- ✅ No security implications

### Medium Risk
- ⚠️ Loop timing critical (must verify ≤ 30 ms)
- ⚠️ TSL2591 state machine (must test all edges)
- ⚠️ Input stats tracking (must prevent overflows)

### Mitigation
- Comprehensive timing measurements
- Unit tests for state machine
- Integration testing for concurrent operations
- Performance benchmarking

---

## Implementation Complexity

**Estimated Effort:**
- **Code Changes:** 200-300 lines (rotated reads + async TSL2591 + stats)
- **Testing:** 50-100 hours (unit + integration + performance validation)
- **Documentation:** 10-15 hours (spec + updates to 9 files)
- **Hardware Testing:** 5-10 hours (responsive input validation)

**Total: ~100-150 hours development and validation**

---

## Success Criteria

**Hard Criteria (Objective):**
- [ ] Loop iteration time ≤ 30 ms (90% of cycles)
- [ ] Input response < 100 ms (button press to display)
- [ ] All sensor reads within allocated time budget
- [ ] No missed sensor reads (≥ 1 per second)
- [ ] Serial output shows per-sensor and aggregated timing

**Soft Criteria (Subjective):**
- [ ] Display feels responsive (subjective user feedback)
- [ ] No regressions in sensor values or display
- [ ] Code changes are maintainable and well-documented

---

## Conclusion

**FEATURE-SPEC-006 is fully specified and ready for implementation.**

The specification comprehensively defines:
1. ✅ Functional requirements (5 major features)
2. ✅ Domain entities and data changes
3. ✅ Business rules and constraints (6 new rules)
4. ✅ API changes (5 new functions, 1 removed, 4 modified)
5. ✅ UI/display changes (latency only, no visual changes)
6. ✅ Testing requirements (11 new test cases)
7. ✅ Documentation updates (9 files)
8. ✅ Error handling and edge cases (5 scenarios)
9. ✅ Open questions for design decisions (6 items)

**No implementation will be performed until user confirms these specifications and makes decisions on the 6 open questions.**

---

**Specification Version:** 1.0  
**Created:** 2026-09-24  
**Status:** ✅ Ready for Implementation Review
