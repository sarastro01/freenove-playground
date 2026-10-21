# Business Rules: sensor-pages-dashboard

**Last Updated:** 2026-09-25  
**Firmware Type:** Embedded bare-metal (includes 1-Wire temperature sensors)  
**Business Domain:** Sensor data acquisition, display, input handling, 1-Wire discovery

---

## Overview

This document catalogs all conditional behaviors, state transitions, calculations, limits, and validation rules implemented in sensor-pages-dashboard. Each rule is reverse-engineered from the actual source code.

---

## Sensor Detection & Management Rules

### BR-001: Sensor Detection is One-Time (Boot-Time Only)

**Description:**  
All sensors are probed exactly once during `setup()` via `detectSensors()`. Detection results are cached in `SensorStatus available` flags. No re-detection occurs at runtime, even if sensors become unavailable or reconnect.

**Trigger:**  
System boot; `setup()` initialization.

**Conditions:**  
- I2C bus is operational (Wire initialized @ 400 kHz).
- Each sensor type has a unique I2C address or protocol.

**Behavior:**  
For each sensor type, `detectSensors()` attempts to probe once:
- BME280 @ 0x77: `available.bme280 = bme280.begin(0x77, &Wire)`
- Gamepad seesaw @ 0x50: `available.gamepad = gamepad.begin(0x50)`, config GPIO interrupts
- INA219 @ 0x40: `available.ina219 = ina219.begin(&Wire)`
- Encoder seesaw @ 0x36: `available.encoder = encoder.begin(0x36)`, init NeoPixel
- SCD30 @ 0x61: `available.scd30 = scd30Command(SCD30_CMD_START)`
- SI1145 @ 0x60: `available.si1145 = si1145.begin()`
- TSL2591 @ 0x29: `available.tsl2591 = tsl2591.begin()`, then config gain/timing
- VEML6070 @ 0x38/0x39: Dual-address probe
- VEML6075 @ 0x10: `available.veml6075 = readReg16(0x10, 0x0C, veml6075DeviceId)` (read ID reg)
- DS2484 @ 0x18: `available.ds2484 = ds2484Master.begin(&Wire, DS2484_ADDR)` (Adafruit_DS248x initialization)
  - Library: Adafruit_DS248x @ 1.2.0
  - I2C address: 0x18 (hardcoded constant DS2484_ADDR)
  - On success: calls initDs2484() to guard 1-Wire bus scan initialization
  - On failure: ds2484 flag remains false; page 9 displays error screen

If any probe fails, the corresponding `available.X` remains `false` (default initialized).

**Failure Behavior:**  
If a sensor is not detected at boot:
- `available.X = false`
- No read attempts occur for that sensor (guarded by `if (available.X)` checks).
- Dedicated page for that sensor displays error screen ("Sensor nicht vorhanden" in red).
- No error recovery or retry.

**Evidence:**  
- `void detectSensors()` lines 553–574 (main.cpp)
- `SensorStatus available{}` declaration (global, line 59)
- Guard pattern: `if (available.ina219) return unavailable(...)` in all page renderers

**Confidence:**  
HIGH

---

### BR-002: Stale Value Persistence on I2C Read Failure

**Description:**  
If any sensor read operation fails (I2C error, CRC mismatch, timeout), the global value struct is **not updated**. The last-known value persists indefinitely without any staleness indicator or timeout.

**Trigger:**  
Reading from a sensor when:
- I2C bus is congested or malfunctioning
- Device does not respond within timeout
- CRC validation fails (SCD30)
- Fewer bytes received than expected

**Conditions:**  
- Read function returns `false` on failure
- Global struct is only updated on successful read
- No error state, no counter, no timestamp

**Behavior:**  
```cpp
// Example: readVeml6075
bool readVeml6075(Veml6075Values &values) {
  if (!readReg16(...)) return false;  // I2C error
  // If we reach here, I2C succeeded
  values.uvaComp = ...;  // Update only on success
  return true;
}

// In loop: 
if (available.veml6075) readVeml6075(veml6075Values);  // May fail silently
// If fails, veml6075Values unchanged
```

**Failure Behavior:**  
- Display continues to show last-read value
- User has no indication value is stale
- If sensor permanently fails, user never knows

**Evidence:**  
- `readVeml6075()` lines 193–199
- `readScd30()` lines 169–179 (entire struct only updated if all CRCs pass)
- `readGamepadAt40Hz()` lines 239–253
- Loop guards: `if (available.X) readX(...)`

**Confidence:**  
HIGH

---

### BR-003: Page Navigation Disabled Without Gamepad

**Description:**  
Gamepad Y/A buttons control page navigation (previous/next). If gamepad is not detected at boot (`available.gamepad = false`), page navigation via buttons is disabled. User is warned at boot but can still use page buttons on display (if they existed as UI elements).

**Trigger:**  
Boot-time detection failure; `gamepad.begin()` returns false.

**Conditions:**  
- Gamepad not connected or malfunctioning
- `available.gamepad = false`

**Behavior:**  
```cpp
// In loop():
if (available.gamepad && (int32_t)(now - nextGamepadMs) >= 0) {
  readGamepadAt40Hz();
  // ... page nav logic ...
}
// If available.gamepad is false, entire block skipped
```

At boot, if gamepad missing:
```cpp
if (!available.gamepad) Serial.println("WARNING: Gamepad QT not found at 0x50");
```

**Failure Behavior:**  
- Y/A buttons have no effect
- currentPage remains at 0 (BME280)
- Warning message printed to Serial @ 115200 baud
- Only workaround: restart with gamepad connected

**Evidence:**  
- `loop()` lines 623–633 (gamepad read guarded by `if (available.gamepad)`)
- `setup()` line 619 (warning message)
- Edge detection logic lines 625–627 requires both gamepad reading and Y/A button checks

**Confidence:**  
HIGH

---

## Sensor-Specific Rules

### BR-004: INA219 Coulomb Accumulation (Energy Counter)

**Description:**  
The INA219 power monitor reads current at 50 Hz (every 20 µs). Current is integrated over time into a Coulomb counter (`chargeMah`), accumulating energy with the formula:

```
chargeMah += current_mA * elapsed_µs / 3.6e9
```

This counter persists across read cycles and accumulates indefinitely until reboot (no reset mechanism).

**Trigger:**  
Every 20 µs (50 Hz sampling via `micros()` precision).

**Conditions:**  
- INA219 detected at boot (`available.ina219 = true`)
- `readIna219At50Hz()` called (every 20 µs gate fires)
- `initialized = true` (first read has passed)
- Elapsed time < 200 ms (sanity check)

**Behavior:**  
```cpp
void readIna219At50Hz() {
  const uint32_t nowUs = micros();
  if (ina219Values.initialized) {
    const uint32_t elapsedUs = nowUs - ina219Values.lastSampleUs;
    if (elapsedUs < 200000) {  // Sanity check: ignore gaps > 200ms
      ina219Values.chargeMah += ina219Values.current * (float)elapsedUs / 3600000000.0f;
    }
  } else {
    ina219Values.initialized = true;  // First read: no accumulation
  }
  ina219Values.lastSampleUs = nowUs;
  // Read current from sensor
  ina219Values.current = -ina219.getCurrent_mA();
  // ...
}
```

**Special Case:**  
- **First Read:** `initialized = false` → no accumulation occurs, only flag set to true.
- **Large Time Gap:** If elapsed > 200 ms (timer rollover, ISR delay), gap is discarded; no accumulation for that cycle.
- **micros() Rollover:** `micros()` wraps every ~71 minutes, but signed comparison `(int32_t)(nowUs - lastSampleUs) >= 0` handles wrap-around correctly.

**Failure Behavior:**  
- If INA219 I2C read fails, current value not updated; accumulation uses previous (stale) current.
- Accumulator never resets except on reboot.
- No bounds checking; `chargeMah` can overflow at ~10 billion mAh (IEEE 754 float precision loss).

**Evidence:**  
- `readIna219At50Hz()` lines 379–397
- Timing gate in `loop()` lines 644–648: `if ((int32_t)(nowUs - nextInaSampleUs) >= 0)`
- Display: `pageIna219()` line 346 shows `chargeMah` in mAh

**Confidence:**  
HIGH

**Coulomb Accumulation Overflow:**
- Field: chargeMah (float, IEEE 754 32-bit)
- Accumulation: += current_mA * elapsed_µs / 3.6e9
- Persistence: Unbounded until reboot (no reset mechanism)
- Overflow risk: Precision loss after ~10 billion mAh (~1.1 million hours at 10 mA)
- Current behavior: No overflow detection or warning
- Recommendation: Accept as-is for typical deployments < 10,000 hours (1+ year)
[RESOLVED ISSUE-006: Documented known limitation and overflow behavior]

---

### BR-005: SCD30 CRC8 Validation (All-or-Nothing)

**Description:**  
The SCD30 CO₂ sensor uses Sensirion proprietary protocol with CRC8 checksums. The 18-byte response contains 3 data fields (CO₂, temperature, humidity) × 2 data bytes + 1 CRC byte each. If **any** CRC fails, the **entire read is rejected** and `scd30Values` is not updated.

**Trigger:**  
Periodic sensor read attempt (every 500 ms, guarded by `scd30Ready()` check).

**Conditions:**  
- SCD30 detected at boot (`available.scd30 = true`)
- `scd30Ready()` returns true (sensor ready status)
- Wire.requestFrom(0x61, 18) succeeds (all 18 bytes received)

**Behavior:**  
```cpp
bool readScd30(Scd30Values &values) {
  if (!scd30Command(SCD30_CMD_READ) || Wire.requestFrom(SCD30_ADDR, 18) != 18) 
    return false;
  uint8_t raw[18];
  for (uint8_t i = 0; i < 18; i++) raw[i] = Wire.read();
  
  // Validate all 6 CRC8 bytes (3 fields × 2 data + 1 CRC each)
  for (uint8_t i = 0; i < 18; i += 3) 
    if (scd30Crc(&raw[i], 2) != raw[i + 2]) 
      return false;  // ANY CRC mismatch: reject entire read
  
  // Only if ALL CRCs pass: update values
  values.co2 = scd30Float(&raw[0]);
  values.temp = scd30Float(&raw[6]);
  values.humidity = scd30Float(&raw[12]);
  return true;
}
```

**CRC8 Algorithm:**  
- Polynomial: 0x31
- Initial value: 0xFF
- See `scd30Crc()` lines 159–165

**Failure Behavior:**  
- Any CRC mismatch → `readScd30()` returns false
- `scd30Values` **not updated** (persists last valid read)
- No error logged, no counter incremented
- User sees stale CO₂/temp/humidity on page 5

**Evidence:**  
- `readScd30()` lines 169–179
- `scd30Crc()` lines 159–165 (CRC8 implementation)
- Guard in `readSensors()` line 202: `if (available.scd30 && scd30Ready()) readScd30(scd30Values);`

**Confidence:**  
HIGH

**CRC8 Verification Test Vectors (Sensirion official):**
| Input Bytes | Expected CRC |
|---|---|
| 0xBE, 0xEF | 0x92 |
| 0x00, 0x00 | 0x81 |
| 0x00, 0x01 | 0xB0 |
| 0x02, 0x02 | 0x3A |

Implementation verification:
- uint8_t test1 = scd30Crc((uint8_t[]){0xBE, 0xEF}, 2); assert(test1 == 0x92);
- uint8_t test2 = scd30Crc((uint8_t[]){0x00, 0x00}, 2); assert(test2 == 0x81);
[RESOLVED ISSUE-016: CRC8 test vectors added for verification]

---

### BR-006: VEML6075 UV Compensation Formulas

**Description:**  
VEML6075 raw ADC counts (UVA, UVB) are compensated using sensor-specific factors (COMP1, COMP2) read from registers 0x0A and 0x0B. Compensation formulas include subtraction and `max(0, ...)` clamping.

**Trigger:**  
Periodic sensor read (every 500 ms in `readSensors()`).

**Conditions:**  
- VEML6075 detected at boot
- `readReg16()` succeeds for all 4 registers (UVA, UVB, COMP1, COMP2)

**Behavior:**  
```cpp
bool readVeml6075(Veml6075Values &values) {
  // Read raw values from sensor
  if (!readReg16(VEML6075_ADDR, VEML6075_REG_UVA, values.uva) || 
      !readReg16(VEML6075_ADDR, VEML6075_REG_UVB, values.uvb) ||
      !readReg16(VEML6075_ADDR, VEML6075_REG_COMP1, values.comp1) || 
      !readReg16(VEML6075_ADDR, VEML6075_REG_COMP2, values.comp2)) 
    return false;
  
  // Apply compensation formulas
  values.uvaComp = max(0.0f, (float)values.uva - 2.22f * values.comp1 - 1.33f * values.comp2);
  values.uvbComp = max(0.0f, (float)values.uvb - 2.95f * values.comp1 - 1.74f * values.comp2);
  
  // Compute UV index
  values.index = ((values.uvaComp * 0.001461f) + (values.uvbComp * 0.002591f)) * 0.5f;
  return true;
}
```

**Constants:** Compensation coefficients are hardcoded:
- UVA subtraction: 2.22×COMP1 + 1.33×COMP2
- UVB subtraction: 2.95×COMP1 + 1.74×COMP2
- UV index coefficients: 0.001461 (UVA), 0.002591 (UVB), 0.5 (scaling factor)

**Clamping:** `max(0.0f, ...)` ensures compensated values never go negative (physical non-negativity constraint).

**Failure Behavior:**  
- If any register read fails, entire struct not updated
- Stale values persist (values, comp factors, and computed index all frozen)

**Evidence:**  
- `readVeml6075()` lines 193–199
- Constants appear in compensation calculation (lines 197–198)

**Confidence:**  
HIGH

---

### BR-007: VEML6070 UV Level Classification

**Description:**  
VEML6070 raw ADC value (16-bit, combined MSB:LSB from addresses 0x38/0x39) is classified into 5 discrete UV level categories based on hardcoded thresholds.

**Trigger:**  
Page 8 (VEML6070) rendered; calculation performed each render.

**Conditions:**  
- VEML6070 detected at boot
- Page currently displayed is page 8

**Behavior:**  
```cpp
void pageVeml6070() {
  // ...
  const char *level = veml6070Values.raw < 200 ? "Very low" :
                      veml6070Values.raw < 500 ? "Low" :
                      veml6070Values.raw < 1000 ? "Moderate" :
                      veml6070Values.raw < 2000 ? "High" :
                      "Very high";
  row(124, "UV level:", level, TFT_CYAN);
  // ...
}
```

**Thresholds:**
| Threshold | Level |
|-----------|-------|
| raw < 200 | "Very low" |
| 200 ≤ raw < 500 | "Low" |
| 500 ≤ raw < 1000 | "Moderate" |
| 1000 ≤ raw < 2000 | "High" |
| raw ≥ 2000 | "Very high" |

**Special Case:**  
Classification is **display-only** (not persisted in data struct). Recalculated every render.

**Failure Behavior:**  
- If VEML6070 not detected, page shows error screen
- If last read failed (I2C error), stale raw value used for classification
- No indication of classification staleness

**Evidence:**  
- `pageVeml6070()` lines 433–438

**Confidence:**  
HIGH



### BR-010: Encoder LED Color Cycling (4-State Cycle)

**Description:**  
Encoder button presses cycle the LED through 4 hardcoded RGB colors in a fixed sequence: blue → green → red → purple → blue. Color changes only on falling edge (button pressed, not released).

**Trigger:**  
Encoder button falling edge (transition from released to pressed).

**Conditions:**  
- Encoder detected at boot (`available.encoder = true`)
- Encoder page active or page just changed (read gates: `currentPage == 4 || currentPage != lastPage`)
- Button transitions from not-pressed to pressed

**Behavior:**  
```cpp
void readEncoderAt50Hz() {
  // ...
  encoderValues.pressed = encoder.digitalRead(ENCODER_BUTTON_PIN) == LOW;
  
  if (encoderValues.pressed && !encoderLastPressed) {  // Falling edge
    encoderPressCount++;
    encoderColorIndex = (encoderColorIndex + 1) % 4;  // Cycle 0→1→2→3→0
    updateEncoderLed();  // Immediate NeoPixel update
  }
  
  encoderLastPressed = encoderValues.pressed;
  encoderValues.presses = encoderPressCount;  // Sync to struct
  encoderValues.color = encoderColorNames[encoderColorIndex];
  // ...
}
```

**Color Palette:**  
```cpp
const uint32_t encoderColors[] = {
  seesaw_NeoPixel::Color(0, 96, 255),    // 0 = blue
  seesaw_NeoPixel::Color(0, 255, 32),    // 1 = green
  seesaw_NeoPixel::Color(255, 48, 0),    // 2 = red
  seesaw_NeoPixel::Color(180, 0, 255)    // 3 = purple
};
const char *const encoderColorNames[] = {"blue", "green", "red", "purple"};
```

**Press Count:** Incremented on each falling edge; persists until reboot.

**LED Update Timing:** Immediate (within same read cycle); does not wait for next display refresh.

**Failure Behavior:**  
- If NeoPixel I2C command fails, LED color does not update (no error indication)
- Press count still incremented (state update succeeds, hardware command fails)
- Color struct field updated to next color name, but LED remains old color

**Evidence:**  
- `readEncoderAt50Hz()` lines 263–289
- Encoder color palette lines 113–122
- LED update function lines 257–262: `updateEncoderLed()`

**Confidence:**  
HIGH

---

### BR-011: Encoder Dial Brightness Adjustment (Delta × 8, Clamped 0–255)

**Description:**  
Rotating the encoder dial changes LED brightness. Dial rotation delta is multiplied by 8 and added to current brightness, then clamped to 0–255 range. Changes trigger immediate NeoPixel update.

**Trigger:**  
Encoder dial rotation (delta != 0 detected in read cycle).

**Conditions:**  
- Encoder detected at boot
- Encoder page active or page just changed
- `encoderValues.delta != 0`

**Behavior:**  
```cpp
if (encoderValues.delta != 0) {
  const int32_t nextBrightness = (int32_t)encoderBrightness + encoderValues.delta * 8;
  encoderBrightness = (uint8_t)constrain(nextBrightness, 0, 255);
  updateEncoderLed();  // Immediate update
}
```

**Calculation:**
- Each dial tick = delta of ±1 (or more for multi-click)
- Each tick = ±8 brightness units
- Example: dial +1 tick → brightness += 8; dial -2 ticks → brightness -= 16
- Clamped to 0–255 (valid uint8_t range)

**Default Brightness:** 64 (initialized at global scope: line 111).

**Failure Behavior:**  
- If LED update fails, brightness struct still updated (next render shows new brightness, but LED unchanged)
- No indication of mismatch between struct and hardware

**Evidence:**  
- `readEncoderAt50Hz()` lines 276–281
- Default brightness line 111: `uint8_t encoderBrightness = 64;`
- Constrain line 278: `constrain(nextBrightness, 0, 255)`

**Confidence:**  
HIGH

---

### BR-012: Gamepad Button Page Navigation (Edge Detection, Modulo Wrap)

**Description:**  
Gamepad Y button (active-low) navigates to previous page; A button navigates to next page. Navigation uses falling edge detection (button press transition). Page indices wrap around 0–9 via modulo arithmetic.

**Trigger:**  
Every 25 ms (40 Hz gamepad read) when gamepad detected.

**Conditions:**  
- Gamepad detected at boot (`available.gamepad = true`)
- Y or A button transitions from released (1) to pressed (0)

**Behavior:**  
```cpp
if (available.gamepad && (int32_t)(now - nextGamepadMs) >= 0) {
  const uint32_t buttonsBefore = gamepadValues.buttons;  // Save previous state
  readGamepadAt40Hz();  // Read current state
  
  const bool yPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_Y));      // Current
  const bool aPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_A));      // Current
  const bool lastY = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_Y));                 // Previous
  const bool lastA = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_A));                 // Previous
  
  if (yPressed && !lastY)  // Y falling edge: 0 prev, 1 curr → 1 prev, 0 curr
    currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;  // Prev (modulo wrap)
  
  if (aPressed && !lastA)  // A falling edge
    currentPage = (currentPage + 1) % PAGE_COUNT;  // Next (modulo wrap)
  
  nextGamepadMs = now + INPUT_REFRESH_MS;
}
```

**Button State Encoding:**  
- Active-low (hardware convention): 0 = pressed, 1 = released
- Negation `!(...& (1UL << pin))` inverts to intuitive: 1 = pressed, 0 = released

**Page Count:** 9 (indices 0–8): BME280, Gamepad QT, INA219, Rotary Encoder, SCD30, SI1145, TSL2591, VEML6070, VEML6075

**Wrap-Around:**  
- Next from page 8 → page 0 (via `(8 + 1) % 9 = 0`)
- Prev from page 0 → page 8 (via `(0 + 9 - 1) % 9 = 8`)

**Failure Behavior:**  
- If gamepad I2C read fails, `gamepadValues.buttons` not updated; previous state used
- Stale button state may trigger false page navigation (if prior read had different buttons)
- If gamepad disconnects mid-session, last-read buttons persist; navigation stops

**Evidence:**  
- Loop code lines 623–633
- Button definitions lines 17–22
- Page count line 47: `constexpr uint8_t PAGE_COUNT = 9;`

**Confidence:**  
HIGH


**CRITICAL DESIGN CONSTRAINT - Navigation Fallback:**

The page navigation mechanism is **entirely dependent on the gamepad Y/A buttons**. No fallback navigation exists:
- If gamepad is not detected at boot: navigation is **permanently disabled**
- If gamepad fails after boot: user is **stuck on current page indefinitely**
- No alternative navigation methods are implemented (no encoder dial override, no touch screen, no Serial commands)

This is an acceptable design limitation for this application because:
1. Gamepad (seesaw @ 0x50) is optional sensor, but page navigation is primary use case
2. User expectation: application is unusable without gamepad (not graceful degradation)
3. Implementation simplified by not implementing fallback logic

**Requirement for Deployment:** Gamepad device must be present and functional.

[RESOLVED ISSUE-003: Documented design constraint that gamepad is required for page navigation]

---

### BR-013: Gamepad Joystick Analog Values Inverted (1023 - raw)

**Description:**  
Gamepad joystick X and Y analog readings are inverted via formula `1023 - raw`. This reverses the direction mapping (high raw ADC = low output, vice versa).

**Trigger:**  
Every 25 ms gamepad read.

**Conditions:**  
- Gamepad detected at boot
- Gamepad read cycle fires

**Behavior:**  
```cpp
void readGamepadAt40Hz() {
  if (!available.gamepad) return;
  
  gamepadValues.x = 1023 - gamepad.analogRead(14);  // Inverted
  gamepadValues.y = 1023 - gamepad.analogRead(15);  // Inverted
  // ...
}
```

**Inversion Rationale:**  
Hardware ADC returns 0 (left/up) to 1023 (right/down). Firmware inverts to make display more intuitive: 1023 (right/down) displayed as high, 0 (left/up) as low.

**Display:**  
Values displayed as-is on Gamepad page; no further transformation.

**Failure Behavior:**  
- If `analogRead()` fails (hardware error), reading not updated; stale inverted value persists

**Evidence:**  
- `readGamepadAt40Hz()` lines 243–244

**Confidence:**  
HIGH

---

## Sensor Caching & Error Handling Rules — **NEW (FEATURE-SPEC-005)**

### BR-SC-001: Unified Sensor Data Caching Architecture (BME280, TSL2591, SI1145) — **NEW**

**Description:**  
Three sensors (BME280, TSL2591, SI1145) that previously performed direct I2C reads during page rendering are refactored to use the unified caching architecture. Sensor data is read once every 500 ms in `readSensors()`, cached in global structs, and page rendering functions read only from cache (no I2C during rendering).

**Trigger:**  
- `readSensors()` called every 500 ms via `SENSOR_REFRESH_MS` timer gate in main loop
- Sensor availability detected at boot (`available.bme280`, `available.tsl2591`, `available.si1145`)

**Conditions:**  
- Sensor detected at boot (`available.X = true`)
- 500 ms refresh interval elapsed
- I2C bus operational

**Behavior:**  
1. **In `readSensors()` function (every 500 ms):**
   ```cpp
   void readSensors() {
     if (available.bme280) readBme280(bme280Values);
     if (available.tsl2591) readTsl2591(tsl2591Values);
     if (available.si1145) readSi1145(si1145Values);
     // ... other sensor reads ...
   }
   ```

2. **Each `readBme280()`, `readTsl2591()`, `readSi1145()` function:**
   - Guard: Return immediately if sensor not available
   - Record start time via `micros()`
   - Read I2C values from sensor using manufacturer API
   - Perform plausibility checks (range validation)
   - If checks fail: Log error to Serial and **RETURN without updating cache**
   - If checks pass: Update cache struct and log timing to Serial
   - Record end time and output performance: `"SENSOR read time: XXX µs"`

3. **In page rendering functions (`pageBme280()`, `pageTsl2591()`, `pageSi1145()`):**
   - Read **only from cache structs** (no direct I2C I/O)
   - Example: `tft.drawString(String(bme280Values.temperature, 1), ...)`
   - No sensor availability guard needed (cache always valid, even if stale)

**Benefits:**  
- Reduces I2C load during display refresh (improves responsiveness)
- Separates sensor data acquisition from UI rendering
- Consistent with existing VEML6075, VEML6070, SCD30, DS2484 architecture

**Failure Behavior:**  
- If I2C read fails (plausibility check rejects values), cache not updated
- Page continues to display last known values (stale data)
- Error logged to Serial with sensor name and specific failure reason
- Next successful read (500 ms later) updates cache again

**Evidence:**  
- FEATURE-SPEC-005-SENSOR-CACHING-ARCHITECTURE.md
- New functions: `readBme280()`, `readTsl2591()`, `readSi1145()` (main.cpp, post-implementation)
- Modified function: `readSensors()` includes three new conditional calls
- Affected page renderers: `pageBme280()`, `pageTsl2591()`, `pageSi1145()` (changed from direct I2C to cache reads)

**Confidence:**  
HIGH (pending implementation)

---

### BR-SC-002: Sensor Data Initialization with NaN — **NEW**

**Description:**  
All three new cache structs (Bme280Values, Tsl2591Values, Si1145Values) are initialized with NaN (not-a-number) values at startup, not `0.0f`. This allows distinguishing "never read" from "real measurement = 0" or "measurement error".

**Trigger:**  
System boot; `setup()` initialization.

**Conditions:**  
- Sensor detected at boot (`available.X = true`)
- `initializeSensorCaches()` function called in `setup()`

**Behavior:**  
```cpp
void initializeSensorCaches() {
  // BME280 cache
  bme280Values.temperature = NAN;
  bme280Values.humidity = NAN;
  bme280Values.pressure = NAN;
  bme280Values.altitude = NAN;
  bme280Values.dewPoint = NAN;
  
  // TSL2591 cache
  tsl2591Values.fullSpectrum = NAN;
  tsl2591Values.infrared = NAN;
  tsl2591Values.visible = NAN;
  tsl2591Values.lux = NAN;
  tsl2591Values.gain = 0;          // Enum value, not NaN
  tsl2591Values.integrationTime = 0;  // Enum value, not NaN
  
  // SI1145 cache
  si1145Values.visible = 0;  // ADC count, not NaN
  si1145Values.infrared = 0; // ADC count, not NaN
  si1145Values.uv = 0;       // Raw count, not NaN
  si1145Values.uvIndex = NAN;
}

// In setup():
initializeSensorCaches();
```

**Special Cases:**  
- Non-float fields (uint16_t, uint32_t) initialized to 0 (cannot be NaN)
- Only float fields use NaN initialization
- Requires: `#include <math.h>` for `NAN` constant

**UI Behavior:**  
- Page rendering detects NaN values via `isnan(value)` check
- Display shows "---" or "--" instead of numeric value
- After first successful sensor read (500 ms), page shows real values

**Failure Behavior:**  
- If sensor detected but never reads successfully (I2C permanently failed), displays remain "--" indefinitely
- User sees "uninitialized" indicator rather than "0" (which might be misinterpreted as valid measurement)

**Evidence:**  
- `initializeSensorCaches()` function called in `setup()` (post-implementation)
- Cache struct definitions: Bme280Values, Tsl2591Values, Si1145Values (main.cpp)
- Page renderers check: `if (isnan(bme280Values.temperature))` before displaying

**Confidence:**  
HIGH (pending implementation)

---

### BR-SC-003: Sensor I2C Error Detection via Plausibility Checks — **NEW**

**Description:**  
Each sensor read function includes plausibility checks (range validation) to detect I2C errors. If sensor values fall outside physically valid ranges, the read is considered failed and cache not updated.

**Trigger:**  
`readBme280()`, `readTsl2591()`, `readSi1145()` functions during 500 ms sensor polling cycle.

**Conditions:**  
- Sensor detected at boot
- I2C read completed (bytes received)
- Returned values within/outside valid ranges

**Behavior (BME280 Example with Dew Point Calculation):**  
```cpp
void readBme280(Bme280Values &values) {
  if (!available.bme280) return;
  
  uint32_t startTime = micros();
  
  // Read sensor values
  float temp = bme280.readTemperature();
  float rh = bme280.readHumidity();
  float press = bme280.readPressure() / 100.0f;  // Pa -> hPa
  float alt = bme280.readAltitude(1013.25);
  
  // Plausibility checks (valid ranges)
  if (temp < -40 || temp > 85 || 
      rh < 0 || rh > 100 || 
      press < 300 || press > 1100) {
    Serial.printf("ERROR: BME280 read returned invalid values: T=%.1f, RH=%.1f, P=%.1f\n",
                  temp, rh, press);
    return;  // Cache NOT updated; stale values persist
  }
  
  // All checks passed: compute dew point using Alduchov & Eskridge formula
  // Accuracy: ±0.35°C, valid range: 0-50°C, 1-100% RH
  const float a = 17.62;
  const float b = 243.12;
  float alpha = ((a * temp) / (b + temp)) + log(rh / 100.0f);
  float dewPoint = (b * alpha) / (a - alpha);
  
  // Update cache
  values.temperature = temp;
  values.humidity = rh;
  values.pressure = press;
  values.altitude = alt;
  values.dewPoint = dewPoint;
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("BME280 read time: %lu µs\n", duration);
}
```

**Dew Point Formula Details (Alduchov & Eskridge, more accurate than simplified formula):**
- Simplified formula: `Td ≈ T - (100% - RH%) / 5` (±3°C accuracy)
- Alduchov & Eskridge: Uses logarithmic approximation with coefficients a=17.62, b=243.12 (±0.35°C accuracy)
- Valid range: Temperature 0–50°C, Relative Humidity 1–100%
- Requires: `#include <math.h>` for `log()` function

**Valid Ranges:**
- **BME280:** T[-40, 85]°C, RH[0, 100]%, P[300, 1100]hPa
- **TSL2591:** fullSpectrum ≠ 0 OR infrared ≠ 0 (reject all-zero as I2C error)
- **SI1145:** visible ≠ 0 OR infrared ≠ 0 OR uv ≠ 0 (reject all-zero as I2C error)

**Error Logging:**  
All plausibility check failures logged to Serial with sensor name and specific failure details:
```
ERROR: BME280 read returned invalid values: T=95.0, RH=120.0, P=1200.0
ERROR: TSL2591 read returned zero values
ERROR: SI1145 read returned all zeros
```

**Failure Behavior:**  
- Cache struct not updated; last known values persist
- Error message printed to Serial (visible in PlatformIO Serial Monitor)
- No retry or recovery; next read attempt occurs 500 ms later
- User sees stale values on page (no indication of staleness without Serial log)

**Evidence:**  
- `readBme280()` plausibility checks (post-implementation, lines TBD)
- `readTsl2591()` zero-check (post-implementation, lines TBD)
- `readSi1145()` all-zero check (post-implementation, lines TBD)
- Serial output examples in FEATURE-SPEC-005

**Confidence:**  
HIGH (pending implementation)

---

### BR-SC-004: Sensor Read Timing Measurement & Serial Output — **NEW**

**Description:**  
Each sensor read function measures I/O time using `micros()` timer and logs performance to Serial in microseconds. This allows monitoring I2C performance and detecting slow reads caused by bus congestion or sensor issues.

**Trigger:**  
Every successful sensor read in `readBme280()`, `readTsl2591()`, `readSi1145()` functions (every 500 ms, assuming success).

**Conditions:**  
- Sensor read completed successfully (plausibility checks passed)
- Timing recorded via `micros()` at start and end of read operation

**Behavior:**  
```cpp
void readBme280(Bme280Values &values) {
  if (!available.bme280) return;
  
  uint32_t startTime = micros();
  // Read sensor and validate
  // ...
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("BME280 read time: %lu µs\n", duration);
}
```

**Expected Timings (Approximate):**
- **BME280:** 5–50 ms (multiple registers, conversion wait)
- **TSL2591:** 5–20 ms (2–3 I2C reads + lux calculation)
- **SI1145:** 5–15 ms (3 I2C reads, no heavy computation)

**Serial Output Format:**
```
BME280 read time: 12345 µs
TSL2591 read time: 8234 µs
SI1145 read time: 6789 µs
```

**Use Case:**  
- Debugging I2C bus performance
- Detecting slow sensor responses (due to electromagnetic interference, cable issues, etc.)
- Validating that 500 ms sensor refresh cycle has sufficient headroom (< 400 ms read time)

**Failure Behavior:**  
- If read fails (plausibility check fails), timing not logged (function returns early)
- Successful reads always log timing

**Evidence:**  
- Gamepad read timing: `readGamepadAt40Hz()` lines 619–629 (existing pattern)
- Encoder read timing: `readEncoderAt50Hz()` lines 581–609 (existing pattern)
- INA219 read timing: `readIna219At50Hz()` lines 636–665 (existing pattern)
- New functions follow same pattern: `readBme280()`, `readTsl2591()`, `readSi1145()`

**Confidence:**  
HIGH (pending implementation; follows proven pattern)

---

## Display & Rendering Rules

### BR-015: Page Redraw Only on Page Change (Header Skip)

**Description:**  
The `drawCurrentPage(bool pageChanged)` function renders either full page (header + data) or data-only (no header), depending on `pageChanged` flag. Header redraw is expensive (full screen clear), so it's skipped when only data updates.

**Trigger:**  
Multiple call sites in loop with different `pageChanged` values:
1. `currentPage != lastPage`: full redraw (pageChanged=true)
2. Periodic data updates: data-only (pageChanged=false)

**Conditions:**  
- Page renderer function called
- `pageChanged` parameter determines header behavior

**Behavior:**  
```cpp
void drawCurrentPage(bool pageChanged) {
  const char *titles[] = {
    "BME280 (0x77)",
    "Gamepad QT (0x50)",
    "INA219 (0x40)",
    "Rotary Encoder (0x36)",
    "SCD30 (0x61)",
    "SI1145 (0x60)",
    "TSL2591 (0x29)",
    "VEML6070 (0x38/0x39)",
    "VEML6075 (0x10)"
  };
  
  if (pageChanged) 
    drawHeader(titles[currentPage]);  // Full screen clear + header
  
  switch (currentPage) {
    case 0: pageBme280(); break;
    // ... dispatch to page renderer
  }
}
```

**drawHeader() Operations:**
```cpp
void drawHeader(const char *title) {
  tft.fillScreen(TFT_BLACK);        // Clear entire 320×480 display
  tft.setTextDatum(TL_DATUM);
  writeText(6, 8, title, TFT_CYAN, 4);  // Cyan title at (6,8)
  tft.drawFastHLine(6, 42, 468, TFT_DARKGREY);  // Divider line
}
```

**Call Sites:**
1. Line 649: `if (currentPage != lastPage) drawCurrentPage(true);` → Full redraw when page changes
2. Line 666: `if (rescanDs18b20() && currentPage == 1) drawCurrentPage(true);` → Full redraw if devices changed on page 1
3. Line 673: `if ((currentPage == 4 || currentPage == 2) && ...) drawCurrentPage(false);` → Data-only refresh on live pages (Encoder, Gamepad)
4. Line 677: `if ((int32_t)(now - nextSensorMs) >= 0) { readSensors(); drawCurrentPage(false); ...}` → Data-only refresh every 500ms

**Optimization:** Screen clear is avoided for data-only redraws, reducing flickering and SPI overhead.

**Failure Behavior:**  
- If `pageChanged=true` but page didn't change, screen flickers unnecessarily
- If `pageChanged=false` but page did change, old header persists (corrupted display) [potential bug, but not observed in current logic]

**Evidence:**  
- `drawCurrentPage()` lines 545–555
- `drawHeader()` lines 130–136
- Call sites lines 649, 666, 673, 677

**Confidence:**  
HIGH

---

### BR-016: Page-Specific Refresh Rates (Multi-Rate Display)

**Description:**  
Different pages are refreshed at different rates based on whether they display real-time input (live pages) or sensor data (slow pages).

**Trigger:**  
Main loop cycle; multiple timing gates.

**Conditions:**  
- Current page is active
- Corresponding time gate fires

**Behavior:**  

**Live Pages (Gamepad page 2, Encoder page 4):** Refresh every 20 ms (50 Hz)
```cpp
if ((currentPage == 4 || currentPage == 2) && 
    (int32_t)(now - nextSelectedDisplayMs) >= 0) {
  drawCurrentPage(false);  // Data-only
  nextSelectedDisplayMs = now + INPUT_REFRESH_MS;  // 20ms
}
```

**All Other Pages:** Refresh every 500 ms (2 Hz) via sensor data update
```cpp
if ((int32_t)(now - nextSensorMs) >= 0) {
  readSensors();
  drawCurrentPage(false);  // Data-only
  nextSensorMs = now + SENSOR_REFRESH_MS;  // 500ms
}
```

**Rationale:**  
- Live pages (Gamepad, Encoder) show user interaction; 50 Hz feels responsive
- Sensor pages (BME280, SCD30, etc.) change slowly; 2 Hz is sufficient

**Special Case:**  
- Encoder only reads CONTINUOUSLY (not page-conditional; line 1856-1858). This ensures LED updates and stats aggregation work correctly even when encoder page is not displayed.

**Failure Behavior:**  
- If timing gate misfires, page doesn't update until next cycle (up to 500ms lag)
- Live pages may appear unresponsive if I2C congestion blocks read

**Evidence:**  
- Lines 673–675: live page refresh
- Lines 677–680: sensor page refresh
- Lines 637–641: encoder read gate (only page 4 or on page change)
- Constants: `INPUT_REFRESH_MS = 20`, `SENSOR_REFRESH_MS = 500` (lines 47–48)

**Confidence:**  
HIGH

---

### BR-017: Sensor Unavailable Error Display

**Description:**  
If a sensor is not detected at boot, the dedicated page displays a standardized error screen with red "Sensor nicht vorhanden" message, I2C address in yellow, and navigation hints.

**Trigger:**  
User navigates to page for absent sensor; page renderer checks `available.X` flag.

**Conditions:**  
- Sensor detection failed at boot (`available.X = false`)
- Page renderer called (via `drawCurrentPage()`)

**Behavior:**  
```cpp
void unavailable(const char *name, const char *address, const char *addressLabel = "I2C-Adresse:") {
  writeText(6, 64, "Sensor nicht vorhanden", TFT_RED, 2);
  row(96, addressLabel, address, TFT_YELLOW);
  writeText(6, 142, "Y: vorherige Seite", TFT_WHITE);
  writeText(6, 166, "A: naechste Seite", TFT_WHITE);
  footer("Keine Daten verfuegbar");
}

// Example call from page renderer:
void pageIna219() {
  if (!available.ina219) return unavailable("INA219", "0x40");
  // ... normal page content
}
```

**Error Display Layout:**
- y=64: "Sensor nicht vorhanden" (red, font size 2)
- y=96: Address label + address (yellow)
- y=142–166: Navigation instructions (white)
- Footer: "Keine Daten verfuegbar" (dark grey)

**Navigation:** User can still navigate away using Y (prev) or A (next) buttons.

**Failure Behavior:**  
- No attempt to re-detect sensor or retry connection
- Error screen persists indefinitely
- Only workaround: reboot with sensor connected

**Evidence:**  
- `unavailable()` lines 145–152
- Guard pattern in all page renderers: `if (!available.X) return unavailable(...);`

**Confidence:**  
HIGH

---

## Input & Control Rules

### BR-018: Encoder Read (Continuous, Non-Blocking) — **UPDATED**

**Description:**  
Encoder is read CONTINUOUSLY at 50 Hz (every 20 ms), regardless of current page. This enables:
- LED updates (color cycling, brightness) even when encoder page (page 4) is not displayed
- Stats aggregation across all time (not just when page 4 active)
- Responsive button/dial input for page navigation in all contexts

**Trigger:**  
Main loop cycle; every 20 ms timing gate.

**Conditions:**  
- Encoder detected at boot
- Time gate fires (every 20 ms, `INPUT_REFRESH_MS`)

**Behavior:**  
```cpp
if (available.encoder && (int32_t)(now - nextEncoderMs) >= 0) {
  readEncoderAt50Hz();  // Always reads, no page conditional
  nextEncoderMs = now + INPUT_REFRESH_MS;  // Every 20 ms
}
```

**Implication:**  
- Encoder position, button, and LED state are ALWAYS up-to-date, even on non-encoder pages
- User can change LED brightness from any page (not just page 4)
- User can press button to navigate from any page (fully responsive)
- Stats aggregation captures all encoder activity, not just when page 4 is visible

**Rationale:** 
- User experience: Encoder should be responsive from any page for navigation
- LED updates: Color/brightness changes should be visible immediately when dial is rotated
- Statistics: Continuous aggregation provides meaningful data on encoder usage patterns
- Power: Cost of 50 Hz polling is minimal compared to benefit of always-on responsiveness

**Failure Behavior (if conditional read remained):**  
- User presses encoder button on page 0 (BME280) → navigation might not work immediately; LED wouldn't update
- User rotates encoder dial on page 3 (INA219) → brightness doesn't change until navigating to page 4

**Evidence:**  
- Loop lines 1856-1858
- Condition: `available.encoder && (int32_t)(now - nextEncoderMs) >= 0` (unconditional, no page check)
- Source: [01-requirements.md](01-requirements.md), FR-004

**Confidence:**  
HIGH

---

## Data Handling & Validation Rules

### BR-019: Gamepad Button State Encoding (Active-Low)

**Description:**  
Gamepad button states use active-low convention: 0 = pressed, 1 = released. Library function `digitalReadBulk()` returns a bitmask with this encoding. Firmware inverts via `!(state & mask)` to get intuitive boolean.

**Trigger:**  
Every 25 ms gamepad read.

**Conditions:**  
- Hardware button pressed (GPIO pulled low)

**Behavior:**  
```cpp
// Raw bitmask from library (active-low):
gamepadValues.buttons = gamepad.digitalReadBulk(BUTTON_MASK);

// To check if button pressed:
bool pressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_Y));  // Negation inverts active-low

// Display helper:
String gamepadButtonState(uint8_t pin) {
  return (gamepadValues.buttons & (1UL << pin)) ? "released" : "PRESSED";
}
```

**Inversion Logic:**
- Raw: bit = 0 (pressed), bit = 1 (released)
- Intuitive: `!(bit & mask)` gives 1 (pressed), 0 (released)

**Display:** Page shows "PRESSED" or "released" (text, not numeric).

**Failure Behavior:**  
- Hardware pullup failure → bit remains 0 (looks like pressed)
- Short-circuit → bit forced low (stuck pressed)
- I2C read error → previous state persists; display mismatch

**Evidence:**  
- `readGamepadAt40Hz()` line 244: `gamepadValues.buttons = gamepad.digitalReadBulk(BUTTON_MASK);`
- Display helper lines 483–485
- Edge detection lines 625–626 (inverts for edge detect)

**Confidence:**  
HIGH

---

### BR-020: Gamepad Keypad Events (Last-Event Capture)

**Description:**  
Gamepad seesaw device maintains a hardware keypad event queue. Firmware reads keypad count and optionally decodes the last event (edge + key index), but does **not** consume the queue for page navigation. Events are captured for debug/display only.

**Trigger:**  
Every 25 ms gamepad read.

**Conditions:**  
- Gamepad detected at boot
- Event queue has pending events (`eventCount > 0`)

**Behavior:**  
```cpp
gamepadValues.eventCount = gamepad.getKeypadCount();
gamepadValues.hasEvent = false;

if (gamepadValues.eventCount > 0) {
  keyEventRaw event{};
  if (gamepad.readKeypad(&event, 1)) {
    gamepadValues.lastEvent = event.reg;
    gamepadValues.hasEvent = true;
    // Note: Only ONE event read per cycle; queue not drained
  }
}
```

**Event Decoding:**
```cpp
uint8_t edge = lastEvent & 0x03;      // Bits 0–1: SEESAW_KEYPAD_EDGE_FALLING (1) or RISING (2)
uint8_t key  = lastEvent >> 2;         // Bits 2–7: Key pin index (0–15)
```

**Page Navigation:** NOT triggered by keypad events. Page navigation uses button state polling (BR-012), not event queue.

**Display:** "Last event:" shows on Gamepad page (e.g., "2 down" for key 2 falling edge).

**Failure Behavior:**  
- If event queue overflows, old events lost (hardware limitation)
- If I2C read fails, event not captured; `hasEvent` remains false
- Queue not drained per-cycle; if multiple events pending, only newest read

**Evidence:**  
- `readGamepadAt40Hz()` lines 250–256
- Display helper `gamepadEventText()` lines 488–497
- Page display line 506: `row(268, "Last event:", gamepadEventText(), TFT_CYAN);`

**Confidence:**  
HIGH

---

## Timing & Synchronization Rules

### BR-021: Time-Gate Rollover Safety (Signed Comparison)

**Description:**  
Main loop scheduling uses signed comparison `(int32_t)(now - deadline) >= 0` for time-gate checks. This handles timer rollover correctly every 49 days (`millis()`) or 71 minutes (`micros()`).

**Trigger:**  
Every loop cycle; multiple timing gates checked.

**Conditions:**  
- Timer register (`millis()` or `micros()`) wraps around to 0 after max value

**Behavior:**  
```cpp
static uint32_t nextFuncMs = 0;
uint32_t now = millis();

if ((int32_t)(now - nextFuncMs) >= 0) {  // Signed comparison
  // Execute function
  nextFuncMs = now + interval;  // Set next deadline relative to now
}
```

**Example Rollover Scenario (millis):**  
- Timer max: 2^32 µs = 4,294,967,295 ≈ 4.3 billion
- Rollover period: 2^32 ms = 49.7 days
- At rollover: `now = 0`, `nextFunc = 4,294,967,000` (near-max value)
- Signed: `(int32_t)(0 - 4,294,967,000)` = negative, wraps correctly as signed 32-bit

**Why Signed?** Unsigned comparison would fail at rollover (0 < large_value).

**Alternative (Unused):** Could use `(int32_t)(now - nextFuncMs) >= 0` or `(now - nextFuncMs) <= deadline_tolerance` (unsigned, brittle).

**Failure Behavior:**  
- Without signed comparison, gate would misbehave at timer rollover
- Functions would not execute for several seconds (until timer value exceeds previous deadline)
- With signed comparison, gate resets correctly after rollover

**Evidence:**  
- All timing gates: lines 623, 637, 644, 649, 666, 673, 677
- Examples: `(int32_t)(now - nextGamepadMs) >= 0`, `(int32_t)(millis() - ds18b20ConversionStartedMs) < 100`, etc.

**Confidence:**  
HIGH

---

### BR-022: INA219 Accumulation Time-Gap Sanity Check (200 ms Max)

**Description:**  
When accumulating Coulomb charge on INA219, gaps > 200 ms are discarded (not accumulated). This prevents spurious large accumulations from system delays, ISR latency, or timer glitches.

**Trigger:**  
Every INA219 50 Hz sample cycle (20 µs gate).

**Conditions:**  
- INA219 already initialized (has prior sample)
- Elapsed time since last sample is measured

**Behavior:**  
```cpp
if (ina219Values.initialized) {
  const uint32_t elapsedUs = nowUs - ina219Values.lastSampleUs;
  
  if (elapsedUs < 200000) {  // 200 ms threshold
    // Normal accumulation
    ina219Values.chargeMah += ina219Values.current * (float)elapsedUs / 3.6e9;
  }
  // else: gap too large; skip accumulation for this cycle
}
```

**Threshold:** 200 ms = 200,000 µs (hardcoded; not configurable).

**Rationale:**  
- Normal 50 Hz sampling: delta ≈ 20,000 µs; well below threshold
- Threshold tolerates up to 10× normal gap (system delays, interrupt handling)
- Gap > 10× suggests error (missed samples, overflow, hang) → conservatively skip

**Failure Behavior:**  
- If system hiccup causes 1-second gap, that gap skipped; charge accumulation pauses
- Missing 1 second of current data (conservative, avoids false accumulation)
- No indication to user; Coulomb counter just has a gap

**Evidence:**  
- `readIna219At50Hz()` lines 383–386
- Constant: 200,000 µs hardcoded

**Confidence:**  
HIGH

---

## Initialization & Boot Rules

### BR-024: VEML6075 Config Write on Boot

**Description:**  
After detecting VEML6075 at boot (reading ID register), firmware writes configuration register 0x00 with value 0x0000 to enable the sensor.

**Trigger:**  
Boot-time sensor detection; `detectSensors()`.

**Conditions:**  
- VEML6075 ID register read succeeds (sensor present)

**Behavior:**  
```cpp
available.veml6075 = readReg16(VEML6075_ADDR, VEML6075_REG_ID, veml6075DeviceId);
if (available.veml6075) 
  writeReg16(VEML6075_ADDR, VEML6075_REG_CONF, 0x0000);  // Enable sensor
```

**Config Value:** 0x0000 (hardcoded; [UNKNOWN] - exact meaning per VEML6075 datasheet).

**Failure Behavior:**  
- If config write fails (I2C error), flag remains true (detection passed)
- Sensor may not operate correctly if config not applied
- No error indication; silently continues

**Evidence:**  
- `detectSensors()` lines 560–561

**Confidence:**  
HIGH

---

### BR-025: Encoder Default LED Brightness (64 / 255)

**Description:**  
Encoder NeoPixel LED brightness is initialized to 64 (on scale 0–255) at boot. This is applied when encoder is first detected.

**Trigger:**  
Boot-time encoder detection; `detectSensors()`.

**Conditions:**  
- Encoder detected at boot

**Behavior:**  
```cpp
uint8_t encoderBrightness = 64;  // Global init (line 111)

// In detectSensors():
available.encoder = encoder.begin(ENCODER_ADDR);
if (available.encoder) {
  // ...
  encoderLed.begin(ENCODER_ADDR);
  encoderLed.setBrightness(encoderBrightness);  // Apply initial brightness
  updateEncoderLed();
}
```

**Default Brightness:** 64 ≈ 25% of max (64 / 255 ≈ 0.25).

**Failure Behavior:**  
- If encoder not detected, brightness remains 64 (unused)
- Brightness command sent to NeoPixel; if I2C fails, LED may be dim or off

**Evidence:**  
- Global init line 111: `uint8_t encoderBrightness = 64;`
- Boot config lines 571–575: brightness set and first update

**Confidence:**  
HIGH

---

### BR-026: TSL2591 Gain & Timing Config (Boot-Time Only)

**Description:**  
After detecting TSL2591 light sensor at boot, firmware configures gain to MED (medium) and integration time to 200 ms. These settings are not reconfigured at runtime.

**Trigger:**  
Boot-time sensor detection; `detectSensors()`.

**Conditions:**  
- TSL2591 detected at boot

**Behavior:**  
```cpp
available.tsl2591 = tsl2591.begin();
if (available.tsl2591) {
  tsl2591.setGain(TSL2591_GAIN_MED);
  tsl2591.setTiming(TSL2591_INTEGRATIONTIME_200MS);
}
```

**Config:**
- Gain: `GAIN_MED` (Adafruit enum; sensitivity level)
- Integration time: 200 ms (conversion/averaging window)

**Rationale:** Fixed config chosen empirically for typical indoor/outdoor light levels.

**Failure Behavior:**  
- If config fails (I2C error), detection flag remains true
- Sensor operates with default settings (possibly incorrect gain/timing)
- No error indication

**Evidence:**  
- `detectSensors()` lines 563–567

**Confidence:**  
HIGH

---

## Summary Table: Business Rules

| BR ID | Category | Rule | Confidence |
|-------|----------|------|-----------|
| BR-001 | Sensor Mgmt | Sensor detection one-time at boot | HIGH |
| BR-002 | Sensor Mgmt | Stale value persistence on I2C failure | HIGH |
| BR-003 | Input | Page navigation disabled without gamepad | HIGH |
| BR-004 | INA219 | Coulomb accumulation (current integration) | HIGH |
| BR-005 | SCD30 | CRC8 validation all-or-nothing | HIGH |
| BR-006 | VEML6075 | UV compensation formulas | HIGH |
| BR-007 | VEML6070 | UV level classification (5 bins) | HIGH |
| BR-010 | Encoder | LED color cycling (4-state) | HIGH |
| BR-011 | Encoder | Dial brightness adjustment (delta×8, clamp 0-255) | HIGH |
| BR-012 | Gamepad | Button page navigation (edge detect, modulo wrap) | HIGH |
| BR-013 | Gamepad | Joystick analog values inverted | HIGH |
| BR-015 | Display | Page redraw only on change (header skip) | HIGH |
| BR-016 | Display | Page-specific refresh rates (20ms live, 500ms slow) | HIGH |
| BR-017 | Display | Sensor unavailable error screen | HIGH |
| BR-018 | Encoder | Encoder read gate (page 4 only, unless page changing) | HIGH |
| BR-019 | Input | Gamepad button active-low encoding | HIGH |
| BR-020 | Input | Gamepad keypad events (debug only, not nav) | HIGH |
| BR-021 | Timing | Time-gate rollover safety (signed comparison) | HIGH |
| BR-022 | INA219 | Accumulation time-gap sanity check (200ms max) | HIGH |
| BR-024 | Init | VEML6075 config write on boot | HIGH |
| BR-025 | Init | Encoder default LED brightness (64/255) | HIGH |
| BR-026 | Init | TSL2591 gain & timing config (boot-time only) | HIGH |

---

## Testing Coverage: Which Business Rules Must Be Verified

**NOTE:** No automated tests currently exist (as of 2026-09-21). See docs/10-testing.md for testing strategy.

This section maps each business rule to recommended test coverage and identifies functional vs. implementation-specific behavior.

### Testing Classification Matrix

| BR | Category | Test Type | Priority | Functional/Implementation | Notes |
|----|----------|-----------|----------|--------------------------|-------|
| BR-001 | Sensor Detection | Integration | HIGH | Functional | All sensors enumerated correctly; must test with each sensor absent |
| BR-002 | Stale Data | Integration | HIGH | Functional | Verify I2C error doesn't corrupt struct; test with bad CRCs |
| BR-003 | Gamepad Gate | Integration | HIGH | Functional | Page nav disabled if no gamepad; test navigation locked to page 0 |
| BR-004 | Coulomb Counter | Unit | HIGH | Functional | Test accumulation formula and time-gap sanity check in isolation |
| BR-005 | CRC8 Validation | Unit | HIGH | Functional | Test CRC8 algorithm with valid/corrupted data; all-or-nothing semantics |
| BR-006 | UV Compensation | Unit | MEDIUM | Functional | Test compensation formulas (2.22×comp1, etc.) with known inputs |
| BR-007 | UV Level Bins | Unit | LOW | Implementation | Hardcoded thresholds; verify binning logic only |
| BR-010 | LED Color Cycle | Unit | MEDIUM | Functional | Test modulo-4 cycling; verify color sequence |
| BR-011 | LED Brightness | Unit | MEDIUM | Functional | Test delta×8 formula; verify clamping to 0–255 |
| BR-012 | Page Navigation | Unit | HIGH | Functional | Test falling-edge detection; verify modulo wrap 0–9 |
| BR-013 | Joystick Inversion | Unit | LOW | Implementation | Inversion formula 1023–raw; verify on display |
| BR-015 | Page Redraw Gate | Integration | MEDIUM | Implementation | Full redraw when page changes; data-only otherwise |
| BR-016 | Refresh Rates | Integration | MEDIUM | Functional | Verify 20ms (live) vs 500ms (slow) refresh rates |
| BR-017 | Error Display | Integration | MEDIUM | Functional | Navigate to missing sensor; verify error screen renders |
| BR-018 | Encoder Gate | Integration | LOW | Implementation | Encoder reads only on page 4; stale state on other pages acceptable |
| BR-019 | Active-Low Encoding | Unit | LOW | Implementation | Button bit inversion via `!(state & mask)` |
| BR-020 | Keypad Events | Integration | LOW | Implementation | Events captured but not used for navigation; debug only |
| BR-021 | Rollover Safety | Unit | CRITICAL | Functional | Timer wrap-around safety via signed comparison; must pass after 49 days or 71 min |
| BR-022 | Gap Sanity Check | Unit | HIGH | Functional | Verify 200ms gap threshold; gaps >200ms skipped in accumulation |
| BR-024 | VEML6075 Config | Integration | LOW | Implementation | Write 0x0000 to config register; no error if fails silently |
| BR-025 | LED Init Brightness | Unit | LOW | Implementation | Default brightness 64/255 (25%); verify clamping |
| BR-026 | TSL2591 Config | Integration | LOW | Implementation | Boot-time gain/timing config; not reconfigurable at runtime |

---

### Unit Tests (No Hardware Required)

**Test these business rules with pure functions/algorithms:**

#### Recommended Unit Tests (Priority: HIGH/CRITICAL)

| Business Rule | Test Name | Inputs | Expected Behavior | Evidence |
|--|--|--|--|--|
| **BR-004** Coulomb | `test_coulomb_accumulation_formula()` | current=100mA, elapsed=1s | charge += 0.02778 mAh | Formula exact match |
| **BR-004** Coulomb | `test_coulomb_gap_skip()` | elapsed > 200ms | accumulation skipped | No change to chargeMah |
| **BR-004** Coulomb | `test_coulomb_first_read_no_accumulation()` | initialized=false | no accumulation occurs | chargeMah unchanged |
| **BR-005** CRC8 | `test_crc8_valid_data()` | SCD30 18-byte response | CRC matches byte 17 | All 6 CRCs pass |
| **BR-005** CRC8 | `test_crc8_detects_single_bit_flip()` | Corrupt one CRC byte | validation fails | readScd30() returns false |
| **BR-005** CRC8 | `test_crc8_all_or_nothing()` | Corrupt byte 5 of 18 | entire read rejected | scd30Values not updated |
| **BR-012** NaviGation | `test_page_forward_wrap()` | page=9, A button | page→0 | (9+1)%10 = 0 ✓ |
| **BR-012** Navigation | `test_page_backward_wrap()` | page=0, Y button | page→9 | (0-1+10)%10 = 9 ✓ |
| **BR-012** Navigation | `test_falling_edge_detection()` | prev=released, curr=pressed | edge triggers | page changes |
| **BR-021** Rollover | `test_rollover_safety_at_boundary()` | now=0, deadline=4.3B | gate fires | `(int32_t)(0 - 4.3B) >= 0` ✓ |
| **BR-022** Gap Check | `test_gap_sanity_check_passes()` | elapsed=150ms | accumulation proceeds | chargeMah updated |
| **BR-022** Gap Check | `test_gap_sanity_check_fails()` | elapsed=300ms | accumulation skipped | chargeMah unchanged |

#### Recommended Unit Tests (Priority: MEDIUM)

| Business Rule | Test Name | Inputs | Expected Behavior | Evidence |
|--|--|--|--|--|
| **BR-006** UV Comp | `test_uv_compensation_formula()` | uva=1000, comp1=50, comp2=25 | uvaComp=855.75 | Exact formula match |
| **BR-006** UV Comp | `test_uv_compensation_clamping()` | Large comp factors | uvaComp≥0 | max(0, ...) applied |
| **BR-010** LED Color | `test_encoder_led_color_cycle()` | 4 button presses | blue→green→red→purple→blue | Cyclic pattern |
| **BR-011** LED Bright | `test_led_brightness_clamp_max()` | brightness=250, delta=10 | brightness→255 | Clamped to max |
| **BR-011** LED Bright | `test_led_brightness_clamp_min()` | brightness=5, delta=-10 | brightness→0 | Clamped to min |
| **BR-019** Active-Low | `test_button_inversion()` | raw bit=0 (pressed) | `!(bit&mask)` = 1 | Correct logic |
| **BR-025** Init Bright | `test_default_brightness()` | encoderBrightness initial | brightness=64 | 64/255 ✓ |

---

### Integration Tests (Hardware Required)

**Test these business rules with real I2C bus, 1-Wire, display, and timing:**

#### Recommended Integration Tests (Priority: HIGH/CRITICAL)

| Business Rule | Test Name | Setup | Procedure | Expected Result | Failure Mode |
|--|--|--|--|--|--|
| **BR-001** Sensor Detect | `test_all_sensors_enumerated()` | All sensors connected | Boot & check available[] | All available[].X=true | Missing sensor: available[].X=false (expected) |
| **BR-001** Sensor Detect | `test_missing_sensor_graceful()` | One sensor disconnected | Boot & navigate to page | Error screen displayed | No crash; navigation works |
| **BR-002** Stale Data | `test_i2c_read_failure_preserves_value()` | SCD30 connected; simulate I2C error | Read sensor with error | Previous value persists | Value corrupted or changed |
| **BR-003** Gamepad Gate | `test_navigation_disabled_no_gamepad()` | Gamepad disconnected | Try Y/A buttons on page 0 | Page remains 0 | Page changes (navigation enabled) |
| **BR-016** Refresh Rate | `test_gamepad_page_20ms_refresh()` | Gamepad connected & page 2 active | Measure refresh cycles | 50 Hz (20ms) ±5% | Slower or irregular refresh |
| **BR-016** Refresh Rate | `test_sensor_page_500ms_refresh()` | BME280 page active | Measure refresh cycles | 2 Hz (500ms) ±5% | Faster or irregular refresh |
| **BR-017** Error Display | `test_missing_sensor_error_screen()` | Sensor disconnected | Navigate to its page | "Sensor nicht vorhanden" in red | Normal data displayed or crash |

#### Recommended Integration Tests (Priority: MEDIUM)

| Business Rule | Test Name | Setup | Procedure | Expected Result | Failure Mode |
|--|--|--|--|--|--|
| **BR-004** Coulomb | `test_coulomb_real_data_accumulates()` | INA219 connected; known load | Run 10s at 1A | chargeMah ≈ 0.0278 | No accumulation or incorrect value |
| **BR-005** CRC8 Real | `test_scd30_crc_real_read()` | SCD30 connected | Read sensor 10 times | All reads valid; display CO₂ | CRC failures or stale data |
| **BR-006** UV Real | `test_veml6075_compensation()` | VEML6075 connected | Read sensor; calculate index | UV index ≥ 0 (clamped) | Negative values or overflow |
| **BR-010** LED Color | `test_encoder_led_color_change()` | Encoder connected | Press button 4 times | LED cycles through 4 colors | No LED change or wrong sequence |
| **BR-011** LED Bright | `test_encoder_led_brightness_adjust()` | Encoder connected | Rotate dial; measure LED PWM | Brightness changes proportionally | LED stays same brightness |
| **BR-012** Nav Real | `test_gamepad_nav_real_buttons()` | Gamepad connected | Press Y/A buttons | Pages navigate correctly | Page doesn't change |
| **BR-015** Page Redraw | `test_page_change_full_redraw()` | Any page active | Change page | Header + data redrawn | Only data redrawn; old header visible |

#### Recommended Integration Tests (Priority: LOW/IMPLEMENTATION)

| Business Rule | Test Name | Setup | Procedure | Expected Result | Notes |
|--|--|--|--|--|--|
| **BR-007** UV Levels | `test_veml6070_binning_display()` | VEML6070 connected | Display shows bin category | "Very low"/"Low"/etc. | Binning is display-only; low priority |
| **BR-013** Invert | `test_gamepad_joystick_inversion()` | Gamepad connected; page 2 active | Move joystick; observe values | 1023-raw applied correctly | Visual inspection only |
| **BR-018** Encoder Gate | `test_encoder_stale_on_other_pages()` | Encoder connected | Navigate to page 0; press button | LED doesn't update | Low impact; expected behavior |
| **BR-020** Keypad | `test_gamepad_keypad_events()` | Gamepad connected; page 2 active | Press keypad buttons | "Last event:" displayed | Debug only; navigation unaffected |
| **BR-024** Init | `test_veml6075_config_write()` | VEML6075 connected | Boot system | Sensor enabled (0x0000 written) | Integration test; config effect [UNKNOWN] |
| **BR-026** Init | `test_tsl2591_gain_config()` | TSL2591 connected | Boot system; measure sensitivity | Gain applied (MED) | Boot-time config; not reconfigurable |

---

### Functional vs. Implementation-Specific Behaviors

**Functional Behaviors** (must be tested; represent intended business logic):
- BR-004: Coulomb accumulation formula (business requirement for energy tracking)
- BR-005: CRC8 validation (data integrity requirement)
- BR-012: Page navigation (UI requirement)
- BR-021: Rollover safety (system stability requirement)

**Implementation-Specific Behaviors** (monitor but not business-critical):
- BR-007: VEML6070 binning thresholds (arbitrary display choices)
- BR-013: Joystick inversion (signal conditioning; user perception only)
- BR-015: Full vs. data-only redraw (performance optimization)
- BR-018: Encoder read gate to page 4 (power optimization; acceptable stale state)
- BR-019: Active-low bit inversion (hardware convention; internal detail)
- BR-024: VEML6075 config value 0x0000 (empirical; no documentation)
- BR-026: TSL2591 gain/timing (empirical; fixed at boot, not reconfigurable)

---

### Edge Cases & Negative Test Cases

#### Edge Cases to Test

| Rule | Edge Case | Test Strategy |
|------|-----------|----------------|
| BR-004 | Timer rollover (49 days) | Simulate micros() wrap; verify signed comparison handles it |
| BR-012 | Rapid button presses | Press Y/A 20 times/second; verify no double navigation |
| BR-021 | Timing gate at exact rollover | Set deadline = 0xFFFFFFFF; verify gate fires at wrap |
| BR-022 | Gap exactly 200ms | Elapsed=200000µs; verify on boundary; then 200001µs (should skip) |

#### Negative Test Cases (Expected Failures)

| Rule | Negative Case | Expected Behavior |
|------|---------------|--------------------|
| BR-001 | Missing sensor at boot | available[].X=false; page shows error screen |
| BR-002 | SCD30 CRC corruption | Entire read rejected; stale values persist |
| BR-003 | Gamepad not detected | Page navigation disabled; lock at page 0 |
| BR-005 | SCD30 I2C timeout | Timeout handled gracefully; stale data displayed |
| BR-017 | User navigates to missing sensor | Error screen displayed; navigation works |

---

## Display Rules

### BR-027: I2C Address Display (Header Only)

**Description:**  
Each sensor page displays the I2C address ONLY in the page header (title). The address is not displayed elsewhere on the page to avoid redundancy.

**Title Format:**  
`"${SensorName} (${Address})"`

**Behavior:**  
- Address format: Hexadecimal with "0x" prefix (e.g., "0x77")
- Dual-address sensors: Slash-separated (e.g., "0x38/0x39")
- Trigger: Page header rendered on page change or boot
- Location: Header/title only (y=8)

**Page Examples:**

| Page | Title |
|------|-------|
| 0 | BME280 (0x77) |
| 1 | Gamepad QT (0x50) |
| 2 | INA219 (0x40) |
| 3 | Rotary Encoder (0x36) |
| 4 | SCD30 (0x61) |
| 5 | SI1145 (0x60) |
| 6 | TSL2591 (0x29) |
| 7 | VEML6070 (0x38/0x39) |
| 8 | VEML6075 (0x10) |

**Implementation:**  
- Titles: Hardcoded strings in `titles[]` array (lines 444–453 in main.cpp)
- Display: `drawHeader(titles[currentPage])` call (line 459)
- No data rows or footer redundancy

**Evidence:**  
- `const char *titles[]` array (lines 444–453 in main.cpp)
- `drawHeader(titles[currentPage])` call (line 459)
- Page functions start at y=76 for first data row (address not repeated)

**Rationale:**
Single display location reduces screen clutter while providing immediate sensor identification.

**Confidence:**  
HIGH

---

## Large Display Mode Rules (INA219 Page 2) — **NEW**

### BR-028: Large Display Mode Entry (Button B on Page 2)

**Description:**  
User enters large display mode for INA219 power monitor by pressing Button B while on Page 2. Mode displays a single sensor value at a time in enlarged, centered format.

**Trigger:**  
Button B falling-edge (press) detected while `currentPage == 2` (INA219 page).

**Conditions:**  
- Page 2 (INA219) currently active
- `largeDisplayMode == false` (not already in mode)
- Button B transitions from released to pressed

**Behavior:**  
```cpp
if (currentPage == 2 && bPressed && !lastB) {
  largeDisplayMode = true;
  largeDisplayIndex = 0;  // Start at first value (Laufzeit)
  // Next render cycle calls pageLargeDisplayIna219(0)
}
lastB = bPressed;
```

**Display Effect:**  
- Page 2 switches from standard data rows layout to large display (single value centered)
- First value displayed: Index 0 (Laufzeit / Runtime)
- Page title hidden (alternate layout, all space for large value)
- All screen real estate used for large label + value + unit

**Failure Behavior:**  
- If button press not detected (I2C error on gamepad read), mode entry fails silently
- Page remains in standard view

**Evidence:**  
- Global variables: `largeDisplayMode`, `largeDisplayIndex`, `largeDisplayButton` 
- Loop logic: Button edge detection + mode state transition

**Confidence:**  
HIGH

---

### BR-029: Large Display Value Cycling Forward (Button B)

**Description:**  
While in large display mode, pressing Button B cycles to the next value in the 7-value list. Values wrap around: after index 6 (Power), cycling forward returns to index 0 (Laufzeit).

**Trigger:**  
Button B falling-edge (press) detected while `largeDisplayMode == true`.

**Conditions:**  
- Large display mode active (`largeDisplayMode == true`)
- Button B transitions from released to pressed
- Current index: 0–6

**Behavior:**  
```cpp
if (largeDisplayMode && bPressed && !lastB) {
  largeDisplayIndex = (largeDisplayIndex + 1) % 7;  // Modulo wrap
  // Next render cycle displays new value via pageLargeDisplayIna219(largeDisplayIndex)
}
lastB = bPressed;
```

**Value Order (Cycle Index 0→1→2→3→4→5→6→0):**

| Index | Value | Label |
|-------|-------|-------|
| 0 | Runtime | Laufzeit |
| 1 | Bus Voltage | Batterie |
| 2 | Shunt Voltage | Versorgung |
| 3 | Current | Strom |
| 4 | Charge | Ladung |
| 5 | Load Voltage | Shunt-Spannung |
| 6 | Power | Power |

**Wrap-Around:**  
- From index 6, next press advances to index 0 (via modulo 7 arithmetic)
- No bounds checking needed; modulo ensures valid range

**Failure Behavior:**  
- If button press not detected, index unchanged; no cycle occurs

**Evidence:**  
- Modulo arithmetic: `(largeDisplayIndex + 1) % 7`
- Value mapping table in BR-028 / Section 2.9 data model

**Confidence:**  
HIGH

---

### BR-030: Large Display Value Cycling Reverse (Button X)

**Description:**  
While in large display mode, pressing Button X cycles to the previous value in the 7-value list. Values wrap around: from index 0 (Laufzeit), cycling backward returns to index 6 (Power).

**Trigger:**  
Button X falling-edge (press) detected while `largeDisplayMode == true`.

**Conditions:**  
- Large display mode active (`largeDisplayMode == true`)
- Button X transitions from released to pressed
- Current index: 0–6

**Behavior:**  
```cpp
if (largeDisplayMode && xPressed && !lastX) {
  largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7;  // Modulo wrap with offset
  // Next render cycle displays new value via pageLargeDisplayIna219(largeDisplayIndex)
}
lastX = xPressed;
```

**Wrap-Around Logic:**  
- `(0 - 1 + 7) % 7 = 6` (from index 0 → index 6)
- `(6 - 1 + 7) % 7 = 5` (from index 6 → index 5)
- Offset of 7 ensures non-negative value before modulo

**Failure Behavior:**  
- If button press not detected, index unchanged; no cycle occurs

**Evidence:**  
- Reverse modulo arithmetic: `(largeDisplayIndex - 1 + 7) % 7`
- Value mapping: same 7-value list as BR-029

**Confidence:**  
HIGH

---

### BR-031: Large Display Mode Exit & Page Navigation Disable [CRITICAL]

**Description:**  
Pressing Button Y while in large display mode exits the mode, returning to standard Page 2 (INA219) view. **CRITICAL:** Page navigation (Y/A buttons) is completely disabled while in large display mode and only re-enabled upon exit. Button Y is interpreted as "exit mode" (NOT "previous page"), and Button A is completely inactive.

**Trigger:**  
Button Y falling-edge (press) detected while `largeDisplayMode == true`.

**Conditions:**  
- Large display mode active (`largeDisplayMode == true`)
- Button Y transitions from released to pressed

**Behavior - Mode Exit:**  
```cpp
if (largeDisplayMode && yPressed && !lastY) {
  largeDisplayMode = false;
  largeDisplayIndex = 0;  // Reset index
  // Next render cycle calls pageIna219() in normal mode
}
lastY = yPressed;
```

**Behavior - Page Navigation Disable During Mode:**  
```cpp
if (largeDisplayMode) {
  // Page navigation buttons are NOT processed
  // yPressed and aPressed button presses are consumed by mode logic, not page nav
  
  // In Button Y handler:
  if (yPressed && !lastY) {
    // Interpreted as "exit mode", not "previous page"
    largeDisplayMode = false;
  }
  
  // In Button A handler:
  if (aPressed && !lastA) {
    // Button A completely inactive; no effect
    // Do nothing
  }
}
else {
  // Normal page navigation (when NOT in large display mode)
  if (yPressed && !lastY) {
    currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;  // Previous page
  }
  if (aPressed && !lastA) {
    currentPage = (currentPage + 1) % PAGE_COUNT;  // Next page
  }
}
```

**Critical Constraint:**  
The page navigation mechanism is **functionally disabled** while `largeDisplayMode == true`. This means:
- User CANNOT navigate to other pages while cycling through large display values
- Only way to exit large display mode is Button Y (interpreted as "exit", not "prev page")
- Button A has no effect (reserved for future use or standard page navigation only)
- Prevents accidental page changes during value cycling
- Re-enables normal circular pagination only after exit

**Display Indicator:**  
Footer instructions clarify button behavior: "B:next X:prev Y:exit" (not "Y: previous page" as in normal view).

**Failure Behavior:**  
- If Button Y press not detected, mode remains active; no exit possible
- User must press Y button again on next cycle
- Once in mode, cannot escape without working Button Y

**Evidence:**  
- Global state: `largeDisplayMode` flag used to conditionally skip page navigation logic
- Button handler order: mode exit logic checked before normal page navigation
- Loop conditional: `if (!largeDisplayMode) { /* page nav */ }`

**Confidence:**  
HIGH

**Design Rationale:**  
This constraint is intentional and by design:
1. **Safety:** Prevents accidental page jumps while focused on cycling values
2. **UX Clarity:** Button actions are context-sensitive and consistent within each mode
3. **Scope Limitation:** Feature scoped to Page 2; page navigation not available during large display
4. **Future Extension:** When feature extended to other pages, each page gets its own exit button behavior

---

### BR-032: Button A Inactive in Large Display Mode

**Description:**  
While in large display mode (Page 2 INA219), Button A (gamepad) is completely inactive and has no effect. This ensures only Buttons B (forward), X (reverse), and Y (exit) are available for large display control.

**Trigger:**  
Button A press (or release) while `largeDisplayMode == true`.

**Conditions:**  
- Large display mode active (`largeDisplayMode == true`)
- Button A state changes (pressed or released)

**Behavior:**  
```cpp
if (largeDisplayMode) {
  if (aPressed && !lastA) {
    // No action taken
    // Do nothing; button press consumed by large display mode check
  }
}
// Button A only affects page navigation when largeDisplayMode == false
```

**Rationale:**  
- Button A would normally navigate to next page (BR-012)
- In large display mode, page navigation is disabled (BR-031)
- Button A must remain inactive to avoid confusion
- Button action clearly documented in UI footer

**Failure Behavior:**  
- No unintended side effects; button is safely ignored
- User may press Button A thinking it will navigate; no effect (by design)

**Evidence:**  
- Loop logic: Button A press ignored when `largeDisplayMode == true`
- No page navigation triggered by Button A in large display mode

**Confidence:**  
HIGH

---

### BR-033: Large Display Real-Time Updates & Value Formatting

**Description:**  
Values displayed in large display mode are read directly from global `ina219Values` struct on every render cycle with no caching. All formatting (decimal places, units, colors) must match standard `pageIna219()` display exactly to ensure visual consistency between modes.

**Trigger:**  
Every render cycle (500 ms for INA219 page) while `largeDisplayMode == true`.

**Conditions:**  
- Large display mode active
- Render cycle fires
- `pageLargeDisplayIna219(largeDisplayIndex)` called

**Behavior - Real-Time Read:**  
```cpp
// In getLargeDisplayValue(valueIndex):
switch (valueIndex) {
  case 0:  // Laufzeit
    return String(millis() / 1000);  // Integer seconds, no decimals
  case 1:  // Batterie
    return String(ina219Values.bus, 3);  // 3 decimals from current struct
  case 2:  // Versorgung
    return String(ina219Values.shunt, 3);  // 3 decimals from current struct
  case 3:  // Strom
    return String(ina219Values.current, 3);  // 3 decimals from current struct
  case 4:  // Ladung
    return String(ina219Values.chargeMah, 3);  // 3 decimals from current struct
  case 5:  // Shunt-Spannung (Load)
    return String(ina219Values.load, 3);  // 3 decimals from current struct
  case 6:  // Power
    return String(ina219Values.power, 3);  // 3 decimals from current struct
}
```

**No Caching:**  
- Values fetched from struct on every render
- Reflects latest measurement (within 500 ms)
- If sensor reading failed, stale value from last successful read is displayed (consistent with BR-002)

**Formatting Consistency:**  
All values formatted identically to standard `pageIna219()` display:
- Runtime: Integer (no decimals)
- Voltage values: 3 decimals
- Current/Power: 3 decimals
- Charge: 3 decimals
- Units and colors match exactly

**Color Application:**  
Colors applied per `getLargeDisplayColor(valueIndex)`:
- Cyan (Laufzeit, Ladung): Derived/computed values
- Green (Batterie, Versorgung, Shunt-Spannung): Primary measurements
- Orange (Strom, Power): Secondary measurements

**Failure Behavior:**  
- If INA219 sensor read fails in previous cycle, stale value persists (per BR-002)
- Stale value displayed in large format same as normal display
- No error indication in large display mode (no "STALE" warning)

**Evidence:**  
- Helper functions: `getLargeDisplayValue()`, `getLargeDisplayColor()`, `getLargeDisplayUnit()`
- Struct access: all read from global `ina219Values` without buffering
- Refresh cycle: page native 500 ms rate, inherited from standard Page 2 behavior

**Confidence:**  
HIGH

---

## 2.10 Large Display Mode Business Rules (INA219 Page 2) — **NEW**

### BR-028: Large Display Mode Entry

**Rule:**  
When Button B is pressed AND currently on Page 2 (INA219) AND NOT already in large display mode, enter large display mode with initial index 0 (Laufzeit).

**Condition:**
```cpp
if (currentPage == 2 && bPressed && !lastB && !largeDisplayMode)
```

**Action:**
```cpp
largeDisplayMode = true;
largeDisplayIndex = 0;
```

**Effect:**  
- Next `pageIna219()` call renders `pageLargeDisplayIna219(0)` instead of standard page
- Display shows "Laufzeit" (runtime) in large format
- Navigation buttons Y/A become inactive; B/X become cycle buttons

**Scope:** Page 2 (INA219) only. Does not apply to other pages.

**Constraint:** Only applies if `available.ina219 == true`.

**Evidence:** Lines 973–978 in src/main.cpp

---

### BR-029: Large Display Mode Forward Cycle

**Rule:**  
When Button B is pressed AND currently in large display mode, cycle to the next value index (forward with wrap-around).

**Condition:**
```cpp
if (largeDisplayMode && bPressed && !lastB)
```

**Action:**
```cpp
largeDisplayIndex = (largeDisplayIndex + 1) % 7;
```

**Effect:**  
- Index increments: 0→1→2→3→4→5→6→0 (wrap-around at 7)
- Next render displays new value in large format
- All 7 values (Laufzeit, Batterie, Versorgung, Strom, Ladung, Shunt-Spannung, Power) are cycled

**Scope:** Only active while `largeDisplayMode == true`.

**Evidence:** Lines 979–982 in src/main.cpp

---

### BR-030: Large Display Mode Reverse Cycle

**Rule:**  
When Button X is pressed AND currently in large display mode, cycle to the previous value index (backward with wrap-around).

**Condition:**
```cpp
if (largeDisplayMode && xPressed && !lastX)
```

**Action:**
```cpp
largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7;  // Wrap safely to 0-6
```

**Effect:**  
- Index decrements: 6→5→4→3→2→1→0→6 (wrap-around at -1)
- Next render displays new value in large format
- Exact inverse of BR-029; pressing X then B returns to same value

**Scope:** Only active while `largeDisplayMode == true`. Button X is inactive outside this mode.

**Evidence:** Lines 985–987 in src/main.cpp

---

### BR-031: Large Display Mode Exit

**Rule:**  
When Button Y is pressed AND currently in large display mode, exit large display mode and immediately render standard INA219 page.

**Condition:**
```cpp
if (largeDisplayMode && yPressed && !lastY)
```

**Action:**
```cpp
largeDisplayMode = false;
largeDisplayIndex = 0;
drawHeader("INA219 (0x40)");  // Clears screen atomically with fillScreen(TFT_BLACK)
pageIna219();  // Renders standard page data
```

**Effect:**  
- Transition is immediate (not deferred to next refresh cycle)
- Standard page with all 7 values displayed cleanly (no artifacts)
- Navigation buttons Y/A restored to normal functions
- Mode state reset for safety

**Scope:** Only active while `largeDisplayMode == true`.

**Rendering Strategy:** Immediate rendering (within button handler) ensures atomic clear (`fillScreen`) before new content, preventing visual artifacts.

**Evidence:** Lines 1000–1005 in src/main.cpp

---

### BR-032: Button A Inactivity in Large Display Mode

**Rule:**  
When Button A is pressed AND currently in large display mode, the button is ignored (no action taken).

**Condition:**
```cpp
if (largeDisplayMode && aPressed && !lastA)
  // Do nothing; no branch taken
```

**Effect:**  
- No page navigation occurs
- Large display mode remains active
- User cannot accidentally navigate away from Page 2 while in large display mode

**Rationale:** Prevents accidental page navigation while viewing single value. User must explicitly press Y to exit mode before navigating.

**Scope:** Only active while `largeDisplayMode == true`. Button A works normally outside this mode (BR-005).

**Evidence:** Lines 1008–1011 in src/main.cpp

---

### BR-033: Large Display Mode Safety Reset on Page Change

**Rule:**  
When leaving Page 2 (INA219) while in large display mode, forcibly reset large display state to prevent state leak to other pages.

**Condition:**
```cpp
if (lastPage == 2 && currentPage != 2 && largeDisplayMode)
```

**Action:**
```cpp
largeDisplayMode = false;
largeDisplayIndex = 0;
```

**Effect:**  
- Large display mode is deactivated
- If user navigates back to Page 2 later, large display mode starts fresh from index 0
- No stale state persists across page boundaries

**Scope:** Automatic safety mechanism. Always active during page change handling (line 1026–1029).

**Rationale:** Multi-page navigation requires clean state; large display flags must not leak to other page renderers.

---

## Non-Blocking Rotated Sensor Architecture Rules (FEATURE-SPEC-006)

### BR-SC-005: Rotated Sensor Read Scheduling (10-Slot, Phase-Based, 2000ms Cycle)

**Rule:**  
All slow I2C sensors (BME280, TSL2591, SI1145, VEML6075, VEML6070, SCD30, DS18B20) shall be read in a rotating schedule with 10 slots organized into three phases (START async → READ sync → CHECK async), completing a full cycle every 2000 ms.

**Slot Schedule (10 slots × 200ms = 2000ms cycle):**

**Phase 1: START ASYNC (Slots 0-1, 0-200ms)**
```
Slot 0 (0ms):   startTsl2591Measurement()   → State: IDLE → MEASURING
Slot 1 (200ms): startDs18b20Conversion()    → State: IDLE → CONVERTING
```

**Phase 2: READ SYNC (Slots 2-7, 400-1400ms)**
```
Slot 2 (400ms):  readBme280()
Slot 3 (600ms):  readSi1145()
Slot 4 (800ms):  readVeml6075()
Slot 5 (1000ms): readVeml6070()
Slot 6 (1200ms): readScd30()
Slot 7 (1400ms): performDs2484Scan() (every 60s)
```

**Phase 3: CHECK ASYNC (Slots 8-9, 1600-1800ms)**
```
Slot 8 (1600ms): checkTsl2591Async()  → State: MEASURING → IDLE if ready
Slot 9 (1800ms): checkDs18b20Async()  → State: CONVERTING → IDLE if ready
```

**Slot Dispatch Algorithm:**
```cpp
uint32_t currentSensorSlot = (millis() / SENSOR_SLOT_INTERVAL_MS) % SLOT_COUNT;
// SENSOR_SLOT_INTERVAL_MS = 200, SLOT_COUNT = 10
// Time-based modulo arithmetic: startup-safe, wallclock-synchronized
```

**Main Loop Gate:**
```cpp
static uint32_t lastSensorSlotDispatched = 0xFFFFFFFF;
if (currentSensorSlot != lastSensorSlotDispatched) {
  rotatedReadSensor(currentSensorSlot);  // Dispatch to appropriate handler
  lastSensorSlotDispatched = currentSensorSlot;
}
```

**Constants:**
```cpp
#define ENABLE_STATS_REPORTING 1
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;   // Slot interval (immutable)
constexpr uint32_t SLOT_COUNT = 10;                // Slots per cycle (immutable)
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000; // Stats period (immutable)
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;  // Adjustable
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;   // Adjustable
```

**Behavior:**
- Each slot executes ONE sensor operation (START async or READ sync or CHECK async)
- Maximum blocking time per slot: <100 ms (single sensor read)
- Full cycle: 2000 ms for all 10 slots (increased from previous 1400ms)
- Async sensors start early (Slots 0-1), read late (Slots 8-9) with guaranteed margins
- Time-based calculation ensures automatic synchronization (no state counting)

**Why 10 Slots (2000ms)?**
- DS18B20 at 12-bit precision requires 750ms conversion time
- Start time: Slot 1 (200ms elapsed)
- Check time: Slot 9 (1800ms elapsed)
- Elapsed between start and check: 1800 - 200 = 1600ms > 750ms ✓
- Extra 850ms margin for safety and unexpected delays

**Rationale:** Phase-based organization clarifies intent (START async → allow background processing → CHECK async). 2000ms cycle allows sufficient margins for longest sensor (DS18B20 at 12-bit).

**Evidence:** 
- Rotation dispatch logic in `rotatedReadSensor()` function at line ~1539
- Main loop gate in `loop()` at line ~2494
- FEATURE-SPEC-006.md, Section "Detailed Specification"

**Confidence:** HIGH (fully implemented, verified via multiple builds)

---

### BR-SC-006: Asynchronous TSL2591 Measurement (Two-Phase Read, Slot 0 → Slot 8)

**Rule:**  
TSL2591 sensor shall be read using a two-phase async pattern to avoid blocking on 200 ms integration time.

**Pattern:**
```
Phase 1 (Slot 0, 0 ms):  startTsl2591Measurement()
  → Trigger ADC conversion on hardware
  → Return immediately (blocking: ~0 ms)
  → Set tsl2591State = MEASURING
  → Record tsl2591MeasureStartMs = millis()

Phase 2 (Slot 8, 1600 ms):  checkTsl2591Async()
  → Check if (millis() - tsl2591MeasureStartMs) >= 200ms
  → If ready: call readTsl2591(), cache result, set state to IDLE
  → If not ready: return without reading (retry next cycle)
```

**Timeline Guarantee:**
```
0ms:    Measurement started (Slot 0)
200ms:  Hardware integration complete
1600ms: Check result (Slot 8)
Margin: 1600 - 200 = 1400ms > 200ms integration ✓
```

**State Machine:**
```cpp
enum class Tsl2591State { IDLE = 0, MEASURING = 1 };
static Tsl2591State tsl2591State = Tsl2591State::IDLE;
static uint32_t tsl2591MeasureStartMs = 0;

void startTsl2591Measurement() {
  if (!available.tsl2591) return;
  // Trigger hardware ADC
  tsl2591State = Tsl2591State::MEASURING;
  tsl2591MeasureStartMs = millis();
}

void checkTsl2591Async() {
  if (tsl2591State != Tsl2591State::MEASURING) return;
  uint32_t elapsedMs = millis() - tsl2591MeasureStartMs;
  if (elapsedMs >= TSL2591_INTEGRATION_TIME_MS) {
    // Measurement complete, read result
    readTsl2591(tsl2591Values);  // Cache result
    tsl2591State = Tsl2591State::IDLE;
  }
  // Otherwise: not ready, skip read (will retry in next cycle)
}
```

**Configuration:**
```cpp
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;  // Adjustable (default 200ms)
```

**Benefit:** Eliminates 200 ms blocking on sensor integration. Measurement happens in background across 1600ms window.

**Failure Mode:** If readiness check is bypassed or timing is too short, system blocks for 200 ms (input latency affected).

**Evidence:** 
- rotatedReadSensor(0) calls startTsl2591Measurement()
- rotatedReadSensor(8) calls checkTsl2591Async()
- FEATURE-SPEC-006.md, Section "FR-2: Asynchronous TSL2591"
- State machine enums in main.cpp

**Confidence:** HIGH (fully implemented, verified via builds)

---

### BR-SC-007: Asynchronous DS18B20 Temperature Conversion (Two-Phase Read, Slot 1 → Slot 9)

**Rule:**  
DS18B20 1-Wire temperature sensors shall be read using a two-phase async pattern to avoid blocking on 750 ms conversion time (12-bit precision).

**Pattern:**
```
Phase 1 (Slot 1, 200 ms):  startDs18b20Conversion()
  → Send 1-Wire Convert T (0x44) command to all discovered sensors
  → Return immediately (blocking: ~0 ms)
  → Set ds18b20State = CONVERTING
  → Record ds18b20ConvertStartMs = millis()

Phase 2 (Slot 9, 1800 ms):  checkDs18b20Async()
  → Check if (millis() - ds18b20ConvertStartMs) >= 750ms
  → If ready: Read temperature scratchpad from each sensor via 1-Wire
  →          Update ds2484Values.sensors[i].temperature
  →          Apply three-strike lockout (see BR-SC-010)
  →          Set state to IDLE
  → If not ready: return without reading (retry next cycle)
```

**Timeline Guarantee:**
```
200ms:  Conversion started (Slot 1)
950ms:  Hardware conversion complete (~750ms later)
1800ms: Check result (Slot 9)
Margin: 1800 - 200 = 1600ms elapsed > 750ms conversion ✓
```

**State Machine:**
```cpp
enum class Ds18b20State { IDLE = 0, CONVERTING = 1 };
static Ds18b20State ds18b20State = Ds18b20State::IDLE;
static uint32_t ds18b20ConvertStartMs = 0;

void startDs18b20Conversion() {
  if (!available.ds2484 || ds2484Values.sensorCount == 0) return;
  // Send 1-Wire reset + skip ROM (0xCC) + Convert T (0x44) to all sensors
  ds18b20State = Ds18b20State::CONVERTING;
  ds18b20ConvertStartMs = millis();
}

void checkDs18b20Async() {
  if (ds18b20State != Ds18b20State::CONVERTING) return;
  uint32_t elapsedMs = millis() - ds18b20ConvertStartMs;
  if (elapsedMs >= DS18B20_CONVERSION_TIME_MS) {
    // Conversion complete, read results
    readDs18b20Results();  // Updates ds2484Values.sensors[i].temperature
    ds18b20State = Ds18b20State::IDLE;
  }
  // Otherwise: not ready, skip read (will retry in next cycle)
}
```

**Configuration:**
```cpp
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;  // 12-bit standard resolution (adjustable)
```

**Resolution Modes:** 
- 8-bit: ~95 ms
- 10-bit: ~375 ms
- 12-bit: ~750 ms (default)

**Benefit:** Eliminates 750 ms blocking on 1-Wire conversion. Conversion happens in background across 1600ms window.

**Failure Mode:** If readiness check is bypassed or timing is too short, system blocks for 750 ms (catastrophic for input latency).

**Evidence:** 
- rotatedReadSensor(1) calls startDs18b20Conversion()
- rotatedReadSensor(9) calls checkDs18b20Async()
- FEATURE-SPEC-006.md, Section "FR-3: Asynchronous DS18B20"
- State machine enums in main.cpp
- readDs18b20Results() function at line ~1408

**Confidence:** HIGH (fully implemented, verified via builds)

---

### BR-SC-008: Display Update Only on Value Change or Timer Event

**Rule:**  
Display shall be redrawn only when sensor values actually change OR on timer-based events (INA219 timer), not on every read cycle.

**Conditions for Redraw:**
```cpp
bool shouldRedraw = false;

// Condition 1: Page changed
if (currentPage != lastPage) {
  drawCurrentPage(true);  // Full redraw (header + data)
  shouldRedraw = true;
}

// Condition 2: Sensor values changed
if (!shouldRedraw && valuesChanged(currentCache, previousCache)) {
  drawCurrentPage(false);  // Partial redraw (data only)
  shouldRedraw = true;
}

// Condition 3: Timer event (INA219 seconds counter incremented)
if (!shouldRedraw && (ina219Values.timer != lastIna219Timer)) {
  drawCurrentPage(false);  // Partial redraw (data only)
  lastIna219Timer = ina219Values.timer;
  shouldRedraw = true;
}

// Condition 4: No condition met → No redraw
if (!shouldRedraw) {
  // Skip drawCurrentPage() entirely
}
```

**Value Comparison:**
```cpp
bool valuesChanged(const SensorCache &current, const SensorCache &previous) {
  // Compare all relevant fields for current page
  // Return true if ANY value differs from previous
}
```

**Benefit:** Reduces unnecessary TFT drawing (CPU/SPI load), eliminates flicker.

**Performance Impact:**
- Display updates only when needed (smart refresh)
- Input remains responsive (< 200 ms)
- Power consumption reduced

**Edge Cases:**
- First read: All comparisons are against uninitialized previous values (NaN), so redraw occurs
- Page change: Always triggers full redraw (expected behavior)

**Evidence:** 
- FEATURE-SPEC-006.md, Section "FR-4: Smart Display Updates"
- Display change detection logic in loop

**Confidence:** MEDIUM (pending implementation)

---

### BR-SC-008: DS18B20 Three-Strike Lockout (Error Handling)

**Rule:**  
If a DS18B20 1-Wire temperature sensor fails to read 3 consecutive times, it shall be marked as unavailable and no longer polled until system reboot.

**Behavior:**
```cpp
struct Ds18b20Sensor {
  uint8_t address[8];        // 1-Wire ROM address
  float temperature;         // Latest temperature in °C
  bool available;            // Online/offline flag
  uint8_t errorCount;        // 0-3 (3 = lockout)
  uint32_t lastReadMs;       // Timestamp of last successful read
};

void readDs18b20Results() {
  for (int i = 0; i < ds2484Values.sensorCount; i++) {
    // Read temperature from sensor...
    if (/* read succeeded */) {
      ds2484Values.sensors[i].temperature = temp_value;
      ds2484Values.sensors[i].errorCount = 0;  // Reset on success
      ds2484Values.sensors[i].lastReadMs = millis();
    } else {
      // Read failed
      ds2484Values.sensors[i].errorCount++;
      if (ds2484Values.sensors[i].errorCount >= 3) {
        // Three-strike lockout
        ds2484Values.sensors[i].available = false;
        Serial.printf("[DS18B20] Sensor %d locked out after 3 errors\n", i);
      }
    }
  }
  ds18b20State = Ds18b20State::IDLE;
}
```

**Failure Cases Counted:**
- I2C timeout or error during 1-Wire read
- CRC mismatch on temperature value
- Out-of-range temperature (plausibility check fails)
- Any return value indicating read failure

**Lockout Behavior:**
- After 3 consecutive failures: `sensor.available = false`
- Sensor no longer polled in subsequent cycles
- Page 9 (DS2484) displays "Disconnected" for locked-out sensors
- No automatic recovery (requires reboot to reset `errorCount` and `available`)

**Recovery (Requires Reboot):**
- Upon boot, `performDs2484Scan()` rediscovery:
  - Resets errorCount = 0 for all sensors
  - Resets available = true for all discovered sensors
  - If sensor is reconnected, it's rediscovered and re-enabled

**Rationale:** Prevents continuous failed I2C attempts on permanently disconnected or broken sensors. Three-strike threshold balances error tolerance with fast failure detection.

**Evidence:**
- Ds18b20Sensor struct with errorCount field
- readDs18b20Results() function at line ~1408
- performDs2484Scan() initialization in setup() and Slot 7

**Confidence:** HIGH (fully implemented, verified via code review)

---

### BR-SC-009: Guaranteed Input Latency < 100 ms

**Rule:**  
Gamepad and Encoder input shall be read at high frequency (40–50 Hz) independent of sensor rotation, guaranteeing input response within 100 ms maximum.

**Timing Gates:**
```cpp
// Gamepad: 40 Hz (25 ms gate)
if ((int32_t)(now - nextGamepadMs) >= 0) {
  readGamepadAt40Hz();
  nextGamepadMs = now + 25;  // INPUT_REFRESH_MS ≤ 25 ms
}

// Encoder: 50 Hz (20 ms gate, CONTINUOUSLY — not page-conditional)
if ((int32_t)(now - nextEncoderMs) >= 0) {
  readEncoderAt50Hz();       // Always runs, not page-dependent
  nextEncoderMs = now + 20;  // 50 Hz continuous polling
}

// INA219: 50 kHz via micros()
if ((int32_t)(nowUs - nextInaSampleUs) >= 0) {
  readIna219At50Hz();
  if ((int32_t)(now - nextEncoderMs) >= 0) {
    readEncoderAt50Hz();
    nextEncoderMs = now + 20;  // INPUT_REFRESH_MS ≤ 20 ms
  }
}

// INA219: 50+ Hz (20 µs gate via micros)
if ((int32_t)(micros() - lastIna219Read) >= 20) {
  readIna219At50Hz();
  lastIna219Read = micros();
}
```

**Guarantees:**
- Maximum input latency: 200 ms (worst case: sensor block for 200 ms, then input reads next)
- Worst case: Button press at 0 ms + sensor read in progress (200 ms) + input gate (25 ms max) = 225 ms (still acceptable)
- Typical case: < 50 ms (gamepad read every 25 ms + display update < 20 ms)

**Independent Scheduling:**
- Sensor rotation: Slot-based (200 ms each)
- Input reads: Fixed frequency (25–20 ms gates)
- NO coupling between sensor and input schedules

**Rationale:** User cannot perceive latency > 200 ms; ensures responsive UI.

**Evidence:** 
- FEATURE-SPEC-006.md, Section "FR-1: Rotated Sensor Reading"
- Input timing gates in loop

**Confidence:** HIGH

---

### BR-SC-010: Configurable Stats Reporting (Per-Input & Per-Sensor)

**Rule:**  
Per-sensor read timing information shall be logged to Serial continuously, with aggregated input device statistics available via optional compile-time flag.

**Configuration:**
```cpp
#define ENABLE_STATS_REPORTING 1  // 1 = enabled (default), 0 = disabled (compile-time)
```

**Behavior When ENABLED (default):**

**Per-Sensor Timing (ALWAYS logged on every read):**
```
BME280 read time: 1963 µs
TSL2591 I2C read time: 294 µs (integration wait: 1000 ms)
SI1145 read time: 650 µs
VEML6075 read time: 873 µs
VEML6070 read time: 221 µs
SCD30 read time: 1635 µs
```

**Aggregated Input Device Stats (every 1000 ms):**

Three separate stat lines printed once per second, each formatted as:
```
<Device> stats (1000ms): reads=<count>, min=<min_µs> µs, max=<max_µs> µs, total=<total_µs> µs
```

Example output:
```
Gamepad stats (1000ms): reads=40, min=4800 µs, max=5234 µs, total=201456 µs
Encoder stats (1000ms):  reads=50, min=987 µs, max=1156 µs, total=50123 µs
INA219 stats (1000ms):  reads=50, min=1100 µs, max=1289 µs, total=61234 µs
```

Where:
- **reads:** Count of reads executed in last 1000 ms window
- **min:** Minimum single read duration (microseconds)
- **max:** Maximum single read duration (microseconds)
- **total:** Sum of all read durations (microseconds) — useful for CPU load analysis

**Data Structures:**
```cpp
struct GamepadStats {
  uint32_t minTime, maxTime, totalTime;
  uint16_t readCount;
};

struct EncoderStats {
  uint32_t minTime, maxTime, totalTime;
  uint32_t readCount;
};

struct Ina219Stats {
  uint32_t minTime, maxTime, totalTime;
  uint32_t readCount;
};

static GamepadStats gamepadStats = {};
static EncoderStats encoderStats = {};
static Ina219Stats ina219Stats = {};

static uint32_t lastGamepadStatsLogMs = 0;
static uint32_t lastEncoderStatsLogMs = 0;
static uint32_t lastIna219StatsLogMs = 0;
```

**Behavior When DISABLED:**

Per-sensor timing still logged (always on):
```
BME280 read time: 1963 µs
TSL2591 I2C read time: 294 µs (integration wait: 1000 ms)
```

Aggregated input device stats suppressed (no `Gamepad/Encoder/INA219 stats` output).

**Rationale:**
- Input device stats reveal performance bottlenecks: which device takes longest? Is variance acceptable?
- Per-read logs provide baseline diagnostics for debugging
- Combined stats (aggregated + individual) enable comprehensive performance profiling
- Production systems: Disable stats to reduce serial output noise
- Development: Enable stats for performance optimization

**Print Functions:**
```cpp
void printGamepadStats();   // Prints when (int32_t)(now - lastGamepadStatsLogMs) >= 1000
void printEncoderStats();   // Prints when (int32_t)(now - lastEncoderStatsLogMs) >= 1000
void printIna219Stats();    // Prints when (int32_t)(now - lastIna219StatsLogMs) >= 1000
```

All three functions called every loop iteration, but only print when timer expires and reset counters.

**Memory Impact:** Negligible (~90 bytes for three stat structs + timing variables)

**Evidence:** 
- Lines 184, 218-240, 1303-1373, 1389-1410 in src/main.cpp
- Per-read timing calls in `readBme280()`, `readSi1145()`, `readVeml6075()`, `readVeml6070()`, `readScd30()`, `readTsl2591Async()`, `readDs2484Temperatures()`
- Input timing aggregation in `readGamepadAt40Hz()`, `readEncoderAt50Hz()`, `readIna219At50Hz()`

**Confidence:** HIGH

---

### BR-SC-011: Display Rate Limiting (5 Hz Maximum)

**Rule:**  
Display refresh shall be rate-limited to maximum 5 Hz (200 ms minimum interval between redraws) to prevent flickering caused by rapid runtime value changes (e.g., INA219 seconds counter increments every second).

**Implementation:**
```cpp
static uint32_t nextLargeDisplayRefreshMs = 0;  // Rate limit timer

// In loop, display update only if:
if ((largeDisplayValueChanged() || valuesChanged() || (int32_t)(now - nextDisplayMs) >= 0) &&
    (int32_t)(now - nextLargeDisplayRefreshMs) >= 0) {  // Rate limit gate
  drawCurrentPage(false);
  updateCachedValues();
  nextDisplayMs = now + SENSOR_SLOT_INTERVAL_MS;
  nextLargeDisplayRefreshMs = now + 200;  // Reset timer for next window
}
```

**Trigger Conditions:**
Display updates when ANY of these conditions are true:
1. Page changed (forces full redraw immediately)
2. Sensor value changed (detected by `valuesChanged()` or `largeDisplayValueChanged()`)
3. Timer event: `(int32_t)(now - nextDisplayMs) >= 0` (SENSOR_SLOT_INTERVAL_MS = 200 ms)

BUT rate limit `nextLargeDisplayRefreshMs` MUST allow (>= 200 ms since last redraw).

**Behavior:**
- First condition met → update `nextLargeDisplayRefreshMs = now + 200`
- If second condition met within 200 ms → skip redraw (rate limited)
- At 200 ms boundary → redraw allowed again
- Result: Maximum 5 Hz (minimum 200 ms between redraws)

**Benefit:**
- Eliminates visual flicker on INA219 large display mode when runtime counter increments
- Reduces unnecessary TFT_eSPI SPI traffic and CPU load
- Display remains responsive (< 200 ms latency on user action)

**Evidence:**
- Lines 1894-1903 in src/main.cpp (rate limit logic)
- INA219 runtime changes every second, triggering display updates (guarded by rate limit)
- nextLargeDisplayRefreshMs tracking variable (line ~216)

**Confidence:** HIGH

---

### BR-SC-012: TSL2591 Optimized Read (Direct I2C Register Access)

**Rule:**  
TSL2591 light sensor shall use direct I2C register read instead of Adafruit library API to obtain accurate sensor read timing (excluding integration wait) and avoid blocking on library integration time.

**Implementation:**

Instead of Adafruit library call (blocking, includes 200 ms integration time):
```cpp
// ✗ OLD - BLOCKS FOR 200+ ms
uint32_t full = tsl2591.getFullLuminosity();
```

Use direct I2C register read:
```cpp
// ✓ NEW - NON-BLOCKING (~295 µs)
uint32_t startTime = micros();
Wire.beginTransmission(0x29);          // TSL2591 I2C address
Wire.write(0x14);                      // DATA0 register address
Wire.endTransmission();
Wire.requestFrom(0x29, 4);
uint16_t data0 = (Wire.read() << 8) | Wire.read();  // CHAN0 (visible + IR)
uint16_t data1 = (Wire.read() << 8) | Wire.read();  // CHAN1 (IR only)
uint32_t duration = micros() - startTime;

// Cache values
tsl2591Values.fullSpectrum = data0;      // CHAN0
tsl2591Values.infrared = data1;          // CHAN1
tsl2591Values.visible = data0 - data1;   // Calculated: visible = CHAN0 - CHAN1

Serial.printf("TSL2591 I2C read time: %lu µs (integration wait: %lu ms)\n", 
              duration, elapsedMs);
```

**TSL2591 I2C Protocol (Relevant Registers):**
- **I2C Address:** 0x29 (hardcoded)
- **DATA0 Register (0x14):** Contains CHAN0 data (lower 16 bits: visible light + IR)
- **DATA1 Register (0x15):** Contains CHAN1 data (lower 16 bits: IR only)
- Both are 16-bit values, read via 2-byte I2C burst

**Bitwise Value Extraction:**
```
data0 = full spectrum (visible + infrared)
data1 = infrared only
visible = data0 - data1  // Calculate visible by subtraction
```

**Measurement Results:**
- Actual I2C read time: ~295 µs (direct register access)
- Integration wait time: 1000 ms (200 ms sensor integration time) — handled asynchronously in FEATURE-SPEC-006 rotation
- Improvement: 241000 µs (library) → 295 µs (direct) = **812× faster** measurement

**Why Direct I2C Instead of Library?**
1. **Timing Accuracy:** Library call includes integration wait, masking actual I2C communication speed
2. **Non-Blocking:** Can read cached values without waiting for integration to complete
3. **Performance Profiling:** Accurate timing measurements needed for FEATURE-SPEC-006 (rotated sensor polling)
4. **Async Measurement:** Integration time handled separately in rotation slot 1 (start) → slot 7 (read)

**Rationale:**
- FEATURE-SPEC-006 requires accurate per-sensor read time measurement
- Adafruit library blocking behavior incompatible with non-blocking architecture
- Direct register access provides ~300 µs reads suitable for rotation slots

**Evidence:**
- Lines 877-933 in src/main.cpp (TSL2591 async read implementation)
- TSL2591 datasheet: Register 0x14 (DATA0), 0x15 (DATA1) definitions
- FEATURE-SPEC-006.md: Async measurement pattern (start in slot 1, read in slot 6+)

**Confidence:** HIGH

---

## Universal Large Display Mode Rules (All Pages) — **NEW (FEATURE-SPEC-007)**

### BR-SP-001: Large Display Mode Entry (All Pages)

**Rule:**  
Button B press on any sensor page (0–9) activates large display mode, starting at value index 0 for that page.

**Trigger:**  
Button B press (falling-edge detection) while on sensor page in standard view mode.

**Conditions:**  
- Current page is a sensor page (0–9)
- Not already in large display mode
- No page navigation in progress

**Behavior:**  
1. Set `largeDisplayMode.active = true`
2. Set `largeDisplayMode.currentPage = currentPage`
3. Set `largeDisplayMode.valueIndex = 0` (always start at first value)
4. Display immediately refreshes to show large display mode
5. Page navigation buttons (Y/A) disabled until mode exit

**Failure Behavior:**  
If sensor unavailable (error screen displayed):
- Button B press ignored; page remains in error view
- Mode not entered

**Evidence:**  
- Button handler: Mode-aware logic for Button B
- FEATURE-SPEC-007: Entry specifications for all 10 pages
- Global state: `largeDisplayMode` struct tracks active page and value index

---

### BR-SP-002: Large Display Mode Forward Value Cycling (Button B)

**Rule:**  
Button B press in large display mode advances value index to next value (with wrap-around from last → 0).

**Trigger:**  
Button B press while `largeDisplayMode.active = true`.

**Conditions:**  
- Large display mode currently active
- Multiple values available on current page (valueCount ≥ 2)

**Behavior:**  
1. Increment `largeDisplayMode.valueIndex` by 1
2. Wrap at boundary: `valueIndex = (valueIndex + 1) % valueCount`
3. Immediately retrieve new value from sensor-specific helper (no I2C call; cached data only)
4. Queue display update (subject to 200 ms rate limit)
5. Button response feels immediate; display update may lag up to 200 ms if just refreshed

**Value Count per Page:**  
- Page 0 (BME280): 7 values
- Page 1 (Gamepad): 4 values
- Page 2 (INA219): 7 values
- Page 3 (Encoder): 5 values
- Page 4 (SCD30): 3 values
- Page 5 (SI1145): 4 values
- Page 6 (TSL2591): 4 values
- Page 7 (VEML6070): 3 values
- Page 8 (VEML6075): 6 values
- Page 9 (DS2484): 3–9 values (3 per discovered 1-Wire sensor, max 3 sensors)

**Evidence:**  
- Button handler: Context-sensitive B button logic
- Per-page value definitions: Documented in docs/07-ui.md

---

### BR-SP-003: Large Display Mode Reverse Value Cycling (Button X)

**Rule:**  
Button X press in large display mode decrements value index to previous value (with wrap-around from 0 → last).

**Trigger:**  
Button X press while `largeDisplayMode.active = true`.

**Conditions:**  
- Large display mode currently active
- Multiple values available on current page (valueCount ≥ 2)

**Behavior:**  
1. Decrement `largeDisplayMode.valueIndex` by 1 (with modulo wrap)
2. Safe calculation: `valueIndex = (valueIndex - 1 + valueCount) % valueCount`
3. Immediately retrieve new value from sensor-specific helper (no I2C call; cached data only)
4. Queue display update (subject to 200 ms rate limit)
5. Button response feels immediate; display update may lag up to 200 ms if just refreshed

**Evidence:**  
- Button handler: Context-sensitive X button logic
- Modulo arithmetic: Safe wrap-around using addition before subtraction

---

### BR-SP-004: Large Display Mode Exit (Button Y)

**Rule:**  
Button Y press in large display mode exits mode and returns to standard page view.

**Trigger:**  
Button Y press while `largeDisplayMode.active = true`.

**Conditions:**  
- Large display mode currently active

**Behavior:**  
1. Set `largeDisplayMode.active = false`
2. Set `largeDisplayMode.valueIndex = 0` (reset for next entry)
3. Page remains on same sensor page (no navigation)
4. Display immediately refreshes to show standard page view
5. Page navigation buttons (Y/A) re-enabled

**Trade-off:**  
- Y button repurposed in large display mode (exit) vs. standard mode (previous page navigation)
- Users cannot navigate pages while in large display mode; must exit first

**Evidence:**  
- Button handler: Mode-aware Y button logic (context-sensitive)
- Page navigation disabled: Documented as constraint in FEATURE-SPEC-007

---

### BR-SP-005: Button A Inactive in Large Display Mode

**Rule:**  
Button A has no function in large display mode (remains inactive).

**Trigger:**  
Button A press while `largeDisplayMode.active = true`.

**Behavior:**  
Button A press is ignored; no action taken.

**Rationale:**  
- Button A reserved for future use or consistency with other input devices
- Large display mode navigation: B (next) + X (prev) + Y (exit) only
- No need for "next page" in large display mode

**Evidence:**  
- Button handler: No branch for Button A in large display mode
- FEATURE-SPEC-007: Input specification for all pages

---

### BR-SP-006: Auto-Exit on Page Navigation (Y/A in Standard Mode)

**Rule:**  
If user navigates to a different page (Y or A press in standard mode) while another page is in large display mode, that mode automatically exits.

**Trigger:**  
Button Y or A press in standard page view.

**Conditions:**  
- Large display mode is active (on any page)
- User navigates to different page

**Behavior:**  
1. Current page Y/A button advances to next/previous page
2. `largeDisplayMode.active = false` (auto-exit)
3. `largeDisplayMode.valueIndex = 0` (reset)
4. New page renders in standard view (not large display)
5. User can then press B on new page to enter large display

**Edge Case:**  
If user presses B to enter large display on page 0, then presses Y (prev page), mode exits and user navigates to page 9 in standard view.

**Evidence:**  
- Page navigation logic: Checks `largeDisplayMode.active` before navigating
- State reset: Ensures clean transition between pages

---

### BR-SP-007: Display Rate Limiting in Large Display Mode

**Rule:**  
Large display mode respects the same 200 ms display rate limit (5 Hz maximum) as standard page rendering.

**Trigger:**  
Display update requested after value index change or page change.

**Behavior:**  
1. User presses B/X to change value index → valueIndex updated immediately (no rate limit)
2. Page renderer calls `renderLargeDisplay(pageIndex, valueIndex)` → calls sensor-specific helper to get label/value/unit/color
3. Display update queued but subject to rate limit gate: `(int32_t)(now - nextLargeDisplayRefreshMs) >= 0`
4. If rate limit not met: Display not redrawn yet; but helper data remains ready for next refresh cycle
5. Display eventually updates within 200 ms

**Trade-off:**  
- Button presses feel responsive (immediate valueIndex change)
- Visual display update may lag up to 200 ms (acceptable for user experience)
- Rate limit prevents excessive SPI traffic and CPU load

**Configuration:**  
```cpp
static uint32_t nextLargeDisplayRefreshMs = 0;  // Rate limit timer (shared with standard pages)
```

**Evidence:**  
- Display loop logic: Rate limit gate enforced before `tft.begin()` SPI operations
- FR-015: Display rate limiting applies to both standard and large display modes

---

### BR-SP-008: Unified Renderer for All Pages

**Rule:**  
Large display rendering uses single `renderLargeDisplay()` function for all pages (0–9), with page-specific helper function providing label/value/unit/color data.

**Trigger:**  
Page render cycle when `largeDisplayMode.active = true`.

**Behavior:**  
1. `renderCurrentPage()` detects large display mode and calls `renderLargeDisplay(currentPage, largeDisplayMode.valueIndex)`
2. `renderLargeDisplay()` performs layout operations:
   - Clear display
   - Draw title (sensor name), Font 4, Cyan, centered
   - Draw label (value name), Font 4, Cyan, centered
   - Draw value (numeric/text), Font 8, color-coded, centered
   - Draw unit, Font 2, White, centered
   - Draw footer (\"B:next X:prev Y:exit\"), Font 1, Dark Grey, centered
3. For data retrieval, `renderLargeDisplay()` calls sensor-specific helper:
   ```cpp
   String value = sensorType_getLargeDisplayData(valueIndex, &label, &unit, &color);
   ```
4. Helper retrieves data from global cache struct (BME280Values, INA219Values, etc.) — no I2C calls
5. Helper returns all four pieces of information for `renderLargeDisplay()` to use

**Per-Page Helper Functions:**  
Each sensor page defines one combined helper function (not four separate):
- `bme280_getLargeDisplayData(index, &label, &unit, &color)` → returns value string
- `gamepad_getLargeDisplayData(index, &label, &unit, &color)` → returns value string
- `ina219_getLargeDisplayData(index, &label, &unit, &color)` → returns value string (already exists)
- ... (similar for pages 3–9)

**Code Organization:**  
- Unified renderer: Single function `renderLargeDisplay()` in main.cpp display section
- Per-page helpers: Defined immediately after standard page renderer (e.g., after `pageBme280()`)

**Benefit:**  
- No code duplication in layout rendering
- Easy to add new pages: define one helper + register in `renderLargeDisplay()` switch statement
- Memory efficient: Single renderer + compact helper functions

**Evidence:**  
- FEATURE-SPEC-007 clarification: \"Unified rendering system\"
- Implementation approach: Encapsulation of display layout logic in single function

---

### BR-SP-009: Large Display Mode and Sensor Unavailable

**Rule:**  
If sensor is unavailable (error screen displayed), Button B press is ignored; large display mode cannot be entered.

**Trigger:**  
Button B press on page with unavailable sensor (error screen showing \"Sensor nicht vorhanden\").

**Behavior:**  
1. Button B press detected
2. Check `available.sensor` flag → false (sensor unavailable)
3. Button press ignored; no mode transition
4. Page remains in error view
5. User cannot cycle error message with B/X buttons

**Rationale:**  
- Error screen is priority; user must address sensor issue first
- Large display mode meaningless without data to display

**Evidence:**  
- Button handler: Guard check on `available.sensor` before enabling large display
- Page rendering: Error screen shown before standard/large display logic executes

---

### BR-SP-010: DS2484 Large Display Mode (Max 3 Sensors)

**Rule:**  
DS2484 1-Wire adapter (Page 9) supports large display mode with up to 3 discovered temperature sensors, showing 3–9 values (3 per sensor: ROM-ID, Temperature, Status).

**Details:**  
- **Sensor discovery:** At boot, DS2484 performs 1-Wire bus scan to find DS18B20/DS1822/DS1820 sensors
- **Max sensors:** 3 sensors (hardware limit of this adapter/1-Wire configuration)
- **Values per sensor:**
  1. ROM-ID: 8-byte hexadecimal address (e.g., \"28:AB:CD:12:34:56:78:9F\")
  2. Temperature: Current reading (e.g., \"22.50°C\") or \"Disconnected\" if not responding
  3. Status: Connection status (\"OK\", \"CRC Error\", \"Not Responding\")
- **Total value count:** 3 × (discovered sensor count) = 3, 6, or 9 values
- **Initial index:** 0 (always ROM-ID of first discovered sensor)
- **Cycling behavior:** B/X buttons cycle through all 3–9 values in order

**Sensor Discovery Example:**  
```
Boot → DS2484 scan → Found 2 DS18B20 sensors
  Sensor 0: 28:AB:CD:12:34:56:78:9F (temp: 22.50°C, status: OK)
  Sensor 1: 28:11:22:33:44:55:66:77 (temp: 19.30°C, status: OK)
Values available: 6 (3 × 2 sensors)
```

**Helper Function:**  
```cpp
String ds2484_getLargeDisplayData(uint8_t valueIndex, 
                                  const char** outLabel,
                                  const char** outUnit,
                                  uint16_t* outColor)
{
  uint8_t sensorNum = valueIndex / 3;    // Which sensor (0, 1, 2)
  uint8_t fieldNum = valueIndex % 3;     // Which field (0=ROM-ID, 1=Temp, 2=Status)
  
  // Retrieve from ds2484 cache struct
  // Return appropriate label/unit/color based on fieldNum
}
```

**Behavior on Discovery Failure:**  
- If no 1-Wire sensors found: Page displays error (\"Keine Sensoren gefunden\")
- Large display mode cannot be entered
- Button B press ignored

**Evidence:**  
- FEATURE-SPEC-007 clarification: \"Limit DS2484 to max 3 sensors\"
- DS2484 address: 0x18 (hardcoded)
- 1-Wire discovery: `performDs2484Scan()` and `readDs2484Temperatures()` functions

---

### BR-034: SCD30 ASC Status Display — **NEW (FEATURE-SPEC-009)**

**Description:**  
The SCD30 page (Page 4) displays the current Automatic Self Calibration (ASC) status in the header area (top-right corner) using color-coded text. ASC status is queried at system startup (~500ms after SCD30 START command) and updated on demand after user toggles.

**Trigger:**  
- System boot: ASC status query issued in non-blocking sensor polling loop
- User interaction: Y button press on SCD30 page triggers ASC toggle and subsequent verification query
- Display refresh: Every 500 ms, page renderer calls `scd30_getAscStatus()` to get current display string

**Conditions:**  
- SCD30 sensor is present (`available.scd30 == true`)
- Page 4 (SCD30) is currently displayed
- Large Display mode is NOT active (or is active on a different page)

**Behavior:**  
1. **Display String:**
   - If `scd30State.ascStatusValid && scd30State.ascEnabled`: Display `"ASC: ON"` in green (TFT_GREEN)
   - If `scd30State.ascStatusValid && !scd30State.ascEnabled`: Display `"ASC: OFF"` in orange (TFT_ORANGE)
   - If `!scd30State.ascStatusValid`: Display `"ASC: --"` in dark grey (TFT_DARKGREY)

2. **Placement:** Top-right corner of page header (x=474 right-aligned, y=8)

3. **Font:** Small (setTextSize(1)) to fit header without crowding title

4. **Update Frequency:** Every display refresh cycle (500 ms for sensor pages)

5. **Failure Handling:**
   - If ASC query fails (I2C error, CRC invalid): `ascStatusValid = false`; display "ASC: --"
   - Display does not crash; page remains responsive
   - Retries next polling cycle (~500 ms later)

**Evidence:**  
- Feature Specification: FEATURE-SPEC-009
- Function: `scd30_getAscStatus()` returns "ON"/"OFF"/"--"
- Caller: `pageScd30()` renderer

**Scope:** Page 4 (SCD30) only. No other pages display ASC status.

**Confidence:** HIGH (defined in FEATURE-SPEC-009)

---

### BR-035: SCD30 ASC Toggle via Gamepad X Button — **NEW (FEATURE-SPEC-009)**

**Description:**  
When the user presses the Gamepad X button while viewing the SCD30 page (Page 4) in normal (non-Large-Display) mode, the system sends an I2C SET ASC command to toggle the ASC state (ON ↔ OFF). After the toggle, the system re-queries the ASC status within the next sensor polling cycle to verify the actual state and detect any errors.

**Trigger:**  
- Gamepad X button press (falling edge: `xPressed && !lastX`)
- Current page is 4 (SCD30)
- Large Display mode is NOT active on SCD30 page

**Conditions:**  
- SCD30 sensor is present (`available.scd30 == true`)
- I2C bus is operational
- Previous toggle command (if any) has completed (not pending)

**Behavior:**  
1. **Optimistic State Update:** Immediately set `scd30State.ascEnabled = !scd30State.ascEnabled`
   - Rationale: Provide immediate visual feedback to user
   - Display updates to show new state (green/orange)

2. **I2C SET ASC Command:** Send wire transmission with:
   - Command bytes: 0x53 0xDA
   - Data bytes: 0x00, then (0x01 if enabling, 0x00 if disabling)
   - CRC byte: Calculated via `scd30Crc([0x00, data_low])`

3. **Set Pending Flag:** `scd30State.pendingAscWrite = true` during transmission; clear after completion

4. **Debounce:** Ignore X button presses for at least 100 ms after last toggle command sent
   - Rationale: Prevents accidental repeated toggles from button bounce or user holding button
   - Prevents I2C bus flooding

5. **Verification Query:** Schedule ASC status query (~500 ms after toggle) to verify actual state
   - If query shows expected state: display reflects reality, no action
   - If query shows different state: log warning, update display to actual state (eventual consistency wins)

6. **Non-Blocking:** ASC toggle and query must NOT block display refresh or sensor polling

**Failure Handling:**  
- **I2C Write Fails:** Return false; revert optimistic state update; display prior state; user can retry toggle
- **Query Fails After Successful Write:** Log warning; display "ASC: --" (unknown); retry next polling cycle
- **Query Succeeds but State Differs:** Log warning (mismatch); update display to actual state

**Error Recovery:**  
User can attempt toggle again immediately (after 100 ms debounce).

**Evidence:**  
- Feature Specification: FEATURE-SPEC-009
- Function: `setAsc(bool enable)` sends I2C command; `getAscStatus(bool&)` verifies
- Input handling: gamepad button edge detection in main loop (lines 2370–2430)

**Scope:** Page 4 (SCD30) only, in normal page view (not Large Display mode).

**Confidence:** HIGH (defined in FEATURE-SPEC-009)

---

### BR-036: SCD30 ASC Toggle NOT Available in Large Display Mode — **NEW (FEATURE-SPEC-009)**

**Description:**  
When Large Display Mode is active on the SCD30 page (Page 4), the Y button behavior reverts to the standard Large Display mode exit function (BR-031). ASC toggle is disabled in Large Display mode. Users must exit Large Display mode to toggle ASC.

**Trigger:**  
- Gamepad Y button press while in Large Display mode on Page 4

**Conditions:**  
- `largeDisplayMode.active == true`
- `largeDisplayMode.currentPage == 4` (SCD30)

**Behavior:**  
1. Exit Large Display mode (same as BR-031)
2. Do NOT send ASC toggle command
3. Display ASC status on Page 4 header (BR-034)
4. User can now press Y again (in normal mode) to toggle ASC

**Rationale:**  
- Large Display Mode is for focused viewing, not sensor control
- Prevents accidental ASC toggles while viewing parameter in enlarged format
- Consistent with existing Large Display mode design (view-only)

**Evidence:**  
- Feature Specification: FEATURE-SPEC-009 (notes Large Display mode unchanged)
- Input handling: Check `largeDisplayMode.active` before processing ASC toggle

**Scope:** Page 4 (SCD30) only, in Large Display mode.

**Related:** BR-031 (Large Display Mode Exit), BR-035 (ASC Toggle in Normal Mode)

**Confidence:** HIGH (defined in FEATURE-SPEC-009)

---

## Ambiguities & [UNKNOWN] Items

| Item | Impact | Note |
|------|--------|------|
| Gamepad seesaw firmware version | BR-003, BR-019 | Assumed ≥3.x; not checked at runtime |
| I2C timeout values | BR-002 | Wire library internal timeouts [UNKNOWN] |
| TSL2591 GAIN_MED and INTEGRATIONTIME_200MS exact values | BR-026 | Enums from Adafruit library; not defined in firmware |
| VEML6075 config register 0x00 meaning | BR-024 | Hardcoded value 0x0000; exact effect unknown without datasheet |
| INA219 200 ms gap threshold rationale | BR-022 | Empirically chosen; no documentation of origin |
| Encoder delta per mechanical tick | BR-011 | Assumed ±1 per tick; actual hardware behavior unknown |

