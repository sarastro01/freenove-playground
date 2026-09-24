# Requirements: sensor-pages-dashboard

**Last Updated:** 2026-09-25  
**Project Type:** Embedded Firmware (ESP32)  
**Platform:** PlatformIO + Arduino Framework

---

## Overview

sensor-pages-dashboard is a firmware application for an ESP32 microcontroller with a TFT display and multiple sensor inputs. The application provides a paginated sensor monitoring interface with real-time data display, interrupt-driven user input, and multi-rate asynchronous sensor polling. Includes support for 1-Wire temperature sensors via I2C-to-1-Wire adapter.

---

## Functional Requirements

### FR-001: Multi-Sensor Data Acquisition

**Requirement:**  
The system shall acquire data from multiple sensor types via I2C bus and 1-Wire bus.

**Details:**
- Support 10 distinct I2C-based sensors (9 direct I2C + 1 I2C-to-1-Wire adapter)
- Support for 1-Wire temperature sensors via DS2484 I2C adapter (up to 3 sensors)
- Sensor presence detection at boot (no runtime re-detection)
- Graceful degradation: missing sensors do not prevent system operation

**I2C Sensors Supported:**
- BME280 (0x77) — Climate sensor (temp/humidity/pressure)
- Gamepad seesaw (0x50) — Joystick + 6 buttons
- INA219 (0x40) — Power monitor
- Encoder seesaw (0x36) — Rotary encoder + NeoPixel LED
- SCD30 (0x61) — CO₂ sensor with CRC validation
- SI1145 (0x60) — Multi-spectrum sensor
- TSL2591 (0x29) — Light sensor (lux)
- VEML6070 (0x38/0x39) — UV sensor (dual-address)
- VEML6075 (0x10) — UV sensor (UVA/UVB)
- DS2484 (0x18) — I2C-to-1-Wire adapter for temperature sensors

**1-Wire Sensors Supported (via DS2484):**
- DS18B20 (Family Code 0x28) — Digital thermometer
- DS1822 (Family Code 0x22) — Digital thermometer
- DS1820 (Family Code 0x10) — Digital thermometer
- Other 1-Wire temperature sensor types compatible with DallasTemperature library
- Maximum 3 sensors per adapter (hardware limit)

**Evidence:** main.cpp detectSensors() function; all begin() calls; initDs2484(), performDs2484Scan(), readDs2484Temperatures() functions

---

### FR-002: Real-Time Display of Sensor Data

**Requirement:**  
The system shall display acquired sensor data on a TFT screen with color-coded information.

**Details:**
- Resolution: 320 × 480 pixels
- Color: 16-bit RGB565
- Refresh rate: 20 ms (live pages), 500 ms (sensor pages)
- Data formatted with 1–3 decimal places (precision varies by sensor type)
- Color coding: green (primary), orange (secondary), cyan (computed/derived), red (errors)

**Evidence:** 
- TFT initialization: `tft.init()`, `tft.setRotation(3)`
- Page renderers: `pageBme280()`, `pageIna219()`, etc.
- Color constants: TFT_GREEN, TFT_ORANGE, TFT_CYAN, TFT_RED (no TFT_MAGENTA — not readable in sunlight)

---

### FR-003: Circular Pagination Navigation

**Requirement:**  
The system shall provide navigation through 10 sensor pages using gamepad buttons.

**Details:**
- 10 pages (indices 0–9)
- Y button: navigate to previous page (with wrap-around)
- A button: navigate to next page (with wrap-around)
- Page 0 Y button → Page 9
- Page 9 A button → Page 0
- Navigation is falling-edge triggered (button press, not release)
- PAGE_COUNT constant = 10

**Page Order:**
- Page 0: BME280 Climate Sensor
- Page 1: Gamepad QT Input
- Page 2: INA219 Battery Monitor
- Page 3: Rotary Encoder
- Page 4: SCD30 CO₂ Sensor
- Page 5: SI1145 UV/IR Sensor
- Page 6: TSL2591 Light Sensor
- Page 7: VEML6070 UV Sensor
- Page 8: VEML6075 UV Sensor
- Page 9: DS2484 1-Wire Temperature Sensors (NEW)

**Evidence:**
- Page array: 10 titles in drawCurrentPage
- Navigation logic: modulo arithmetic with PAGE_COUNT=10
- Edge detection: `yPressed && !lastY`, `aPressed && !lastA`

---

### FR-004: User Input Processing

**Requirement:**  
The system shall process gamepad button and rotary encoder input in real-time.

**Details:**
- Gamepad buttons read at 40 Hz (25 ms)
- Encoder read at 50 Hz (20 ms) CONTINUOUSLY (not page-conditional)
  - Enables LED updates and stats aggregation even when encoder page not displayed
- Button presses detected via falling-edge (0→1 transition)
- Encoder dial rotation tracked as delta (ticks per cycle)
- LED brightness updated immediately on dial rotation
- LED color cycled immediately on button press
- Encoder read timing aggregated into `EncoderStats` for performance monitoring

**Evidence:**
- Gamepad read: `readGamepadAt40Hz()` with timing aggregation
- Encoder read: `readEncoderAt50Hz()` called every 20 ms (not conditional on page)
- LED update: `updateEncoderLed()` executed on every encoder read
- Stats logging: `printEncoderStats()` prints aggregated stats every 1000 ms

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

### FR-010: 1-Wire Temperature Sensor Discovery & Monitoring

**Requirement:**  
The system shall discover and monitor 1-Wire temperature sensors via DS2484 I2C-to-1-Wire adapter.

**Details:**
- Periodic bus scan every 60 seconds to detect new/missing sensors
- Support for multiple 1-Wire sensor types (DS18B20, DS1822, DS1820, etc.)
- Maximum 3 sensors per adapter (hardware limit of Adafruit breakout)
- Temperature polling every 3 seconds with 12-bit standard resolution
- Display full 8-byte ROM-ID (Family Code + Serial Number + CRC) in HEX format
- Error handling: mark sensors offline after 3 consecutive read failures
- Recovery: re-enable sensors when communication succeeds again
- Error display: "Disconnected" status for offline sensors

**Evidence:** 
- initDs2484(), performDs2484Scan(), readDs2484Temperatures(), pageDs2484() functions in main.cpp
- Ds2484Values struct with sensor array
- DS2484_SCAN_INTERVAL_MS = 60000, DS2484_TEMP_INTERVAL_MS = 3000 constants

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

### FR-012: Large Value Display Mode (INA219 Page 2) — **NEW**

**Requirement:**  
The system shall provide an alternate display mode for INA219 sensor page that shows individual sensor values in enlarged format for improved readability.

**Details:**
- Entry point: Gamepad Button B pressed while on Page 2 (INA219)
- Display mode cycles through 7 sensor values: Laufzeit → Batterie → Versorgung → Strom → Ladung → Shunt-Spannung → Power (wraps to Laufzeit)
- Navigation controls in large display mode: Button B = next value, Button X = previous value, Button Y = exit mode, Button A = inactive
- **Critical:** Page navigation (Y/A buttons) is disabled while in large display mode; only re-enabled upon exit to standard page view
- Values update in real-time using global ina219Values struct (no caching)
- Refresh rate: Inherit page native refresh rate (500 ms for INA219 page)
- Display layout: Label (large font) + Value (very large font, Font 8) + Unit + Footer instructions (all centered)
- Scope: INA219 page only (Page 2); future extension to other pages planned
- Helper functions generic for future extension to other sensor pages

**Evidence:** 
- FEATURE-SPEC-004-INA219-LARGE-VALUE-DISPLAY.md
- Global state: largeDisplayMode (bool), largeDisplayIndex (uint8_t, 0–6)
- New functions: pageLargeDisplayIna219(), getLargeDisplayLabel(), getLargeDisplayValue(), getLargeDisplayColor(), getLargeDisplayUnit()

---

### FR-013: Unified Sensor Data Caching Architecture — **NEW**

**Requirement:**  
BME280, TSL2591, and SI1145 sensors shall use the same caching architecture as other sensors (VEML6075, VEML6070, SCD30, DS2484).

**Details:**
- Sensor data read in `readSensors()` function at 500 ms interval (via `SENSOR_REFRESH_MS`)
- Each sensor type has dedicated struct to cache latest readings: `Bme280Values`, `Tsl2591Values`, `Si1145Values`
- Global instances: `bme280Values`, `tsl2591Values`, `si1145Values` initialized to NaN (not valid) at startup
- Read functions: `readBme280()`, `readTsl2591()`, `readSi1145()` with error logging and timing measurement
- Page display functions read **only** from cached structs (no direct I2C operations during rendering)
- Error handling: If I2C read fails (detected via plausibility checks), cache **not updated** and error logged to Serial
- Performance: Reduces I2C load during display refresh, improves UI responsiveness

**Sensor-Specific Details:**
- **BME280:** Cache contains temperature, humidity, pressure, altitude, dewPoint (calculated via Alduchov & Eskridge formula, ±0.35°C accuracy)
- **TSL2591:** Cache contains fullSpectrum (uint16_t), infrared (uint16_t), visible (calculated), lux (float), gain, integrationTime
  - Implemented via direct I2C register read (DATA0, DATA1 at addresses 0x14-0x17)
  - Actual I2C read time: ~295 µs (not including 200 ms integration wait)
  - Bitwise extraction: DATA0 = CHAN0 (visible + IR), DATA1 = CHAN1 (IR only), visible = DATA0 - DATA1
  - Replaces Adafruit library `getFullLuminosity()` to avoid blocking on integration time
- **SI1145:** Cache contains visible (uint16_t), infrared (uint16_t), uv (uint16_t raw), uvIndex (float = uv / 100.0f)

**Evidence:**
- FEATURE-SPEC-005-SENSOR-CACHING-ARCHITECTURE.md
- Struct definitions: lines TBD in main.cpp (post-implementation)
- New functions: readBme280(), readTsl2591(), readSi1145() with serial timing output
- Modified function: readSensors() includes calls to new read functions with availability guards

---

### FR-014: Non-Blocking Rotated Sensor Polling Architecture — **NEW (FEATURE-SPEC-006)**

**Requirement:**  
All slow I2C sensors (BME280, TSL2591, SI1145, VEML6075, VEML6070, SCD30, DS18B20) shall be polled in a rotating schedule to eliminate blocking on the main loop and guarantee responsive user input.

**Details:**
- **Rotation cycle:** 7 slots × 200 ms = 1.4 seconds per full cycle
- **Slot allocation:**
  - Slot 0 (0 ms): BME280 read (~2.2 ms)
  - Slot 1 (200 ms): TSL2591 async START (~0 ms, integration continues in background)
  - Slot 2 (400 ms): SI1145 read (~0.7 ms)
  - Slot 3 (600 ms): VEML6075 read (~? ms)
  - Slot 4 (800 ms): VEML6070 read (~? ms)
  - Slot 5 (1000 ms): SCD30 read (~? ms with CRC validation)
  - Slot 6 (1200 ms): DS18B20 1-Wire async START (~0 ms, conversion continues in background)
  - Slot 7 (1400 ms): Check async results (TSL2591, DS18B20) and read when ready

- **Async Measurement Patterns:**
  - **TSL2591:** `startTsl2591Measurement()` in Slot 1 → `readTsl2591Async()` checks readiness in Slot 7+ (~200 ms later)
  - **DS18B20:** `startDs18b20Conversion()` in Slot 6 → `readDs18b20Async()` checks readiness (~750 ms later, handles 12-bit conversion time)

- **Guaranteed Performance:**
  - Maximum blocked time per loop: 200 ms (≤ 1 sensor read per slot)
  - Input response: Always < 200 ms (gamepad/encoder read independent of sensor rotation)
  - Sensor freshness: Each sensor read at least once per 1.4 seconds (acceptable for slow sensors)

- **High-Frequency Inputs (Independent):**
  - Gamepad: 40 Hz polling (25 ms gate), unaffected by sensor schedule
  - Encoder: 50 Hz polling (20 ms gate, CONTINUOUSLY — not page-conditional), unaffected by sensor schedule
  - INA219: 50 Hz polling (20 µs via micros), unaffected by sensor schedule

- **Display Update Behavior:**
  - Full redraw: On page change only (< 20 ms)
  - Partial redraw: Only when sensor values actually change (≤ 200 ms after sensor read)
  - Timer update: Every 1 second for INA219 page (timer field must refresh)
  - No-op redraw: If values unchanged since last read (saves CPU, eliminates flicker)

**Configuration:**
```cpp
#define ENABLE_STATS_REPORTING 1              // 1 = aggregated stats, 0 = off (compile-time)
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;     // Slot interval (do not change)
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;    // Stats reporting interval
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;    // Async measurement wait time
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;     // 12-bit 1-Wire conversion time
```

**Performance Benefits:**
- Eliminates 250 ms blocking on TSL2591 integration time (FEATURE-SPEC-005 issue)
- Guarantees < 200 ms input latency (hardware-bound)
- Reduces unnecessary display redraws (saves power)
- Maintains sensor accuracy with acceptable freshness

**Evidence:**
- FEATURE-SPEC-006.md (full specification document)
- Async state machines: `Tsl2591State`, `Ds18b20State` enums
- Rotation logic: `rotatedReadSensor()` function dispatches to slot handler
- Display change detection: Value comparison before `drawCurrentPage()` call

---

### FR-015: Display Rate Limiting (Anti-Flicker)

**Requirement:**  
The display shall enforce maximum refresh rate of 5 Hz (200 ms minimum between updates) to prevent flickering when sensor values change rapidly.

**Details:**
- **Rate limit:** 200 ms minimum interval between display redraws (5 Hz maximum)
- **Applies to:** Both full redraws (page change) and partial redraws (value change)
- **Trigger:** Display updates only on:
  1. Page change (immediate full redraw)
  2. Sensor value change (detected via `valuesChanged()` or `largeDisplayValueChanged()`)
  3. Timer event (e.g., INA219 runtime seconds counter increments)
- **Rate limiter implementation:** `nextLargeDisplayRefreshMs` gate tracking (200 ms interval)
- **Benefit:** Eliminates visual flicker on INA219 large display mode when runtime value updates rapidly
- **Trade-off:** Display response latency ≤ 200 ms (acceptable for embedded UI)

**Configuration:**
```cpp
static uint32_t nextLargeDisplayRefreshMs = 0;  // Rate limit timer
```

**Evidence:**
- Display update logic in `loop()`: lines ~1894-1903 (rate limit check)
- Runtime counter changes every second, triggering display update (guarded by rate limit)
- Prevents excessive TFT_eSPI SPI traffic and CPU load

---

### FR-016: Universal Large Display Mode for All Sensor Pages — **NEW (FEATURE-SPEC-007)**

**Requirement:**  
The system shall support large display mode (single value focus, enlarged font) on ALL 10 sensor pages (Pages 0–9).

**Details:**
- **Entry:** Button B press on any sensor page activates large display mode
- **Exit:** Button Y press exits large display mode, returns to standard page view
- **Value count per page:** 3–8 displayable values (sensor-dependent) — Uses central parameter definitions (FEATURE-SPEC-008)
  - Page 0 (BME280): 7 values (Temperature, Pressure, Humidity, Dew Point, Altitude, Air Quality, Temp Trend)
  - Page 1 (Gamepad): 8 values (Joystick X/Y, Buttons X/Y/A/B, Select/Start, Button Mask)
  - Page 2 (INA219): 7 values (Runtime, Battery, Supply, Current, Charge, Shunt Voltage, Power) — existing, verified
  - Page 3 (Encoder): 7 values (Position, Delta, Button, Presses, LED Color, LED Brightness, Seesaw Temp)
  - Page 4 (SCD30): 3 values (CO2 Level, Temperature, Humidity)
  - Page 5 (SI1145): 4 values (Visible Light, Infrared, UV Index, UV Raw)
  - Page 6 (TSL2591): 4 values (Full Spectrum, Infrared, Visible, Lux)
  - Page 7 (VEML6070): 3 values (UV Raw, UV Level, LSB/MSB)
  - Page 8 (VEML6075): 6 values (UVA Raw, UVB Raw, UVA Compensated, UVB Compensated, UV Index, Compensation)
  - Page 9 (DS2484): 3–9 values (3 per discovered 1-Wire sensor, max 3 sensors → 9 values total)
- **Initial entry point:** Always starts at value index 0 for each page
- **Sensor-specific display:** Each page defines its own set of displayable values (not necessarily all measurements)
- **Consistency:** All pages follow same navigation pattern (B=next, X=prev, Y=exit)
- **Page navigation disabled:** Y/A buttons disabled in large display mode (Y exits instead)

**Evidence:**
- FEATURE-SPEC-007.md — Full specification with per-page value definitions
- Page rendering logic: Modified to support conditional large display rendering
- Button handler logic: Context-sensitive (large display mode vs. standard mode)

---

### FR-017: Large Display Mode Value Navigation — **NEW (FEATURE-SPEC-007)**

**Requirement:**  
The system shall allow users to cycle through displayable values within large display mode using gamepad buttons.

**Details:**
- **Button B (Forward):** Advance to next value; wraps at last value → index 0
- **Button X (Backward):** Advance to previous value; wraps at index 0 → last value
- **Button Y (Exit):** Exit large display mode, return to standard page, reset index to 0
- **Button A (Inactive):** No function in large display mode
- **Cycling behavior:** Button press updates value index immediately (display update bypasses 200 ms rate limit)
- **Display update:** Only the final rendered display is rate-limited (200 ms minimum between redraws)
- **Rate limit applies to:** Display refresh timing, NOT button response timing

**Evidence:**
- Button handling logic: Mode-aware context switching
- Display rate limiting: Applied after value index changes, not blocking value cycling
- FEATURE-SPEC-007 clarifications: Cycling bypasses rate limit

---

### FR-018: Large Display Mode Rendering — **NEW (FEATURE-SPEC-007)**

**Requirement:**  
The system shall render large display mode with enlarged, centered value display for readability at distance.

**Details:**
- **Layout components:**
  - Title: Sensor name (e.g., "BME280 Large Display"), Font 4, Cyan, centered
  - Label: Current value name (e.g., "Temperatur"), Font 4, Cyan, centered
  - Value: Numeric or text value, Font 8 (largest), color-coded, centered
  - Unit: Measurement unit (e.g., "°C", "%"), Font 2, White, centered
  - Footer: Navigation instructions ("B:next X:prev Y:exit"), Font 1, Dark Grey, centered
- **Font sizes:** Uses TFT_eSPI font sizing (Font 1 smallest, Font 8 largest)
- **Color coding:** Inherits from standard page colors (Green/Orange/Cyan, never Magenta)
- **Readability:** Display readable from 5+ meters distance (rationale: presentation/field diagnostics use cases)

**Evidence:**
- FEATURE-SPEC-007.md — Display layout specifications
- TFT_eSPI font constants (fonts 1–8 available)
- Color scheme: Consistent with FR-002 (standard page colors)

---

### FR-019: Unified Large Display Function Architecture — **NEW (FEATURE-SPEC-007)**

**Requirement:**  
The system shall implement large display rendering via single unified renderer function and combined per-page helper function.

**Details:**
- **Unified renderer:** Single `renderLargeDisplay(uint8_t pageIndex, uint8_t valueIndex)` function handles layout for all pages
- **Per-page helper:** Combined function per sensor (not 4 separate functions):
  ```cpp
  // Example: For BME280 page
  String bme280_getLargeDisplayData(uint8_t valueIndex, 
                                     const char** outLabel,    // OUTPUT: "Temperatur"
                                     const char** outUnit,     // OUTPUT: "°C"
                                     uint16_t* outColor);      // OUTPUT: TFT_GREEN
  ```
- **Data flow:**
  1. Button press triggers value index change (no display update yet)
  2. Page render calls `renderLargeDisplay(page, valueIndex)`
  3. `renderLargeDisplay()` calls sensor-specific helper to get label/value/unit/color
  4. Helper retrieves data from cache struct (no I2C calls)
  5. `renderLargeDisplay()` draws layout with retrieved data
  6. Display update subject to 200 ms rate limit
- **Code organization:** Helpers defined near standard page renderer (e.g., after `pageBme280()` function)
- **Memory efficiency:** Single combined function reduces code duplication vs. 4 separate functions per page
- **Scalability:** Adding new page easy: define one helper function + register with pageIndex in switch statement

**Evidence:**
- FEATURE-SPEC-007 clarification: "Combine into single function"
- Implementation approach: Progressive 5-phase rollout
- Backward compatibility: Existing INA219 large display (FR-012) refactored to use unified renderer

---

## Non-Functional Requirements

### NFR-001: Real-Time Response

**Requirement:**  
User input (button press) shall result in visible page navigation within 10 ms.

**Details:**
- Gamepad read frequency: 100 Hz (10 ms) [Updated for improved button responsiveness]
- Page rendering: Immediate (no animation delay)
- LED update: Immediate (blocking I2C call)

**Evidence:**
- Timing gate: line 55, `INPUT_REFRESH_MS = 10` (ms)

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
- ✅ All 10 pages render correctly with sensor data (including DS2484 page 9)
- ✅ Data refreshes at specified intervals (20 ms live, 500 ms sensor pages, 3s for temperatures)
- ✅ Missing sensors display error screen
- ✅ Color coding matches specification
- ✅ DS2484 discovers and displays up to 3 1-Wire temperature sensors
- ✅ ROM-IDs displayed in full 8-byte HEX format
- ✅ Offline sensors marked with "Disconnected" status

**Verification:** Manual testing on hardware; visual inspection; 1-Wire sensor discovery validation

---

### AC-002: Navigation

**Criteria:**
- ✅ Y button navigates to previous page (with wrap-around Page 0 ↔ Page 9)
- ✅ A button navigates to next page (with wrap-around Page 9 ↔ Page 0)
- ✅ Page transitions are immediate (no lag)
- ✅ Navigation works even if some sensors missing
- ✅ All 10 pages are accessible in circular order
- ✅ PAGE_COUNT constant correctly set to 10

**Verification:** Manual testing; edge detection verification; all page transitions

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

**V

### AC-006: DS2484 1-Wire Temperature Monitoring (NEW)

**Criteria:**
- ✅ DS2484 adapter detected on I2C address 0x18 at boot
- ✅ 1-Wire bus scan discovers all connected sensors (max 3)
- ✅ Temperature readings updated every 3 seconds
- ✅ Bus scan performed every 60 seconds
- ✅ ROM-IDs captured and displayed in HEX format (8 bytes: "28:AB:CD:EF:01:02:03:04")
- ✅ Multi-sensor-type support (DS18B20, DS1822, DS1820, etc.)
- ✅ Error handling: 3 consecutive failures → "Disconnected" status
- ✅ Recovery: sensors re-enabled when communication succeeds
- ✅ All sensors displayed on single page (Page 9)

**Verification:** Hardware testing with real DS2484 + 1-3 temperature sensors; test cases in docs/10-testing.mderification:** Integration testing; I2C analyzer or simulation

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

