# Requirements: sensor-pages-dashboard

**Last Updated:** 2026-09-21  
**Project Type:** Embedded Firmware (ESP32)  
**Platform:** PlatformIO + Arduino Framework

---

## Overview

sensor-pages-dashboard is a firmware application for an ESP32 microcontroller with a TFT display and multiple sensor inputs. The application provides a paginated sensor monitoring interface with real-time data display, interrupt-driven user input, and multi-rate asynchronous sensor polling.

---

## Functional Requirements

### FR-001: Multi-Sensor Data Acquisition

**Requirement:**  
The system shall acquire data from multiple sensor types via I2C bus.

**Details:**
- Support 9 distinct I2C-based sensors
- Sensor presence detection at boot (no runtime re-detection)
- Graceful degradation: missing sensors do not prevent system operation

**Sensors Supported:**
- BME280 (0x77) — Climate sensor (temp/humidity/pressure)
- Gamepad seesaw (0x50) — Joystick + 6 buttons
- INA219 (0x40) — Power monitor
- Encoder seesaw (0x36) — Rotary encoder + NeoPixel LED
- SCD30 (0x61) — CO₂ sensor with CRC validation
- SI1145 (0x60) — Multi-spectrum sensor
- TSL2591 (0x29) — Light sensor (lux)
- VEML6070 (0x38/0x39) — UV sensor (dual-address)
- VEML6075 (0x10) — UV sensor (UVA/UVB)

**Evidence:** main.cpp detectSensors() function; all begin() calls

---

### FR-002: Real-Time Display of Sensor Data

**Requirement:**  
The system shall display acquired sensor data on a TFT screen with color-coded information.

**Details:**
- Resolution: 320 × 480 pixels
- Color: 16-bit RGB565
- Refresh rate: 20 ms (live pages), 500 ms (sensor pages)
- Data formatted with 1–3 decimal places (precision varies by sensor type)
- Color coding: green (primary), orange (secondary), cyan (computed), magenta (accumulated)

**Evidence:** 
- TFT initialization: `tft.init()`, `tft.setRotation(3)`
- Page renderers: `pageBme280()`, `pageIna219()`, etc.
- Color constants: TFT_GREEN, TFT_ORANGE, TFT_CYAN, TFT_MAGENTA

---

### FR-003: Circular Pagination Navigation

**Requirement:**  
The system shall provide navigation through 9 sensor pages using gamepad buttons.

**Details:**
- 9 pages (indices 0–8)
- Y button: navigate to previous page (with wrap-around)
- A button: navigate to next page (with wrap-around)
- Page 0 Y button → Page 8
- Page 8 A button → Page 0
- Navigation is falling-edge triggered (button press, not release)

**Evidence:**
- Page array: 9 titles (lines 448–457 in drawCurrentPage)
- Navigation logic: lines 527–529 in loop()
- Edge detection: `yPressed && !lastY`, `aPressed && !lastA`

---

### FR-004: User Input Processing

**Requirement:**  
The system shall process gamepad button and rotary encoder input in real-time.

**Details:**
- Gamepad buttons read at 40 Hz (25 ms)
- Encoder read at 50 Hz (20 ms) when on encoder page or page changing
- Button presses detected via falling-edge (0→1 transition)
- Encoder dial rotation tracked as delta (ticks per cycle)
- LED brightness updated immediately on dial rotation
- LED color cycled immediately on button press

**Evidence:**
- Gamepad read: `readGamepadAt40Hz()` lines 239–253
- Encoder read: `readEncoderAt50Hz()` lines 263–289
- LED update: `updateEncoderLed()` lines 257–262

---

### FR-005: Asynchronous Non-Blocking Sensor Polling

**Requirement:**  
Sensor polling shall not block the main loop or delay user input responsiveness.

**Details:**
- INA219: 50 Hz sampling via high-precision `micros()` timer
- SCD30: Guarded by `scd30Ready()` status check before read
- VEML sensors: Simple register reads (no conversion wait)

**Evidence:**
- INA219 timing: line 648, 20µs gate
- SCD30 guard: line 602, `if (available.scd30 && scd30Ready())`

---

### FR-006: Coulomb Counter for Energy Accumulation

**Requirement:**  
The system shall accumulate electrical charge from the INA219 power monitor using Coulomb integration.

**Details:**
- Formula: `chargeMah += current_mA * elapsed_µs / 3.6e9`
- Accumulation starts after first read (no initial count-up)
- Time gaps > 200 ms discarded (sanity check)
- Counter persists across read cycles until reboot
- Counter unbounded (potential overflow at ~10 billion mAh)

**Evidence:**
- Initialization: line 383, `if (ina219Values.initialized)`
- Accumulation formula: line 385
- Gap check: line 384, `if (elapsedUs < 200000)`

---

### FR-007: CRC8 Validation for SCD30

**Requirement:**  
The system shall validate all data from SCD30 sensor using CRC8 checksums.

**Details:**
- 18-byte response contains 3 fields × (2 data bytes + 1 CRC byte)
- Polynomial: 0x31
- Initial value: 0xFF
- All-or-nothing: any CRC mismatch rejects entire read
- Failed reads do not update sensor values (stale data persists)

**Evidence:**
- CRC algorithm: lines 159–165
- Read validation: lines 175–176, `for (uint8_t i = 0; i < 18; i += 3)`

---

### FR-008: UV Compensation Formulas

**Requirement:**  
The system shall apply manufacturer-provided compensation formulas to raw UV sensor data.

**Details:**
- VEML6075: Compensate raw UVA/UVB with linear regression (COMP1, COMP2 factors)
- Formulas: `uvaComp = max(0, uva - 2.22*comp1 - 1.33*comp2)`, etc.
- Clamping: Negative compensated values clamped to 0 (physical constraint)
- UV index derived: `((uvaComp * 0.001461) + (uvbComp * 0.002591)) * 0.5`

**Evidence:**
- Compensation: lines 197–199

---

### FR-009: Encoder LED Control

**Requirement:**  
The system shall provide interactive control of a NeoPixel RGB LED via rotary encoder.

**Details:**
- Color cycling: 4-state cycle (blue→green→red→purple) on button press
- Brightness adjustment: dial delta × 8, clamped 0–255
- Updates immediate (within 20 ms read cycle)
- Default brightness: 64 (25% of 255)

**Evidence:**
- Color array: lines 113–115
- Brightness calculation: lines 278–281
- LED update: line 257, `updateEncoderLed()`

---

### FR-011: Error Handling & Graceful Degradation

**Requirement:**  
The system shall handle sensor failures and I2C errors without halting operation.

**Details:**
- Missing sensor: Display error screen ("Sensor nicht vorhanden")
- I2C read failure: Persist last-known value (stale data)
- No error logging or retry mechanism
- Navigation and other sensors unaffected by individual failures

**Evidence:**
- Error screen: `unavailable()` function lines 145–152
- Stale data: `if (available.X) readX(...)` guards (e.g., line 600)

---

## Non-Functional Requirements

### NFR-001: Real-Time Response

**Requirement:**  
User input (button press) shall result in visible page navigation within 25 ms.

**Details:**
- Gamepad read frequency: 40 Hz (25 ms)
- Page rendering: Immediate (no animation delay)
- LED update: Immediate (blocking I2C call)

**Evidence:**
- Timing gate: line 623, `INPUT_REFRESH_MS = 20` (ms)

---

### NFR-002: Power Consumption (Implicit)

**Requirement:**  
The system shall continuously operate without thermal issues or excessive power draw.

**Details:**
- No power management implemented
- Continuous polling at fixed rates (no sleep modes)
- I2C bus clock: 400 kHz (standard mode)
- SPI display clock: ~80 MHz (configurable, TFT_eSPI setup)

**Evidence:**
- Wire.setClock(400000) line 590
- No sleep/hibernate logic in main loop

---

### NFR-003: Memory Footprint

**Requirement:**  
The application shall fit within ESP32 SRAM constraints.

**Details:**
- Data structs: ~300 bytes total (negligible for 320 KB heap)
- Global variables: Statically allocated (no dynamic memory allocation)
- Display buffer: Managed by TFT_eSPI library

**Evidence:**
- Struct sizes documented in docs/04-data-model.md
- No malloc/free calls in firmware

---

### NFR-004: Deterministic Timing (Best-Effort)

**Requirement:**  
Sensor polling shall occur at designated intervals within tolerance.

**Details:**
- Timing gates use signed comparison for rollover safety
- 20 µs precision for INA219 (micros())
- 20 ms precision for most other operations (millis())

**Evidence:**
- Time-gate pattern: line 648, `(int32_t)(nowUs - nextInaSampleUs) >= 0`

---

## Design Constraints

### DC-001: Compile-Time Configuration

**Constraint:**  
No runtime configuration or settings storage.

**Details:**
- All sensor addresses hardcoded
- All timing intervals hardcoded
- All display colors hardcoded
- No EEPROM/NVS writes
- Reconfiguration requires firmware rebuild

**Rationale:** Embedded device; persistent storage adds complexity without user benefit.

---

### DC-002: Single-Threaded Execution

**Constraint:**  
No RTOS tasks; bare-metal superloop only.

**Details:**
- Arduino framework runs FreeRTOS internally but firmware uses cooperative scheduling only
- No concurrent sensors (poll one at a time)
- No task priorities or interrupts in application layer

**Rationale:** Simplicity; ESP32 core speed adequate for all polling rates.

---

### DC-003: No Persistence Layer

**Constraint:**  
All state resides in RAM; no disk I/O or SPIFFS.

**Details:**
- Data lost on reboot
- Coulomb counter resets on reboot
- Sensor list resets on reboot (hot-plug detection only at runtime)

**Rationale:** Embedded firmware; persistence adds complexity without clear use case.

---

## Dependency Constraints

### DEP-001: Adafruit Libraries

**Constraint:**  
Application relies on Adafruit sensor libraries for I2C abstraction.

**Details:**
- INA219, TSL2591, SI1145, BME280: Adafruit Arduino libraries
- seesaw (encoder/gamepad): Adafruit seesaw library
- NeoPixel: seesaw_NeoPixel (part of seesaw library)
- TFT display: TFT_eSPI library (third-party, Freenove variant)
- Temperature sensors: DallasTemperature + OneWire (standard Arduino)

**Risk:** Library version mismatches; dependency updates may break compatibility.

---

### DEP-002: Hardware Platform

**Constraint:**  
Application targets ESP32 microcontroller (specific pin assignments).

**Details:**
- SDA (I2C): GPIO32
- SCL (I2C): GPIO25
- Display CS: GPIO15, DC: GPIO2
- Display resolution: 320 × 480 (landscape rotation)

**Portability:** Not portable to other platforms without hardware redesign.

---

## Acceptance Criteria

### AC-001: Sensor Data Display

**Criteria:**
- ✅ All 9 pages render correctly with sensor data
- ✅ Data refreshes at specified intervals (20 ms live, 500 ms sensor)
- ✅ Missing sensors display error screen
- ✅ Color coding matches specification

**Verification:** Manual testing on hardware; visual inspection

---

### AC-002: Navigation

**Criteria:**
- ✅ Y button navigates to previous page (with wrap-around 0→9)
- ✅ A button navigates to next page (with wrap-around 9→0)
- ✅ Page transitions are immediate (no lag)
- ✅ Navigation works even if some sensors missing

**Verification:** Manual testing; edge detection verification

---

### AC-003: Coulomb Counter

**Criteria:**
- ✅ Charge accumulates continuously
- ✅ Accumulation survives across read cycles (persists)
- ✅ Time gaps > 200 ms are skipped
- ✅ Display shows accumulated mAh

**Verification:** Manual measurement with known current; oscilloscope timing verification

---

### AC-004: Encoder LED Control

**Criteria:**
- ✅ LED color cycles on button press (blue→green→red→purple)
- ✅ LED brightness adjusts with dial rotation (delta × 8)
- ✅ LED updates immediately (no lag)
- ✅ Brightness clamped 0–255

**Verification:** Visual inspection; timing verification

---

### AC-005: CRC8 Validation

**Criteria:**
- ✅ Valid SCD30 reads pass CRC checks and update display
- ✅ Invalid reads (simulated CRC corruption) are rejected
- ✅ Failed reads do not update displayed values

**Verification:** Integration testing; I2C analyzer or simulation

---

## Known Limitations

### LIMIT-001: No Test Suite

**Limitation:**  
No automated unit or integration tests.

**Impact:** 
- Changes require manual testing
- Regression detection manual
- No CI/CD validation

**Mitigation:** See docs/10-testing.md for recommended testing strategy.

---

### LIMIT-002: I2C Error Recovery

**Limitation:**  
No retry mechanism for I2C read failures.

**Impact:**  
- Single transient I2C error causes 500 ms data stall
- Stale data displayed indefinitely

**Mitigation:** More robust I2C polling with retry (future work).

---

### LIMIT-003: No Settings UI

**Limitation:**  
All configuration at compile time only.

**Impact:**  
- Cannot adjust timing intervals or colors without rebuild
- Cannot enable/disable individual sensors

**Mitigation:** Add EEPROM-backed settings (future work).

---

### LIMIT-004: Unbounded Coulomb Counter

**Limitation:**  
`chargeMah` accumulator never overflows (implicit behavior).

**Impact:**  
- After ~71 years at 1mA, counter loses precision (IEEE 754 float limits)
- No indication of precision loss

**Mitigation:** Document precision guarantees; consider 64-bit accumulator.

---

## Testing Requirements

**See docs/10-testing.md for comprehensive testing strategy.**

No automated tests currently exist (known limitation LIMIT-001). Critical behaviors that MUST be manually verified:
1. All 9 pages render and display sensor data
2. Navigation wraps correctly at boundaries
3. Coulomb counter accumulates without overflow
4. LED responds immediately to encoder interaction
5. CRC8 validation rejects corrupted SCD30 data

---

## Open Questions / [UNKNOWN]

1. **INA219 Time-Gap Threshold:** Why 200 ms? (arbitrary safety margin?)
2. **VEML6075 Compensation Constants:** Where do 2.22, 2.95, 0.001461, 0.002591 come from? (datasheet formulas not documented)
3. **Display Rotation:** Why 90° instead of 0° or 180°? (physical orientation preference?)
5. **Page Title Ordering:** Why is BME280 page 0 (not DSC30 or INA219)?
6. **LED Default Brightness:** Why 64 (not 128 or 255)?

