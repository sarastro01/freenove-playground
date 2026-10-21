# Testing Strategy & Coverage: sensor-pages-dashboard

**Last Updated:** 2026-09-25  
**Current Status:** ⚠️ **NO AUTOMATED TESTS** (Manual testing only, includes DS2484 1-Wire tests)  
**Test Framework:** None (PlatformIO Unity available but not used)

---

## Executive Summary

**Current State:**  
- Zero automated tests exist in the project
- No test infrastructure configured (PlatformIO Unity not enabled)
- All validation is manual (visual inspection, oscilloscope, serial monitor)

**Why No Tests:**
1. **Embedded Hardware Dependency:** Firmware requires real ESP32 + sensors to run
2. **I2C/1-Wire Hardware:** Cannot mock I2C without specialized test fixtures
3. **TFT Display Output:** No headless rendering; display verification is visual
4. **Real-Time Constraints:** Timing validation requires oscilloscope/logic analyzer
5. **Flash Constraints:** Adding test code reduces available heap

**Recommended Approach:**  
Hybrid strategy combining **unit tests** (decoupled from hardware) and **integration tests** (hardware-in-the-loop).

---

## Critical Behaviors Requiring Testing

Based on reverse-engineered business rules (docs/06-business-rules.md), the following behaviors are **mission-critical** and MUST be verified before production:

### TIER 1: Safety-Critical (Must Be Tested)

| Behavior | Why Critical | Test Category |
|----------|-------------|----------------|
| Coulomb accumulation never loses data between reads | Power accounting; billing/diagnostics | Unit |
| CRC8 validation rejects corrupted SCD30 data | Data integrity; safety | Unit |
| Timing gates use signed arithmetic (rollover safe) | System stability; no data loss | Unit |
| DS18B20 state machine terminates (no infinite wait) | Real-time response; no hangups | Integration |
| Page navigation wraps correctly (0↔10 boundaries) | Usability; no stuck states (10 pages now) | Unit |
| INA219 time-gap sanity check prevents overflow | Accumulator integrity | Unit |
| LED updates immediately (< 20 ms latency) | User feedback; responsiveness | Integration |
| DS2484 1-Wire bus scan discovers all sensors | Discovery correctness; max 3 sensors | Integration |

### TIER 2: Functional (Should Be Tested)

| Behavior | Why Important | Test Category |
|----------|---------------|----------------|
| All 10 pages render without crashes | Core functionality (10 pages now) | Integration |
| Page refresh rates (20 ms vs 500 ms) met | Performance | Integration |
| Sensor missing → error screen (no crash) | Graceful degradation | Integration |
| Gamepad falling-edge detection works | Correct page navigation | Integration |
| Encoder LED color cycles correctly | User feature | Unit |
| DS2484 discovers and displays 1-Wire sensors | Multi-sensor support; discovery | Integration |
| I2C address decoding (all 10 sensors) correct | Correct sensor read | Unit |
| **BME280 caching & dew point accuracy (FR-013, BR-SC-001)** | **Cache architecture correctness; ±0.35°C Alduchov & Eskridge** | **Integration** |
| **TSL2591 caching & bitwise extraction (FR-013, BR-SC-001)** | **Correct CHAN0/CHAN1 decoding from getFullLuminosity()** | **Integration** |
| **SI1145 caching & UV index (FR-013, BR-SC-001)** | **Correct raw/100 UV formula** | **Integration** |
| **Sensor error detection via plausibility checks (BR-SC-003)** | **Invalid data rejected; cache not updated** | **Integration** |
| **Sensor timing measurement & logging (BR-SC-004)** | **Serial output for performance debugging** | **Integration** |
| **Large Display Mode entry/exit (FR-012)** | **INA219 alternative UI; user feature** | **Integration** |
| **Large Display Mode value cycling (FR-012)** | **Correct wrap-around on all 7 values** | **Integration** |
| **Large Display Mode value formatting (FR-012)** | **Consistency with standard page** | **Unit** |

---

## Test Cases: Sensor Caching Architecture (FEATURE-SPEC-005, FR-013) — **NEW**

### TC-001: BME280 Caching & Plausibility Checks

**Setup:** Device with BME280 detected at boot (`available.bme280 = true`). Device on Page 0 (BME280 page).

**Action:**
1. Power on device and allow system to stabilize.
2. Observe Serial Monitor for timing output.
3. Ensure BME280 is connected and functional.
4. Monitor for 3-5 sensor read cycles (≈1.5–2.5 seconds).

**Expected Result:**
- Serial output shows: `BME280 read time: XXXX µs` (5–50 ms expected)
- Page 0 displays valid temperature, humidity, pressure, altitude, dew point values
- Values update every 500 ms (SENSOR_REFRESH_MS)
- Dew point calculation correct: `Td ≈ T - (RH%-compensated value)` via Alduchov & Eskridge formula
- No direct I2C operations during page rendering (no slowdown when page drawn)

**Verification:**
- Manual inspection of Serial output for timing
- Temperature in valid range: -40 to +85°C
- Humidity in valid range: 0 to 100%
- Pressure in valid range: 300 to 1100 hPa
- Dew point = f(T, RH) via formula (verify calculation manually or via calculator)
- Page render completes in < 100 ms (no visible lag)

**Error Condition:**
If I2C read fails (sensor disconnected/faulty):
- Serial shows: `ERROR: BME280 read returned invalid values: T=XX, RH=YY, P=ZZ`
- Page displays last known values (stale)
- No crash; page remains responsive

---

### TC-002: TSL2591 Caching & Bitwise Extraction

**Setup:** Device with TSL2591 detected at boot (`available.tsl2591 = true`). Device on Page 6 (TSL2591 page).

**Action:**
1. Power on device and allow system to stabilize.
2. Observe Serial Monitor for timing output.
3. Ensure TSL2591 is connected and receiving light.
4. Shine bright light on sensor (e.g., phone flashlight).
5. Monitor for 3 read cycles while varying light.

**Expected Result:**
- Serial output shows: `TSL2591 read time: XXXX µs` (5–20 ms expected)
- Page 6 displays:
  - fullSpectrum (uint16_t): ADC raw count
  - infrared (uint16_t): IR raw count
  - visible (calculated): fullSpectrum - infrared
  - lux (float): Calculated via manufacturer formula
  - gain and integrationTime (for reference)
- Values respond to light changes within 500 ms (next refresh)
- `visible` always = `fullSpectrum - infrared` (verify calculation)

**Verification:**
- Serial timing logged consistently
- Bitwise extraction correct: lower 16 bits = fullSpectrum, upper 16 bits = infrared
- Lux value increases when light increases (proportional relationship)
- Page updates every 500 ms without lag

**Error Condition:**
If I2C read returns all zeros (sensor disconnected/I2C error):
- Serial shows: `ERROR: TSL2591 read returned zero values`
- Page displays last known values (stale)
- No crash

---

### TC-003: SI1145 Caching & UV Index Calculation

**Setup:** Device with SI1145 detected at boot (`available.si1145 = true`). Device on Page 5 (SI1145 page).

**Action:**
1. Power on device and allow system to stabilize.
2. Observe Serial Monitor for timing output.
3. Ensure SI1145 is connected and exposed to UV light (sunlight or UV lamp).
4. Monitor for 3-5 read cycles.
5. Vary light exposure (e.g., move indoors/outdoors or cover sensor).

**Expected Result:**
- Serial output shows: `SI1145 read time: XXXX µs` (5–15 ms expected)
- Page 5 displays:
  - visible (uint16_t): Light ADC count (0–65535)
  - infrared (uint16_t): IR ADC count (0–65535)
  - uv (uint16_t): Raw UV value from register 0x2C
  - uvIndex (float): `uv / 100.0f` (calculated)
- UV index increases in bright/direct sunlight, decreases indoors (physically correct)
- All values non-negative (no invalid data)
- Page updates every 500 ms

**Verification:**
- Serial timing logged consistently
- UV index formula correct: uvIndex = uv / 100.0f (verify spot-checks)
- UV index physically reasonable:
  - Indoor (typical): 0.5–2.0
  - Outdoor (cloudy): 2–5
  - Outdoor (sunny): 5–11 (can be higher at high altitude/equator)
- Page updates smoothly every 500 ms

**Error Condition:**
If I2C read returns all zeros (sensor disconnected/I2C error):
- Serial shows: `ERROR: SI1145 read returned all zeros`
- Page displays last known values (stale)
- No crash

---

### TC-004: Error Detection via Plausibility Checks (BME280 Out-of-Range)

**Setup:** Device with BME280 in normal operation. Device on Page 0 (BME280).

**Action:**
1. Run normally for 3–5 read cycles to establish baseline values.
2. (Simulated failure) Trigger I2C bus error by:
   - Disconnecting BME280 SCL/SDA line briefly, or
   - Introducing electromagnetic interference near I2C bus
3. Observe Serial Monitor for error message.
4. Observe page for stale vs. new values.
5. Reconnect sensor and allow recovery.

**Expected Result:**
- When error occurs: Serial shows `ERROR: BME280 read returned invalid values: T=XX, RH=YY, P=ZZ`
- Page does NOT update to invalid values; continues to show last valid measurement
- After sensor reconnected: Serial shows `BME280 read time: XXXX µs` (error cleared)
- Page updates to new valid values

**Verification:**
- Cache not updated on failed reads (stale data confirms cache logic)
- Error message appears in Serial log (timing and I2C error coincide)
- Recovery automatic after sensor restored (no manual reset required)
- No display corruption or crashes

---

### TC-005: Sensor Initialization with NaN

**Setup:** Device powered on for first time (fresh flash). Device on Page 0 (BME280).

**Action:**
1. Power on device.
2. Observe Serial Monitor output for first 500 ms (before first sensor read).
3. Observe Page 0 display during this initial period.
4. Wait for first refresh at 500 ms mark.

**Expected Result:**
- During first 0–500 ms: Page 0 displays "---" or similar for temperature, humidity, pressure, altitude, dew point (indicating NaN values, not 0.0)
- Serial output shows no sensor timing yet (first read hasn't fired)
- At ~500 ms: Serial shows `BME280 read time: XXXX µs`
- Page updates to show real values (e.g., temperature = 24.5°C, not 0.0°C)

**Verification:**
- NaN values correctly initialized (not 0.0f)
- UI correctly displays "---" for NaN (distinguish from valid zero measurement)
- First successful read updates cache and page
- Display shows valid values after first read

**Note:** Requires page rendering code to check `isnan()` and display "---" for uninitialized values.

---

### TC-006: Sensor Timing Under I2C Load

**Setup:** All available sensors connected and detected at boot. Device running normal polling cycle.

**Action:**
1. Monitor Serial output for all sensor read times over 10–20 read cycles (5–10 seconds).
2. Record min/max/average timing for each sensor.
3. Verify total sensor read time < 400 ms (leaving headroom for 500 ms SENSOR_REFRESH_MS).

**Expected Result:**
- Individual sensor timings:
  - BME280: 5–50 ms
  - TSL2591: 5–20 ms
  - SI1145: 5–15 ms
  - (Other sensors: historical baseline)
- Total combined read time for all sensors: < 400 ms
- No timing anomalies or exceptions
- Page refresh maintains 500 ms cycle (data updates consistently)

**Verification:**
- Serial log shows timings within expected ranges
- No read exceeds 100 ms (would indicate I2C bus hang)
- Combined load leaves headroom for UI rendering

**Note:** Useful for detecting I2C bus issues (slow pulls, high capacitance, signal integrity).

---

## Test Cases: Large Display Mode (FR-012, INA219 Page 2)
| Sensor missing → error screen (no crash) | Graceful degradation | Integration |
| Gamepad falling-edge detection works | Correct page navigation | Integration |
| Encoder LED color cycles correctly | User feature | Unit |
| DS2484 discovers and displays 1-Wire sensors | Multi-sensor support; discovery | Integration |
| I2C address decoding (all 10 sensors) correct | Correct sensor read | Unit |
| **Large Display Mode entry/exit (FR-012)** | **INA219 alternative UI; user feature** | **Integration** |
| **Large Display Mode value cycling (FR-012)** | **Correct wrap-around on all 7 values** | **Integration** |
| **Large Display Mode value formatting (FR-012)** | **Consistency with standard page** | **Unit** |

### Test Cases: Large Display Mode (FR-012, INA219 Page 2) — **NEW**

#### TEST-FR012-001: Entry Point

**Setup:** Device on Page 2 (INA219), standard page layout displayed.

**Action:** Press Button B.

**Expected Result:**
- Large display mode activates
- Display shows "Laufzeit" value in Font 8, large format
- Header "INA219 Large Display" appears (Font 4, Cyan)
- Separator line visible
- Footer "B:next X:prev Y:exit" appears (Font 2, DarkGrey)
- No page change to other page

**Verification:**
```
largeDisplayMode == true
largeDisplayIndex == 0
currentPage == 2 (unchanged)
```

**Note:** Prerequisite is `available.ina219 == true`.

---

#### TEST-FR012-002: Forward Cycle (Button B)

**Setup:** In large display mode, viewing index 0 (Laufzeit).

**Action:** Press Button B.

**Expected Result:**
- Display rotates to index 1 (Batterie)
- Value updates to show Load Voltage
- Unit shows "V"
- Label and value refresh

**Verification:**
```
largeDisplayIndex == 1
Display shows load voltage value
```

**Repeat:** Press B five more times, verify indices cycle: 2→3→4→5→6.

---

#### TEST-FR012-003: Forward Cycle Wrap-Around

**Setup:** In large display mode, viewing index 6 (Power).

**Action:** Press Button B.

**Expected Result:**
- Display wraps back to index 0 (Laufzeit)
- Cycling is unbroken (no artifacts)

**Verification:**
```
largeDisplayIndex == 0
Display shows runtime (Laufzeit)
```

---

#### TEST-FR012-004: Reverse Cycle (Button X)

**Setup:** In large display mode, viewing index 0 (Laufzeit).

**Action:** Press Button X.

**Expected Result:**
- Display rotates to index 6 (Power)
- Value updates to show power measurement
- Unit shows "mW"

**Verification:**
```
largeDisplayIndex == 6
Display shows power value
```

**Repeat:** Press X five more times, verify indices cycle backward: 5→4→3→2→1→0.

---

#### TEST-FR012-005: Reverse Cycle Wrap-Around

**Setup:** In large display mode, viewing index 0 (Laufzeit).

**Action:** Press Button X.

**Expected Result:**
- Display wraps back to index 6 (Power)
- Cycling is unbroken (no glitch)

**Verification:**
```
largeDisplayIndex == 6
Display shows power (Power)
```

---

#### TEST-FR012-006: Exit Mode (Button Y) - Clean Transition

**Setup:** In large display mode (any index).

**Action:** Press Button Y.

**Expected Result:**
- Immediate return to standard page layout
- All 7 INA219 values visible (label + value + unit rows)
- No visual artifacts, glitches, or leftover text
- Transition is clean and smooth

**Verification:**
```
largeDisplayMode == false
largeDisplayIndex == 0 (reset)
currentPage == 2 (unchanged)
Display shows standard 7-row layout
```

---

#### TEST-FR012-007: Button A Inactivity

**Setup:** In large display mode on Page 2.

**Action:** Press Button A.

**Expected Result:**
- No page navigation occurs
- Large display mode remains active
- Display unchanged

**Verification:**
```
currentPage == 2 (unchanged)
largeDisplayMode == true (unchanged)
largeDisplayIndex == [same as before] (unchanged)
```

---

#### TEST-FR012-008: Real-Time Value Updates

**Setup:** In large display mode, viewing index 1 (Batterie = Load Voltage).

**Action:**
1. Observe voltage value on display.
2. Modify load (e.g., connect/disconnect device).
3. Wait ~500 ms (page refresh cycle).
4. Observe voltage value again.

**Expected Result:**
- Value updates every ~500 ms to reflect live `ina219Values.load`
- Updates match standard page value (when toggled back to standard view)

**Verification:**
- Voltage changes between observations match actual circuit load change
- Formatting remains consistent (3 decimals for load voltage)

---

#### TEST-FR012-009: Value Formatting Consistency

**Setup:** Device cycles between large display and standard page for same value.

**Action:**
1. Navigate to Page 2, press B (enter large display, index 1: Batterie).
2. Observe value format (decimal places, unit).
3. Press Y (exit, back to standard page).
4. Find the corresponding value row in standard layout.
5. Compare formatting.

**Expected Result:**
- Decimal places match exactly (3 decimals for Batterie)
- Unit matches exactly ("V")
- Color may differ (Font 8 vs Font 2), but value numeric part is identical

**Verification:**
- Large display "12.456" matches standard page "12.456" exactly
- No formatting discrepancy

**Repeat** for indices 3 (Strom: 1 decimal) and 6 (Power: 0 decimals) to verify decimal consistency.

---

#### TEST-FR012-010: Page Change Safety Reset

**Setup:** In large display mode on Page 2 (any index).

**Action:** Press Button A (navigate to next page).

**Expected Result:**
- Page navigation occurs normally (currentPage increments)
- Large display mode is forcibly reset: `largeDisplayMode = false`, `largeDisplayIndex = 0`
- If user navigates back to Page 2 later, large display starts fresh

**Verification:**
```
currentPage == 3 (or next page)
largeDisplayMode == false (reset despite mode being active)
largeDisplayIndex == 0 (reset)
```

**Note:** This test verifies safety mechanism BR-033.

---

## Test Cases: Display Rate Limiting (FR-015, BR-SC-011) — **NEW**

### TC-DISPLAY-001: Display Rate Limiting (5 Hz Max, 200 ms Minimum)

**Setup:** Device on Page 2 (INA219), enter large display mode (Button B). Serial monitor open to capture timing.

**Action:**
1. Enter large display mode on INA219 page (Button B)
2. Watch display and measure time between redraws (use oscilloscope on TFT DC line, or log timing via Serial)
3. Observe for 10+ seconds while sensor values update

**Expected Result:**
- Display updates never faster than 200 ms apart (5 Hz maximum)
- All redraw events have ≥ 200 ms gap between them
- Display remains responsive but not flickering
- Value changes visible immediately but respecting rate limit

**Verification:**
- Measure timestamps of display update calls (add debug timing to Serial output if needed)
- Confirm minimum 200 ms between consecutive `drawCurrentPage()` calls
- No redraw at rates faster than 5 Hz (e.g., not 10 Hz, not 20 Hz)
- No visual flicker observed on display when INA219 runtime counter increments

**Test Data (Expected Timing Sequence):**
```
Time (ms)  Event
0          Display update (page change)
200+       INA219 runtime increments (1 second later)
200-400    Display update (if rate limit allows)
600        Runtime increments (2 seconds)
600-800    Display update allowed
1000       Runtime increments (3 seconds)
1000-1200  Display update allowed
```

**Rationale:** Rate limiting prevents flicker caused by rapid runtime value changes (INA219 seconds counter increments every second). Without rate limit, display would redraw excessively, causing CPU/SPI load and visual flicker.

**Related Business Rule:** BR-SC-011 (Display Rate Limiting)

---

### TC-DISPLAY-002: Rate Limit Disabled (Performance Benchmark)

**Setup:** Temporarily modify code to disable rate limiting:
```cpp
// Disable rate limit for testing (comment out nextLargeDisplayRefreshMs check)
// if ((int32_t)(now - nextLargeDisplayRefreshMs) >= 0) {
//     ... redraw allowed
// }
```

**Action:**
1. Recompile with rate limit disabled
2. Upload modified firmware
3. Enter large display mode on Page 2
4. Measure display update frequency over 10 seconds
5. Observe CPU load and visual flicker

**Expected Result (WITH Rate Limit Disabled):**
- Display updates faster than 200 ms interval (measured ≥ 10 Hz or more)
- Visual flicker observable when INA219 runtime changes
- Excessive TFT_eSPI SPI traffic (measurable via oscilloscope on SCLK)
- Potential UI lag if rendering takes too long per cycle

**Comparison:**
| Condition | Update Frequency | Flicker | CPU Load |
|-----------|------------------|---------|----------|
| **WITH rate limit (BR-SC-011)** | ≤ 5 Hz (≥ 200 ms) | None | Low |
| **WITHOUT rate limit** | > 5 Hz (< 200 ms) | Visible | Higher |

**Rationale:** Proves rate limiting is necessary and functioning correctly.

---

### TC-DISPLAY-003: Rate Limit Across Page Changes

**Setup:** Device on Page 2 (INA219), large display mode active. Press Y to exit and navigate to another page.

**Action:**
1. In large display mode on Page 2
2. Press Y to exit (returns to standard page view)
3. Press A to navigate to Page 3
4. Measure time between display updates during navigation

**Expected Result:**
- Page 2 → Page 3 transition: Display updates immediately (no rate limit delay)
- Page 3 data appears within < 50 ms (no 200 ms wait)
- Rate limit does NOT block page navigation (page change triggers full redraw immediately)
- Rate limit only applies WITHIN a page when values change, not during page transitions

**Verification:**
- Timestamp display updates during page navigation
- Confirm no unnecessary 200 ms delay on page change
- Display responsive and feels immediate when changing pages

**Related Code:** Lines 1884-1888 in src/main.cpp (page change bypasses rate limit)

**Rationale:** Rate limit should NOT impede page navigation responsiveness. Only value changes within a page are rate-limited.

---

### TIER 3: Implementation-Specific (Should Be Monitored, Not Strictly Tested)

| Behavior | Why It Matters | Test Category |
|----------|---------------|----------------|
| Display frame rate (IPS) | Performance optimization | Profiling |
| Memory fragmentation (no heap corruption) | Stability over time | Profiling |
| I2C clock 400 kHz (not 100 or 1000) | Hardware reliability | Configuration |
| SPI clock ~80 MHz (TFT_eSPI setup) | Performance | Configuration |
| Enum page order (BME280=0, INA219=3, etc.) | User experience | Documentation |

---

## Test Strategy: 3-Layer Approach

### Layer 1: Unit Tests (Decoupled from Hardware)

**Purpose:** Validate core algorithms in isolation

**Testable Functions:**

#### Test Suite: CRC8 Algorithm

```cpp
// TEST: CRC8 validates valid data
void test_crc8_valid_sequence() {
  uint8_t data[] = {0x02, 0x04, 0xBD, ...};  // SCD30 response
  uint8_t crc = computeCrc8(data, 17);
  ASSERT_EQUAL(crc, data[17]);  // Must match last byte
}

// TEST: CRC8 detects single-bit corruption
void test_crc8_detects_corruption() {
  uint8_t data[] = {...};
  data[4] ^= 0x01;  // Flip one bit
  bool valid = validateScd30Crc(data);
  ASSERT_FALSE(valid);
}
```

**Rationale:** CRC algorithm is pure function (no I2C dependency); can run on desktop.

#### Test Suite: Coulomb Accumulation

```cpp
// TEST: Accumulator adds charge for known current & time
void test_coulomb_accumulation() {
  float current_mA = 100.0;
  uint32_t elapsed_us = 1000000;  // 1 second
  float charge_mah = 0.0;
  
  charge_mah += current_mA * elapsed_us / 3.6e9;
  ASSERT_FLOAT_EQUAL(charge_mah, 0.02778, 0.0001);  // 100mA × 1s ÷ 3600
}

// TEST: Accumulator skips time gaps > 200ms
void test_coulomb_gap_skip() {
  uint32_t elapsed_us = 300000;  // 300 ms
  if (elapsed_us < 200000) {
    charge_mah += current_mA * elapsed_us / 3.6e9;
    ASSERT_UNREACHABLE();
  }
}

// TEST: Accumulator survives overflow (IEEE 754 precision)
void test_coulomb_ieee754_precision() {
  float charge = 3.4e38;  // Near max float32
  charge += 1.0;  // Will lose precision
  ASSERT_FLOAT_EQUAL(charge, 3.4e38);  // IEEE 754 behavior
}
```

**Rationale:** Accumulation formula is pure math; no hardware needed.

#### Test Suite: Timing Gate Rollover Safety

```cpp
// TEST: Signed comparison handles 32-bit rollover
void test_timing_rollover_safety() {
  uint32_t now = 0xFFFFFFF0;  // Near 32-bit max
  uint32_t deadline = 0x00000010;  // Wrapped around
  
  int32_t delta = (int32_t)(now - deadline);
  ASSERT_TRUE(delta >= 0);  // Should fire (not go negative)
}

// TEST: Timing gate fires when deadline reached
void test_timing_gate_fires() {
  uint32_t now = 1000;
  uint32_t deadline = 500;
  ASSERT_TRUE((int32_t)(now - deadline) >= 0);
}

// TEST: Timing gate blocks before deadline
void test_timing_gate_blocks() {
  uint32_t now = 300;
  uint32_t deadline = 500;
  ASSERT_FALSE((int32_t)(now - deadline) >= 0);
}
```

**Rationale:** Rollover detection is pure arithmetic; critical for system stability.

#### Test Suite: Page Navigation (Modulo Arithmetic)

```cpp
// TEST: Forward wrap (page 9 + 1 = 0)
void test_page_nav_forward_wrap() {
  uint8_t page = 9;
  page = (page + 1) % 10;
  ASSERT_EQUAL(page, 0);
}

// TEST: Backward wrap (page 0 - 1 = 9)
void test_page_nav_backward_wrap() {
  uint8_t page = 0;
  page = (page - 1 + 10) % 10;  // Note: need +10 to avoid underflow
  ASSERT_EQUAL(page, 9);
}

// TEST: Forward navigation in middle (5 + 1 = 6)
void test_page_nav_forward_middle() {
  uint8_t page = 5;
  page = (page + 1) % 10;
  ASSERT_EQUAL(page, 6);
}
```

**Rationale:** Navigation logic determines UI usability; pure algorithm.

#### Test Suite: Encoder LED Color Cycling

```cpp
// TEST: Color cycles correctly (blue→green→red→purple)
void test_encoder_led_color_cycle() {
  const char* colors[] = {"blue", "green", "red", "purple"};
  uint8_t idx = 0;
  
  idx = (idx + 1) % 4;
  ASSERT_EQUAL(colors[idx], "green");
  
  idx = (idx + 1) % 4;
  ASSERT_EQUAL(colors[idx], "red");
  
  idx = (idx + 1) % 4;
  ASSERT_EQUAL(colors[idx], "purple");
  
  idx = (idx + 1) % 4;
  ASSERT_EQUAL(colors[idx], "blue");
}

// TEST: Brightness clamping works
void test_encoder_led_brightness_clamp() {
  uint8_t brightness = 250;
  int8_t delta = 10;
  brightness = constrain(brightness + delta * 8, 0, 255);
  ASSERT_EQUAL(brightness, 255);  // Clamped to max
  
  brightness = 5;
  delta = -1;
  brightness = constrain(brightness + delta * 8, 0, 255);
  ASSERT_EQUAL(brightness, 0);  // Clamped to min
}
```

**Rationale:** LED control logic is pure arithmetic; colorspace conversion can be unit-tested.

#### Test Suite: UV Compensation Formula

```cpp
// TEST: Negative compensation clamps to 0
void test_uv_compensation_clamping() {
  float uva = 10.0;
  float comp1 = 100.0;
  float comp2 = 100.0;
  
  float uva_comp = max(0.0, uva - 2.22*comp1 - 1.33*comp2);
  ASSERT_FLOAT_EQUAL(uva_comp, 0.0);  // Clamped
}

// TEST: Positive compensation applied correctly
void test_uv_compensation_formula() {
  float uva = 1000.0;
  float comp1 = 50.0;
  float comp2 = 25.0;
  
  float uva_comp = max(0.0, uva - 2.22*comp1 - 1.33*comp2);
  ASSERT_FLOAT_EQUAL(uva_comp, 1000 - 111 - 33.25, 0.01);  // 855.75
}

// TEST: UV index formula applied correctly
void test_uv_index_calculation() {
  float uva_comp = 100.0;
  float uvb_comp = 100.0;
  
  float index = ((uva_comp * 0.001461) + (uvb_comp * 0.002591)) * 0.5;
  ASSERT_FLOAT_EQUAL(index, 0.20260, 0.0001);
}
```

**Rationale:** Compensation formulas are hardware-independent pure math.

#### Test Suite: Gamepad Falling-Edge Detection

```cpp
// TEST: Falling edge detected (0→1 transition)
void test_gamepad_falling_edge() {
  bool current_state = true;   // Button pressed
  bool last_state = false;     // Was not pressed
  
  if (current_state && !last_state) {
    ASSERT_TRUE();  // Edge detected
  }
}

// TEST: Rising edge NOT detected
void test_gamepad_rising_edge_ignored() {
  bool current_state = false;  // Button released
  bool last_state = true;      // Was pressed
  
  if (current_state && !last_state) {
    ASSERT_FALSE();  // Should not trigger
  }
}

// TEST: Stable high state NOT detected as edge
void test_gamepad_stable_high() {
  bool current_state = true;
  bool last_state = true;
  
  if (current_state && !last_state) {
    ASSERT_UNREACHABLE();  // Should not trigger
  }
}
```

**Rationale:** Edge detection is pure boolean logic; no hardware needed.

### Layer 2: Integration Tests (Hardware-in-the-Loop)

**Purpose:** Validate behavior with real I2C bus, 1-Wire, and display

**Requires:** ESP32 board + all sensors connected

#### Test Suite: I2C Device Detection

```cpp
// TEST: BME280 detected at 0x77
void test_i2c_bme280_detection() {
  Wire.beginTransmission(0x77);
  int err = Wire.endTransmission();
  ASSERT_EQUAL(err, 0);  // Device present
}

// TEST: All 9 sensors enumerated at boot
void test_i2c_all_sensors_enumerated() {
  uint8_t addresses[] = {0x40, 0x10, 0x29, 0x36, 0x38, 0x60, 0x61, 0x77, 0x50};
  for (int i = 0; i < 9; i++) {
    Wire.beginTransmission(addresses[i]);
    int err = Wire.endTransmission();
    // If present, err=0; if absent, err=2 (NACK)
    // Firmware handles both cases gracefully
    ASSERT_TRUE(err == 0 || err == 2);
  }
}

// TEST: Missing sensor doesn't break bus
void test_i2c_missing_sensor_isolation() {
  // Simulate sensor 0x40 (INA219) missing
  // Other sensors should still work
  Wire.beginTransmission(0x10);  // VEML6075
  int err = Wire.endTransmission();
  ASSERT_EQUAL(err, 0);  // Should work
}
```

**Rationale:** I2C enumeration is critical; requires real bus.

#### Test Suite: CRC8 Real Data

```cpp
// TEST: Valid SCD30 response passes CRC
void test_scd30_real_crc_valid() {
  // Real 18-byte response from SCD30
  uint8_t response[] = {
    0x02, 0x04, 0xBD,  // CO2 = 516 ppm
    0x02, 0x15, 0x58,  // Temperature = 21.34 °C
    0x04, 0x60, 0x8A   // Humidity = 28.50 %
    // ... (repeat pattern with CRC bytes)
  };
  
  bool valid = validateScd30Crc(response);
  ASSERT_TRUE(valid);
}

// TEST: Corrupted SCD30 response fails CRC
void test_scd30_real_crc_corrupted() {
  uint8_t response[] = {/* valid data with CRC */};
  response[2] ^= 0x01;  // Corrupt CRC byte
  
  bool valid = validateScd30Crc(response);
  ASSERT_FALSE(valid);
}
```

**Rationale:** Real SCD30 data needed to verify CRC algorithm matches.

#### Test Suite: DS18B20 Async State Machine

```cpp
// TEST: Async state machine completes without hanging
void test_ds18b20_state_machine_completes() {
  requestTemperatures();
  pending = true;
  
  delay(100);  // Wait conversion
  
  float temp = getTempC(address);
  pending = false;
  
  ASSERT_TRUE(temp > -127.0);  // Valid temp
  ASSERT_FALSE(pending);  // State machine terminated
}

// TEST: Temperatures read correctly after request
void test_ds18b20_temperature_accuracy() {
  requestTemperatures();
  delay(100);
  
  float temp = getTempC(address);
  ASSERT_TRUE(temp > 15.0);  // Room temp sanity check
  ASSERT_TRUE(temp < 35.0);  // Reasonable range
}

// TEST: Hot-plug detection works
void test_ds18b20_hotplug_detection() {
  uint8_t count_before = ds18b20GetDeviceCount();
  
  // Disconnect a device
  delay(1000);
  
  rescanDs18b20();
  uint8_t count_after = ds18b20GetDeviceCount();
  
  ASSERT_TRUE(count_after < count_before);
}
```

**Rationale:** Async state machine requires real timing; needs hardware.

#### Test Suite: Page Rendering (No Crash)

```cpp
// TEST: Each page renders without crashing
void test_page_0_bme280_renders() {
  currentPage = 0;
  drawCurrentPage();
  // If crash happens, test fails; if completes, pass
  ASSERT_TRUE(true);
}

void test_page_1_ds18b20_renders() {
  currentPage = 1;
  drawCurrentPage();
  ASSERT_TRUE(true);
}

// ... (repeat for pages 2-9)

// TEST: Page refresh rate meets spec
void test_page_refresh_rate() {
  unsigned long start = millis();
  
  for (int i = 0; i < 100; i++) {
    currentPage = 0;
    drawCurrentPage();
  }
  
  unsigned long elapsed = millis() - start;
  float rate = 1000.0 * 100 / elapsed;
  
  ASSERT_TRUE(rate >= 50);  // At least 50 FPS (500ms slow pages)
}
```

**Rationale:** Display rendering requires real TFT hardware.

#### Test Suite: Encoder LED Latency

```cpp
// TEST: LED update latency < 20 ms
void test_encoder_led_update_latency() {
  unsigned long start = micros();
  updateEncoderLed();
  unsigned long elapsed = micros() - start;
  
  ASSERT_TRUE(elapsed < 20000);  // 20 ms limit
}

// TEST: LED color changes immediately on button press
void test_encoder_led_color_immediate() {
  uint8_t start_color = encoderColorIndex;
  
  // Simulate button press
  simulateButtonPress();
  readEncoderAt50Hz();
  
  uint8_t new_color = encoderColorIndex;
  ASSERT_NOT_EQUAL(new_color, start_color);
}
```

**Rationale:** LED latency requires real seesaw I2C communication.

#### Test Suite: Gamepad Navigation Latency

```cpp
// TEST: Page change latency < 25 ms (gamepad cycle)
void test_gamepad_navigation_latency() {
  unsigned long start = millis();
  
  uint8_t page_before = currentPage;
  
  // Simulate Y button press
  simulateGamepadButtonPress(GAMEPAD_BUTTON_Y);
  readGamepadAt40Hz();
  drawCurrentPage();
  
  unsigned long elapsed = millis() - start;
  uint8_t page_after = currentPage;
  
  ASSERT_NOT_EQUAL(page_before, page_after);
  ASSERT_TRUE(elapsed < 25);  // 25 ms budget
}
```

**Rationale:** Gamepad response time requires real hardware timing.

#### Test Suite: DS2484 1-Wire Temperature Sensors (NEW)

##### Integration Test T1: DS2484 Detection & Initialization
1. Verify DS2484 breakout connected to I2C bus (SDA=GPIO32, SCL=GPIO25)
2. Power on ESP32
3. Observe serial monitor output
4. **Expected:** `available.ds2484 = true` (logged or verified in code); DS2484 initialized successfully
5. Navigate to Page 9 (DS2484)
6. **Expected:** Page displays without error; "DS2484 1-Wire (0x18)" title shown

##### Integration Test T2: 1-Wire Bus Scan & Sensor Discovery
1. Connect 1–3 DS18B20/DS1822/DS1820 sensors to DS2484 DQ pin
2. Power on ESP32
3. Navigate to Page 9
4. Wait for initial bus scan (< 2 seconds)
5. **Expected:** 
   - Status shows "OK - X Sensors" (X = number of connected sensors, 1–3)
   - Each sensor displays 8-byte ROM-ID in HEX format (e.g., "28:AB:CD:EF:01:02:03:04")
   - Sensor availability marked correctly (online = temperature visible, offline = "Disconnected")

##### Integration Test T3: Temperature Polling & Display Update
1. Page 9 displaying 1–3 sensors with temperature values
2. Observe temperature display for 10 seconds
3. Physically warm one sensor with finger or heat source
4. Wait 3 seconds (polling interval)
5. **Expected:** Temperature for that sensor increases (e.g., from 22.50°C to 24.30°C)
6. Cool sensor back down
7. **Expected:** Temperature decreases after next poll (3s)

##### Integration Test T4: Sensor Disconnect Detection
1. Page 9 showing 3 connected sensors, all online
2. Physically disconnect 1 sensor from DS2484 DQ pin
3. Wait for next bus scan (60 seconds) or up to 3 failed reads (9 seconds)
4. **Expected:** Disconnected sensor shows "Disconnected" status in red
5. Other sensors continue showing temperature normally

##### Integration Test T5: Sensor Reconnection & Recovery
1. Page 9 showing 1 sensor as "Disconnected"
2. Reconnect sensor to DS2484 DQ pin
3. Wait for next bus scan (60 seconds) or successful temperature read
4. **Expected:** Sensor re-enabled, shows temperature again (no longer "Disconnected")
5. Error counter reset internally (verified via code inspection)

##### Integration Test T6: Multi-Sensor-Type Support
1. Connect 2× DS18B20 (Family Code 0x28) + 1× DS1822 (Family Code 0x22) to DS2484
2. Navigate to Page 9
3. **Expected:** 
   - All 3 sensors discovered and displayed
   - ROM-IDs show correct family codes (28 for DS18B20, 22 for DS1822)
   - All temperatures readable and correct

##### Integration Test T7: PAGE_COUNT Navigation (10 Pages Circular)
1. Navigate using Gamepad:
   - From Page 8 (VEML6075), press A (next)
   - **Expected:** Move to Page 9 (DS2484)
2. From Page 9, press A (next)
   - **Expected:** Wrap around to Page 0 (BME280)
3. From Page 0, press Y (previous)
   - **Expected:** Wrap around to Page 9 (DS2484)
4. Continue navigation through all 10 pages
   - **Expected:** All pages accessible, no stuck states

##### Integration Test T8: Long-Running Stability (30-minute test)
1. Page 9 displaying DS2484 sensors
2. Let system run for 30 minutes
3. Periodically navigate to other pages and back to Page 9
4. **Expected:**
   - No crashes or freezes
   - Temperature values update correctly every 3 seconds
   - Bus scan completes every 60 seconds without corruption
   - No memory leaks (heap remains stable)
   - ROM-IDs consistent across scans

##### Integration Test T9: Error Recovery after I2C Glitch
1. Page 9 displaying 2 sensors, both online
2. Temporarily disrupt I2C bus (e.g., wiggle SDA/SCL cables briefly, < 100 ms)
3. Observe error counter increment
4. **Expected:** After 1–2 successful reads following the glitch, sensor remains online
5. After 3 consecutive errors, sensor marked "Disconnected"
6. When I2C recovers, sensor can be re-enabled

##### Integration Test T10: Full Feature Integration
1. Complete setup: DS2484 + 3 sensors + BME280 + INA219 + other sensors all connected
2. Navigate between all 10 pages, including Page 9 (DS2484)
3. Perform following actions simultaneously:
   - INA219 charging/discharging with variable load
   - Temperature sensors exposed to temperature changes
   - Gamepad navigation between pages
4. **Expected:**
   - All sensor pages update independently without interference
   - Page 9 displays correct DS2484 data while other pages update normally
   - No deadlocks or data corruption
   - Display refresh rate maintained (20 ms live, 500 ms sensors)

#### Test Suite: Large Display Mode (INA219 Page 2) — **NEW**

**Purpose:** Verify alternate display mode for INA219 sensor showing individual values in enlarged format

##### Unit Tests: Large Display Mode Logic

```cpp
// TEST: Large display value index cycling forward
void test_large_display_index_forward() {
  uint8_t index = 0;
  index = (index + 1) % 7;
  ASSERT_EQUAL(index, 1);
  
  index = 6;
  index = (index + 1) % 7;
  ASSERT_EQUAL(index, 0);  // Wraps around
}

// TEST: Large display value index cycling backward
void test_large_display_index_backward() {
  uint8_t index = 3;
  index = (index - 1 + 7) % 7;
  ASSERT_EQUAL(index, 2);
  
  index = 0;
  index = (index - 1 + 7) % 7;
  ASSERT_EQUAL(index, 6);  // Wraps around
}

// TEST: Large display mode state transitions
void test_large_display_mode_state() {
  bool largeDisplayMode = false;
  
  // Enter mode
  largeDisplayMode = true;
  ASSERT_TRUE(largeDisplayMode);
  
  // Exit mode
  largeDisplayMode = false;
  ASSERT_FALSE(largeDisplayMode);
}
```

**Rationale:** Index cycling is pure modulo arithmetic; page navigation logic is boolean state.

##### Integration Tests: Large Display Mode (Hardware-in-the-Loop)

**Test Scenario:** INA219 sensor connected and functioning

```cpp
// TEST: Enter large display mode (Button B on Page 2)
void test_large_display_entry() {
  uint8_t currentPage = 2;  // INA219 page
  bool bPressed = true;     // Button B pressed
  bool lastB = false;       // Was not pressed last cycle
  bool largeDisplayMode = false;
  
  if (currentPage == 2 && bPressed && !lastB) {
    largeDisplayMode = true;
  }
  lastB = bPressed;
  
  ASSERT_TRUE(largeDisplayMode);
}

// TEST: Cycle through all 7 values in large display mode
void test_large_display_all_values() {
  bool largeDisplayMode = true;
  uint8_t largeDisplayIndex = 0;
  
  // Simulate Button B presses (forward cycle)
  for (int i = 0; i < 7; i++) {
    // Each press cycles the index
    largeDisplayIndex = (largeDisplayIndex + 1) % 7;
  }
  
  // After 7 forward cycles, should be back at index 0
  ASSERT_EQUAL(largeDisplayIndex, 0);
}

// TEST: Button X reverses cycle direction
void test_large_display_reverse_cycle() {
  bool largeDisplayMode = true;
  uint8_t largeDisplayIndex = 3;
  
  bool xPressed = true;
  bool lastX = false;
  
  if (largeDisplayMode && xPressed && !lastX) {
    largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7;  // 3 → 2
  }
  lastX = xPressed;
  
  ASSERT_EQUAL(largeDisplayIndex, 2);
}

// TEST: Button Y exits large display mode
void test_large_display_exit() {
  bool largeDisplayMode = true;
  uint8_t largeDisplayIndex = 4;
  
  bool yPressed = true;
  bool lastY = false;
  
  if (largeDisplayMode && yPressed && !lastY) {
    largeDisplayMode = false;
    largeDisplayIndex = 0;  // Reset index on exit
  }
  lastY = yPressed;
  
  ASSERT_FALSE(largeDisplayMode);
  ASSERT_EQUAL(largeDisplayIndex, 0);
}

// TEST: Button A is inactive in large display mode
void test_large_display_button_a_inactive() {
  bool largeDisplayMode = true;
  uint8_t currentPage = 2;
  uint8_t nextPage = currentPage;
  
  bool aPressed = true;
  bool lastA = false;
  
  if (largeDisplayMode) {
    // Button A ignored; page does not change
    if (aPressed && !lastA) {
      // No action
    }
  }
  lastA = aPressed;
  
  ASSERT_EQUAL(nextPage, currentPage);  // Page unchanged
}

// TEST: Page navigation disabled during large display mode
void test_large_display_page_nav_disabled() {
  bool largeDisplayMode = true;
  uint8_t currentPage = 2;
  
  bool yPressed = true;
  bool lastY = false;
  
  if (!largeDisplayMode) {  // Normal page navigation only when mode OFF
    if (yPressed && !lastY) {
      currentPage = (currentPage + 10 - 1) % 10;  // Would navigate
      ASSERT_FAIL();  // Should not reach here
    }
  }
  
  ASSERT_EQUAL(currentPage, 2);  // Page unchanged
}

// TEST: Real-time value display (no caching)
void test_large_display_real_time_update() {
  // Simulate sensor read updating ina219Values
  ina219Values.bus = 12.456;
  ina219Values.current = 1.234;
  
  // Large display function reads directly
  String value = getLargeDisplayValue(1);  // Batterie index
  ASSERT_EQUAL(value, "12.456");  // Fresh data
  
  // Simulate second read
  ina219Values.bus = 12.789;
  value = getLargeDisplayValue(1);
  ASSERT_EQUAL(value, "12.789");  // Updated data
}

// TEST: Formatting consistency between normal and large display
void test_large_display_formatting_consistency() {
  // Standard display format (3 decimals)
  String standard_value = String(ina219Values.bus, 3);  // "12.456"
  
  // Large display format (should be identical)
  String large_value = getLargeDisplayValue(1);  // Batterie index
  
  ASSERT_EQUAL(standard_value, large_value);
}

// TEST: Color scheme consistency
void test_large_display_color_consistency() {
  // Standard page Page 2 colors
  // Index 1 (Batterie/Bus voltage) should be Green
  uint16_t color = getLargeDisplayColor(1);
  ASSERT_EQUAL(color, TFT_GREEN);
  
  // Index 0 (Laufzeit/Runtime) should be Cyan
  color = getLargeDisplayColor(0);
  ASSERT_EQUAL(color, TFT_CYAN);
}

// TEST: Helper functions return correct labels
void test_large_display_labels() {
  ASSERT_EQUAL(strcmp(getLargeDisplayLabel(0), "Laufzeit"), 0);
  ASSERT_EQUAL(strcmp(getLargeDisplayLabel(1), "Batterie"), 0);
  ASSERT_EQUAL(strcmp(getLargeDisplayLabel(6), "Power"), 0);
}

// TEST: Helper functions return correct units
void test_large_display_units() {
  ASSERT_EQUAL(strcmp(getLargeDisplayUnit(0), "s"), 0);         // Runtime
  ASSERT_EQUAL(strcmp(getLargeDisplayUnit(1), "V"), 0);        // Batterie
  ASSERT_EQUAL(strcmp(getLargeDisplayUnit(3), "mA"), 0);       // Strom
  ASSERT_EQUAL(strcmp(getLargeDisplayUnit(6), "mW"), 0);       // Power
}
```

**Expected Behavior:**
- Mode entry triggered by Button B on Page 2 (INA219)
- All 7 values display correctly with proper formatting and colors
- Forward/reverse cycling wraps at boundaries (0↔6)
- Mode exit via Button Y clears mode flag and resets index
- Page navigation completely disabled during large display mode
- Button A completely inactive (no side effects)
- Values update in real-time from `ina219Values` struct
- Formatting and colors identical to standard Page 2 display

**Failure Cases to Test:**
- Button press not detected (I2C error) → mode unchanged
- Mode stuck (Y button fails) → requires reboot
- Value formatting mismatch between modes → visual inconsistency
- Page navigation accidentally enabled during mode → unexpected page changes
- Color scheme incorrect → misleading value categorization

---

### Layer 3: Performance & Profiling (Monitoring)

**Purpose:** Detect regressions; ensure no memory leaks or timing violations

#### Profiling: Memory Usage

```cpp
// PROFILE: Heap usage after initialization
void profile_heap_usage() {
  uint32_t free_before = ESP.getFreeHeap();
  
  setup();
  
  uint32_t free_after = ESP.getFreeHeap();
  uint32_t allocated = free_before - free_after;
  
  Serial.printf("Heap used: %u bytes (limit: 320000)\n", allocated);
  ASSERT_TRUE(allocated < 100000);  // Should be << 320 KB
}

// PROFILE: Global struct sizes
void profile_struct_sizes() {
  Serial.printf("SensorStatus: %u bytes\n", sizeof(SensorStatus));
  Serial.printf("Ina219Values: %u bytes\n", sizeof(Ina219Values));
  // ... all 8 structs
  // Total should be ~300 bytes
}
```

**Rationale:** Monitor memory to catch structural regressions.

#### Profiling: Loop Cycle Time

```cpp
// PROFILE: Main loop cycle time
void profile_loop_cycle_time() {
  unsigned long start = micros();
  
  loop();
  
  unsigned long elapsed = micros() - start;
  Serial.printf("Loop cycle: %u µs\n", elapsed);
  
  // Should be << 20000 µs (20 ms to leave margin for all reads)
}
```

**Rationale:** Detect if new code adds unexpected latency.

#### Profiling: I2C Bus Speed

```cpp
// PROFILE: I2C transaction time
void profile_i2c_speed() {
  unsigned long start = micros();
  
  Wire.beginTransmission(0x40);
  Wire.write(0x01);
  int err = Wire.endTransmission();
  uint8_t data[2];
  Wire.requestFrom(0x40, 2);
  data[0] = Wire.read();
  data[1] = Wire.read();
  
  unsigned long elapsed = micros() - start;
  Serial.printf("INA219 read: %u µs\n", elapsed);
  
  // Should be < 1000 µs (1 ms)
}
```

**Rationale:** Verify I2C clock settings and bus performance.

---

## Recommended Test Implementation Plan

### Phase 1: Unit Tests (No Hardware Required)

**Effort:** 2–3 days  
**Tools:** PlatformIO Unity (built-in)  
**Setup:**

1. Enable Unity in `platformio.ini`:
   ```ini
   [env:esp32dev]
   test_framework = unity
   test_ignore_path = test/  ; or test/hw_only/
   test_ignore = test_*_hw.cpp
   ```

2. Create test files in `test/`:
   - `test_crc8.cpp` — CRC algorithm
   - `test_coulomb.cpp` — Accumulation
   - `test_timing.cpp` — Rollover safety
   - `test_navigation.cpp` — Page wrapping
   - `test_led_color.cpp` — Encoder LED
   - `test_uv_compensation.cpp` — UV formulas
   - `test_gamepad_edge.cpp` — Button detection

3. Run tests:
   ```bash
   pio test -e esp32dev
   ```

**Coverage:** ~70% of logic (algorithms + pure functions)

### Phase 2: Integration Tests (With Hardware)

**Effort:** 3–5 days  
**Setup:**

1. Create test board configuration:
   ```bash
   # Requires: ESP32 + all 9 I2C sensors + DS18B20 + display
   pio test -e esp32dev_hw
   ```

2. Create test files in `test/`:
   - `test_i2c_enumeration_hw.cpp` — Device detection
   - `test_scd30_crc_hw.cpp` — Real SCD30 data
   - `test_ds18b20_async_hw.cpp` — State machine
   - `test_page_rendering_hw.cpp` — Display
   - `test_encoder_led_latency_hw.cpp` — Timing
   - `test_gamepad_latency_hw.cpp` — Navigation

3. Run hardware tests:
   ```bash
   pio test -e esp32dev_hw
   ```

**Coverage:** ~95% of application (all major pathways)

### Phase 3: CI/CD Integration (Automated)

**Effort:** 1–2 days  
**Setup:**

1. GitHub Actions workflow (`.github/workflows/test.yml`):
   ```yaml
   on: [push, pull_request]
   jobs:
     unit-tests:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v3
         - uses: platformio/cache-action@v1
         - run: pio test -e esp32dev
   ```

2. Requires: Automated hardware test fixture (advanced)

**Benefit:** Every commit validated before merge

---

## Test Execution Matrix

| Test Category | Executable | Hardware Required | Effort | Value |
|---------------|-----------|-------------------|--------|-------|
| **CRC8 Algorithm** | `pio test test_crc8` | No | 0.5d | HIGH |
| **Coulomb Accumulation** | `pio test test_coulomb` | No | 0.5d | HIGH |
| **Timing Rollover** | `pio test test_timing` | No | 0.5d | HIGH |
| **Page Navigation** | `pio test test_navigation` | No | 0.5d | HIGH |
| **LED Color Cycle** | `pio test test_led_color` | No | 0.5d | MEDIUM |
| **UV Compensation** | `pio test test_uv_compensation` | No | 1d | MEDIUM |
| **Gamepad Edge Detect** | `pio test test_gamepad_edge` | No | 0.5d | MEDIUM |
| **I2C Enumeration** | `pio test test_i2c_hw` | Yes (sensors) | 1d | HIGH |
| **SCD30 CRC Real Data** | `pio test test_scd30_crc_hw` | Yes (SCD30) | 0.5d | HIGH |
| **DS18B20 Async** | `pio test test_ds18b20_hw` | Yes (DS18B20) | 1d | HIGH |
| **Page Rendering** | `pio test test_pages_hw` | Yes (display) | 2d | HIGH |
| **Encoder LED Latency** | `pio test test_led_latency_hw` | Yes (encoder) | 0.5d | MEDIUM |
| **Gamepad Latency** | `pio test test_gamepad_latency_hw` | Yes (gamepad) | 0.5d | MEDIUM |
| **Memory Profiling** | `pio run --monitor` | Yes (ESP32) | 0.5d | LOW |
| **Loop Cycle Profiling** | `pio run --monitor` | Yes (ESP32) | 0.5d | LOW |
| **I2C Speed Profiling** | `pio run --monitor` | Yes (ESP32) | 0.5d | LOW |

**Total Effort (All Tests):** ~12 days (distributed across team)  
**Minimal Viable Testing (TIER 1 only):** ~2 days

---

## Testing Known Limitations

### LIM-T001: I2C Mocking Difficulty

**Limitation:**  
Real I2C devices required for realistic integration tests.

**Workaround:**
- Use Arduino I2C emulator (rarely available)
- Manual hardware testing (preferred)
- Simulation with oscilloscope (expensive)

**Evidence:** Adafruit libraries don't expose mock-able interfaces.

### LIM-T002: TFT Display Verification

**Limitation:**  
Cannot verify display rendering without real hardware.

**Workaround:**
- Visual inspection (manual)
- Frame buffer comparison (requires display abstraction)
- Screenshot comparison (future feature)

**Evidence:** TFT_eSPI library is hardware-specific.

### LIM-T003: Real-Time Timing Precision

**Limitation:**  
Cannot guarantee microsecond-precision timing in test harness.

**Workaround:**
- Use oscilloscope/logic analyzer (verification, not testing)
- Accept ±10% timing variance in tests
- Test rollover safety logic in isolation

**Evidence:** Arduino `micros()` is hardware-dependent.

### LIM-T004: Sensor Data Simulation

**Limitation:**  
Cannot easily simulate realistic sensor data (e.g., SCD30 CRC, UV compensation).

**Workaround:**
- Use real sensors for integration tests
- Manually create CRC-valid test vectors (already done above)
- Mock at library boundary (Adafruit_SCD30, etc.)

**Evidence:** Sensor protocols are hardware-specific.

### LIM-T005: Flash Space Constraints

**Limitation:**  
Adding test code increases binary size; may exceed ESP32 flash limits.

**Workaround:**
- Use conditional compilation (`#ifdef TESTING`)
- Store test vectors in PROGMEM
- Run tests on separate branch (don't merge to production)

**Evidence:** ESP32 typical flash: 4 MB; production firmware: ~500 KB; test code: ~200 KB.

---

## Manual Testing Checklist

**When Automated Tests Unavailable**  
Use this checklist to validate changes before deployment:

- [ ] All 9 pages render without crashes or visual artifacts
- [ ] Page navigation wraps correctly (0 ↔ 9)
- [ ] Gamepad buttons work: Y=prev, A=next
- [ ] Encoder dial adjusts LED brightness (0–255 range)
- [ ] Encoder button cycles LED color (4-state cycle)
- [ ] LED updates within 1 frame (< 20 ms visual latency)
- [ ] SCD30 page displays CO₂, temp, humidity (or "Sensor nicht vorhanden")
- [ ] DS18B20 page detects new devices within 3 seconds
- [ ] DS18B20 page detects removed devices within 3 seconds
- [ ] INA219 Coulomb counter accumulates (visible change per second @ 1A)
- [ ] INA219 does not reset on long idle (>100ms gap)
- [ ] VEML6075 UV index formula applied (clamped to ≥ 0)
- [ ] All sensor data formats (decimals, colors) match spec
- [ ] No memory leaks (check `ESP.getFreeHeap()` over 10 min runtime)
- [ ] Serial output shows no error messages
- [ ] Compilation succeeds: `pio run -t upload` exit code 0

---

## Regression Test Suite (Minimal)

**When Time is Limited**  
Run these tests before every release:

1. **CRC8 Algorithm** (`test_crc8.cpp`) — 5 min
2. **Coulomb Accumulation** (`test_coulomb.cpp`) — 5 min
3. **Timing Rollover** (`test_timing.cpp`) — 5 min
4. **All Pages Render** (manual) — 10 min
5. **Navigation Wrap** (manual) — 5 min
6. **LED Response** (manual) — 5 min
7. **Memory Leak Check** (manual) — 5 min

**Total Time:** ~40 minutes for critical path

---

## Known Issues Found by Manual Testing

(None currently documented; update as bugs discovered)

---

## Future Testing Improvements

### Future-T001: Hardware Test Fixture

**Goal:** Automated I2C + 1-Wire fixture for continuous integration

**Approach:**
- Raspberry Pi as test controller
- Programmable I2C bus multiplexer
- Simulated sensor responses via GPIO
- GitHub Actions integration

**Effort:** 2–3 weeks

### Future-T002: Display Frame Buffer Comparison

**Goal:** Automated visual regression testing

**Approach:**
- Capture TFT frame buffer
- Compare against golden reference
- Report pixel differences

**Effort:** 1 week

### Future-T003: Real-Time Profiler

**Goal:** Continuous monitoring of timing constraints

**Approach:**
- Instrumentation in main loop
- Log cycle times to SPIFFS
- Alert if thresholds exceeded

**Effort:** 1 week

### Future-T004: Fuzz Testing for CRC

**Goal:** Ensure CRC8 correctness over all inputs

**Approach:**
- Random byte sequences
- Verify CRC matches Poly 0x31 spec
- Property-based testing (QuickCheck-style)

**Effort:** 1 week

---

## Test Cases: Non-Blocking Rotated Sensor Architecture (FEATURE-SPEC-006)

### TC-Rotated-001: Sensor Rotation Scheduling (10-Slot Cycle, 2000ms)

**Setup:** Device powered on, all sensors detected at boot. Monitor main loop execution via Serial output and timing measurements.

**Action:**
1. Power on device.
2. Observe Serial output for 3-5 full cycles (~6-10 seconds = 10 slots × 200ms × 3-5 cycles).
3. Note timestamps for each sensor read operation.
4. Verify timing gates and slot transitions.
5. Verify phase-based organization (START async → READ sync → CHECK async).

**Expected Result:**
- Serial output shows sensor reads in phase-based rotation:
  ```
  **Phase 1: START ASYNC (Slots 0-1, 0-200ms)**
  [0 ms]   Slot 0: TSL2591 async START time: 45 µs
  [200 ms] Slot 1: DS18B20 async START time: 50 µs
  
  **Phase 2: READ SYNC (Slots 2-7, 400-1400ms)**
  [400 ms]   Slot 2: BME280 read time: 2156 µs
  [600 ms]   Slot 3: SI1145 read time: 689 µs
  [800 ms]   Slot 4: VEML6075 read time: XXX µs
  [1000 ms]  Slot 5: VEML6070 read time: XXX µs
  [1200 ms]  Slot 6: SCD30 read time: XXX µs
  [1400 ms]  Slot 7: DS2484 Scan (60s gate, or skipped)
  
  **Phase 3: CHECK ASYNC (Slots 8-9, 1600-1800ms)**
  [1600 ms] Slot 8: TSL2591 async result read time: 1045 µs
  [1800 ms] Slot 9: DS18B20 async result read time: 2345 µs
  
  [2000 ms] <- New cycle starts at Slot 0
  ```
- Each slot executes within 200 ms interval (or less for async operations)
- No blocking > 100 ms observed (no stalled output)
- Full cycle completes every 2000 ms (2.0 seconds)
- Phase-based organization clearly visible

**Verification:**
- Capture Serial output for 20+ seconds
- Calculate slot timing: verify each slot fires at expected time ±10ms
- Verify 10 slots per cycle (not 7)
- Verify cycle time is 2000ms (not 1400ms)
- Verify phase order: Phase 1 (slots 0-1) → Phase 2 (slots 2-7) → Phase 3 (slots 8-9)
- No skipped slots or double executions

**Error Condition:**
- If slot timing exceeds 200 ms: Sensor read is too slow (timing violation)
- If cycle time ≠ 2000 ms: Slot counter may be miscalculated or using old 1400ms values
- If Serial output stalls: Main loop may be blocked (critical failure)
- If only 7 slots observed: Code still using old architecture (must rebuild)

---

### TC-Rotated-002: TSL2591 Async Measurement (Two-Phase Read, Slot 0 → Slot 8)

**Setup:** Device with TSL2591 detected at boot (`available.tsl2591 = true`). Monitor TSL2591 page and Serial output.

**Action:**
1. Power on device and navigate to TSL2591 page (page 6).
2. Observe Serial output for 3-4 full rotation cycles (~8 seconds).
3. Note TSL2591 async START (Slot 0) and async CHECK (Slot 8) operations.
4. Verify timing: START at 0ms, CHECK at 1600ms (1600ms > 200ms integration ✓).
5. Verify lux values update after async completion.

**Expected Result:**
- Serial output shows async pattern:
  ```
  Slot 0 (0 ms):     TSL2591 async START time: 45 µs
  Slot 8 (1600 ms):  TSL2591 async result read time: 1045 µs
  (Next cycle, ~2000ms later)
  Slot 0 (2000 ms):  TSL2591 async START time: 45 µs
  Slot 8 (3600 ms):  TSL2591 async result read time: 1045 µs
  ```
- Time from START to CHECK: ~1600 ms (> 200ms integration time required ✓)
- Lux value on page updates only when async CHECK completes (Slot 8)
- No blocking observed (display remains responsive during measurement)
- User can change pages while TSL2591 measurement in progress

**Verification:**
- Measure time difference: `check_timestamp - start_timestamp ≈ 1600 ms ±50ms`
- Verify 1600ms > 200ms integration time (margin = 1400ms)
- Verify `Tsl2591State` transitions: `IDLE → MEASURING (Slot 0) → IDLE (Slot 8)`
- Check lux value in `tsl2591Values` cache increases after Slot 8 check
- Ensure page navigation not blocked during async measurement
- Verify lux changes are proportional to light level

**Error Condition:**
- If CHECK time < START time + 200ms: Measurement not complete (garbage value)
- If state machine stuck in MEASURING: Next measurement cannot start (deadlock)
- If readiness check bypassed: 200 ms blocking (violates non-blocking requirement)
- If lux value is 0 always: Async result not being used (check readiness logic)

---

### TC-Rotated-003: DS18B20 Async Conversion (Two-Phase Read, Slot 1 → Slot 9)

**Setup:** Device with DS2484 detected at boot (`available.ds2484 = true`) and at least 1 DS18B20 sensor discovered. Monitor DS18B20 page (page 9) and Serial output.

**Action:**
1. Power on device and navigate to DS18B20 page (page 9).
2. Observe Serial output for 2-3 full rotation cycles (~6 seconds).
3. Note DS18B20 async START (Slot 1) and async CHECK (Slot 9) operations.
4. Verify timing: START at 200ms, CHECK at 1800ms (1600ms elapsed > 750ms conversion ✓).
5. Verify temperature values update after async completion.
6. Disconnect one DS18B20 sensor mid-measurement to test error handling.

**Expected Result:**
- Serial output shows async pattern:
  ```
  Slot 1 (200 ms):   DS18B20 async START time: 50 µs
  Slot 9 (1800 ms):  DS18B20 async result read time: 2345 µs
  (Next cycle, ~2000ms later)
  Slot 1 (2200 ms):  DS18B20 async START time: 50 µs
  Slot 9 (3800 ms):  DS18B20 async result read time: 2345 µs
  ```
- Time from START to CHECK: ~1600 ms (750ms conversion + 850ms margin ✓)
- Temperature values on page update only after async CHECK completes (Slot 9)
- Disconnected sensor shows "Disconnected" status after 3 consecutive read failures
- Page refresh remains responsive (<100ms latency)

**Verification:**
- Measure time difference: `check_timestamp - start_timestamp ≈ 1600 ms ±50ms`
- Verify 1600ms > 750ms conversion time at 12-bit precision
- Verify `Ds18b20State` transitions: `IDLE → CONVERTING (Slot 1) → IDLE (Slot 9)`
- Check temperature values in `ds2484Values.sensors[].temperature` after Slot 9
- Verify three-strike lockout: after 3 errors, sensor.available = false
- Verify locked-out sensor does not appear in poll loop (no retry attempts)
- Verify temperature changes are realistic (<10°C/sec typically)

**Error Condition:**
- If CHECK time < START time + 750ms: Conversion not complete (stale temperature)
- If state machine stuck in CONVERTING: Next conversion cannot start (deadlock)
- If readiness check bypassed: 750 ms blocking (CRITICAL, violates latency requirement)
- If three-strike not working: Locked-out sensor keeps getting polled (wastes I2C)
- If sensor not re-enabled after reconnect: Requires reboot (expected behavior)

---

### TC-Rotated-004: Display Update Only on Value Change

**Setup:** Device powered on. Navigate to sensor page (e.g., BME280, page 0). Monitor display updates and serial timing.

**Action:**
1. Power on device and go to sensor page.
2. Observe page for 5 seconds (multiple sensor read cycles).
3. Count number of display redraws (visual flicker/refresh).
4. Check Serial output to verify if values changed.
5. Leave page unchanged and observe redraw count.

**Expected Result:**
- Display redraws only when values change (few redraws per minute if data stable)
- No continuous flicker (which would indicate constant redraws)
- Serial output shows sensor read times; page renders only if values differ
- If sensor returns same values for 10 read cycles, page redraws ≤ 2 times (page change + 1 data update)

**Verification:**
- Manually count display flickers (visual inspection)
- Compare to expected: With stable sensor values, redraw frequency < 1/second
- Confirm no "pageChanged" false positives (page not changing but redrawing anyway)
- Check that INA219 timer update (every 1 second) triggers redraw even if other values unchanged

**Error Condition:**
- If page redraws every cycle: Always-redraw logic not implemented (wastes CPU/power)
- If page never redraws after first display: Value comparison broken (stale display)
- If INA219 timer doesn't trigger redraw: Display stuck (timer updates invisible)

---

### TC-Rotated-005: Input Latency < 200 ms (Guaranteed Response)

**Setup:** Device powered on. Gamepad joystick connected. Monitor page response timing.

**Action:**
1. Power on device and go to page 1 (Gamepad page).
2. Press Gamepad button (Y or A) to navigate.
3. Measure time from button press to visible page change.
4. Repeat 10 times, varying timing of button press (before sensor read, during, after).
5. Intentionally press button while TSL2591/DS18B20 measurements in progress.

**Expected Result:**
- Page change visible within 200 ms of button press (worst case)
- Typical response: < 50 ms (responsive feel)
- Even if button pressed mid-sensor-read: Response within 200 ms maximum
- No input loss or dropped button presses

**Verification:**
- Oscilloscope or video slow-motion to measure latency accurately
- Test at least 5 button presses per scenario (before/during/after sensor read)
- Measure frame update time on display (typical TFT refresh ~40 ms per frame)
- Verify no input queuing (button press should NOT delay next press response)

**Error Condition:**
- If page change takes > 200 ms: Input latency too high (fails specification)
- If button press missed or queued: Input handling broken (user experience poor)
- If input latency increases over time: Memory leak or state corruption (critical)

---

### TC-Rotated-006: Configurable Stats Reporting

**Setup:** Device with `#define ENABLE_STATS_REPORTING 1` (enabled, default).

**Action:**
1. Power on device with stats enabled.
2. Observe Serial output for 2 seconds.
3. Verify aggregated stats printed every 1 second.
4. Rebuild firmware with `#define ENABLE_STATS_REPORTING 0` (disabled).
5. Flash and observe Serial output for 2 seconds.

**Expected Result:**

*With ENABLED (default):*
```
BME280 read time: 2156 µs
TSL2591 async START time: 45 µs
SI1145 read time: 689 µs
VEML6075 read time: 587 µs
VEML6070 read time: 498 µs
SCD30 read time: 356 µs
DS18B20 async START time: 50 µs
=== Sensor Stats (1000 ms) ===
BME280:       min=2100 µs, max=2300 µs, avg=2156 µs, reads=5
TSL2591:      async_starts=1, async_ready=1, avg_wait=205 ms
SI1145:       min=650 µs, max=750 µs, avg=689 µs, reads=5
VEML6075:     min=550 µs, max=650 µs, avg=600 µs, reads=1
VEML6070:     min=450 µs, max=550 µs, avg=500 µs, reads=1
SCD30:        min=300 µs, max=400 µs, avg=350 µs, reads=1
DS18B20:      async_starts=1, async_ready=1, avg_wait=925 ms
===================================
```

*With DISABLED:*
```
BME280 read time: 2156 µs
TSL2591 async START time: 45 µs
SI1145 read time: 689 µs
VEML6075 read time: 587 µs
VEML6070 read time: 498 µs
SCD30 read time: 356 µs
DS18B20 async START time: 50 µs
(No "=== Sensor Stats ===" section)
```

**Verification:**
- Count lines in Serial output: Enabled ≈ 15 lines/sec, Disabled ≈ 7 lines/sec
- Verify aggregated stats section present when enabled, absent when disabled
- Confirm per-sensor timing always logged regardless of flag
- Check that disabled mode is cleaner for production use

**Error Condition:**
- If stats printed even when disabled: Flag not respected (bug)
- If stats not printed when enabled: Aggregation logic missing (bug)
- If per-sensor timing missing: Always-log requirement not met (bug)

---

## Summary: Current Test Status

| Aspect | Current | Recommendation |
|--------|---------|-----------------|
| **Automated Tests** | 0 | Add 7 unit tests (Phase 1) |
| **Integration Tests** | 0 | Add 6 hardware tests (Phase 2) |
| **Manual Checklists** | None | Adopt 15-item checklist (quick start) |
| **CI/CD** | None | Add GitHub Actions (Phase 3) |
| **Test Coverage** | 0% | Target 70% unit + 95% integration |
| **Estimated Time to Full Testing** | — | 12 days (phased approach) |
| **Minimum Viable Testing** | — | 2 days (TIER 1 unit tests) |

---

## Test Cases: Universal Large Display Mode (FEATURE-SPEC-007) — **NEW**

### TC-LDM-001: Large Display Mode Entry (Button B on All Pages)

**Setup:** Device with at least BME280 sensor (Page 0) working. Device boots to Page 0 in standard view.

**Action:**
1. Observe Page 0 displaying standard sensor data (7 rows of labels + values)
2. Press Button B once
3. Observe display

**Expected Result:**
- Display immediately switches to large display mode
- Page title "BME280 Large Display" displayed at top (Font 4, Cyan)
- Value label "Temperatur" displayed (Font 4, Cyan)
- Large temperature value displayed centered (Font 8, Green, e.g., "24.50")
- Unit "°C" displayed below (Font 2, White)
- Footer "B:next  X:prev  Y:exit" displayed at bottom (Font 1, Dark Grey)
- Display readable from 5+ meters away

**Verification:**
- Visual inspection; font size and colors match specification
- Large display layout correct
- No crashes or display artifacts

**Repeat for All 10 Pages:**
- Pages 0–9 should support entry via Button B
- Each page shows correct title, initial value, unit, color
- Verify at least one other page (e.g., Page 2 INA219, Page 4 SCD30)

**Error Condition:**
If sensor unavailable (error screen shown):
- Button B press is ignored
- Page remains in error view
- Large display mode not entered

---

### TC-LDM-002: Large Display Mode Value Cycling Forward (Button B in Mode)

**Setup:** Device on Page 0, large display mode active, displaying value index 0 (Temperatur).

**Action:**
1. Press Button B repeatedly (5–10 times)
2. Observe display changes

**Expected Result:**
- First B press: Advance to index 1 (Luftdruck/hPa, Green)
- Second B press: Advance to index 2 (Feuchtigkeit/%, Orange)
- Third B press: Advance to index 3 (Taupunkt/°C, Cyan)
- ... continue through all 7 values ...
- Seventh B press: Advance to index 0 (wraps around back to Temperatur)
- Each press cycles to next value with updated label/value/unit/color
- Button response feels immediate (< 50 ms)
- Display update within 200 ms of button press

**Verification:**
- All values cycle correctly with no gaps
- Wrap-around behavior works (last value → index 0)
- Labels, values, units, and colors correct for each value
- No crashes during cycling

**Repeat for Other Pages:**
- Verify Page 1 (Gamepad, 4 values): 0→1→2→3→0
- Verify Page 2 (INA219, 7 values): 0→...→6→0
- Verify Page 4 (SCD30, 3 values): 0→1→2→0

---

### TC-LDM-003: Large Display Mode Value Cycling Backward (Button X in Mode)

**Setup:** Device on Page 0, large display mode active, displaying value index 0.

**Action:**
1. Press Button X once
2. Observe display

**Expected Result:**
- Display jumps to index 6 (last value, wraps backward from 0)
- Value label "Temperaturtrend" displayed
- Value shown with correct unit and color
- Display updates within 200 ms

**Action:**
3. Press Button X again
4. Observe display

**Expected Result:**
- Display shows index 5 (Kompensation)
- Continue pressing X; should cycle backward through all indices

**Verification:**
- Backward cycling works correctly
- Wrap-around from 0 to (valueCount-1) correct
- All values reachable via backward cycling
- No display artifacts

---

### TC-LDM-004: Large Display Mode Exit (Button Y in Mode)

**Setup:** Device on Page 0, large display mode active, displaying some value (e.g., index 2).

**Action:**
1. Press Button Y

**Expected Result:**
- Large display mode immediately exits
- Page 0 returns to standard view (7 rows of labeled data)
- Display shows normal temperature, humidity, pressure, etc.
- Page has not changed (still on Page 0)

**Verification:**
- Standard layout restored (not large display)
- Page index unchanged (still Page 0)
- No crashes

**Action:**
2. Press Button B again

**Expected Result:**
- Large display mode re-enters at value index 0 (Temperatur)
- Value index was reset by exit

**Verification:**
- Mode re-entry works; index reset to 0 confirmed

---

### TC-LDM-005: Page Navigation Disabled in Large Display Mode

**Setup:** Device on Page 0, large display mode active.

**Action:**
1. Press Button A (next page button)
2. Observe display

**Expected Result:**
- Display remains on Page 0 large display mode
- Page navigation does NOT occur
- Button A has no effect

**Verification:**
- Page did not change
- Display still shows Page 0 large display

**Action:**
3. Press Button Y (previous page button in standard mode, but exit in large mode)

**Expected Result:**
- Large display mode exits (as expected from TC-LDM-004)
- Returns to Page 0 standard view

**Verification:**
- Y button correctly exits (not navigating page)
- Page navigation re-enabled after exit

---

### TC-LDM-006: Auto-Exit on Page Navigation

**Setup:** Device on Page 0, large display mode active.

**Action:**
1. Press Button Y to exit large display
2. Press Button A to navigate to next page (Page 1)
3. Observe Page 1 in standard view

**Expected Result:**
- Page 1 displays in standard view (not large display)
- Large display mode is inactive on Page 1

**Verification:**
- Navigation worked after exit
- New page shows standard view (no stale large display state)
- User can press B on Page 1 to enter its own large display mode

---

### TC-LDM-007: Display Rate Limiting in Large Display Mode

**Setup:** Device on Page 0, large display mode active. Serial monitor shows timing.

**Action:**
1. Rapidly press Button B 10 times in quick succession (< 500 ms total)
2. Observe display and Serial output

**Expected Result:**
- Value index updates immediately (valueIndex increments with each press)
- Display updates may lag behind button presses due to 200 ms rate limit
- Serial output shows display updates happening at ~5 Hz (200 ms intervals)
- Display does not flicker; updates are smooth

**Verification:**
- Button response immediate
- Display rate limited to 5 Hz (200 ms min interval)
- No excessive SPI traffic
- No flicker

---

### TC-LDM-008: All Pages Support Large Display (10 Pages)

**Setup:** Device with as many sensors available as possible.

**Action:**
1. Navigate to Page 0 (BME280)
2. Press Button B → verify large display mode
3. Press Button Y to exit
4. Navigate to Page 1 (Gamepad) via Button A
5. Press Button B → verify large display mode
6. Press Button Y to exit
7. Repeat for Pages 2–9

**Expected Result:**
- All 10 pages support entry into large display mode via Button B
- Each page shows correct page title, value count, labels, units, colors
- No page is missing large display support

**Verification:**
- All 10 pages tested
- Each page has unique value set and correct layout
- No crashes or missing pages

**Example Expected Titles:**
- Page 0: "BME280 Large Display"
- Page 1: "Gamepad QT Large Display"
- Page 2: "INA219 Large Display"
- ... etc.

---

### TC-LDM-009: Large Display with Unavailable Sensor

**Setup:** Device with sensor disconnected (e.g., BME280 unplugged).

**Action:**
1. Navigate to Page 0
2. Verify error screen displayed ("Sensor nicht vorhanden")
3. Press Button B

**Expected Result:**
- Button B press is ignored
- Large display mode is NOT entered
- Page remains showing error screen
- B/X buttons do not function

**Verification:**
- Error screen is priority; large display unavailable
- Button presses ignored correctly
- No crash or undefined behavior

---

### TC-LDM-010: DS2484 Large Display Mode (If 1-Wire Sensors Available)

**Setup:** Device with DS2484 (Page 9) and 1–3 DS18B20 sensors connected.

**Action:**
1. Navigate to Page 9 (DS2484)
2. Verify standard view shows discovered sensors
3. Press Button B
4. Observe large display mode

**Expected Result:**
- Large display shows first sensor ROM-ID (e.g., "28:AB:CD:12:34:56:78:9F", Cyan)
- Press Button B → shows first sensor temperature (e.g., "22.50°C", Green)
- Press Button B → shows first sensor status (e.g., "OK", Green)
- Press Button B → shows second sensor ROM-ID (if available)
- Continue until all sensors cycled
- Wrap-around to first sensor after last

**Value Count Verification:**
- 1 sensor discovered: 3 values (0, 1, 2)
- 2 sensors discovered: 6 values (0–5)
- 3 sensors discovered: 9 values (0–8)

**Verification:**
- Dynamic value count correct
- All sensors reachable via cycling
- ROM-IDs, temperatures, status correct
- Wrap-around works

**Error Case:**
- 0 sensors discovered: Error screen; large display cannot enter

---

### Summary: Large Display Mode Test Coverage

| Test | Purpose | Hardware Required |
|------|---------|---|
| TC-LDM-001 | Entry via Button B on all 10 pages | All sensors |
| TC-LDM-002 | Forward cycling (Button B) | Page 0 + 2 others |
| TC-LDM-003 | Backward cycling (Button X) | Page 0 + 2 others |
| TC-LDM-004 | Exit via Button Y | Any page with large display |
| TC-LDM-005 | Page navigation disabled | Any page with large display |
| TC-LDM-006 | Auto-exit on navigate | 2 adjacent pages |
| TC-LDM-007 | Display rate limiting (5 Hz) | Any page; monitor serial timing |
| TC-LDM-008 | All 10 pages support mode | All 10 sensors (or mock errors) |
| TC-LDM-009 | Unavailable sensor behavior | 1 sensor disconnected |
| TC-LDM-010 | DS2484 dynamic value count | DS2484 + 1–3 DS18B20 sensors |

**Minimum Viable Testing:**
- TC-LDM-001: Entry (Pages 0, 2, 4)
- TC-LDM-002: Forward cycling (Page 0)
- TC-LDM-003: Backward cycling (Page 0)
- TC-LDM-004: Exit (Page 0)
- **Time estimate:** 30 minutes

---

## Appendix: Test Code Template (PlatformIO Unity)

Save as `test/test_example.cpp`:

```cpp
#include <unity.h>
#include "../src/main.cpp"  // Include firmware code

void setUp(void) {
  // Called before each test
  Serial.begin(115200);
}

void tearDown(void) {
  // Called after each test
}

void test_example_passes() {
  TEST_ASSERT_EQUAL(4, 2 + 2);
}

void test_example_fails() {
  TEST_ASSERT_EQUAL(5, 2 + 2);  // Will fail
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_example_passes);
  RUN_TEST(test_example_fails);
  UNITY_END();
  return 0;
}
```

Run: `pio test -e esp32dev`

---

## Test Cases: SCD30 ASC Control & Display (FEATURE-SPEC-009, FR-010) — **NEW**

### UT-001: SCD30 CRC8 Validation for ASC Status Response

**Purpose:** Verify CRC8 calculation for ASC status query responses.

**Test Type:** Unit Test

**Setup:**
- Isolated `scd30Crc()` function test (no hardware)
- Test vectors: Known ASC status responses with valid CRC

**Test Vectors:**
```
ASC ON (0x00 0x01):
  CRC(0x00, 0x01) = 0xFE
  Expected response bytes: [0x00, 0x01, 0xFE]

ASC OFF (0x00 0x00):
  CRC(0x00, 0x00) = 0xFF
  Expected response bytes: [0x00, 0x00, 0xFF]
```

**Action:**
1. Compute `scd30Crc([0x00, 0x01])` → expect 0xFE
2. Compute `scd30Crc([0x00, 0x00])` → expect 0xFF
3. Verify CRC validation: `crc == response[2]`

**Expected Result:**
- ✅ CRC matches expected values
- ✅ CRC validation function correctly rejects mismatched CRC

**Error Condition:**
- ❌ CRC mismatch → validation fails, query returns false

---

### UT-002: `setAsc(bool enable)` I2C Command Encoding

**Purpose:** Verify correct I2C command bytes for ASC enable/disable.

**Test Type:** Unit Test (mocked I2C)

**Setup:**
- Mock `Wire.beginTransmission()`, `Wire.write()`, `Wire.endTransmission()`
- Track all bytes written

**Test Cases:**

**Case 1: Enable ASC**
- Input: `setAsc(true)`
- Expected Wire.write calls:
  - `0x53` (command high)
  - `0xDA` (command low)
  - `0x00` (data high)
  - `0x01` (data low = enable)
  - `CRC8([0x00, 0x01])` = 0xFE

**Case 2: Disable ASC**
- Input: `setAsc(false)`
- Expected Wire.write calls:
  - `0x53`, `0xDA`, `0x00`, `0x00`, `0xFF`

**Action:**
1. Call `setAsc(true)`
2. Verify Wire.write calls match expected sequence
3. Repeat for `setAsc(false)`

**Expected Result:**
- ✅ Correct command bytes sent in order
- ✅ CRC calculated correctly
- ✅ Function returns true if endTransmission == 0

**Error Condition:**
- ❌ Wrong byte sequence → test fails
- ❌ I2C error (endTransmission != 0) → function returns false

---

### UT-003: `getAscStatus(bool& outEnabled)` Response Parsing

**Purpose:** Verify correct parsing of ASC status response from sensor.

**Test Type:** Unit Test (mocked I2C)

**Setup:**
- Mock `Wire.requestFrom()` to return 3 bytes
- Mock `Wire.read()` to return status bytes
- Test vectors: ASC ON, ASC OFF, invalid CRC

**Test Cases:**

**Case 1: ASC ON (valid)**
- Response bytes: `[0x00, 0x01, 0xFE]` (0x01 = ON, CRC valid)
- Expected: `getAscStatus(enabled)` returns true, `enabled = true`
- State update: `ascEnabled = true`, `ascStatusValid = true`

**Case 2: ASC OFF (valid)**
- Response bytes: `[0x00, 0x00, 0xFF]` (0x00 = OFF, CRC valid)
- Expected: `getAscStatus(enabled)` returns true, `enabled = false`
- State update: `ascEnabled = false`, `ascStatusValid = true`

**Case 3: Invalid CRC**
- Response bytes: `[0x00, 0x01, 0xAA]` (CRC mismatch)
- Expected: `getAscStatus(enabled)` returns false, outEnabled unchanged
- State update: `ascStatusValid = false`

**Case 4: I2C Read Fails**
- Wire.requestFrom returns 0 bytes
- Expected: `getAscStatus(enabled)` returns false
- State: No change

**Action:**
1. For each test case, set up mock I2C response
2. Call `getAscStatus(enabled)`
3. Verify return value and outEnabled content
4. Verify Scd30State fields updated correctly

**Expected Result:**
- ✅ Valid responses parsed correctly
- ✅ Invalid CRC rejected
- ✅ I2C errors handled gracefully
- ✅ State struct updated only on success

---

### UT-004: `scd30_getAscStatus()` Display String

**Purpose:** Verify correct display string generation for ASC status.

**Test Type:** Unit Test

**Setup:**
- No I2C or hardware; test pure string formatting logic
- Set `scd30State` fields to various combinations

**Test Cases:**

**Case 1: ASC ON, status valid**
```cpp
scd30State.ascEnabled = true;
scd30State.ascStatusValid = true;
String result = scd30_getAscStatus();
ASSERT_EQUAL(result, "ON");
```

**Case 2: ASC OFF, status valid**
```cpp
scd30State.ascEnabled = false;
scd30State.ascStatusValid = true;
String result = scd30_getAscStatus();
ASSERT_EQUAL(result, "OFF");
```

**Case 3: Status unknown (invalid)**
```cpp
scd30State.ascStatusValid = false;
String result = scd30_getAscStatus();
ASSERT_EQUAL(result, "--");
```

**Expected Result:**
- ✅ All three display strings correct
- ✅ No crashes on edge cases

---

### IT-001: SCD30 Page Displays ASC Status Badge

**Purpose:** Verify ASC status badge appears on SCD30 page.

**Test Type:** Integration Test (hardware in loop)

**Setup:**
- Device booted with SCD30 connected
- Wait ~2 seconds for ASC status query to complete
- Navigate to Page 4 (SCD30)

**Action:**
1. Observe SCD30 page on display
2. Check top-right area for "ASC: ON" or "ASC: OFF" or "ASC: --"
3. Verify color: green (ON), orange (OFF), grey (--)

**Expected Result:**
- ✅ ASC status badge visible and readable
- ✅ Color matches state
- ✅ Text doesn't overlap title or other elements
- ✅ Page updates every ~500 ms

**Verification Methods:**
- Visual inspection
- Serial log shows `ASC status query: ON/OFF`
- Display screenshot/pixel check

**Error Condition:**
- ❌ Badge missing or invisible
- ❌ Wrong color or text
- ❌ Page freezes during ASC query

---

### IT-002: X Button Toggles ASC in Normal Mode

**Purpose:** Verify X button toggles ASC state on SCD30 page.

**Test Type:** Integration Test (hardware in loop)

**Setup:**
- Device booted with SCD30 connected
- Navigate to Page 4 (SCD30)
- NOT in Large Display mode
- Initial ASC state: ON

**Action:**
1. Press X button on Gamepad
2. Wait ~100 ms (debounce)
3. Observe display: badge should change to "ASC: OFF"
4. Wait ~500 ms for verification query
5. Press X button again
6. Observe: badge changes to "ASC: ON"

**Expected Result:**
- ✅ First X press: ASC ON → OFF (visual feedback immediate)
- ✅ Second X press: ASC OFF → ON
- ✅ State persists (doesn't revert)
- ✅ Page remains responsive (no lag)
- ✅ Serial log shows: "ASC toggle: enabling" / "ASC toggle: disabling"

**Verification:**
- Visual inspection of badge change
- Serial output confirms I2C commands sent and verified
- Gamepad button press logged in debug output

**Error Condition:**
- ❌ Badge doesn't change
- ❌ Page freezes or lags
- ❌ I2C error logged without recovery

---

### IT-003: X Button Cycles Backward in Large Display Mode (Not Toggle ASC)

**Purpose:** Verify X button in Large Display mode cycles backward, doesn't toggle ASC.

**Test Type:** Integration Test (hardware in loop)

**Setup:**
- Navigate to Page 4 (SCD30)
- Press B button to enter Large Display mode
- Current value displayed large (e.g., CO2 concentration)

**Action:**
1. Press X button to cycle backward through parameters
2. Observe: parameter cycles (e.g., CO2 → Humidity → Temperature → CO2)
3. Verify ASC status badge doesn't change
4. Exit Large Display mode (Y button)
5. Check ASC status: should be unchanged

**Expected Result:**
- ✅ X button cycles backward through parameters in Large Display
- ✅ ASC status unchanged (no toggle occurred)
- ✅ No ASC I2C command sent during Large Display mode
- ✅ Serial shows: parameter cycling, NO "ASC toggle" message

**Verification:**
- Visual inspection of parameter cycling
- Serial log confirms backward cycle, no ASC command sent

---

### IT-004: ASC Toggle Verification & Error Recovery

**Purpose:** Verify ASC toggle is re-queried and actual state is displayed even if toggle fails.

**Test Type:** Integration Test (hardware in loop, error injection)

**Setup:**
- Navigate to Page 4 (SCD30)
- ASC currently ON
- Deliberately cause I2C noise or disconnect sensor briefly during toggle

**Action:**
1. Press Y button → toggle command sent
2. Observer sees optimistic state change (ON → OFF in badge)
3. I2C error occurs during or after toggle
4. Next ASC status query fails
5. Wait ~1 second for display refresh

**Expected Result:**
- ✅ Display shows "ASC: --" (unknown) after query failure
- ✅ Page doesn't crash or freeze
- ✅ Sensor remains detectable on page (not marked unavailable)
- ✅ Serial log shows warning: "ASC toggle verification failed"
- ✅ User can retry toggle

**Verification:**
- Visual inspection of badge changing to "--"
- Serial log output shows error and retry

**Error Condition:**
- ❌ Page crashes or hangs
- ❌ Badge stuck on incorrect state
- ❌ No error recovery

---

### AT-001: ASC Status Always Visible on SCD30 Page

**User Story:** "As a developer, I want to see whether ASC is ON or OFF at a glance without needing the serial monitor."

**Acceptance Criteria:**
- ✅ After boot: SCD30 page displays ASC status (not "--", but actual ON/OFF)
- ✅ Status visible in top-right corner (readable at arm's length)
- ✅ Status updates within 500 ms after any change
- ✅ Color coding consistent: green=ON, orange=OFF, grey=unknown
- ✅ Persists across page navigation (returns to same page, status unchanged)

**Test Method:** Manual verification on hardware after ~2-second boot stabilization.

---

### AT-002: X Button Reliably Toggles ASC

**User Story:** "As a developer, I want to toggle ASC with a single X button press; no need to press twice or wait."

**Acceptance Criteria:**
- ✅ Single X press toggles state (not double-toggle or no-op)
- ✅ Toggle works even if last toggle was seconds ago
- ✅ Toggle debounce prevents accidental repeats (100 ms minimum)
- ✅ Display updates immediately (optimistic feedback)
- ✅ Verification query confirms actual state within 500 ms
- ✅ Page doesn't freeze or become unresponsive

**Test Method:**
1. Press X rapidly (multiple times)
2. Observe badge changes ON ↔ OFF
3. Verify no missed or repeated toggles

---

### AT-003: Large Display Mode Unchanged

**User Story:** "Large Display mode should work the same for SCD30 as all other pages; X cycles backward, not toggle ASC."

**Acceptance Criteria:**
- ✅ B button enters Large Display mode (shows 1 large parameter)
- ✅ Y button exits Large Display mode (ASC not toggled)
- ✅ X button cycles backward through CO2, Temp, Humidity (3 values, in reverse)
- ✅ ASC status displayed on return to normal page view
- ✅ Large Display cycle doesn't include ASC as 4th parameter
- ✅ X button doesn't toggle ASC while in Large Display

**Test Method:** Manual navigation through Large Display on Page 4.

---

### AT-004: Error Handling is Transparent

**User Story:** "If ASC query fails, the system should recover gracefully without confusing the user."

**Acceptance Criteria:**
- ✅ Failed query shows "ASC: --" (not error message or crash)
- ✅ User can still toggle even if status unknown
- ✅ Page remains responsive and fully functional
- ✅ Retry happens automatically (~500 ms)
- ✅ System recovers to correct state once I2C stabilizes

**Test Method:** Induce I2C errors (noise, loose connection, etc.) and verify graceful degradation.

---

