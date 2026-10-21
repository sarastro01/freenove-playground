# FEATURE-SPEC-010: Wire Library Elimination via Sensor Libraries

**Status:** Specification Phase (Analysis Complete)  
**Created:** 2026-10-21  
**Last Updated:** 2026-10-21  
**Priority:** MEDIUM  
**Scope:** Code Quality, Technical Debt Reduction  
**Impact Level:** LOW (No UI/feature changes, internal refactoring only)

---

## 1. Executive Summary

Remove all direct `Wire` library calls from main.cpp by replacing them with purpose-built Arduino sensor libraries. This eliminates raw I2C protocol handling, improves maintainability, reduces technical debt, and leverages vendor-validated implementations.

**Current State:**
- 40 direct Wire calls across 7 functions
- 2 sensors using raw I2C (VEML6075, VEML6070)
- 1 performance test using raw I2C (TSL2591 micro-benchmark)
- 3 legacy SCD30 functions (no longer used after SparkFun migration)
- 2 generic helper functions (readReg16, writeReg16)

**Target State:**
- 0 direct Wire calls in application code
- All sensors using manufacturer-provided or Adafruit libraries
- Consistent pattern across all 10 sensors
- Improved error handling and CRC validation

---

## 2. Functional Requirements

### FR-010-001: Eliminate Raw I2C for VEML6075 UV Sensor

**Requirement:**  
Replace `readReg16()` / `writeReg16()` / `readVeml6075()` with Adafruit VEML6075 Library.

**Details:**
- Add `adafruit/Adafruit VEML6075 Library @ ^1.1.1` to platformio.ini
- Create global instance: `Adafruit_VEML6075 veml6075(&Wire)`
- Initialize in `detectSensors()`: `available.veml6075 = veml6075.begin()`
- Replace `readVeml6075()` to call library methods:
  - `veml6075.read()` (reads all registers)
  - `veml6075Values.uva = veml6075.readUVA()`
  - `veml6075Values.uvb = veml6075.readUVB()`
  - `veml6075Values.comp1` and `comp2` via register access
- Library handles I2C protocol, CRC, timing internally
- Behavior unchanged from user perspective

**Affected Code:**
- platformio.ini: Add VEML6075 library
- main.cpp line 2329: Initialization in detectSensors()
- main.cpp line ~850: Remove readReg16() (if no other uses)
- main.cpp line ~861: Remove writeReg16() (if no other uses)
- main.cpp line ~957: Refactor readVeml6075() to use library

**Success Criteria:**
- Compilation without errors
- VEML6075 detected at boot (available.veml6075 = true)
- UV values read and displayed identically to previous behavior
- Test: Page 8 displays UVA/UVB/UV Index (no visual changes)
- No Wire.begin/write/read calls in VEML6075 code path

---

### FR-010-002: Eliminate Raw I2C for VEML6070 UV Sensor

**Requirement:**  
Replace `readVeml6070()` with Adafruit VEML6070 Library.

**Details:**
- Add `adafruit/Adafruit VEML6070 @ ^1.1.2` to platformio.ini
- Create global instance: `Adafruit_VEML6070 veml6070(&Wire)`
- Initialize in `detectSensors()`: `available.veml6070 = veml6070.begin(VEML6070_INTEGRATION_TIME_1T)`
- Replace `readVeml6070()` to call library:
  - `veml6070Values.raw = veml6070.readUV()`
- Library handles dual-address (0x38/0x39) protocol internally
- Library handles I2C timing and retries

**Affected Code:**
- platformio.ini: Add VEML6070 library
- main.cpp line 2344: Initialization in detectSensors()
- main.cpp line ~969: Refactor readVeml6070() to use library

**Success Criteria:**
- Compilation without errors
- VEML6070 detected at boot (available.veml6070 = true)
- UV values read identically to previous behavior
- Test: Page 7 displays UV reading (no visual changes)
- No Wire calls in VEML6070 code path

---

### FR-010-003: Replace SCD30 Raw Measurement Reading

**Requirement:**  
Replace `readScd30()` raw 18-byte Wire reads with SparkFun SCD30 library methods.

**Details:**
- SparkFun SCD30 Library already added (FEATURE-SPEC-009)
- Current `readScd30()` uses `Wire.requestFrom(SCD30_ADDR, 18)`
- Library provides: `scd30Sensor.getCO2()`, `getTemperature()`, `getHumidity()`
- These methods already use I2C internally with proper delays
- Replace function body to call library methods instead of Wire

**Current Code (lines ~910-912):**
```cpp
bool readScd30(Scd30Values &values) {
  if (!scd30Command(SCD30_CMD_READ) || Wire.requestFrom(SCD30_ADDR, 18) != 18) return false;
  uint8_t raw[18];
  for (uint8_t i = 0; i < 18; i++) raw[i] = Wire.read();
  // ... CRC validation and byte parsing ...
}
```

**New Code Pattern:**
```cpp
bool readScd30(Scd30Values &values) {
  if (!scd30Sensor.dataAvailable()) return false;
  values.co2 = scd30Sensor.getCO2();
  values.temp = scd30Sensor.getTemperature();
  values.humidity = scd30Sensor.getHumidity();
  return true;
}
```

**Affected Code:**
- main.cpp line ~910: Refactor readScd30() to use library methods
- main.cpp lines 870-894: Remove legacy functions (scd30Crc, scd30Command, scd30Ready)
- Check boot detection: `detectSensors()` - verify SCD30 init via library

**Success Criteria:**
- Compilation without errors
- CO2, temperature, humidity values read identically
- Test: Page 4 displays values (no visual changes)
- ASC control still works (uses library's setAutoSelfCalibration/getAutoSelfCalibration)
- No Wire calls in SCD30 measurement code path

---

### FR-010-004: Remove TSL2591 Micro-Benchmarking Wire Calls

**Requirement:**  
Remove or refactor direct Wire calls used for performance measurement of TSL2591 reads.

**Details:**
- Lines 1388-1399 contain direct I2C reads to measure "pure Wire overhead"
- This is a performance test, not production sensor reading
- TSL2591 is already read via Adafruit library in normal polling
- Options:
  A) Remove the micro-benchmark entirely (simplest)
  B) Use library methods and measure library call overhead instead

**Current Code (lines ~1388-1399):**
```cpp
Wire.beginTransmission(0x29);
Wire.write(0x14);
Wire.endTransmission();
Wire.requestFrom(0x29, 4);
if (Wire.available() >= 4) { /* read data */ }
```

**Affected Code:**
- main.cpp lines 1388-1399: Remove or refactor startTsl2591Measurement()
- No dependency elsewhere (async measurement for performance test)

**Success Criteria:**
- Compilation without errors
- If removed: No TSL2591 timing data in Serial output (acceptable)
- If refactored: Use library methods, serial output unchanged
- No Wire calls in TSL2591 code path

---

### FR-010-005: Remove Legacy SCD30 Helper Functions

**Requirement:**  
Remove `scd30Crc()`, `scd30Command()`, `scd30Ready()` if no longer used.

**Details:**
- These functions were used for raw I2C control before SparkFun library migration
- After FR-010-003, these are no longer called
- SparkFun library provides CRC and command handling internally
- Clean up: Remove dead code

**Affected Code:**
- main.cpp line 870: `uint8_t scd30Crc()` - DELETE if unused
- main.cpp line 882: `bool scd30Command()` - DELETE if unused
- main.cpp line 890: `bool scd30Ready()` - DELETE if unused
- Check detectSensors() for any remaining usage

**Success Criteria:**
- Compilation without errors
- Verify no references remain to these functions
- Codebase is cleaner (fewer dead code paths)

---

### FR-010-006: Review and Remove readReg16/writeReg16 Helpers

**Requirement:**  
Confirm `readReg16()` and `writeReg16()` are only used by VEML6075, then remove.

**Details:**
- Generic helper functions for 16-bit register access
- After FR-010-001, VEML6075 library replaces these calls
- Verify no other code uses these functions
- If confirmed: Remove both functions
- If other uses found: Document and keep

**Affected Code:**
- main.cpp line 850: `bool readReg16()` - DELETE if only used by VEML6075
- main.cpp line 861: `bool writeReg16()` - DELETE if only used by VEML6075
- Grep for any other usages

**Success Criteria:**
- Compilation without errors
- Confirmed no other code paths use these functions
- Codebase is cleaner

---

## 3. Validation Rules

### VR-010-001: Library Availability Validation
**Rule:** All selected libraries must be available in PlatformIO registry with compatible versions.
**Validation:** During platformio.ini update, verify `pio lib search` returns results.

### VR-010-002: Sensor Detection Compatibility
**Rule:** Sensor detection in `detectSensors()` must work identically with new libraries.
**Validation:** `available.X` flags set to `true` only if library initialization succeeds.

### VR-010-003: Value Range Validation
**Rule:** Values read via libraries must fall within same ranges as before.
**Validation:** Existing value range checks in display code should not be modified.

### VR-010-004: Performance Tolerance
**Rule:** I2C read timing must not degrade by more than 20% due to library overhead.
**Validation:** Compare Serial timing logs before/after (if applicable).

### VR-010-005: CRC Validation
**Rule:** Libraries must validate CRC/checksums internally (not our concern).
**Validation:** No manual CRC code required; libraries handle it.

---

## 4. Authorization & Security

### SEC-010-001: No Authentication Required
No user authentication, device authentication, or cryptographic validation needed.

### SEC-010-002: No Data Exposure
Sensor libraries do not transmit data externally. I2C protocol remains internal.

### SEC-010-003: No New Privileges
Code continues to run at same privilege level (bare metal, no OS).

---

## 5. Error Cases

### ERR-010-001: Library Initialization Fails
**Scenario:** Adafruit library .begin() returns false for VEML6075 or VEML6070.
**Behavior:** `available.veml6075/veml6070 = false`; corresponding page displays error screen.
**No Change:** Same behavior as before (sensor not detected).

### ERR-010-002: Library Not Found at Compile Time
**Scenario:** platformio.ini lists library that doesn't exist.
**Behavior:** Compilation fails with "Could not find library" error.
**Recovery:** Verify library name/version in platformio.ini; check PlatformIO registry.

### ERR-010-003: Sensor Read via Library Returns Invalid Value
**Scenario:** Library .read() succeeds but returns 0xFFFF or out-of-range.
**Behavior:** Code can optionally validate and reject (as before).
**No Change:** Same validation logic applies whether data comes from Wire or library.

---

## 6. Data Model Changes

**No changes to data structures.**

All sensor value structs (Bme280Values, Ina219Values, Scd30Values, etc.) remain unchanged.
Libraries are drop-in replacements for I2C protocol handling; data representation is identical.

---

## 7. API Changes

### New Library-Based APIs

| Sensor | Old API | New API | Library |
|--------|---------|---------|---------|
| VEML6075 | `readReg16(addr, reg, &value)` | `veml6075.readUVA()`, `readUVB()` | Adafruit VEML6075 |
| VEML6070 | `Wire.requestFrom()` | `veml6070.readUV()` | Adafruit VEML6070 |
| SCD30 | `Wire.requestFrom(SCD30_ADDR, 18)` | `scd30Sensor.getCO2()`, `getTemperature()` | SparkFun SCD30 |
| TSL2591 | `Wire.beginTransmission()` (benchmark) | `tsl2591.getFullLuminosity()` (existing) | Adafruit TSL2591 |

**No change to public-facing APIs.** All sensor page displays remain identical.

---

## 8. UI Changes

**No UI changes.** All pages display data identically. Only internal I2C mechanism changes.

---

## 9. Affected Existing Requirements

### FR-001: Multi-Sensor Data Acquisition
**Impact:** Implementation method changes (raw I2C → libraries), but requirement unchanged.
- ✅ Still acquire data from all 10 sensors
- ✅ Still support 1-Wire via DS2484 (no change)
- ✅ Still gracefully degrade if sensors missing
- ✅ Sensor detection still one-time at boot

### FR-002: Real-Time Display
**Impact:** None. Display behavior unchanged.

### FR-003: Circular Pagination
**Impact:** None. Navigation unchanged.

### BR-001: Sensor Detection One-Time
**Impact:** Minor: Detection now via library .begin() instead of manual probes.
- ✅ Still one-time at boot
- ✅ Still cached in `available.X` flags
- ✅ Detection pattern: `available.sensor = library.begin()` (vs. `Wire.beginTransmission()`)

### BR-002: Stale Value Persistence
**Impact:** None. Same behavior: failed reads don't update cache.

---

## 10. Tests Required

### TST-010-001: Sensor Detection Tests
**Test:** Verify all 10 sensors detected correctly with new libraries.
**Expected:** `available.X == true` for connected sensors; `false` for missing.
**Method:** Visual inspection of Serial output + device verification.

### TST-010-002: Data Reading Tests
**Test:** Verify values read and displayed identically on all pages.
**Expected:** Same values as before (within measurement noise).
**Method:** Compare side-by-side displays before/after.

### TST-010-003: Page Display Tests
**Test:** Navigate all 10 pages; verify data and colors display correctly.
**Expected:** No visual changes; all pages functional.
**Method:** Manual user testing on device.

### TST-010-004: Error Case Tests
**Test:** Disconnect a sensor; verify page displays error gracefully.
**Expected:** Error screen (no crash, no garbage data).
**Method:** Manual disconnection of one sensor per page.

### TST-010-005: Compilation Tests
**Test:** Build project; verify no compile errors or warnings.
**Expected:** Clean build.
**Method:** Run `pio run` and verify success message.

### TST-010-006: Regression Tests
**Test:** Run existing test suite (if any); verify no failures.
**Expected:** All existing tests still pass.
**Method:** Execute test harness (if available).

---

## 11. Documentation Updates Required

### DOC-010-001: docs/12-dependencies.md
Update library list:
- Add Adafruit VEML6075 Library @ ^1.1.1
- Add Adafruit VEML6070 @ ^1.1.2
- Note that all sensors now use manufacturer libraries
- Note removal of Wire library from application code

### DOC-010-002: docs/05-api.md
Remove sections on raw I2C SCD30/VEML6075/VEML6070 protocol.
Add references to library documentation:
- SparkFun SCD30: getCO2(), getTemperature(), getHumidity()
- Adafruit VEML6075: readUVA(), readUVB()
- Adafruit VEML6070: readUV()

### DOC-010-003: docs/02-architecture.md
Update architecture section:
- "All sensors use manufacturer-provided libraries (Adafruit/SparkFun)"
- "No direct Wire protocol calls in application code"
- "I2C transport layer fully abstracted by libraries"

### DOC-010-004: docs/04-data-model.md
Minor update:
- Note that Scd30Values, Veml6075Values, Veml6070Values structs unchanged
- Libraries do not affect data representation

### DOC-010-005: Session file: FEATURE-SPEC-010-ANALYSIS.md
Create analysis document (in session workspace, not committed):
- Library research results
- Compatibility findings
- Breaking change analysis (none found)
- Library version selection rationale

---

## 12. Open Questions

### OQ-010-001: readReg16/writeReg16 Other Uses?
**Question:** Are readReg16() and writeReg16() used anywhere except VEML6075?
**Impact:** Determines if these functions can be safely deleted.
**Status:** Need to grep entire codebase.
**Resolution Path:** Search complete → confirm only VEML6075 → delete both functions.

### OQ-010-002: TSL2591 Micro-Benchmark Purpose?
**Question:** Is the TSL2591 micro-benchmark (lines 1388-1399) used for production monitoring or historical data collection?
**Impact:** Determines if it should be removed or refactored with library calls.
**Status:** Appears to be development/test code, not production.
**Resolution Path:** Confirm with stakeholder → remove or refactor.

### OQ-010-003: SCD30 Measurement API Equivalence?
**Question:** Does SparkFun library's getCO2/getTemperature/getHumidity() provide same precision/timing as readScd30()?
**Impact:** Determines if 1:1 replacement is appropriate.
**Status:** Libraries documented to be sensor-compliant.
**Resolution Path:** Read SparkFun library source; compare with current implementation.

### OQ-010-004: VEML6075 Compensation Registers?
**Question:** How are COMP1/COMP2 compensation registers handled by Adafruit library?
**Impact:** Determines if library API exposes them or if manual access needed.
**Status:** Adafruit documentation silent on this.
**Resolution Path:** Inspect Adafruit VEML6075 library source code.

### OQ-010-005: Version Compatibility?
**Question:** Are selected library versions compatible with current platformio.ini settings (espressif32 @ 6.5.0, Arduino Framework 2.0.14)?
**Impact:** Build might fail if dependencies mismatch.
**Status:** Adafruit libraries generally compatible, but need verification.
**Resolution Path:** Attempt build with new libraries; resolve conflicts if any.

---

## 13. Dependencies & Version Lock

### Current Dependencies (Already in platformio.ini):
```
adafruit/Adafruit INA219 @ ^1.2.3
adafruit/Adafruit TSL2591 Library @ ^1.4.5
adafruit/Adafruit SI1145 Library @ ^1.2.0
adafruit/Adafruit BME280 Library @ ^2.3.0
adafruit/Adafruit seesaw Library @ ^1.7.9
adafruit/Adafruit DS248x @ ^1.2.0
sparkfun/SparkFun SCD30 Arduino Library @ ^1.0.20
paulstoffregen/OneWire @ ^2.3.8
```

### New Dependencies (To Be Added):
```
adafruit/Adafruit VEML6075 Library @ ^1.1.1
adafruit/Adafruit VEML6070 @ ^1.1.2
```

### Removed Dependencies:
None. All existing libraries remain in use.

---

## 14. Acceptance Criteria

### AC-010-001
✅ All 10 sensors communicate via libraries (zero direct Wire calls).

### AC-010-002
✅ Compilation succeeds with no errors or warnings.

### AC-010-003
✅ All 10 sensor pages display data identically to previous behavior.

### AC-010-004
✅ Sensor detection at boot works correctly (all connected sensors detected).

### AC-010-005
✅ Graceful degradation works: missing sensors don't prevent startup.

### AC-010-006
✅ ASC control on SCD30 still functional (X-button toggle works).

### AC-010-007
✅ Codebase contains no dead code from removed functions.

### AC-010-008
✅ All tests pass (compilation + device tests).

### AC-010-009
✅ Documentation updated to reflect library-based architecture.

### AC-010-010
✅ No functional changes from user perspective (black-box equivalence).

---

## 15. Summary of Changes

| Component | Current | New | Impact |
|-----------|---------|-----|--------|
| VEML6075 | readReg16/Wire calls | Adafruit_VEML6075 library | Cleaner, vendor-maintained |
| VEML6070 | readVeml6070/Wire calls | Adafruit_VEML6070 library | Cleaner, vendor-maintained |
| SCD30 Measurements | readScd30/Wire.requestFrom | SparkFun library methods | Consistent with ASC control |
| TSL2591 Benchmark | Direct Wire I/O | Remove or use library | Simplifies codebase |
| SCD30 Legacy | scd30Crc/Command/Ready | DELETE | Remove dead code |
| readReg16/writeReg16 | Generic I2C helpers | DELETE if only VEML6075 used | Clean up helpers |
| Dependencies | 8 libraries | 10 libraries | Add 2 Adafruit UV libraries |
| Wire calls in app | 40 calls across 7 functions | 0 calls | Complete elimination ✓ |

