# Consistency Audit: sensor-pages-dashboard Documentation

**Audit Date:** 2026-09-21  
**Scope:** All documentation files (docs/01-12, 14)  
**Methodology:** Cross-file comparison of entities, values, requirements, business rules, UI specs, and reconstruction dependencies  
**Goal:** Determine documentation sufficiency for rebuilding application without source code access

---

## Executive Summary

**Total Issues Found:** 24  
**Critical Issues:** 3  
**High Issues:** 6  
**Medium Issues:** 10  
**Low Issues:** 5  

**Conclusion:** Documentation is **mostly sufficient** for reconstruction, but contains **significant gaps** in:
1. Timing intervals (conflicting values for gamepad)
2. Hardware specifications (missing exact register addresses, datasheets referenced)
3. Algorithm details (dew point formula, Coulomb accumulation precision, UV compensation coefficients)
4. Version pinning (Arduino Core, DallasTemperature sentinel values, TFT_eSPI fork)
5. Error handling (behavior on I2C timeouts, CRC failures, device disconnections)

**Recommendation:** Resolve all CRITICAL and HIGH issues before attempting reconstruction. Mark [UNKNOWN] values in reconstruction plan as research tasks requiring datasheet consultation.

---

## Issues by Category

---

## CRITICAL Issues

### ISSUE-001: Gamepad Timing Contradiction

| Property | Value |
|----------|-------|
| **ID** | ISSUE-001 |
| **Severity** | CRITICAL |
| **Affected Docs** | 01-requirements.md, 02-architecture.md, 09-configuration.md |
| **Problem** | Conflicting refresh rates for gamepad input: 25 ms vs 20 ms |

**Evidence:**

- **01-requirements.md (FR-004, line 89):**
  ```
  - Gamepad buttons read at 40 Hz (25 ms)
  ```

- **02-architecture.md (line 186):**
  ```
  - **Read by:** `readGamepadAt40Hz()` every 25 ms (only when available).
  ```

- **09-configuration.md (line 238):**
  ```
  | Gamepad input | 25 ms | INPUT_REFRESH_MS (implied 20ms, line 48) | loop line 623 | 40 Hz gamepad sampling |
  ```
  Note: "implied 20ms" contradicts "25 ms" in same row

**Problem Description:**  
Three documents give conflicting values. 01-requirements says "40 Hz (25 ms)". 02-architecture confirms "25 ms". But 09-configuration says "INPUT_REFRESH_MS (implied 20ms)" in same row that also says "25 ms". This creates ambiguity: is it 20 ms or 25 ms?

**Impact:**  
Reconstruction will implement wrong polling frequency, potentially breaking:
- Page navigation responsiveness
- Button press detection timing
- Loop cycle timing calculations

**Proposed Clarification:**
1. Verify source code line 623 actual gate interval
2. Confirm 40 Hz = 25 ms (correct: 1000 ms / 40 Hz = 25 ms)
3. Remove "implied 20ms" note from 09-configuration.md
4. Specify constant name: `GAMEPAD_REFRESH_MS = 25`

**Test Case:**
```cpp
// Verify 40 Hz = 25 ms conversion
uint32_t freq_hz = 40;
uint32_t interval_ms = 1000 / freq_hz;
ASSERT_EQUAL(interval_ms, 25);  // Must be 25, not 20
```

---

### ISSUE-002: Missing Critical Algorithm: Dew Point Calculation

| Property | Value |
|----------|-------|
| **ID** | ISSUE-002 |
| **Severity** | CRITICAL |
| **Affected Docs** | 07-ui.md (Page 0 BME280), 04-data-model.md |
| **Problem** | Dew point formula not specified; only label documented |

**Evidence:**

- **07-ui.md (Page 0, line 153):**
  ```
  | Label | "Dew point:" | 172 | White |
  | Value | `${dewPoint}°C` (2 decimals) | 172 | Cyan |
  ```

- **04-data-model.md:**
  - No dewPoint field in Bme280Values struct
  - No computation documented

- **14-reconstruction-plan.md (Phase 7, line 767):**
  ```
  [BLOCKED BY MISSING INFORMATION]: Exact dew point calculation formula not in documentation
  ```

**Problem Description:**  
Page 0 displays "Dew point" but:
1. No dew point field exists in data model (Bme280Values)
2. No computation formula specified
3. Reconstruction plan marks as blocked

**Multiple formulas exist:**
- Simple approximation: `dewPoint = temp - ((100 - humidity) / 5)`
- Magnus formula (more accurate)
- Arden Buck equation (empirical)

Without exact formula, reconstructed code will have wrong dew point values.

**Impact:**  
- Displayed dew point values incorrect
- Inconsistent with original application behavior
- User may rely on dew point for environmental decisions (HVAC, etc.)

**Proposed Clarification:**
1. Check original source code `main.cpp` for exact formula
2. Document formula with coefficients and precision
3. Add `float dewPoint` field to Bme280Values struct
4. Add computation to `readBme280()` function specification
5. Specify rounding/precision rules

**Test Case:**
```cpp
// Example: if formula is Magnus approximation
float temp = 20.0f;
float humidity = 60.0f;
float expected_dew = 11.9f;  // approximate
float computed_dew = temp - ((100 - humidity) / 5);
ASSERT_WITHIN(computed_dew, expected_dew, 1.0f);
```

---

### ISSUE-003: Gamepad Page Navigation Disabled Without Sensor

| Property | Value |
|----------|-------|
| **ID** | ISSUE-003 |
| **Severity** | CRITICAL |
| **Affected Docs** | 06-business-rules.md (BR-003), 14-reconstruction-plan.md (Phase 5) |
| **Problem** | Application becomes non-navigable if gamepad unavailable; no fallback documented |

**Evidence:**

- **06-business-rules.md (BR-003, line 47-78):**
  ```
  **Description:**  
  Gamepad Y/A buttons control page navigation. If gamepad not detected at boot 
  (`available.gamepad = false`), page navigation via buttons is disabled.
  
  **Behavior:**
  If available.gamepad is false, entire block skipped
  ...
  Only workaround: restart with gamepad connected
  ```

- **14-reconstruction-plan.md (Phase 5, line 456):**
  ```
  - Print warnings to Serial if optional sensors missing (e.g., gamepad)
  ```

**Problem Description:**  
BR-003 explicitly documents that gamepad absence disables all page navigation. No fallback navigation mechanism (encoder dial alternative, touch screen, serial commands) documented.

If gamepad fails/disconnects after boot, user is stuck on current page forever.

**Unrealistic Assumption:**  
Project documented as allowing graceful degradation (FR-001), but page navigation is non-optional. This contradicts graceful degradation principle.

**Missing Specification:**
- No fallback navigation method documented
- No alternative input specified
- No recovery procedure specified

**Impact:**  
- Application unusable without gamepad
- Contradicts stated non-functional requirement (graceful degradation)
- Reconstruction must decide: implement fallback or accept design flaw?

**Proposed Clarification:**
1. Specify if encoder dial should provide fallback navigation (Y=prev, A=next via dial)
2. Document if missing gamepad is truly fatal or acceptable
3. Add fallback page navigation mechanism, OR
4. Document as known limitation: "Gamepad is required; absence leaves application non-navigable"

**Test Case:**
```cpp
// Test 1: Navigation works WITH gamepad
available.gamepad = true;
// Y button press → should change page

// Test 2: Navigation disabled WITHOUT gamepad (current behavior)
available.gamepad = false;
// Y button press → should NOT change page (or crash/block)
```

---

## HIGH Issues

### ISSUE-004: Inconsistent Timing Gate Terminology and Constant Names

| Property | Value |
|----------|-------|
| **ID** | ISSUE-004 |
| **Severity** | HIGH |
| **Affected Docs** | 06-business-rules.md, 09-configuration.md, 14-reconstruction-plan.md |
| **Problem** | Constant names inconsistent; some referenced as `_MS` suffix, others hardcoded |

**Evidence:**

- **09-configuration.md (Timing table, line 237-242):**
  ```
  | Sensor polling | 500 ms | `SENSOR_REFRESH_MS` | line 47 |
  | Gamepad input | 25 ms | INPUT_REFRESH_MS (implied 20ms, line 48) | loop line 623 |
  | Encoder input | 20 ms | `INPUT_REFRESH_MS` | line 48 |
  | INA219 sampling | 20 µs | Not defined (gate line 644) | hardcoded |
  | DS18B20 rescan | 3000 ms | `DS18B20_RESCAN_MS` | line 48 |
  | DS18B20 conversion | 100 ms | Fixed (line 315) | `updateDs18b20()` |
  | Page redraw (live) | 20 ms | `INPUT_REFRESH_MS` | line 673 |
  | Page redraw (slow) | 500 ms | `SENSOR_REFRESH_MS` | line 677 |
  ```

- **06-business-rules.md (BR-004, line 160):**
  ```
  const uint32_t nowUs = micros();
  if (ina219Values.initialized) {
    const uint32_t elapsedUs = nowUs - ina219Values.lastSampleUs;
    if (elapsedUs < 200000) {  // Hardcoded 200 ms
  ```

**Problem Description:**

1. **INA219 time-gap sanity check:** Hardcoded `200000` microseconds (200 ms) — no named constant
2. **DS18B20 conversion wait:** Hardcoded `100 ms` — no named constant
3. **INA219 sampling:** "Not defined (gate line 644)" — uses timing gate, not named constant
4. **Page redraw timing:** Mixes constant names and hardcoded values

**Impact:**
- Confusing for developers: which values are configurable, which are hardcoded?
- Reconstruction must guess which values should be #define constants
- No clear naming convention for timing-related constants

**Proposed Clarification:**
1. Define ALL timing intervals as named constants:
   ```cpp
   #define SENSOR_REFRESH_MS 500
   #define GAMEPAD_REFRESH_MS 25
   #define ENCODER_REFRESH_MS 20
   #define INA219_SAMPLE_US 20
   #define INA219_GAP_SANITY_MS 200
   #define DS18B20_RESCAN_MS 3000
   #define DS18B20_CONVERSION_MS 100
   #define PAGE_REDRAW_LIVE_MS 20
   #define PAGE_REDRAW_SLOW_MS 500
   ```

2. Document which can be modified vs hardcoded by design

---

### ISSUE-005: UV Compensation Formula Coefficients Undocumented

| Property | Value |
|----------|-------|
| **ID** | ISSUE-005 |
| **Severity** | HIGH |
| **Affected Docs** | 04-data-model.md, 06-business-rules.md, 14-reconstruction-plan.md |
| **Problem** | UV compensation uses specific numerical coefficients without derivation or reference |

**Evidence:**

- **04-data-model.md (Veml6075Values, line 73-76):**
  ```
  | `uvaComp` | float | Computed | `readVeml6075()` every 500ms | **Formula:** `max(0, uva - 2.22*comp1 - 1.33*comp2)` |
  | `uvbComp` | float | Computed | `readVeml6075()` every 500ms | **Formula:** `max(0, uvb - 2.95*comp1 - 1.74*comp2)` |
  | `index` | float | Computed | `readVeml6075()` every 500ms | **Formula:** `((uvaComp*0.001461 + uvbComp*0.002591) * 0.5)` |
  ```

- **06-business-rules.md (BR-006, line 279-291):**
  ```cpp
  uvaComp = max(0.0f, (float)uva - 2.22f * comp1 - 1.33f * comp2);
  uvbComp = max(0.0f, (float)uvb - 2.95f * comp1 - 1.74f * comp2);
  index   = ((uvaComp * 0.001461f) + (uvbComp * 0.002591f)) * 0.5f;
  ```

**Problem Description:**

Six magic numbers (2.22, 1.33, 2.95, 1.74, 0.001461, 0.002591) with:
- No derivation
- No reference to VEML6075 datasheet
- No uncertainty bounds
- No precision/rounding specification

Questions:
- Are these exact sensor calibration values or approximations?
- Does precision matter (e.g., 0.001461 vs 0.00146)?
- Are these coefficients from VEML6075 datasheet or empirical?
- Do they change per sensor unit?

**Impact:**
- Reconstructed code will have incorrect UV index if coefficients are sensor-specific
- If coefficients are wrong, environmental readings unreliable
- No way to verify correctness without original hardware

**Proposed Clarification:**
1. Document source of coefficients (datasheet, empirical, calibration)
2. Add precision requirements (if 4 decimals matter, say so)
3. Add comment in formula with reference to section of datasheet
4. Specify if these vary per sensor or are universal

---

### ISSUE-006: Coulomb Accumulation Precision and Overflow Unchecked

| Property | Value |
|----------|-------|
| **ID** | ISSUE-006 |
| **Severity** | HIGH |
| **Affected Docs** | 01-requirements.md, 04-data-model.md, 06-business-rules.md |
| **Problem** | Coulomb counter is unbounded float with no overflow detection or handling |

**Evidence:**

- **01-requirements.md (FR-006, line 121):**
  ```
  - Counter unbounded (potential overflow at ~10 billion mAh)
  ```

- **04-data-model.md (Ina219Values, line 118):**
  ```
  | `chargeMah` | float | 4 B | [0, ∞) | ... | **Persists across cycles; accumulates unbounded** |
  ```

- **06-business-rules.md (BR-004, line 195):**
  ```
  **Known Limitation:** Accumulator never resets except on reboot.
  ```

**Problem Description:**

1. **Unbounded accumulation:** Coulomb counter only resets on reboot, accumulates indefinitely
2. **Float overflow:** IEEE 754 32-bit float loses precision after ~10 billion mAh
3. **No error detection:** No check for overflow, no warning displayed
4. **User impact:** After ~1.1 million hours (125 years) at 10 mA, precision lost
5. **Real-world scenario:** Industrial multi-year deployment may hit overflow

**Missing Spec:**
- How should overflow be handled? (display warning? wrap? saturate?)
- Should there be periodic user-visible reset option?
- What precision is acceptable? (mAh, Ah, kAh?)
- Is this documented as known limitation in UI?

**Impact:**
- Long-running deployments will show incorrect accumulated charge
- No warning to user
- Reconstruction code must decide: ignore or implement safeguard?

**Proposed Clarification:**
1. Document expected deployment lifetime
2. Specify overflow handling (warning? saturation? wrap?)
3. Consider implementing periodic reset (e.g., daily soft reset)
4. Display accumulated charge units appropriately (mAh vs Ah based on magnitude)
5. Add warning when approaching float overflow threshold

---

### ISSUE-007: VEML Sensor Register Addresses Not Fully Documented

| Property | Value |
|----------|-------|
| **ID** | ISSUE-007 |
| **Severity** | HIGH |
| **Affected Docs** | 09-configuration.md, 14-reconstruction-plan.md, 05-api.md |
| **Problem** | VEML6075 and VEML6070 register addresses incomplete; datasheets required for implementation |

**Evidence:**

- **09-configuration.md (note at line 180):**
  ```
  [BLOCKED BY MISSING INFORMATION]: Exact register addresses for VEML sensors not fully specified; refer to VEML datasheets
  ```

- **14-reconstruction-plan.md (Phase 9, line 1038):**
  ```
  [BLOCKED BY MISSING INFORMATION]: Exact register addresses; refer to VEML datasheets
  ```

- **06-business-rules.md (BR-005, line 1232):**
  ```
  Config Value: 0x0000 (hardcoded; [UNKNOWN] - exact meaning per VEML6075 datasheet).
  ```

**What IS documented:**

- **04-data-model.md (Veml6075Values):**
  ```
  UVA raw reading (0–65535): register 0x07
  UVB raw reading (0–65535): register 0x09
  Compensation factor 1: register 0x0A
  Compensation factor 2: register 0x0B
  ```

- **05-api.md (line 95):**
  ```
  writeReg16(0x10, 0x00, 0x0000);  // Write Config = 0x0000 (enable sensor)
  ```

**What IS NOT documented:**
- VEML6070 complete register map (only addresses 0x38/0x39 known, but what register is read at each?)
- VEML6075 config register (0x00) format: what does 0x0000 actually enable?
- Initialization sequence for VEML6075 (is 0x0000 write required at boot?)
- Any other VEML registers used (interrupt settings, shutdown, etc.)

**Impact:**
- Reconstruction cannot implement sensor drivers without consulting datasheets
- Blocking dependency on external documentation
- Risk of incorrect initialization sequences

**Proposed Clarification:**
1. Create Appendix with extracted register maps from datasheets:
   ```
   VEML6075 Registers:
   - 0x00: UV Config (read/write)
   - 0x07: UVA Data
   - 0x09: UVB Data
   - 0x0A: UVCOMP1 Data
   - 0x0B: UVCOMP2 Data
   
   VEML6070 Registers:
   - 0x38: Command (write) / Data LSB (read)
   - 0x39: Data MSB (read)
   ```

2. Document initialization sequence for each sensor
3. Specify bit fields for config registers

---

### ISSUE-008: SCD30 Protocol Details Refer to Datasheet (No Specification in Docs)

| Property | Value |
|----------|-------|
| **ID** | ISSUE-008 |
| **Severity** | HIGH |
| **Affected Docs** | 14-reconstruction-plan.md, 06-business-rules.md |
| **Problem** | SCD30 command protocol deferred to datasheet; commands/responses not fully specified |

**Evidence:**

- **14-reconstruction-plan.md (Phase 9, line 1030):**
  ```
  [BLOCKED BY MISSING INFORMATION]: Exact SCD30 command protocol details; refer to SCD30 datasheet
  ```

- **06-business-rules.md (BR-005, lines 210-240):**
  ```
  - Start measurement: `scd30Command(0x0010)`
  - Check ready: `scd30Command(0x0202)` — reads 2-byte status
  - Read data: `scd30Command(0x0300)` — reads 18 bytes (3 floats with CRC8)
  ```

**Partially Documented:**
- 3 command codes (0x0010, 0x0202, 0x0300)
- Response sizes (2 bytes, 18 bytes)
- CRC8 validation requirement

**Not Documented:**
- Command byte order (big-endian? little-endian?)
- Timing: delay between start measurement and data ready?
- CRC8 polynomial for SCD30 (is it same as documented 0x31?)
- Response parsing: byte order of floats?
- Error responses: what if data not ready?
- Initialization sequence: any config commands needed at boot?

**Impact:**
- Reconstruction blocked without SCD30 datasheet
- I2C protocol implementation error-prone without full spec
- CRC validation may fail if polynomial wrong

**Proposed Clarification:**
1. Document byte-by-byte protocol:
   ```
   Command 0x0010: Start measurement
   - TX: [0x00, 0x10] (big-endian)
   - RX: None
   - Timing: measurements ready ~2 seconds later
   
   Command 0x0202: Get data ready status
   - TX: [0x02, 0x02]
   - RX: [MSB, LSB] (big-endian uint16)
   - Response: 1 = data ready, 0 = not ready
   
   Command 0x0300: Read measurement
   - TX: [0x03, 0x00]
   - RX: 18 bytes = [CO2_MSB, CO2_mid, CO2_LSB, CO2_CRC, TEMP_MSB, ...]
   - Parse: CO2, TEMP, HUMIDITY as IEEE 754 floats (big-endian)
   ```

2. Include exact CRC8 implementation for SCD30 (if different from 0x31)
3. Add timing diagram for measurement cycle

---

### ISSUE-009: TFT_eSPI Library Version Not Pinned

| Property | Value |
|----------|-------|
| **ID** | ISSUE-009 |
| **Severity** | HIGH |
| **Affected Docs** | 12-dependencies.md, 14-reconstruction-plan.md, lib/Freenove/TFT_eSPI |
| **Problem** | Freenove TFT_eSPI fork version not specified; static copy in repo not versioned |

**Evidence:**

- **12-dependencies.md (TFT_eSPI section, line 203-206):**
  ```
  | **Version** | Freenove fork of TFT_eSPI (version unknown; locked to local copy) |
  | **Original** | [bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) |
  | **Purpose** | SPI-based TFT display driver (ST7796, ILI9341, etc.) |
  ...
  | **Status** | ⚠️ Static | Freenove's fork may lag behind upstream |
  ```

- **14-reconstruction-plan.md (Phase 1, line 95):**
  ```
  [BLOCKED BY MISSING INFORMATION]: Exact Freenove TFT_eSPI version not pinned in documentation
  ```

**Problem Description:**

1. **No version number:** Freenove fork has no version identifier
2. **No commit hash:** No way to know which version of fork is in repo
3. **No upstream link:** Unclear which TFT_eSPI version it was forked from
4. **No changelog:** No record of Freenove's modifications
5. **Static copy risk:** If repo is cloned without submodule history, version lost

**Impact:**
- Reconstruction cannot fetch correct library version
- If someone rebuilds on clean repo, may get wrong version
- Freenove customizations unknown (display controller support? bug fixes? performance?)
- No way to update fork without risk of breaking changes

**Proposed Clarification:**
1. Create version file in `lib/Freenove/TFT_eSPI/VERSION.txt`:
   ```
   Freenove Fork of TFT_eSPI
   Based on upstream commit: abc123def456
   Fork date: 2025-01
   Freenove modifications: ST7796 driver, FNK0114N board support
   ```

2. Document in 12-dependencies.md:
   ```
   Fork version: 2.5.0-freenove-1
   Base upstream: bodmer/TFT_eSPI @ v2.5.0
   Fork repository: https://github.com/Freenove/...
   ```

3. Add git submodule (if using git) for reproducibility

---

### ISSUE-010: Arduino Core Version Unspecified

| Property | Value |
|----------|-------|
| **ID** | ISSUE-010 |
| **Severity** | HIGH |
| **Affected Docs** | 12-dependencies.md, 02-architecture.md |
| **Problem** | Arduino Core for ESP32 version not pinned; version-dependent behavior not documented |

**Evidence:**

- **12-dependencies.md (line 66):**
  ```
  | **Core Version** | [UNKNOWN] | Depends on espressif32 platform version |
  | **Typical Range** | Arduino Core for ESP32 v2.0–v3.1 | Auto-installed by PlatformIO |
  ```

- **02-architecture.md (line 27):**
  ```
  | **Framework** | Arduino Core Runtime | Arduino core for ESP32 (typically v2.x or v3.x) | FreeRTOS kernel (internal; application uses superloop only) |
  ```

- **12-dependencies.md (note):**
  ```
  [UNKNOWN] Exact Freenove TFT_eSPI version not pinned in documentation
  ```

**Problem Description:**

1. **Version drift:** "Auto-installed by PlatformIO" means unpredictable version
2. **Breaking changes:** Arduino Core v2.x → v3.x had SPIFFS → LittleFS migration (documented in SPIFFS.h shim)
3. **Behavior changes:** Future versions may have different:
   - Timer overflow handling (critical for timing gates)
   - Wire (I2C) library behavior
   - GPIO timing
   - Memory layout

**Risk:**
- User rebuilds 2 years later with Arduino Core v4.x
- Timing gates behave differently (32-bit overflow behavior?)
- I2C bus behaves differently
- Application breaks

**Proposed Clarification:**
1. Pin Arduino Core version in `platformio.ini`:
   ```ini
   platform = espressif32 @ 6.5.0  # Locks Arduino Core version
   ```

2. Document in 12-dependencies.md:
   ```
   Arduino Core: v3.0.3 (auto-installed with espressif32 platform)
   Tested with: Arduino Core v3.0.0 through v3.0.3
   Not tested with: v2.x (may work, untested)
   ```

3. Document version-dependent features:
   ```
   - Timer overflow: millis() wraps every 49 days (same in v2.x, v3.x)
   - SPIFFS vs LittleFS: Core v3.x replaces SPIFFS (included/SPIFFS.h handles)
   - Wire library timeouts: See issue #UNKNOWN in Arduino issues
   ```

---

## MEDIUM Issues

### ISSUE-011: Encoder Reading Frequency Inconsistent Documentation

| Property | Value |
|----------|-------|
| **ID** | ISSUE-011 |
| **Severity** | MEDIUM |
| **Affected Docs** | 01-requirements.md, 02-architecture.md, 09-configuration.md |
| **Problem** | Encoder read frequency documented as both "50 Hz" and "20 ms", with conditional frequency |

**Evidence:**

- **01-requirements.md (FR-004, line 90):**
  ```
  - Encoder read at 50 Hz (20 ms) when on encoder page or page changing
  ```

- **02-architecture.md (line 172-173):**
  ```
  - **Read by:** `readEncoderAt50Hz()` every 20 ms (only when page==4 or on page change).
  ```

- **09-configuration.md (line 239):**
  ```
  | Encoder input | 20 ms | `INPUT_REFRESH_MS` | line 48 | 50 Hz encoder sampling (page 4 only) |
  ```

**Clarifications Needed:**

1. **50 Hz = 20 ms:** Correct (1000 / 50 = 20 ms)
2. **Conditional frequency:** "only when on encoder page or page changing"
   - Does this mean: NOT read on other pages?
   - Or: Read every cycle, but displayed only on page 4?
   - Or: Read only when page==4, idle otherwise?

**Impact on Reconstruction:**
- If encoder not read on non-page-4 pages, LED control stops responsding
- If encoder read but not displayed, unnecessary power consumption
- If conditional gate logic complex, easy to implement wrong

**Proposed Clarification:**
Document the gate logic explicitly:
```
Encoder read frequency:
- When currentPage == 4 (Encoder page): Read at 50 Hz (20 ms)
- When currentPage changing (page != lastPage): Read at 50 Hz (20 ms)
- All other times: Do NOT read encoder

This means:
- Encoder dial input only changes brightness on page 4
- Encoder button press only cycles color on page 4
- If user switches to page 0 (BME280), encoder input is ignored until back to page 4
- LED state persists on other pages (color/brightness frozen)
```

---

### ISSUE-012: Error Screen Behavior on Missing Sensors Not Fully Tested

| Property | Value |
|----------|-------|
| **ID** | ISSUE-012 |
| **Severity** | MEDIUM |
| **Affected Docs** | 06-business-rules.md (BR-017), 07-ui.md, 10-testing.md |
| **Problem** | Error screen for missing sensors documented but test coverage incomplete |

**Evidence:**

- **06-business-rules.md (BR-017, line 1385):**
  ```
  | BR-017 | Error Display | Integration | MEDIUM | Functional | Navigate to missing sensor; verify error screen renders |
  ```

- **10-testing.md (line 50):**
  ```
  | Sensor missing → error screen (no crash) | Graceful degradation | Integration |
  ```

- **07-ui.md (multiple locations):**
  ```
  **Sensor Missing Behavior:**
  If [SENSOR] not detected at boot (`available.[sensor] = false`):
  - Header displays normally
  - Error screen shows "Sensor nicht vorhanden" (red, font 2)
  - Navigation still works (Y/A buttons)
  ```

**Unclear Behavior:**

1. **Error screen for ALL pages:** Does every sensor page have error screen, or only some?
2. **Error screen consistency:** Are all error screens identical or sensor-specific?
3. **Navigation from error screen:** Y/A buttons definitely work?
4. **Footer on error screen:** What footer text shown? ("Keine Daten verfuegbar"?)
5. **Partial sensor groups:** What if VEML6075 is missing but VEML6070 present? (both map to pages 8-9)

**Impact:**
- Reconstruction code must guess error screen behavior for each sensor
- No test cases validate error screen rendering
- Risk of missing edge case where error screen crashes/displays incorrectly

**Proposed Clarification:**
1. Create test matrix in 10-testing.md:
   ```
   | Sensor | Page | Error Screen Text | Footer | Navigation |
   |--------|------|-------------------|--------|-----------|
   | BME280 | 0 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | DS18B20 | 1 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | Gamepad | 2 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | INA219 | 3 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | Encoder | 4 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | SCD30 | 5 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | SI1145 | 6 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | TSL2591 | 7 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | VEML6070 | 8 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   | VEML6075 | 9 | "Sensor nicht vorhanden" | "Keine Daten..." | Y/A work |
   ```

2. Add test cases for each error screen
3. Specify exact error text (German, case-sensitive?)

---

### ISSUE-013: I2C Wire Library Timeout Behavior Unspecified

| Property | Value |
|----------|-------|
| **ID** | ISSUE-013 |
| **Severity** | MEDIUM |
| **Affected Docs** | 02-architecture.md, 09-configuration.md, 06-business-rules.md |
| **Problem** | Wire library timeout handling deferred to internal implementation; no explicit specification |

**Evidence:**

- **02-architecture.md (line 758):**
  ```
  | **I2C bus congestion/timeout** | Wire library's internal timeouts [UNKNOWN] | No explicit recovery; next poll attempt in 20–500 ms |
  ```

- **09-configuration.md (line 171):**
  ```
  | **Timeout** | [UNKNOWN] | Managed by Arduino Wire library |
  ```

- **06-business-rules.md (BR-002, line 113):**
  ```
  **Behavior:**
  ```cpp
  bool readVeml6075(Veml6075Values &values) {
    if (!readReg16(...)) return false;  // I2C error
  ```

**Problem Description:**

1. **No timeout value:** How long does Wire library wait before giving up?
   - 100ms? 1000ms? Configurable?
   - If too long, main loop blocked (breaks real-time)
   - If too short, spurious errors

2. **No error recovery:** What does Wire return on timeout?
   - False? Error code? Thrown exception?
   - Can application set custom timeout?

3. **Silent failure:** No indication to user that sensor read failed
   - Stale data displayed indefinitely
   - No timeout indicator in error screen

**Impact:**
- If I2C bus jammed, application might block indefinitely (or timeout too long)
- Real-time constraints violated
- No way to debug I2C issues without Serial console

**Proposed Clarification:**
1. Document Arduino Wire library timeout (typically 1000 ms for Arduino core)
2. Specify if timeout is configurable in firmware
3. Add Serial debug output on I2C timeout (if enabled)
4. Consider adding timeout counter visible on sensor page (e.g., "[timeout count]" indicator)

---

### ISSUE-014: DallasTemperature Sentinel Value Library Version Dependent

| Property | Value |
|----------|-------|
| **ID** | ISSUE-014 |
| **Severity** | MEDIUM |
| **Affected Docs** | 04-data-model.md, 06-business-rules.md, 07-ui.md |
| **Problem** | DS18B20 disconnection sentinel value `DEVICE_DISCONNECTED_C` defined by library, not in firmware |

**Evidence:**

- **04-data-model.md (line 415):**
  ```
  // [UNKNOWN]: Exact sentinel value depends on DallasTemperature library version
  ```

- **06-business-rules.md (BR-014, line 741):**
  ```
  - [UNKNOWN] Exact sentinel value: not defined in firmware; from DallasTemperature library
  ```

- **07-ui.md (Page 1 DS18B20, line 195):**
  ```
  **Temperature Sentinel:**
  - If `temperature <= DEVICE_DISCONNECTED_C` (~–127 °C): display "disconnected" (red)
  ```

**Problem Description:**

1. **Magic value:** Sentinel value is "~–127 °C" but exact value not documented
2. **Version dependent:** Different DallasTemperature library versions may use different sentinel:
   - v3.x: -127.0°C
   - v4.x: Different value?
   - Future versions: Unknown

3. **Comparison logic:** Firmware uses `<=` comparison (assumes all invalid temps <= sentinel)
   - But what if sensor returns error code as float?
   - What if sensor returns NaN?

4. **Hardware range:** Real DS18B20 operates -55 to +125 °C
   - So -127 is safe sentinel (outside range)
   - But is it ACTUALLY -127, or -124, or -999?

**Impact:**
- Reconstruction may implement wrong sentinel check
- Disconnected sensors not detected if sentinel value wrong
- Different library versions may behave differently

**Proposed Clarification:**
1. Extract exact sentinel from DallasTemperature library version 4.0.4:
   ```cpp
   #include <DallasTemperature.h>
   // DallasTemperature uses DEVICE_DISCONNECTED_C = -127.0
   // Confirmed in library.h:
   //   #define DEVICE_DISCONNECTED_C -127.0
   ```

2. Document in firmware:
   ```cpp
   // DS18B20 disconnection sentinel
   // From DallasTemperature v4.0.4
   constexpr float DS18B20_DISCONNECTED = DEVICE_DISCONNECTED_C;  // -127.0°C
   ```

3. Add test case:
   ```cpp
   // Verify sentinel matches library
   ASSERT_EQUAL(DEVICE_DISCONNECTED_C, -127.0f);
   ```

---

### ISSUE-015: Page Navigation Button Names Inconsistent (Y/A vs semantics)

| Property | Value |
|----------|-------|
| **ID** | ISSUE-015 |
| **Severity** | MEDIUM |
| **Affected Docs** | 06-business-rules.md, 07-ui.md, 05-api.md |
| **Problem** | Button names (Y/A) used consistently but semantic meaning (previous/next) sometimes missing |

**Evidence:**

- **06-business-rules.md (BR-012, line 606):**
  ```
  Gamepad Y button (active-low) navigates to previous page; A button navigates to next page.
  ```

- **07-ui.md (Page 0 BME280, line 147):**
  ```
  - **Y Button:** Navigate to page 9 (VEML6075)
  - **A Button:** Navigate to page 1 (DS18B20)
  ```

- **05-api.md (line 339):**
  ```
  currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;  // Previous page
  currentPage = (currentPage + 1) % PAGE_COUNT;               // Next page
  ```

**Clarifications Needed:**

1. **Button semantics:** Y = previous (–1 page), A = next (+1 page) — CONSISTENT
2. **Error screen documentation:** Do Y/A still work from error screens?
3. **Footer text:** All pages show "Y/A: Seite wechseln" (German) — but exact text should be verified

**Low impact** (mostly consistent), but worth noting for completeness.

---

### ISSUE-016: CRC8 Polynomial Not Verified Against SCD30 Datasheet

| Property | Value |
|----------|-------|
| **ID** | ISSUE-016 |
| **Severity** | MEDIUM |
| **Affected Docs** | 06-business-rules.md, 01-requirements.md |
| **Problem** | CRC8 polynomial documented as 0x31 but not verified against SCD30 datasheet |

**Evidence:**

- **01-requirements.md (FR-007, line 126-127):**
  ```
  - Polynomial: 0x31
  - Initial value: 0xFF
  ```

- **06-business-rules.md (BR-005, line 334-337):**
  ```cpp
  uint8_t scd30Crc(uint8_t* data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
      crc ^= data[i];
      for (uint8_t j = 0; j < 8; j++) {
        if (crc & 0x80)
          crc = (crc << 1) ^ 0x31;
        else
          crc = crc << 1;
      }
    }
    return crc;
  }
  ```

**Problem Description:**

1. **No datasheet reference:** Where is 0x31 polynomial from?
   - SCD30 datasheet?
   - Generic CRC8-ATM?
   - Sensiltion proprietary?

2. **No verification:** No test vector documented
   - Can't verify implementation without known good CRC values
   - If polynomial wrong, CRC validation fails

3. **Initial value:** 0xFF claimed, but is this correct?
   - Sensiltion CRC8 uses 0xFF init?
   - Final XOR value (if any) not documented?

**Impact:**
- If polynomial wrong, CRC validation never passes
- SCD30 data always rejected
- Reconstruction blocked until datasheet consulted

**Proposed Clarification:**
1. Add test vectors to 06-business-rules.md:
   ```
   Example: CO2 = 400 ppm, TEMP = 20°C, HUMIDITY = 60%
   Sensor response: [0x01, 0x90, 0xC?, 0x41, 0xA0, 0x00, 0x42, 0x70, 0x00]
   Expected CRC bytes: [0xC?, 0x00, 0x00]
   
   Test with scd30Crc():
   crc1 = scd30Crc([0x01, 0x90], 2) == expected_crc1
   crc2 = scd30Crc([0x41, 0xA0], 2) == expected_crc2
   crc3 = scd30Crc([0x42, 0x70], 2) == expected_crc3
   ```

2. Reference exact SCD30 datasheet section:
   ```
   CRC8 Algorithm: Sensiltion standard (polynomial 0x31, init 0xFF)
   Reference: SCD30 Interface Description v1.1, Section 5.2
   ```

---

### ISSUE-017: Gamepad Joystick Inversion Not Universally Verified

| Property | Value |
|----------|-------|
| **ID** | ISSUE-017 |
| **Severity** | MEDIUM |
| **Affected Docs** | 05-api.md, 07-ui.md |
| **Problem** | Gamepad joystick X/Y coordinates inverted (1023-raw), but not clear if hardware or software inversion |

**Evidence:**

- **05-api.md (Gamepad device, line 306-317):**
  ```
  X axis (GPIO 14 analog)
    - Raw: 0–1023 (left to right)
    - Firmware transformation: invert (1023 - raw)
    - Final: 1023 = left, 0 = right
  Y axis (GPIO 15 analog)
    - Raw: 0–1023 (top to bottom)
    - Firmware transformation: invert (1023 - raw)
    - Final: 1023 = up, 0 = down
  ```

- **07-ui.md (Page 2 Gamepad, line 227):**
  ```
  - Joystick X: 0–1023 (inverted: `1023 - raw`)
  - Joystick Y: 0–1023 (inverted: `1023 - raw`)
  ```

**Problem Description:**

1. **Unclear rationale:** WHY invert?
   - Is hardware wired backwards?
   - Is this for UI coordinate system (top-left origin)?
   - Is this for game controller convention?

2. **Not tested:** No test confirms inversion is correct/necessary
   - What if hardware is actually normal orientation?
   - User may expect different stick orientation

3. **Possible user confusion:** Display shows "X=1023" when stick all the way left
   - Unintuitive (usually 0 = min, 1023 = max)

**Impact on Reconstruction:**
- Removing inversion breaks user expectations (if they're used to inverted)
- Keeping inversion may be wrong if hardware is standard

**Proposed Clarification:**
1. Document rationale:
   ```
   Gamepad joystick inversion rationale:
   - Freenove gamepad has joystick with max-left=1023, max-right=0
   - Firmware inverts to match standard convention: 0=min, 1023=max
   - This makes: left stick pos=0, right stick pos=1023
   ```

2. Add test with known hardware:
   ```
   Physical: Stick all the way left
   Raw ADC: 1023
   After inversion: 0 (software maps to "minimum X")
   Display: "X=0"
   ```

---

### ISSUE-018: LED Brightness Delta Multiplier (×8) Not Documented

| Property | Value |
|----------|-------|
| **ID** | ISSUE-018 |
| **Severity** | MEDIUM |
| **Affected Docs** | 06-business-rules.md, 04-data-model.md |
| **Problem** | Encoder dial delta multiplied by 8 to control brightness, but scaling rationale not explained |

**Evidence:**

- **06-business-rules.md (BR-011, line 316-319):**
  ```
  **Brightness Update Formula:**
  ```cpp
  int32_t nextBrightness = (int32_t)encoderBrightness + encoderValues.delta * 8;
  encoderBrightness = (uint8_t)constrain(nextBrightness, 0, 255);
  ```

- **04-data-model.md (EncoderValues, line 246):**
  ```
  | `brightness` | uint8_t | Computed | `readEncoderAt50Hz()` | 0–255 | `constrain(brightness + delta*8, 0, 255)` on dial turn | Updated on dial turn; persists until next turn |
  ```

**Problem Description:**

1. **Magic number (8):** Why multiply by 8?
   - Each encoder tick = ±8 brightness levels?
   - Empirical user experience choice?
   - Hardware dependency?

2. **Not documented:** No rationale for scaling factor
   - User experience untested (is ×8 responsive enough? too sensitive?)
   - If changed later, no clear reason

3. **Possible missing info:** Encoder resolution unknown
   - Does each physical notch = ±1 delta? ±4 delta? Hardware dependent

**Impact:**
- Reconstruction implements same ×8 multiplier, but doesn't know if correct
- If encoder hardware different, ×8 might be wrong scaling

**Proposed Clarification:**
1. Document rationale:
   ```
   LED brightness delta scaling:
   - Encoder delta per tick: ±1 (from seesaw library)
   - Scaling factor: ×8 (empirically chosen for responsive user feedback)
   - Result: Each encoder notch changes brightness by ~3% (8/255)
   - Alternative: ×1 (slower), ×16 (faster)
   ```

2. Add configuration note:
   ```
   To adjust LED responsiveness, modify LED_BRIGHTNESS_DELTA_SCALE constant:
   - Current: 8 (each notch = 8 brightness levels)
   - Slower: 4 (each notch = 4 brightness levels)
   - Faster: 16 (each notch = 16 brightness levels)
   ```

---

## LOW Issues

### ISSUE-019: Page Layout Y-Coordinates Not Validated Across All Pages

| Property | Value |
|----------|-------|
| **ID** | ISSUE-019 |
| **Severity** | LOW |
| **Affected Docs** | 07-ui.md (all page definitions) |
| **Problem** | Page layouts show different Y-coordinate schemes; no unified validation |

**Evidence:**

- **07-ui.md (Common Layout, line 47-58):**
  ```
  ├─────────────────────────────────────────┤  y=42 (divider line)
  │ [DATA ROWS - Label & Value]             │  y=52+ (first row at 52, height 24 per row)
  │ [DATA ROWS]                             │
  ...
  ├─────────────────────────────────────────┤
  │ [FOOTER - Footer text]                  │  y=294
  ```

- **07-ui.md (Page 0 BME280, line 111-121):**
  ```
  | Label | "I2C address:" | 52 | White |
  | Value | "0x77" | 52 | Yellow |
  | Label | "Temperature:" | 76 | White |
  | Value | `${temp}°C` | 76 | Green |
  | Label | "Humidity:" | 100 | White |
  | Value | `${humidity}%` | 100 | Green |
  | Label | "Pressure:" | 124 | White |
  | Value | `${pressure}hPa` | 124 | Orange |
  | Label | "Altitude:" | 148 | White |
  | Value | `${altitude}m` | 148 | Cyan |
  | Label | "Dew point:" | 172 | White |
  | Value | `${dewPoint}°C` | 172 | Cyan |
  ```

- **07-ui.md (Page 1 DS18B20, line 219-229):**
  ```
  | Label | "OneWire pin:" | 52 | White |
  | Value | "GPIO23" | 52 | Yellow |
  | Label | "Sensor count:" | 76 | White |
  | Value | `${count}` | 76 | Cyan |
  | Sensor Row | `#${index} ${address}` | 100+(i×24) | White/Yellow |
  | Value | `${temp}°C` or "disconnected" | 100+(i×24) | Green or Red |
  ```

**Problem Description:**

1. **Y-coordinate consistency:** All pages seem to follow 24-pixel row height, but:
   - Page 0 (BME280): 6 data rows + 1 address row = 7 rows total (fits in ~170 pixels)
   - Page 1 (DS18B20): Variable rows (1-8 devices × 24 pixels each) — may overflow
   - Gamepad (Page 2): 9 rows (label+value pairs) = 216 pixels used

2. **No overflow validation:** Does Page 1 check if 8 DS18B20 devices exceed display height?
   ```
   8 devices × 24 pixels/row = 192 pixels from y=100
   Max available: 294 - 100 = 194 pixels
   Result: Barely fits (tight!)
   ```

3. **No scrolling:** Pages don't scroll; if data exceeds height, data is cut off
   - No documentation of "exceeds display" behavior

**Impact:** LOW
- Mostly cosmetic (layouts may look cramped/cut-off if max devices connected)
- No functional impact (application doesn't crash, just unsightly)

**Proposed Clarification:**
Document max displayable items per page:
```
Page 0 (BME280): 6 static rows, fits perfectly
Page 1 (DS18B20): Max 8 devices × 1 row = 8 rows, fits (tight: 192/194 pixels)
Page 2 (Gamepad): 9 rows, fits
Page 3 (INA219): 6 rows, fits
Page 4 (Encoder): 6 rows, fits
Page 5 (SCD30): 3 rows, fits
Page 6 (SI1145): 3 rows, fits
Page 7 (TSL2591): 4 rows, fits
Page 8 (VEML6070): 2 rows, fits
Page 9 (VEML6075): 7 rows, fits

All pages fit within 194-pixel data area (y=100 to y=294).
```

---

### ISSUE-020: Color Palette Uses "Dark Grey" Numerically as Other Value

| Property | Value |
|----------|-------|
| **ID** | ISSUE-020 |
| **Severity** | LOW |
| **Affected Docs** | 07-ui.md, 05-api.md |
| **Problem** | TFT_DARKGREY color value 0x39E7 coincides with hex representation of button bitmask |

**Evidence:**

- **07-ui.md (Color Scheme, line 89):**
  ```
  | TFT_DARKGREY (0x39E7) | Dividers, footers |
  ```

- **05-api.md (Gamepad buttons, line 316):**
  ```
  **Button Mask:** `BUTTON_MASK = (1UL << 2) | (1UL << 5) | (1UL << 6) | (1UL << 1) | (1UL << 0) | (1UL << 16) = 0x39`
  ```

**Clarification:**
- 0x39 ≠ 0x39E7, so this is coincidence only
- No functional issue, just cosmetic confusion

**Proposed Clarification:**
Use full 16-bit color code in all specifications to avoid confusion:
```
TFT_DARKGREY = 0x39E7 (RGB565: 0011_1001_1110_0111)
```

---

### ISSUE-021: No Documentation of Touch Functionality (GPIO 33)

| Property | Value |
|----------|-------|
| **ID** | ISSUE-021 |
| **Severity** | LOW |
| **Affected Docs** | 09-configuration.md, 12-dependencies.md |
| **Problem** | Touch controller (GPIO 33, FT5436) documented in hardware but not used in firmware |

**Evidence:**

- **09-configuration.md (GPIO Pin Assignments, line 159):**
  ```
  | **TOUCH_CS** (Touch Select) | GPIO 33 | Touch controller chip select (if present) |
  ```

- **12-dependencies.md (line 291):**
  ```
  | **Touch** | Resistive | FT5436 [UNKNOWN] | I2C / GPIO | 320×480 | — | GPIO 33 (CS) | Optional touch input (not used) |
  ```

- **14-reconstruction-plan.md (Phase 4, line 426):**
  ```
  **No persistence layer is implemented in sensor-pages-dashboard.**
  ```

**Clarification:**
- Touch functionality exists in hardware but is completely disabled
- No reason documented for why it's not used
- Could be: conflict with other GPIO? Not tested? Not needed?

**Proposed Clarification:**
Document explicitly:
```
Touch Screen Status:
- Hardware: Present (FT5436 resistive controller, GPIO 33 CS)
- Status: **NOT USED** in current firmware
- Reason: [INSERT REASON - conflict? untested? not needed?]
- If enabling: Would require TFT_Touch library integration + page interaction redesign
```

---

### ISSUE-022: Timing Gate Documentation Shows Line Numbers from Source Code

| Property | Value |
|----------|-------|
| **ID** | ISSUE-022 |
| **Severity** | LOW |
| **Affected Docs** | 09-configuration.md (Timing table, line 237-245) |
| **Problem** | Documentation references source code line numbers which don't exist in docs |

**Evidence:**

- **09-configuration.md (line 237-245):**
  ```
  | Sensor polling | 500 ms | `SENSOR_REFRESH_MS` | line 47 |
  | Gamepad input | 25 ms | INPUT_REFRESH_MS | loop line 623 |
  | Encoder input | 20 ms | `INPUT_REFRESH_MS` | line 48 |
  | INA219 sampling | 20 µs | Not defined (gate line 644) | hardcoded |
  ...
  ```

**Problem Description:**
- References to "line 47", "line 623", "line 644" are from original `src/main.cpp`
- Documentation copies made without source, so these line numbers are useless
- Creates false impression that docs are extracted automatically from code

**Impact:** LOW
- Mostly harmless (info about constant names is still useful)
- But confusing for reconstructors who can't find "line 47" in documentation

**Proposed Clarification:**
Remove all line number references, or change to:
```
| Sensor polling | 500 ms | #define SENSOR_REFRESH_MS | ~Top of constants section |
| Gamepad input | 25 ms | #define GAMEPAD_REFRESH_MS | ~Timing constants |
```

---

### ISSUE-023: "Freenove" Capitalization Inconsistent

| Property | Value |
|----------|-------|
| **ID** | ISSUE-023 |
| **Severity** | LOW |
| **Affected Docs** | Multiple (02-architecture.md, 07-ui.md, 12-dependencies.md, 14-reconstruction-plan.md) |
| **Problem** | Manufacturer/library name spelled "Freenove" (capital F, lowercase rest) inconsistently |

**Evidence:**

- Mostly capitalized correctly as "Freenove"
- Some instances as "freenove" (lowercase)
- Documentation: FNK0114N (not freenove-fnk0114n)

**Impact:** VERY LOW - cosmetic only
- No functional impact
- Doesn't affect reconstruction

---

### ISSUE-024: No Documentation of What Happens if Multiple Sensors Fail Simultaneously

| Property | Value |
|----------|-------|
| **ID** | ISSUE-024 |
| **Severity** | LOW |
| **Affected Docs** | 06-business-rules.md, 07-ui.md |
| **Problem** | Documentation covers single sensor failure; behavior with multiple simultaneous failures undocumented |

**Evidence:**

- **06-business-rules.md:**
  ```
  All rules tested individually (single sensor missing)
  ```

- **07-ui.md:**
  ```
  If [SENSOR] not detected at boot → display error
  ```

**Scenario not documented:**
- What if I2C bus completely fails (all 9 sensors undetectable)?
- What if 1-Wire bus fails (all DS18B20 undetectable)?
- Application still runs and shows 10 error screens?
- Or crashes due to all-zeros sensor status?

**Impact:** LOW
- Unlikely scenario (requires multiple hardware failures)
- Application probably still runs (all pages show error screens)
- No documented edge case

---

## Summary Table

| ID | Severity | Title | Docs Affected | Blocking Reconstruction |
|----|----------|-------|----------------|---------|
| 001 | CRITICAL | Gamepad Timing Contradiction (20 vs 25 ms) | 01, 02, 09 | YES |
| 002 | CRITICAL | Missing Dew Point Formula | 07, 04, 14 | YES |
| 003 | CRITICAL | No Fallback Page Navigation | 06, 14 | YES |
| 004 | HIGH | Timing Gate Constant Names Inconsistent | 06, 09, 14 | MEDIUM |
| 005 | HIGH | UV Compensation Coefficients Undocumented | 04, 06, 14 | MEDIUM |
| 006 | HIGH | Coulomb Counter Overflow Unchecked | 01, 04, 06 | MEDIUM |
| 007 | HIGH | VEML Register Addresses Incomplete | 09, 14, 05 | YES |
| 008 | HIGH | SCD30 Protocol Deferred to Datasheet | 14, 06 | YES |
| 009 | HIGH | TFT_eSPI Library Version Not Pinned | 12, 14 | MEDIUM |
| 010 | HIGH | Arduino Core Version Unspecified | 12, 02 | MEDIUM |
| 011 | MEDIUM | Encoder Read Frequency Conditional | 01, 02, 09 | NO |
| 012 | MEDIUM | Error Screen Behavior Untested | 06, 07, 10 | NO |
| 013 | MEDIUM | I2C Wire Timeout Unspecified | 02, 09, 06 | MEDIUM |
| 014 | MEDIUM | DallasTemperature Sentinel Library Dependent | 04, 06, 07 | MEDIUM |
| 015 | MEDIUM | Button Names Inconsistent Semantics | 06, 07, 05 | NO |
| 016 | MEDIUM | CRC8 Polynomial Not Verified | 06, 01 | MEDIUM |
| 017 | MEDIUM | Gamepad Joystick Inversion Rationale | 05, 07 | NO |
| 018 | MEDIUM | LED Brightness Delta ×8 Not Documented | 06, 04 | NO |
| 019 | LOW | Page Y-Coordinates Not Validated | 07 | NO |
| 020 | LOW | Color Value Collision (cosmetic) | 07, 05 | NO |
| 021 | LOW | Touch Functionality Undocumented | 09, 12 | NO |
| 022 | LOW | Source Code Line Number References | 09 | NO |
| 023 | LOW | "Freenove" Capitalization Inconsistent | Multiple | NO |
| 024 | LOW | Multiple Simultaneous Sensor Failures | 06, 07 | NO |

---

## Reconstruction Blockers (Must Resolve First)

### Critical Path Blockers (Cannot Reconstruct Without)

1. **ISSUE-001:** Gamepad timing clarification (20 ms vs 25 ms)
2. **ISSUE-002:** Dew point calculation formula
3. **ISSUE-003:** Page navigation fallback mechanism (if no gamepad, how to navigate?)
4. **ISSUE-007:** VEML register addresses (need datasheets)
5. **ISSUE-008:** SCD30 protocol details (need datasheet)

### Secondary Blockers (Affect Correctness)

6. **ISSUE-009:** TFT_eSPI version (may affect display behavior)
7. **ISSUE-010:** Arduino Core version (may affect timers/I2C)
9. **ISSUE-013:** Wire library timeout behavior
10. **ISSUE-014:** DallasTemperature sentinel value verification
11. **ISSUE-016:** CRC8 test vectors (verify algorithm)

---

## Recommendations for Reconstruction

### Before Starting Phase 1 (Project Init):
1. ✅ Resolve ISSUE-001 (timing)
2. ✅ Resolve ISSUE-002 (dew point)
3. ✅ Resolve ISSUE-003 (navigation fallback)
4. ✅ Resolve ISSUE-007 (VEML addresses)
5. ✅ Resolve ISSUE-008 (SCD30 protocol)
6. ✅ Pin ISSUE-009 (TFT_eSPI version)
7. ✅ Pin ISSUE-010 (Arduino Core version)

### During Phase 5 (Business Logic):
8. ✅ Verify ISSUE-013 (Wire timeout) via testing
9. ✅ Verify ISSUE-014 (DallasTemperature sentinel) via library inspection
10. ✅ Verify ISSUE-016 (CRC8) via test vectors

### During Phase 7 (UI):
11. ✅ Verify ISSUE-012 (error screens) with integration tests
12. ✅ Validate ISSUE-019 (Y-coordinates) on real hardware

### During Phase 10 (Tests):
13. ✅ Add test cases for all MEDIUM issues
14. ✅ Document all LOW issues as known limitations

---

## Conclusion

**Sufficiency Assessment:** Documentation is **sufficient for reconstruction with caveats**.

**Key Findings:**
- ✅ Architecture is well-understood (superloop, timing gates, data model)
- ✅ Page layouts are comprehensive
- ✅ Business rules are mostly documented
- ❌ Critical algorithms incomplete (dew point, VEML config, SCD30 protocol)
- ❌ Version pinning missing (TFT_eSPI, Arduino Core)
- ❌ Hardware specifications refer to external datasheets
- ⚠️ Some contradictions require clarification (timing values, navigation)

**Estimated Effort to Resolve Issues:**
- CRITICAL issues: 2-3 days (research datasheets, test hardware)
- HIGH issues: 2-3 days (extract register maps, verify algorithms)
- MEDIUM issues: 1-2 days (testing, version pinning)
- LOW issues: 1 day (cosmetic fixes, documentation cleanup)

**Total remediation effort: 6-9 days before reconstruction can confidently proceed.**

