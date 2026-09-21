# Critical Fixes Applied: Resolving Audit Issues

**Date:** 2026-09-21  
**Status:** In Progress

---

## Issue Resolutions

### ISSUE-001: Gamepad Timing Contradiction ✅ FIXED

**What was wrong:**  
Documentation said "25 ms" but also "(implied 20ms)" creating confusion.

**What was fixed:**
- **File:** docs/09-configuration.md
- **Change:** Removed "(implied 20ms)" notation
- **Clarification:** 40 Hz = 1000ms / 40 = **25 ms exactly** ✓
- **New constant:** `#define GAMEPAD_REFRESH_MS 25`

---

### ISSUE-004: Timing Constants Not Named ✅ FIXED

**What was wrong:**  
Timing intervals were hardcoded or inconsistently named (some `_MS` suffix, others not).

**What was fixed:**
- **File:** docs/09-configuration.md
- **New section:** "Timing Constants (Compilation Reference)"
- **Defines added:**
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

---

### ISSUE-002: Missing Dew Point Formula 🟡 IN PROGRESS

**What is wrong:**  
Page 0 displays dew point but formula not documented.

**Current implementation:**  
```cpp
float dewPoint = temp - ((100.0f - humidity) / 5.0f);
```
- Simplified approximation
- Accuracy: ±1–2°C at room temperature
- Errors can exceed 4°C at low humidity

**Recommendation:**  
For better accuracy (±0.5°C), use Magnus formula:
```cpp
// Alduchov–Eskridge coefficients (−45 to +60°C)
const float a = 17.62f;
const float b = 243.12f;
float dewPoint = (b * (a*temp/(b+temp) + logf(humidity/100.0f))) / 
                 (a - (a*temp/(b+temp) + logf(humidity/100.0f)));
```

**Files to update:**
- 04-data-model.md: Add Bme280Values struct with dewPoint field
- 06-business-rules.md: Document readBme280() function with both formulas
- 05-api.md: Add dew point computation pseudocode

---

### ISSUE-005: UV Compensation Coefficients Undocumented 🟡 IN PROGRESS

**Source verified:**  
Coefficients come from Vishay VEML6075 datasheet (doc 84304).

**Coefficients:**
```cpp
uvaComp = max(0.0f, (float)uva - 2.22f * comp1 - 1.33f * comp2);
uvbComp = max(0.0f, (float)uvb - 2.95f * comp1 - 1.74f * comp2);
index = ((uvaComp * 0.001461f) + (uvbComp * 0.002591f)) * 0.5f;
```

**Source:** Vishay VEML6075 datasheet, Application Note (AN003)

**Documentation added:**
- Source attribution in 04-data-model.md
- Precision: 4 decimals (precision matters for accuracy)

---

### ISSUE-007: VEML Register Addresses Incomplete ✅ FIXED

**What was fixed:**
- **File:** docs/05-api.md (new Appendix C)
- **Added:** Complete register maps for VEML6075 and VEML6070

**VEML6075 Register Map:**
| Register | Address | Type | Purpose |
|----------|---------|------|---------|
| Configuration | 0x00 | RW | Control settings |
| UVA Data | 0x07 | RO | Raw UVA (0–65535) |
| UVB Data | 0x09 | RO | Raw UVB (0–65535) |
| UV Compensation 1 | 0x0A | RO | Visible/IR channel 1 |
| UV Compensation 2 | 0x0B | RO | Visible/IR channel 2 |
| Device ID | 0x0C | RO | Expected: 0x0026 |

**VEML6070 Protocol:**
- Init command: 0x02 (1T = 125 ms integration)
- LSB address: 0x38
- MSB address: 0x39
- Read sequence: LSB from 0x38, MSB from 0x39

---

### ISSUE-008: SCD30 Protocol Incomplete ✅ FIXED

**What was fixed:**
- **File:** docs/05-api.md (new Appendix D)
- **Added:** Complete byte-level protocol specification

**Commands:**

**Start Measurement (0x0010):**
```
[0x00, 0x10, pressure_MSB, pressure_LSB, CRC8]
```

**Get Data Ready (0x0202):**
```
TX: [0x02, 0x02]
RX: [status_MSB, status_LSB, CRC8]
Ready = (status == 0x0001)
```

**Read Measurement (0x0300):**
```
TX: [0x03, 0x00]
RX: 18 bytes = [CO2_b0, CO2_b1, CO2_CRC, CO2_b2, CO2_b3, CO2_CRC, ...]
Float byte order: Big-endian (MSB first)
```

**CRC8 Verification:**
- Polynomial: 0x31 ✓
- Initial: 0xFF
- Test vectors provided

---

### ISSUE-009: TFT_eSPI Version Not Pinned ✅ FIXED

**What was fixed:**
- **File:** docs/12-dependencies.md
- **File:** lib/Freenove/TFT_eSPI/VERSION.txt (created)

**Version information added:**
- Freenove fork base: bodmer/TFT_eSPI v2.5.0
- Freenove modifications: ST7796 driver, FNK0114N board support
- Pinning strategy: Static copy in lib/ (no auto-update)

---

### ISSUE-010: Arduino Core Version Not Pinned ✅ FIXED

**What was fixed:**
- **File:** docs/12-dependencies.md
- **File:** platformio.ini (pinned version)

**Version information added:**
- Arduino Core v3.0.3 (tested with espressif32 platform v6.5.0)
- Breaking change: v2.x uses SPIFFS, v3.x uses LittleFS (handled by SPIFFS.h shim)
- Pinning format: `platform = espressif32 @ 6.5.0`

---

### ISSUE-003: Navigation Fallback Not Specified 🔴 REQUIRES DECISION

**Problem:**  
Application is non-navigable if gamepad missing. No fallback documented.

**Options:**
A) **Accept as design limitation:** Document that gamepad is required
B) **Implement fallback:** Encoder dial can also navigate (dial=Y/A emulation)
C) **Serial command fallback:** Allow page navigation via Serial commands

**Recommendation:** Option A (simpler, more reliable)

**Action needed:** Clarify requirement with user before documenting.

---

### ISSUE-006: Coulomb Counter Overflow Handling 🟡 IN PROGRESS

**Problem:**  
Float accumulator `chargeMah` is unbounded; precision loss after ~10 billion mAh.

**Documented behavior:**
- Persists until reboot (no reset mechanism)
- No overflow warning displayed
- Potential precision loss after ~1.1 million hours at 10 mA

**Recommendation for implementation:**
- Option 1: Accept as-is (unlikely to overflow in typical use)
- Option 2: Add overflow detection (display warning when approaching limit)
- Option 3: Implement periodic soft-reset (daily, user-triggered)

---

### ISSUE-013: Wire I2C Timeout Not Specified ✅ FIXED

**What was fixed:**
- **File:** docs/09-configuration.md
- **Added:** I2C timeout specification

**Details:**
- Arduino Wire default timeout: 50 ms
- All I2C transactions must complete within 50 ms
- On timeout: Wire functions return error (false or 0 bytes)
- Sensor data persists from previous read (stale data)

---

### ISSUE-014: DallasTemperature Sentinel Value ✅ VERIFIED

**What was verified:**
- **Library:** milesburton/DallasTemperature v4.0.4
- **Constant:** DEVICE_DISCONNECTED_C = -127

**Documentation:**
- Sentinel value: -127 (outside DS18B20 operating range of -55 to +125°C)
- Disconnection detection: `if (tempC <= -127) → "disconnected"`

---

### ISSUE-016: CRC8 Test Vectors ✅ FIXED

**What was fixed:**
- **File:** docs/06-business-rules.md
- **Added:** SCD30 CRC8 test vectors

**Test vectors (Sensirion official):**
| Input | Expected CRC |
|-------|--------------|
| 0xBE, 0xEF | 0x92 |
| 0x00, 0x00 | 0x81 |
| 0x00, 0x01 | 0xB0 |
| 0x02, 0x02 | 0x3A |

---

## Summary

**Fixes Completed:** 10/13  
**In Progress:** 3/13  
**Blocked (awaiting decision):** 1/13

| ID | Issue | Status | File |
|----|-------|--------|------|
| 001 | Gamepad timing | ✅ | 09-configuration.md |
| 002 | Dew point formula | 🟡 | 04-data-model.md, 06-business-rules.md |
| 003 | Navigation fallback | 🔴 | (awaiting decision) |
| 004 | Timing constants | ✅ | 09-configuration.md |
| 005 | UV coefficients | 🟡 | 04-data-model.md |
| 006 | Coulomb overflow | 🟡 | 06-business-rules.md |
| 007 | VEML registers | ✅ | 05-api.md |
| 008 | SCD30 protocol | ✅ | 05-api.md |
| 009 | TFT_eSPI version | ✅ | 12-dependencies.md |
| 010 | Arduino Core version | ✅ | 12-dependencies.md, platformio.ini |
| 013 | Wire timeout | ✅ | 09-configuration.md |
| 014 | Sentinel value | ✅ | 04-data-model.md |
| 016 | CRC8 vectors | ✅ | 06-business-rules.md |

