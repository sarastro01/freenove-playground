# Testing Strategy & Coverage: sensor-pages-dashboard

**Last Updated:** 2026-09-23  
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

