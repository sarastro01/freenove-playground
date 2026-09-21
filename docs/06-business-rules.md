# Business Rules: sensor-pages-dashboard

**Last Updated:** 2026-09-21  
**Firmware Type:** Embedded bare-metal  
**Business Domain:** Sensor data acquisition, display, input handling

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

---

### BR-008: DS18B20 Async Non-Blocking Read Cycle (100 ms)

**Description:**  
DS18B20 temperature sensors use a 3-stage non-blocking async state machine to avoid blocking the main loop during 95+ ms conversion time. Each sensor triggers a conversion request, then waits 100 ms before fetching results.

**Trigger:**  
Every loop iteration; `updateDs18b20()` called continuously.

**Conditions:**  
- DS18B20 devices detected at boot (`ds18b20Values.count > 0`)
- Non-blocking mode enabled at boot: `ds18b20Sensors.setWaitForConversion(false)`

**Behavior:**  

**Stage 1: Request (pending=false)**
```cpp
if (!ds18b20ConversionPending) {
  ds18b20Sensors.requestTemperatures();  // Broadcast conversion start
  ds18b20ConversionStartedMs = millis();
  ds18b20ConversionPending = true;
  return;  // Non-blocking: return immediately
}
```

**Stage 2: Wait (pending=true, elapsed < 100 ms)**
```cpp
if (millis() - ds18b20ConversionStartedMs < 100) 
  return;  // Still waiting: return without doing anything
```

**Stage 3: Fetch (pending=true, elapsed ≥ 100 ms)**
```cpp
for (uint8_t index = 0; index < ds18b20Values.count; index++) {
  ds18b20Values.temperatures[index] = ds18b20Sensors.getTempC(ds18b20Values.addresses[index]);
}
ds18b20ConversionPending = false;  // Reset for next cycle
```

**Cycle Duration:** Exactly 100 ms wait (hardcoded; not configurable).

**Resolution Config:** 9-bit resolution (set at boot: `ds18b20Sensors.setResolution(9)`), which has theoretical max ~95 ms conversion. Empirical 100 ms wait chosen to ensure completion.

**Failure Behavior:**  
- If device disconnects mid-conversion, `getTempC()` returns sentinel value (~–127 °C)
- If `requestTemperatures()` fails (I2C error), no error indication; stale temps persist
- If any `getTempC()` fails, that device's temperature not updated

**Evidence:**  
- `updateDs18b20()` lines 308–329
- Async state machine logic: lines 311–328
- Config at boot: line 617 (`ds18b20Sensors.setWaitForConversion(false)`)

**Confidence:**  
HIGH

---

### BR-009: DS18B20 Hot-Plug Detection (3-Second Rescan)

**Description:**  
The DS18B20 1-Wire bus is rescanned every 3000 ms to detect added/removed devices. Device list change detection uses `memcmp()` on ROM addresses. If device list changes and current page is DS18B20 (page 1), display is immediately redrawn.

**Trigger:**  
Every 3000 ms (gate: `(int32_t)(now - nextDs18b20RescanMs) >= 0`); called in main loop.

**Conditions:**  
- `rescanDs18b20()` runs regardless of device presence
- Compares new device list to previous

**Behavior:**  
```cpp
bool rescanDs18b20() {
  ds18b20Sensors.begin();  // Re-enumerate bus
  const uint8_t newCount = min((uint8_t)ds18b20Sensors.getDeviceCount(), (uint8_t)MAX_DS18B20_SENSORS);
  
  DeviceAddress newAddresses[MAX_DS18B20_SENSORS];
  for (uint8_t index = 0; index < newCount; index++) {
    ds18b20Sensors.getAddress(newAddresses[index], index);
  }
  
  // Detect change via count or ROM address comparison
  const bool changed = newCount != ds18b20Values.count ||
    memcmp(newAddresses, ds18b20Values.addresses, newCount * sizeof(DeviceAddress)) != 0;
  
  if (!changed) return false;  // No change: return false
  
  // Change detected: update and reset state
  ds18b20Values.count = newCount;
  memcpy(ds18b20Values.addresses, newAddresses, newCount * sizeof(DeviceAddress));
  memset(ds18b20Values.temperatures, 0, sizeof(ds18b20Values.temperatures));  // Zero temps
  ds18b20ConversionPending = false;  // Reset async state machine
  return true;  // Signal change occurred
}

// In loop():
if ((int32_t)(now - nextDs18b20RescanMs) >= 0) {
  if (rescanDs18b20() && currentPage == 1) 
    drawCurrentPage(true);  // Full redraw if on page 1
  nextDs18b20RescanMs = now + DS18B20_RESCAN_MS;
}
```

**Side Effects on Device Change:**
- Device count updated
- ROM addresses updated
- All temperatures zeroed (freshness reset)
- Async conversion state reset to `pending=false`
- If on DS18B20 page: full page redraw (header + data)

**Device Limit:** Max 8 devices (`MAX_DS18B20_SENSORS`); excess silently capped by `min()`.

**Failure Behavior:**  
- If 1-Wire bus enumeration fails, previous device list retained (no error indicated)
- If device becomes unreachable but still responds to ROM query, it's retained in list
- Removed device temperatures persist in array (hidden by `count` field, not displayed)

**Evidence:**  
- `rescanDs18b20()` lines 295–313
- Loop gate lines 666–671: rescan and page redraw logic
- Constant: `DS18B20_RESCAN_MS = 3000` (line 48)

**Confidence:**  
HIGH

---

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

**Page Count:** 10 (indices 0–9): BME280, DS18B20, Gamepad, INA219, Encoder, SCD30, SI1145, TSL2591, VEML6070, VEML6075

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
- Page count line 47: `constexpr uint8_t PAGE_COUNT = 10;`

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

### BR-014: Temperature Sentinel Detection (DS18B20 Disconnect)

**Description:**  
DS18B20 sensor reports disconnect via sentinel value (approximately –127 °C, exact value per DallasTemperature library). Page renderer checks for this sentinel and displays "disconnected" in red text instead of temperature.

**Trigger:**  
DS18B20 page rendered; temperature values from struct examined.

**Conditions:**  
- DS18B20 page (page 1) is current page
- Device temperature ≤ `DEVICE_DISCONNECTED_C` sentinel

**Behavior:**  
```cpp
void pageDs18b20() {
  // ...
  for (uint8_t index = 0; index < ds18b20Values.count; index++) {
    const float tempC = ds18b20Values.temperatures[index];
    const bool disconnected = tempC <= DEVICE_DISCONNECTED_C;
    row(100 + index * 24, label.c_str(), 
        disconnected ? "disconnected" : String(tempC, 2) + " C", 
        disconnected ? TFT_RED : TFT_GREEN);
  }
  // ...
}
```

**Sentinel Value:** `DEVICE_DISCONNECTED_C` is a library-defined constant (~–127 °C). [UNKNOWN - exact value depends on DallasTemperature library version]

**Display Behavior:**  
- Temperature ≤ sentinel → red "disconnected" label
- Temperature > sentinel → green value in °C (2 decimal places)

**Failure Behavior:**  
- If device truly disconnected but returns legitimate –100 °C (edge case), still shows "disconnected" [ambiguous]
- No recovery mechanism; device remains disconnected until rescan + reconnect

**Evidence:**  
- `pageDs18b20()` lines 465–479
- Sentinel check line 473: `const bool disconnected = tempC <= DEVICE_DISCONNECTED_C;`
- [UNKNOWN] Exact sentinel value: not defined in firmware; from DallasTemperature library

**Confidence:**  
MEDIUM (depends on library version)

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
  const char *titles[] = {"BME280", "DS18B20", "Gamepad QT", "INA219", "Rotary Encoder", "SCD30", "SI1145", "TSL2591", "VEML6070", "VEML6075"};
  
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
Encoder only reads when `currentPage == 4 || currentPage != lastPage` (lines 637–641). This means encoder position only updates when viewing page 4 or during page transitions. If on another page, encoder state is stale.

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

### BR-018: Encoder Read Gate (Page 4 Only, Unless Page Changing)

**Description:**  
Encoder is only read when on Encoder page (page 4) OR when page is changing. If on any other page, encoder read is skipped, leaving position, button, and LED state stale.

**Trigger:**  
Main loop cycle; encoder read timing gate.

**Conditions:**  
- Encoder detected at boot
- Current page is 4 OR `currentPage != lastPage` (page changing)
- Time gate fires (every 20 ms)

**Behavior:**  
```cpp
if (available.encoder && currentPage == 4 &&
    ((int32_t)(now - nextEncoderMs) >= 0 || currentPage != lastPage)) {
  readEncoderAt50Hz();
  nextEncoderMs = now + INPUT_REFRESH_MS;
}
```

**Implication:**  
- While on page 2 (Gamepad), encoder position is not updated
- While on page 5 (SCD30), encoder button presses are not detected
- When leaving page 4, one final read occurs (due to `currentPage != lastPage` condition)

**Rationale:** Power/performance optimization; encoder is only meaningful on page 4.

**Failure Behavior:**  
- User presses encoder button on page 3 (INA219) → button press ignored; LED doesn't change; press count stale
- User rotates encoder dial on page 0 (BME280) → LED brightness doesn't update until navigating back to page 4

**Evidence:**  
- Loop lines 637–641
- Condition: `currentPage == 4 && (gate || pageChanged)`

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
- Page display line 506: `row(268, "Last event:", gamepadEventText(), TFT_MAGENTA);`

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

### BR-023: DS18B20 Conversion Wait (100 ms Fixed)

**Description:**  
After requesting DS18B20 temperature conversion, firmware waits exactly 100 ms before fetching results. Conversion duration depends on resolution (9-bit ~95 ms, 12-bit ~750 ms). Firmware uses fixed 9-bit resolution; 100 ms ensures completion even with hardware delays.

**Trigger:**  
`updateDs18b20()` enters wait phase (pending=true).

**Conditions:**  
- DS18B20 devices detected
- Conversion request issued
- Time elapsed since request < 100 ms

**Behavior:**  
```cpp
if (!ds18b20ConversionPending) {
  ds18b20Sensors.requestTemperatures();  // Start conversion
  ds18b20ConversionStartedMs = millis();
  ds18b20ConversionPending = true;
  return;  // Non-blocking
}

if (millis() - ds18b20ConversionStartedMs < 100) {  // Wait 100 ms
  return;  // Keep waiting
}

// After 100 ms: fetch
for (uint8_t index = 0; index < ds18b20Values.count; index++) {
  ds18b20Values.temperatures[index] = ds18b20Sensors.getTempC(ds18b20Values.addresses[index]);
}
ds18b20ConversionPending = false;
```

**Boot-Time Config:**
```cpp
ds18b20Sensors.setResolution(9);  // 9-bit: ~95 ms conversion
```

**Rationale:**  
- 9-bit chosen for speed (95 ms < 100 ms margin)
- 100 ms wait chosen empirically; allows margin for clock drift, ISR delays
- Not adjustable; hardcoded constant

**Failure Behavior:**  
- If fetch issued before conversion done (< 95 ms), sensor returns garbage or 0
- If 100 ms timer drifts, may fetch early/late (clock-dependent accuracy)
- No timeout; if conversion never completes, stuck in waiting state indefinitely

**Evidence:**  
- `updateDs18b20()` lines 311–328
- Boot config line 617: `ds18b20Sensors.setResolution(9);`
- Constant lines 315–316: 100 ms hardcoded

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
| BR-008 | DS18B20 | Async non-blocking read state machine (100ms) | HIGH |
| BR-009 | DS18B20 | Hot-plug detection (3-sec rescan) | HIGH |
| BR-010 | Encoder | LED color cycling (4-state) | HIGH |
| BR-011 | Encoder | Dial brightness adjustment (delta×8, clamp 0-255) | HIGH |
| BR-012 | Gamepad | Button page navigation (edge detect, modulo wrap) | HIGH |
| BR-013 | Gamepad | Joystick analog values inverted | HIGH |
| BR-014 | DS18B20 | Temperature sentinel detection (disconnect) | MEDIUM |
| BR-015 | Display | Page redraw only on change (header skip) | HIGH |
| BR-016 | Display | Page-specific refresh rates (20ms live, 500ms slow) | HIGH |
| BR-017 | Display | Sensor unavailable error screen | HIGH |
| BR-018 | Encoder | Encoder read gate (page 4 only, unless page changing) | HIGH |
| BR-019 | Input | Gamepad button active-low encoding | HIGH |
| BR-020 | Input | Gamepad keypad events (debug only, not nav) | HIGH |
| BR-021 | Timing | Time-gate rollover safety (signed comparison) | HIGH |
| BR-022 | INA219 | Accumulation time-gap sanity check (200ms max) | HIGH |
| BR-023 | DS18B20 | Conversion wait fixed 100ms | HIGH |
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
| BR-008 | Async State Machine | Integration | HIGH | Functional | Verify 100ms wait, conversion completion, non-blocking behavior |
| BR-009 | Hot-Plug Detection | Integration | HIGH | Functional | Connect/disconnect devices during runtime; verify detection within 3s |
| BR-010 | LED Color Cycle | Unit | MEDIUM | Functional | Test modulo-4 cycling; verify color sequence |
| BR-011 | LED Brightness | Unit | MEDIUM | Functional | Test delta×8 formula; verify clamping to 0–255 |
| BR-012 | Page Navigation | Unit | HIGH | Functional | Test falling-edge detection; verify modulo wrap 0–9 |
| BR-013 | Joystick Inversion | Unit | LOW | Implementation | Inversion formula 1023–raw; verify on display |
| BR-014 | Disconnect Sentinel | Integration | MEDIUM | Functional | Verify sentinel value triggers "disconnected" display |
| BR-015 | Page Redraw Gate | Integration | MEDIUM | Implementation | Full redraw when page changes; data-only otherwise |
| BR-016 | Refresh Rates | Integration | MEDIUM | Functional | Verify 20ms (live) vs 500ms (slow) refresh rates |
| BR-017 | Error Display | Integration | MEDIUM | Functional | Navigate to missing sensor; verify error screen renders |
| BR-018 | Encoder Gate | Integration | LOW | Implementation | Encoder reads only on page 4; stale state on other pages acceptable |
| BR-019 | Active-Low Encoding | Unit | LOW | Implementation | Button bit inversion via `!(state & mask)` |
| BR-020 | Keypad Events | Integration | LOW | Implementation | Events captured but not used for navigation; debug only |
| BR-021 | Rollover Safety | Unit | CRITICAL | Functional | Timer wrap-around safety via signed comparison; must pass after 49 days or 71 min |
| BR-022 | Gap Sanity Check | Unit | HIGH | Functional | Verify 200ms gap threshold; gaps >200ms skipped in accumulation |
| BR-023 | DS18B20 Wait | Integration | MEDIUM | Implementation | 100ms wait empirically chosen; test with real devices |
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
| **BR-008** Async State | `test_ds18b20_async_complete()` | DS18B20 connected | Request conversion; wait 100ms | Temperature updated | Stuck in pending state; timeout |
| **BR-009** Hot-Plug | `test_ds18b20_new_device_detected()` | 1 device connected; add 2nd mid-run | Wait 3s after connect | Device count increases | Device not detected |
| **BR-009** Hot-Plug | `test_ds18b20_removed_device_detected()` | 2 devices connected; remove 1 | Wait 3s after disconnect | Device count decreases | Device not removed from list |
| **BR-014** Disconnect | `test_ds18b20_disconnect_sentinel()` | DS18B20 with device removed | Display page 1 | "disconnected" in red | Garbage temperature displayed |
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
| **BR-023** Wait | `test_ds18b20_actual_conversion_time()` | DS18B20 connected | Time from request to completion | <100ms elapsed | Conversion incomplete; timeout |

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
- BR-008: Async state machine (real-time requirement; don't block)
- BR-009: Hot-plug detection (dynamic device discovery)
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
| BR-008 | Conversion timeout (>100ms) | Force 1-Wire busy for >100ms; verify timeout handling |
| BR-009 | Max devices (8) | Connect 10 DS18B20s; verify count capped at 8 |
| BR-012 | Rapid button presses | Press Y/A 20 times/second; verify no double navigation |
| BR-014 | Temp ≤ -127 °C | Test boundary; sentinel value vs. legitimate cold temps |
| BR-021 | Timing gate at exact rollover | Set deadline = 0xFFFFFFFF; verify gate fires at wrap |
| BR-022 | Gap exactly 200ms | Elapsed=200000µs; verify on boundary; then 200001µs (should skip) |

#### Negative Test Cases (Expected Failures)

| Rule | Negative Case | Expected Behavior |
|------|---------------|--------------------|
| BR-001 | Missing sensor at boot | available[].X=false; page shows error screen |
| BR-002 | SCD30 CRC corruption | Entire read rejected; stale values persist |
| BR-003 | Gamepad not detected | Page navigation disabled; lock at page 0 |
| BR-005 | SCD30 I2C timeout | Timeout handled gracefully; stale data displayed |
| BR-008 | 1-Wire bus error | Async wait continues; potential hang if error permanent |
| BR-014 | Device truly cold (–100 °C) | Displays "disconnected" (ambiguous; legitimate cold reading) |
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

## Ambiguities & [UNKNOWN] Items

| Item | Impact | Note |
|------|--------|------|
| Gamepad seesaw firmware version | BR-003, BR-019 | Assumed ≥3.x; not checked at runtime |
| I2C timeout values | BR-002 | Wire library internal timeouts [UNKNOWN] |
| TSL2591 GAIN_MED and INTEGRATIONTIME_200MS exact values | BR-026 | Enums from Adafruit library; not defined in firmware |
| VEML6075 config register 0x00 meaning | BR-024 | Hardcoded value 0x0000; exact effect unknown without datasheet |
| INA219 200 ms gap threshold rationale | BR-022 | Empirically chosen; no documentation of origin |
| Encoder delta per mechanical tick | BR-011 | Assumed ±1 per tick; actual hardware behavior unknown |

