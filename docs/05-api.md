# API & External Interfaces: sensor-pages-dashboard

**Last Updated:** 2026-10-23  
**Project Type:** Embedded Firmware (No HTTP/REST APIs)  
**Related Specs:** FEATURE-SPEC-012 (Logging & Diagnostics), FEATURE-SPEC-013 (Boot Logging & System Info)

---

## Executive Summary

**This is NOT a web service.** There are **NO HTTP endpoints, REST APIs, or RPC services.**

The sensor-pages-dashboard is a **bare-metal embedded firmware** running on ESP32. All communication is **hardware-level device protocols**:

| Protocol | Role | Devices | Direction |
|----------|------|---------|-----------|
| **I2C** | Multi-master sensor/controller bus | ESP32 ↔ 10 I2C devices (9 direct + 1 adapter) | Bidirectional |
| **1-Wire** | Single-wire temperature bus (via I2C adapter) | ESP32 ↔ DS2484 ↔ DS18B20 sensors | Bidirectional |
| **SPI** | Display graphics bus | ESP32 ↔ TFT display | Unidirectional (write-only) |
| **Serial UART** | Debug output only | ESP32 → Host (USB) | Unidirectional (write-only) |

**No application-level APIs exist.**

---

## 0. Sensor Data Reading Functions (Unified Caching Architecture) — **NEW (FEATURE-SPEC-005)**

### Overview

Three new sensor read functions implement unified caching architecture. Each function reads from I2C sensor, updates global cache struct, and logs performance timing to Serial.

---

### `void readBme280(Bme280Values &values)`

**Purpose:** Read temperature, humidity, pressure, altitude from BME280 sensor; compute dew point using Alduchov & Eskridge formula.

**Signature:**
```cpp
void readBme280(Bme280Values &values);
```

**Parameters:**
- `values` (Bme280Values&): Reference to global cache struct to update

**Return:** void

**Behavior:**
1. Guard: Return immediately if `available.bme280 = false`
2. Record start time: `startTime = micros()`
3. Read sensor values:
   - `temperature = bme280.readTemperature()` (°C)
   - `humidity = bme280.readHumidity()` (%RH)
   - `pressure = bme280.readPressure() / 100.0f` (Pa → hPa)
   - `altitude = bme280.readAltitude(1013.25f)` (m, using 1013.25 hPa reference)
4. Error check: Verify values in valid ranges:
   - Temperature: -40 to +85°C
   - Humidity: 0 to 100%
   - Pressure: 300 to 1100 hPa
   - If any value out of range: Log "ERROR: BME280 read returned invalid values" to Serial and **RETURN without updating cache**
5. Compute dew point (Alduchov & Eskridge formula, ±0.35°C accuracy):
   ```cpp
   a = 17.62, b = 243.12
   alpha = ((a * T) / (b + T)) + ln(RH% / 100)
   dewPoint = (b * alpha) / (a - alpha)
   ```
6. Record end time and log to Serial: `"BME280 read time: XXX µs"`
7. **Update cache struct** (only on success)

**I2C Address:** 0x77

**Timing Expectation:** 5–50 ms (includes I2C I/O + computation)

**Serial Output Example:**
```
BME280 read time: 12345 µs
```

**Callers:** `readSensors()` (every 500 ms via SENSOR_REFRESH_MS)

**Failure Behavior:** Cache not updated; last known values persist; error logged to Serial.

**Note:** Requires `#include <math.h>` for `log()` function.

---

### `void readTsl2591(Tsl2591Values &values)`

**Purpose:** Read light sensor full spectrum and infrared channels; compute visible light; calculate lux.

**Signature:**
```cpp
void readTsl2591(Tsl2591Values &values);
```

**Parameters:**
- `values` (Tsl2591Values&): Reference to global cache struct to update

**Return:** void

**Behavior:**
1. Guard: Return immediately if `available.tsl2591 = false`
2. Record start time: `startTime = micros()`
3. Read sensor values using Adafruit_TSL2591 API:
   - `fullLum = tsl2591.getFullLuminosity()` (uint32_t with [IR(16-31) | FullSpec(0-15)])
   - `fullSpectrum = fullLum & 0xFFFF` (Lower 16 bits, CHAN0)
   - `infrared = fullLum >> 16` (Upper 16 bits, CHAN1)
   - `visible = fullSpectrum - infrared` (Computed)
   - `lux = tsl2591.calculateLux(fullSpectrum, infrared)` (Manufacturer formula)
   - `gain = tsl2591.getGain()` (Sensor setting, for reference)
   - `integrationTime = tsl2591.getIntegrationTime()` (Sensor setting, for reference)
4. Error check: If both fullSpectrum and infrared are 0:
   - Log "ERROR: TSL2591 read returned zero values" to Serial
   - **RETURN without updating cache**
5. Record end time and log to Serial: `"TSL2591 read time: XXX µs"`
6. **Update cache struct** (only on success)

**I2C Address:** 0x29

**Timing Expectation:** 5–20 ms (includes I2C reads + lux calculation)

**Serial Output Example:**
```
TSL2591 read time: 8234 µs
```

**Callers:** `readSensors()` (every 500 ms via SENSOR_REFRESH_MS)

**Failure Behavior:** Cache not updated; last known values persist; error logged to Serial.

**API Details (Adafruit_TSL2591):**
- `getFullLuminosity()` returns uint32_t packed as: `(CHAN1 << 16) | CHAN0`
- CHAN0 = Full spectrum (visible + infrared)
- CHAN1 = Infrared only
- `calculateLux(ch0, ch1)` applies manufacturer algorithm

---

### `void readSi1145(Si1145Values &values)`

**Purpose:** Read visible light, infrared, and UV channels from SI1145 multi-spectrum sensor.

**Signature:**
```cpp
void readSi1145(Si1145Values &values);
```

**Parameters:**
- `values` (Si1145Values&): Reference to global cache struct to update

**Return:** void

**Behavior:**
1. Guard: Return immediately if `available.si1145 = false`
2. Record start time: `startTime = micros()`
3. Read sensor values using Adafruit_SI1145 API:
   - `visible = si1145.readVisible()` (uint16_t, 0–65535)
   - `infrared = si1145.readIR()` (uint16_t, 0–65535)
   - `uv = si1145.readUV()` (uint16_t raw from register 0x2C, typically 0–500)
4. Error check: If all three values are 0:
   - Log "ERROR: SI1145 read returned all zeros" to Serial
   - **RETURN without updating cache**
5. Compute UV index using standard Adafruit formula:
   - `uvIndex = uv / 100.0f` (Typical range 0–5; dangerous if > 11)
6. Record end time and log to Serial: `"SI1145 read time: XXX µs"`
7. **Update cache struct** (only on success)

**I2C Address:** 0x60

**Timing Expectation:** 5–15 ms (includes I2C reads only)

**Serial Output Example:**
```
SI1145 read time: 6789 µs
```

**Callers:** `readSensors()` (every 500 ms via SENSOR_REFRESH_MS)

**Failure Behavior:** Cache not updated; last known values persist; error logged to Serial.

**UV Index Calculation:** Raw UV register contains index × 100; divide by 100 to get actual UV index.

---

## 1. Large Display Helper Functions (INA219 Page 2) — **NEW**

### Overview

The large display mode provides an alternate UI for the INA219 power monitor (Page 2) that displays individual sensor values in enlarged, centered format. Four helper functions support this feature and are designed generically for future extension to other sensor pages.

### Helper Functions

#### 1. `void pageLargeDisplayIna219(uint8_t valueIndex)`

**Purpose:** Main render function for large display mode; displays one INA219 value in centered, enlarged format.

**Signature:**
```cpp
void pageLargeDisplayIna219(uint8_t valueIndex);
```

**Parameters:**
- `valueIndex` (uint8_t): Index 0–6, selects which INA219 value to display (see mapping table below)

**Display Layout (Landscape 480×320):**
```
┌──────────────────────────────────────────────────────┐
│ INA219 Large Display (y=18, Font 4, Cyan)           │
├──────────────────────────────────────────────────────┤
│ Separator line (y=48)                               │
│                                                      │
│ [LABEL - centered] (y=70, Font 4, Cyan)             │
│                                                      │
│ [VALUE - centered] (y=160, Font 8, Color-coded)     │
│                                                      │
│ [UNIT - centered] (y=240, Font 4, White)            │
│                                                      │
│ B:next X:prev Y:exit (y=295, Font 2, DarkGrey)     │
└──────────────────────────────────────────────────────┘
```

**Behavior:**
- Calls `fillScreen(TFT_BLACK)` to clear display atomically
- Draws header "INA219 Large Display" in Font 4 at (x=240, y=18), TFT_CYAN
- Draws separator line (TFT_DARKGREY) at y=48
- Calls `getLargeDisplayLabel()`, `getLargeDisplayValue()`, `getLargeDisplayUnit()`, `getLargeDisplayColor()` to fetch display components
- Renders label in Font 4 at (x=240, y=70), TFT_CYAN
- Renders value in Font 8 at (x=240, y=160) using `getLargeDisplayColor()` for dynamic color
- Renders unit in Font 4 at (x=240, y=240), TFT_WHITE
- Renders footer with navigation instructions in Font 2 at (x=240, y=295), TFT_DARKGREY
- All text centered using MC_DATUM (middle-center alignment)
- Text datum reset to TL_DATUM after rendering

**Refresh Rate:** Inherits page native rate (500 ms for INA219 page).

**Side Effects:** Updates TFT display via SPI.

**Performance:** Render time should be < 450 ms to maintain 500 ms refresh cycle.

**Callers:** `pageIna219()` when `largeDisplayMode = true`.

---

#### 2. `const char* getLargeDisplayLabel(uint8_t valueIndex)`

**Purpose:** Returns German label for a given value index.

**Signature:**
```cpp
const char* getLargeDisplayLabel(uint8_t valueIndex);
```

**Parameters:**
- `valueIndex` (uint8_t): Index 0–6

**Returns:** Const pointer to German label string (null-terminated)

**Label Mapping:**

| Index | Label | Description |
|-------|-------|-------------|
| 0 | "Laufzeit" | Runtime (HH:MM:SS format) |
| 1 | "Batterie" | Load Voltage (battery terminal voltage) |
| 2 | "Versorgung" | Bus Voltage (supply voltage) |
| 3 | "Strom" | Current (current draw) |
| 4 | "Ladung" | Charge (accumulated mAh) |
| 5 | "Shunt-Spannung" | Shunt Voltage (sensing voltage) |
| 6 | "Power" | Power (instantaneous power) |

**Out of Range:** If `valueIndex > 6`, return "???" (error indicator).

**Callers:** `pageLargeDisplayIna219()`.

---

#### 3. `String getLargeDisplayValue(uint8_t valueIndex)`

**Purpose:** Reads current value from global `ina219Values` struct and returns formatted string. No caching; always reflects latest measurement.

**Signature:**
```cpp
String getLargeDisplayValue(uint8_t valueIndex);
```

**Parameters:**
- `valueIndex` (uint8_t): Index 0–6

**Returns:** String object with formatted value (numeric only, no units or labels)

**Value Formatting & Source:**

| Index | Label | Source | Format | Example | Unit |
|-------|-------|--------|--------|---------|------|
| 0 | Laufzeit | `formatRuntime(micros() - ina219Values.startTimeUs)` | HH:MM:SS | "01:23:45" | (none) |
| 1 | Batterie | `ina219Values.load` | 3 decimals | "12.411" | V |
| 2 | Versorgung | `ina219Values.bus` | 3 decimals | "12.456" | V |
| 3 | Strom | `ina219Values.current` | 1 decimal | "1.2" | mA |
| 4 | Ladung | `ina219Values.chargeMah` | 3 decimals | "123.456" | mAh |
| 5 | Shunt-Spannung | `ina219Values.shunt` | 2 decimals | "0.05" | mV |
| 6 | Power | `ina219Values.power` | 0 decimals (integer) | "15" | mW |

**Special Case (Index 0 - Runtime):**
- Calculated as elapsed time since `ina219Values.startTimeUs` (set at first INA219 read)
- Formatted by `formatRuntime()` function as HH:MM:SS (e.g., "01:23:45")
- Represents total runtime since first sensor measurement

**Consistency Requirement:** Formatting must match standard `pageIna219()` display EXACTLY:
- Decimal places per value: 3/3/1/3/2/0 (indices 1-6)
- No scaling of values (values already in correct units from sensor)
- Same color scheme: Cyan for computed (Laufzeit, Ladung), Green for primary (Batterie, Versorgung, Shunt-Spannung), Orange for secondary (Strom, Power)

**Real-Time Updates:** Reads from `ina219Values` struct on every call; no caching.

**Callers:** `pageLargeDisplayIna219()`.

---

#### 4. `const char* getLargeDisplayUnit(uint8_t valueIndex)`

**Purpose:** Returns unit label for a given value index.

**Signature:**
```cpp
const char* getLargeDisplayUnit(uint8_t valueIndex);
```

**Parameters:**
- `valueIndex` (uint8_t): Index 0–6

**Returns:** Const pointer to unit string (null-terminated) or empty string if no unit

**Unit Mapping:**

| Index | Label | Unit |
|-------|-------|------|
| 0 | Laufzeit | (empty string - time already formatted as HH:MM:SS) |
| 1 | Batterie | "V" (volts) |
| 2 | Versorgung | "V" (volts) |
| 3 | Strom | "mA" (milliamps) |
| 4 | Ladung | "mAh" (milliamp-hours) |
| 5 | Shunt-Spannung | "mV" (millivolts) |
| 6 | Power | "mW" (milliwatts) |

**Out of Range:** If `valueIndex > 6`, return "" (empty string).

**Callers:** `pageLargeDisplayIna219()`.

---

#### 5. `uint16_t getLargeDisplayColor(uint8_t valueIndex)`

**Purpose:** Returns TFT_eSPI color constant for a given value index. Colors match standard `pageIna219()` display scheme.

**Signature:**
```cpp
uint16_t getLargeDisplayColor(uint8_t valueIndex);
```

**Parameters:**
- `valueIndex` (uint8_t): Index 0–6

**Returns:** TFT_eSPI 16-bit RGB565 color constant

**Color Mapping (Consistent with Standard Page Display):**

| Index | Color | Hex | RGB |
|-------|-------|-----|-----|
| 0 | TFT_CYAN | 0x07FF | (0, 255, 255) |
| 1 | TFT_GREEN | 0x07E0 | (0, 255, 0) |
| 2 | TFT_GREEN | 0x07E0 | (0, 255, 0) |
| 3 | TFT_ORANGE | 0xFD20 | (255, 165, 0) |
| 4 | TFT_CYAN | 0x07FF | (0, 255, 255) |
| 5 | TFT_GREEN | 0x07E0 | (0, 255, 0) |
| 6 | TFT_ORANGE | 0xFD20 | (255, 165, 0) |

**Rationale:** Color scheme indicates value category: Cyan (computed), Green (primary), Orange (secondary).

**Out of Range:** If `valueIndex > 6`, return TFT_WHITE (0xFFFF) as error fallback.

**Callers:** `pageLargeDisplayIna219()`.

---

### Function Composition Example

```cpp
// In pageIna219():
if (largeDisplayMode) {
  pageLargeDisplayIna219(largeDisplayIndex);
}
else {
  // Normal page rendering ...
}

// In pageLargeDisplayIna219(valueIndex):
const char* label = getLargeDisplayLabel(valueIndex);      // "Laufzeit"
String value = getLargeDisplayValue(valueIndex);           // "12345"
const char* unit = getLargeDisplayUnit(valueIndex);        // "s"
uint16_t color = getLargeDisplayColor(valueIndex);         // TFT_CYAN

tft.setTextDatum(MC_DATUM);  // Middle-center alignment
tft.setTextColor(TFT_CYAN);
tft.drawString(label, 160, 150, 4);         // Font 4

tft.setTextColor(color);
tft.drawString(value, 160, 200, 8);         // Font 8 (very large)

tft.setTextColor(TFT_WHITE);
tft.drawString(unit, 160, 280, 2);          // Font 2

tft.setTextColor(TFT_DARKGREY);
tft.drawString("B:next X:prev Y:exit", 160, 350, 1);  // Font 1, footer
```

---

### Design for Future Extension

These helper functions are designed generically to support extension to other sensor pages:

1. **Parameterized by page:** Could add a `sensorPage` parameter to identify which sensor's large display mode is active
2. **Value index ranges:** Different sensors may have different numbers of values; indexes could be page-specific
3. **Reusable helpers:** `pageLargeDisplayXxx()` renderer function can be replicated for other sensors using same helper pattern
4. **Consistent UI/UX:** Same layout, fonts, colors, refresh rate across all large display modes

---

## 2. Central getValue Functions (FEATURE-SPEC-008 & Phase 5)

Each sensor implements a central `String sensor_getValue(uint8_t index)` function that returns formatted value strings for display. These functions are used by both standard sensor page renderers and the large display mode.

### Function Signatures

All getValue functions follow this pattern:

```cpp
String bme280_getValue(uint8_t index);    // Returns formatted value string
String gamepad_getValue(uint8_t index);
String ina219_getValue(uint8_t index);
String encoder_getValue(uint8_t index);
String scd30_getValue(uint8_t index);
String si1145_getValue(uint8_t index);
String tsl2591_getValue(uint8_t index);
String veml6070_getValue(uint8_t index);
String veml6075_getValue(uint8_t index);
```

**Parameters:**
- `index` (uint8_t): Zero-based parameter index (0 to PARAM_COUNT-1)
- Returns: Formatted value as String (e.g., "23.45", "1013.25", "45%")

**Behavior:**
- Returns empty string `""` if sensor unavailable or index out of bounds
- All formatting (decimal places, unit conversion) is centralized in function
- Used by page renderers to display values with consistent formatting

### Index Mappings by Sensor

#### BME280 (Page 0)
```cpp
// Index → Parameter
String bme280_getValue(uint8_t index) {
  if (index >= BME280_PARAM_COUNT || !available.bme280) return "";
  switch (index) {
    case 0: return String(bme280Values.temperature, 2);      // "23.45"
    case 1: return String(bme280Values.pressure, 2);         // "1013.25"
    case 2: return String(bme280Values.humidity, 2);         // "45.30"
    case 3: return String(bme280Values.dewPoint, 2);         // "14.12"
    case 4: return String(bme280Values.altitude, 2);         // "234.56"
    case 5: return String(bme280Values.aqiProxy, 2);         // "42.10"
    case 6: return String(bme280Values.tempTrend, 3);        // "0.025"
    default: return "";
  }
}
```

**Parameter Count:** BME280_PARAM_COUNT = 7

#### Gamepad (Page 1)
```cpp
// Index → Parameter
String gamepad_getValue(uint8_t index) {
  if (index >= GAMEPAD_PARAM_COUNT) return "";
  switch (index) {
    case 0: return String(gamepadValues.x);                  // "-500"
    case 1: return String(gamepadValues.y);                  // "250"
    case 2: return String(gamepadValues.buttons, HEX);       // "0x0F"
    case 3: return String(gamepadValues.eventCount);         // "3"
    case 4: return String(gamepadValues.lastEvent, HEX);     // "0x42"
    case 5: return (gamepadValues.hasEvent ? "Yes" : "No");  // "Yes" / "No"
    case 6: return String(gamepadValues.pressure, 2);        // "120.5" (if available)
    case 7: return String(gamepadValues.reserved);           // "0"
    default: return "";
  }
}
```

**Parameter Count:** GAMEPAD_PARAM_COUNT = 8

#### INA219 (Page 2)
```cpp
// Index → Parameter
String ina219_getValue(uint8_t index) {
  if (index >= INA219_PARAM_COUNT || !available.ina219) return "";
  switch (index) {
    case 0: return String(ina219Values.shunt, 4);            // "0.0250"
    case 1: return String(ina219Values.bus, 3);              // "5.000"
    case 2: return String(ina219Values.load, 3);             // "4.975"
    case 3: return String(ina219Values.current, 3);          // "-1.020"
    case 4: return String(ina219Values.power, 2);            // "5.10"
    case 5: return String(ina219Values.chargeMah, 2);        // "12345.67"
    case 6: return (ina219Values.initialized ? "Init" : "--");
    default: return "";
  }
}
```

**Parameter Count:** INA219_PARAM_COUNT = 7

#### Encoder (Page 3)
```cpp
// Index → Parameter
String encoder_getValue(uint8_t index) {
  if (index >= ENCODER_PARAM_COUNT) return "";
  switch (index) {
    case 0: return String(encoderValues.position);           // "1234"
    case 1: return String(encoderValues.delta);              // "2"
    case 2: return (encoderValues.pressed ? "Yes" : "No");   // "Yes" / "No"
    case 3: return String(encoderValues.presses);            // "456"
    case 4: return String(encoderValues.brightness);         // "128"
    case 5: return String(encoderValues.color);              // "blue"
    case 6: return String(encoderValues.reserved);           // "0"
    default: return "";
  }
}
```

**Parameter Count:** ENCODER_PARAM_COUNT = 7

#### SCD30 (Page 4)
```cpp
// Index → Parameter
String scd30_getValue(uint8_t index) {
  if (index >= SCD30_PARAM_COUNT || !available.scd30) return "";
  switch (index) {
    case 0: return String(scd30Values.co2, 2);               // "456.78"
    case 1: return String(scd30Values.temp, 2);              // "23.45"
    case 2: return String(scd30Values.humidity, 2);          // "45.30"
    default: return "";
  }
}
```

**Parameter Count:** SCD30_PARAM_COUNT = 3

#### SI1145 (Page 5)
```cpp
// Index → Parameter
String si1145_getValue(uint8_t index) {
  if (index >= SI1145_PARAM_COUNT || !available.si1145) return "";
  switch (index) {
    case 0: return String(si1145Values.visible);             // "2500"
    case 1: return String(si1145Values.infrared);            // "1200"
    case 2: return String(si1145Values.uvIndex, 2);          // "3.45"
    case 3: return String(si1145Values.uvRaw);               // "150"
    default: return "";
  }
}
```

**Parameter Count:** SI1145_PARAM_COUNT = 4

#### TSL2591 (Page 6)
```cpp
// Index → Parameter
String tsl2591_getValue(uint8_t index) {
  if (index >= TSL2591_PARAM_COUNT || !available.tsl2591) return "";
  switch (index) {
    case 0: return String(tsl2591Values.fullspectrum);       // "5000"
    case 1: return String(tsl2591Values.infrared);           // "2000"
    case 2: return String(tsl2591Values.visible);            // "3000"
    case 3: return String(tsl2591Values.lux, 1);             // "1234.5"
    default: return "";
  }
}
```

**Parameter Count:** TSL2591_PARAM_COUNT = 4

#### VEML6070 (Page 7)
```cpp
// Index → Parameter
String veml6070_getValue(uint8_t index) {
  if (index >= VEML6070_PARAM_COUNT || !available.veml6070) return "";
  switch (index) {
    case 0: return String(veml6070Values.raw);               // "1500"
    case 1: return String(veml6070Values.lsb);               // "220"
    case 2: return String(veml6070Values.msb);               // "5"
    default: return "";
  }
}
```

**Parameter Count:** VEML6070_PARAM_COUNT = 3

#### VEML6075 (Page 8)
```cpp
// Index → Parameter
String veml6075_getValue(uint8_t index) {
  if (index >= VEML6075_PARAM_COUNT || !available.veml6075) return "";
  switch (index) {
    case 0: return String(veml6075Values.uva);               // "5000"
    case 1: return String(veml6075Values.uvb);               // "3000"
    case 2: return String(veml6075Values.uvaComp, 2);        // "4850.25"
    case 3: return String(veml6075Values.uvbComp, 2);        // "2920.15"
    case 4: return String(veml6075Values.index, 2);          // "5.23"
    case 5: return String(veml6075Values.reserved);          // "0"
    default: return "";
  }
}
```

**Parameter Count:** VEML6075_PARAM_COUNT = 6

### Usage Example

Page renderers use getValue to display sensor data:

```cpp
void displaySensorPage(const ParameterTriple* params, uint8_t paramCount, String (*getValueFunc)(uint8_t)) {
  int displayY = 56;
  const int lineHeight = 24;
  
  for (uint8_t i = 0; i < paramCount && displayY < 280; i++) {
    String value = getValueFunc(i);  // Call getValue for this parameter
    String label = String(params[i].label) + ":";
    rowTriple(displayY, label.c_str(), value, params[i].unit, params[i].color);
    displayY += lineHeight;
  }
}
```

### Large Display Mode Integration

getValue functions are also used by large display renderer:

```cpp
void renderLargeDisplay(uint8_t pageIndex, uint8_t valueIndex) {
  // Get value using appropriate getter for this page
  String value = getValueForPage(pageIndex, valueIndex);
  
  // Get parameter definition for label, unit, color
  const ParameterTriple* param = getParameterForPage(pageIndex, valueIndex);
  
  // Render large on screen
  tft.setFreeFont(FF25);
  tft.drawString(param->label, 60, 50, 2);     // Label in regular font
  tft.drawString(value, 60, 120, 7);           // Value in huge font
  tft.drawString(param->unit, 300, 140, 2);    // Unit
}
```

### Summary Table

| Sensor | Page | Function | Count | Available? |
|--------|------|----------|-------|------------|
| BME280 | 0 | bme280_getValue() | 7 | available.bme280 |
| Gamepad | 1 | gamepad_getValue() | 8 | always |
| INA219 | 2 | ina219_getValue() | 7 | available.ina219 |
| Encoder | 3 | encoder_getValue() | 7 | always |
| SCD30 | 4 | scd30_getValue() | 3 | available.scd30 |
| SI1145 | 5 | si1145_getValue() | 4 | available.si1145 |
| TSL2591 | 6 | tsl2591_getValue() | 4 | available.tsl2591 |
| VEML6070 | 7 | veml6070_getValue() | 3 | available.veml6070 |
| VEML6075 | 8 | veml6075_getValue() | 6 | available.veml6075 |
| **Total** | **—** | **9 functions** | **49** | **—** |

---

## 3. I2C Bus Protocol

### Overview

**Master:** ESP32 (GPIO32 SDA, GPIO25 SCL)  
**Clock:** 400 kHz (standard I2C)  
**Addressing:** 7-bit device addresses  
**Topology:** Shared bus, all devices listen  

### Supported I2C Devices & Addresses

All devices are **optional**; firmware gracefully handles absence via `available.X` flags.

#### Device 1: INA219 Current Monitor @ 0x40

**Purpose:** Read bus voltage, shunt voltage, current, power (50 Hz via Adafruit INA219 library).

**Supported Commands (via Adafruit INA219 library):**

| Function | I2C Operation | Returns | Side Effects |
|----------|---|---|---|
| `ina219.begin(&Wire)` | Probe register 0x00 (Config) | bool | Initializes calibration |
| `ina219.getShuntVoltage_mV()` | Read reg 0x01 | float (mV) | None |
| `ina219.getBusVoltage_V()` | Read reg 0x02 | float (V) | None |
| `ina219.getCurrent_mA()` | Read regs 0x04–0x05, compute | float (mA) | None |
| `ina219.getPower_mW()` | Read regs 0x03–0x04, compute | float (mW) | None |

**Register Map (Adafruit INA219 assumed):**

| Address | Name | Access | Bits | Notes |
|---------|------|--------|------|-------|
| 0x00 | Config | RW | 16 | Calibration, averaging, conversion time |
| 0x01 | Shunt V | RO | 16 | Shunt voltage (mV) in two's complement |
| 0x02 | Bus V | RO | 16 | Bus voltage (V), includes status flags |
| 0x03 | Power | RO | 16 | Power measurement (mW) |
| 0x04 | Current | RO | 16 | Current measurement (mA) |
| 0x05 | Calibration | RW | 16 | Calibration register (set by library) |

**Read Cycle (50 Hz via micros()):**
```
every 20 µs:
  shunt_mV = -ina219.getShuntVoltage_mV()  // Negated
  bus_V = ina219.getBusVoltage_V()
  current_mA = -ina219.getCurrent_mA()     // Negated
  power_mW = ina219.getPower_mW()
  load_V = bus_V - shunt_mV/1000
  chargeMah += current_mA * elapsed_µs / 3.6e9  // Coulomb integration
```

**Failure Mode:** If `Wire.beginTransmission()` or `Wire.requestFrom()` fails, all fields in `ina219Values` remain unchanged (stale data persists).

---

#### Device 2: VEML6075 UV Sensor @ 0x10

**Purpose:** Read raw UVA/UVB ADC counts + compensation factors; compute UV index (500 ms cycle).

**Protocol:** Direct I2C register read (Adafruit library NOT used; custom code).

**Register Reads:**

| Register | Address | Bytes | Returns | Formula |
|----------|---------|-------|---------|---------|
| UVA | 0x07 | 2 (LSB:MSB) | uint16_t | Raw UVA ADC count |
| UVB | 0x09 | 2 (LSB:MSB) | uint16_t | Raw UVB ADC count |
| COMP1 | 0x0A | 2 (LSB:MSB) | uint16_t | Compensation factor 1 |
| COMP2 | 0x0B | 2 (LSB:MSB) | uint16_t | Compensation factor 2 |

**Register Write (Boot):**
```cpp
writeReg16(0x10, 0x00, 0x0000);  // Write Config = 0x0000 (enable sensor)
```

**Compensation Formulas:**
```cpp
uvaComp = max(0.0f, (float)uva - 2.22f * comp1 - 1.33f * comp2);
uvbComp = max(0.0f, (float)uvb - 2.95f * comp1 - 1.74f * comp2);
uvIndex = ((uvaComp * 0.001461f) + (uvbComp * 0.002591f)) * 0.5f;
```

**Failure Mode:** If any register read fails, struct not updated; previous values persist.

---

#### Device 3: VEML6070 UV Sensor @ 0x38 (LSB) & 0x39 (MSB)

**Purpose:** Read raw UV ADC count from dual-address sensor (500 ms cycle).

**Protocol:** Dual I2C address (unusual configuration).

**Register Reads:**

| Register | Address | Bytes | Returns |
|----------|---------|-------|---------|
| LSB | 0x38 | 1 | uint8_t (lower byte) |
| MSB | 0x39 | 1 | uint8_t (upper byte) |

**Combination:**
```cpp
raw = ((uint16_t)msb << 8) | lsb;
```

**Failure Mode:** If either address fails to read, struct not updated; previous values persist.

---

#### Device 4: TSL2591 Light Sensor @ 0x29

**Purpose:** Read ambient light intensity (lux) with infrared/visible spectrum split (via Adafruit TSL2591 library; 500 ms cycle but not actively used for page display in this firmware).

**Supported Commands (via Adafruit TSL2591 library):**

| Function | Returns | Side Effects |
|----------|---------|---|
| `tsl2591.begin()` | bool | Initializes sensor |
| `tsl2591.setGain(TSL2591_GAIN_MED)` | void | Sets sensor gain (boot-time config) |
| `tsl2591.setTiming(TSL2591_INTEGRATIONTIME_200MS)` | void | Sets integration time (boot-time config) |
| `tsl2591.getFullSpectrum()` | uint32_t | Returns combined IR + visible ADC counts |
| `tsl2591.getInfrared()` | uint16_t | Returns IR ADC count |
| `tsl2591.getLux()` | uint16_t | Returns computed lux value |

**[Note]** TSL2591 is detected at boot but **no dedicated page in sensor-pages-dashboard** (unlike multi-sensor-dashboard). Values are not cached in a global struct; readings are on-demand via Adafruit library only.

**Failure Mode:** If `tsl2591.begin()` fails at boot, `available.tsl2591 = false`; no readings attempted.

---

#### Device 5: SI1145 Multi-Spectrum Sensor @ 0x60

**Purpose:** Read visible/infrared/UV spectrum data (via Adafruit SI1145 library; 500 ms cycle but not actively used in this firmware).

**Supported Commands (via Adafruit SI1145 library):**

| Function | Returns | Side Effects |
|----------|---------|---|
| `si1145.begin()` | bool | Initializes sensor |
| `si1145.readVisible()` | uint16_t | Visible light ADC count |
| `si1145.readIR()` | uint16_t | Infrared ADC count |
| `si1145.readUV()` | uint16_t | UV ADC count (index × 100) |

**[Note]** SI1145 is detected at boot but **no dedicated page in sensor-pages-dashboard**. Values not cached; library available for on-demand reads.

**Failure Mode:** If `si1145.begin()` fails at boot, `available.si1145 = false`; no readings attempted.

---

#### Device 6: BME280 Climate Sensor @ 0x77

**Purpose:** Read temperature, humidity, pressure (via Adafruit BME280 library; 500 ms cycle).

**Supported Commands (via Adafruit BME280 library):**

| Function | Returns | Side Effects |
|----------|---------|---|
| `bme280.begin(BME280_ADDR, &Wire)` | bool | Initializes sensor |
| `bme280.readTemperature()` | float | Temperature (°C) |
| `bme280.readHumidity()` | float | Relative humidity (%) |
| `bme280.readPressure()` | float | Pressure (Pa) |
| `bme280.readAltitude(seaLevelPressure)` | float | Altitude (m) |

**[Note]** BME280 **has dedicated page 0** in sensor-pages-dashboard. Values displayed every 500 ms.

**Failure Mode:** If `bme280.begin()` fails at boot, `available.bme280 = false`; page shows error ("Sensor nicht vorhanden").

---

#### Device 7: SCD30 CO₂ Sensor @ 0x61

**Purpose:** Read CO₂/temperature/humidity with CRC8 validation (500 ms cycle); query and toggle ASC (Automatic Self Calibration) configuration.

**Protocol:** Sensirion proprietary command + CRC8 checksums (not Adafruit library).

**Commands:**

| Command | Hex | Purpose |
|---------|-----|---------|
| START | 0x0010 | Start measurement |
| READY | 0x0202 | Poll measurement ready status |
| READ | 0x0300 | Fetch CO₂/temp/humidity (18 bytes) |
| SET ASC | 0x53DA | Enable/Disable ASC (NEW — FEATURE-SPEC-009) |
| GET ASC | 0x5301 | Query ASC status (NEW — FEATURE-SPEC-009) |

**Response Format (READ command — 18 bytes, 6 CRC checks):**
```
Bytes:  [0–1] CO₂ MSB:LSB    [2] CO₂ CRC
        [3–5] Temp MSB:LSB   [5] Temp CRC
        [6–8] Humidity MSB:LSB [8] Humidity CRC
```

**CRC8 Algorithm:**
```cpp
uint8_t crc = 0xFF;
for (size_t i = 0; i < length; i++) {
  crc ^= data[i];
  for (uint8_t bit = 0; bit < 8; bit++)
    crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
}
// Result: crc should equal expected checksum
```

**Polynomial:** 0x31  
**Initial Value:** 0xFF

**Float Encoding:** Data bytes are IEEE 754 big-endian; firmware byte-reverses on read.

**Read Sequence:**
```
1. scd30Command(SCD30_CMD_READY)           // Poll status
2. requestFrom(SCD30_ADDR, 3)              // Read status
3. if (status & 1) == ready:
     scd30Command(SCD30_CMD_READ)          // Request data
     requestFrom(SCD30_ADDR, 18)           // Read 18 bytes
     Validate all 6 CRC8 checksums
     if any CRC fails: discard, return false
     Extract & convert 3 floats
4. Return true if all CRC passed
```

**Failure Mode:** If any CRC checksum fails, `readScd30()` returns false; struct not updated.

---

### NEW: ASC Commands (FEATURE-SPEC-009)

#### Command: SET ASC (Enable/Disable)

**Purpose:** Send I2C command to enable or disable Automatic Self Calibration.

**Signature:**
```cpp
bool setAsc(bool enable);
```

**Request Format:**
```
Wire.write(0x53);           // Command High Byte
Wire.write(0xDA);           // Command Low Byte
Wire.write(0x00);           // Data High Byte (always 0x00)
Wire.write(enable ? 0x01 : 0x00);  // Data Low Byte (0x01=ON, 0x00=OFF)
Wire.write(crc8(...));      // CRC8 over data bytes [0x00, 0x01/0x00]
```

**Response:** ACK or NAK (no data bytes returned).

**Behavior:**
1. Guard: Return false if `!available.scd30`
2. Calculate CRC8 over bytes `[0x00, data_low]`
3. Send I2C write sequence (Wire.beginTransmission + 4 Wire.write calls + Wire.endTransmission)
4. Set `scd30State.pendingAscWrite = true` before transmission
5. Clear `scd30State.pendingAscWrite = false` after transmission completes
6. Set `scd30State.ascEnabled = enable` (optimistic feedback)
7. Return true if I2C write succeeded (endTransmission == 0), false if error

**Timing:** ~5 ms I2C operation

**Error Handling:**
- If I2C fails (endTransmission != 0): return false, revert optimistic state
- Next ASC status query will verify actual state

**Example Usage:**
```cpp
// Toggle ASC: query current state, then toggle
if (xButtonPressed && currentPage == 4 && !largeDisplayMode.active) {
  setAsc(!scd30State.ascEnabled);  // Toggle
}
```

---

#### Query: GET ASC Status

**Purpose:** Query current ASC status from sensor.

**Signature:**
```cpp
bool getAscStatus(bool& outEnabled);
```

**Request:**
```
Wire.write(0x53);           // Command High Byte
Wire.write(0x01);           // Command Low Byte
```

**Response (3 bytes):**
```
Byte 0: Status Data High (usually 0x00)
Byte 1: Status Data Low (0x01 = ASC ON, 0x00 = ASC OFF)
Byte 2: CRC8 checksum over bytes 0–1
```

**Behavior:**
1. Guard: Return false if `!available.scd30`
2. Send I2C command (0x53 0x01)
3. Request 3 bytes from sensor
4. If fewer than 3 bytes received: return false
5. Validate CRC8: `scd30Crc(data, 2) == data[2]`
   - If CRC invalid: set `scd30State.ascStatusValid = false`, return false
6. Extract status: `outEnabled = (data[1] & 0x01) != 0`
7. On success:
   - Set `scd30State.ascEnabled = outEnabled`
   - Set `scd30State.ascStatusValid = true`
   - Set `scd30State.lastAscQuery = millis()`
8. Return true on success, false on any error

**Timing:** ~5–10 ms I2C operation

**Error Handling:**
- I2C error: return false, don't modify `scd30State`
- CRC invalid: set `ascStatusValid = false`, return false
- Next query attempt will retry

**Example Usage:**
```cpp
// Query ASC status at startup (~500ms after SCD30 START)
bool enabled;
if (getAscStatus(enabled)) {
  // Status is valid; scd30State.ascEnabled reflects actual state
  Serial.printf("ASC is %s\n", enabled ? "ON" : "OFF");
} else {
  // Query failed; scd30State.ascStatusValid = false
  // Display "ASC: --" (unknown)
}
```

---

#### Helper: `scd30_getAscStatus()` Display String

**Purpose:** Return ASC status as formatted display string for UI.

**Signature:**
```cpp
String scd30_getAscStatus();
```

**Returns:**
- `"ON"` if `scd30State.ascEnabled && scd30State.ascStatusValid` (green text)
- `"OFF"` if `!scd30State.ascEnabled && scd30State.ascStatusValid` (orange text)
- `"--"` if `!scd30State.ascStatusValid` (gray text, status unknown)

**Used By:** SCD30 page renderer (pageScd30) to display ASC status in top-right corner.

---

#### Device 8: Encoder Seesaw (Adafruit QT Rotary Encoder) @ 0x36

**Purpose:** Read rotary encoder position, button state; control NeoPixel RGB LED (50 Hz CONTINUOUSLY, not page-conditional).

**Protocol:** Adafruit seesaw library (ARM Cortex-M0 co-processor on device).

**Supported Commands:**

| Function | Returns | Side Effects |
|----------|---------|---|
| `seesaw.begin(ENCODER_ADDR)` | bool | Initializes co-processor |
| `seesaw.getEncoderPosition()` | int32_t | Absolute encoder position (ticks) |
| `seesaw.pinMode(GPIO, INPUT_PULLUP)` | void | Configure button pin 24 as input |
| `seesaw.digitalRead(GPIO)` | int | Button state (0 = pressed, 1 = released) |
| `encoderLed.begin(ENCODER_ADDR)` | bool | Initialize NeoPixel LED (addresses same device) |
| `encoderLed.setPixelColor(idx, RGB)` | void | Set LED RGB color (0–255 per channel) |
| `encoderLed.setBrightness(0–255)` | void | Set LED brightness |
| `encoderLed.show()` | void | Push color/brightness to hardware |

**LED Colors (Firmware Palette):**
```cpp
const uint32_t encoderColors[] = {
  seesaw_NeoPixel::Color(0, 96, 255),    // Blue
  seesaw_NeoPixel::Color(0, 255, 32),    // Green
  seesaw_NeoPixel::Color(255, 48, 0),    // Red
  seesaw_NeoPixel::Color(180, 0, 255)    // Purple
};
```

**Button Pin:** GPIO 24 (seesaw side, not ESP32 GPIO).

**NeoPixel Pin:** Pin 6 (seesaw side).

**Side Effects:**
- On encoder button press (falling edge): cycle LED color (0→1→2→3→0).
- On encoder dial turn: adjust LED brightness (delta × 8, clamped 0–255).

**Failure Mode:** If `seesaw.begin()` fails at boot, `available.encoder = false`; page shows error.

---

#### Device 9: Gamepad Seesaw (Adafruit Mini I2C STEMMA QT Gamepad) @ 0x50

**Purpose:** Read joystick X/Y, 6 buttons, keypad events (40 Hz polling).

**Protocol:** Adafruit seesaw library (ARM Cortex-M0 co-processor on device).

**Supported Commands:**

| Function | Returns | Side Effects |
|----------|---------|---|
| `gamepad.begin(GAMEPAD_ADDR)` | bool | Initializes co-processor |
| `gamepad.analogRead(14)` | int | Joystick X analog (0–1023) |
| `gamepad.analogRead(15)` | int | Joystick Y analog (0–1023) |
| `gamepad.digitalReadBulk(BUTTON_MASK)` | uint32_t | Button states (bitmask) |
| `gamepad.pinModeBulk(BUTTON_MASK, INPUT_PULLUP)` | void | Configure button pins as inputs (boot) |
| `gamepad.setGPIOInterrupts(BUTTON_MASK, true)` | void | Enable GPIO interrupts (boot) |
| `gamepad.getKeypadCount()` | uint8_t | Pending events in queue |
| `gamepad.readKeypad(&event, 1)` | bool | Read one keypad event |

**Button Configuration:**

| Button | Pin | Bit | Mask | Active Level |
|--------|-----|-----|------|---|
| Y | 2 | 2 | 0x04 | Low (0 = pressed) |
| A | 5 | 5 | 0x20 | Low |
| X | 6 | 6 | 0x40 | Low |
| B | 1 | 1 | 0x02 | Low |
| SELECT | 0 | 0 | 0x01 | Low |
| START | 16 | 16 | 0x10000 | Low |

**Button Mask:** `BUTTON_MASK = (1UL << 2) | (1UL << 5) | (1UL << 6) | (1UL << 1) | (1UL << 0) | (1UL << 16) = 0x39`

**Joystick Convention:**
```cpp
x = 1023 - gamepad.analogRead(14);  // Inverted
y = 1023 - gamepad.analogRead(15);  // Inverted
```

**Keypad Event Structure:**
```cpp
struct keyEventRaw {
  uint8_t reg;  // Edge (bits 0–1) + Key index (bits 2–7)
};
// Decode: edge = reg & 0x03; key = reg >> 2
// Edge: SEESAW_KEYPAD_EDGE_FALLING=1, SEESAW_KEYPAD_EDGE_RISING=2
```

**Page Navigation via Buttons:**
```cpp
// Edge detection (compare to previous state):
yPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_Y));
lastY = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_Y));
if (yPressed && !lastY) {  // Y button falling edge
  currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;  // Previous page
}

// A button falling edge:
aPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_A));
lastA = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_A));
if (aPressed && !lastA) {
  currentPage = (currentPage + 1) % PAGE_COUNT;  // Next page
}
```

**Failure Mode:** If `gamepad.begin()` fails at boot, `available.gamepad = false`; warning printed; page navigation disabled.

---

#### Device 10: DS2484 I2C-to-1-Wire Adapter @ 0x18

**Purpose:** Discover and read temperature from multiple 1-Wire sensors (DS18B20, DS1822, DS1820, etc.) via I2C-to-1-Wire bridge using Adafruit DS248x library.

**Hardware:** Adafruit DS2484 I2C to 1-Wire Bus Adapter Breakout.

**Protocol:** Adafruit DS248x library (low-level 1-Wire commands over I2C).

**Library:** Adafruit DS248x @ 1.2.0

**Supported Commands (via Adafruit_DS248x class):**

| Function | Returns | Purpose |
|----------|---------|----------|
| `begin(TwoWire*, uint8_t address)` | bool | Initialize adapter at address (0x18 default) |
| `OneWireReset()` | bool | Reset 1-Wire bus; returns true if presence pulse detected |
| `OneWireSearchReset()` | bool | Start ROM address search sequence |
| `OneWireSearch(uint8_t* address)` | bool | Find next device; fills 8-byte address array; returns false when search complete |
| `OneWireWriteByte(uint8_t byte)` | bool | Write byte to 1-Wire bus; returns success |
| `OneWireReadByte(uint8_t* byte)` | bool | Read byte from 1-Wire bus; returns success |

**1-Wire Protocol Sequence (readDs2484Temperatures):**

**Broadcast Convert T (all sensors):**
```
1. OneWireReset()             // Presence pulse
2. OneWireWriteByte(0xCC)     // Skip ROM (broadcast to all)
3. OneWireWriteByte(0x44)     // Convert T (start conversion)
4. delay(850)                 // 12-bit resolution = 750 ms typical
```

**Read Temperature (per-device):**
```
1. OneWireReset()             // Presence pulse
2. OneWireWriteByte(0x55)     // Match ROM (select specific device)
3. OneWireWriteByte(addr[0])
4. OneWireWriteByte(addr[1])
5. ... (repeat for all 8 bytes of ROM address)
6. OneWireWriteByte(0xBE)     // Read Scratchpad
7. OneWireReadByte(&LSB)      // Temperature LSB
8. OneWireReadByte(&MSB)      // Temperature MSB
9. Convert: temp_C = raw_16bit * 0.0625
```

**Resolution & Timing:**

| Resolution | Conversion Time | Accuracy |
|------------|---|---|
| 9 bits | ~95 ms | 0.5 °C |
| 10 bits | ~188 ms | 0.25 °C |
| 11 bits | ~375 ms | 0.125 °C |
| **12 bits (default)** | **~750 ms** | **0.0625 °C** |

**Firmware Config:** 12-bit standard resolution (hardcoded in readDs2484Temperatures() with 850 ms delay).

**1-Wire Address Format (64-bit ROM-ID):**
```
Byte 0:   Family Code (0x28=DS18B20, 0x22=DS1822, 0x10=DS1820, etc.)
Bytes 1-6: Serial Number (unique per device)
Byte 7:   CRC8 checksum (validated via OneWire::crc8() function)
```

**Display Format:** HEX string (e.g., "28:AB:CD:EF:01:02:03:04").

**Polling Schedule:**
- **Bus Scan:** Every 60 seconds (detect new/missing sensors)
- **Temperature Polling:** Every 3 seconds (read current values)
- **First Scan:** Forced immediately on boot (despite lastScanMs=0)

**Error Handling:**
- CRC checksum failure: mark sensor offline after 3 consecutive errors
- No ACK on bus operation: increment error counter; skip this read
- Recovery: reset error counter to 0 when successful read occurs
- Max sensors: 3 (hardware limit of Adafruit DS2484 breakout board)

**Serial Debug Output (115200 baud, for development only):**
```
Probing DS2484 at 0x18...
DS2484: FOUND
DS2484: Initialized at 0x18
DS2484: Scanning 1-Wire bus...
DS2484: Found sensor 0: 28:bb:0e:68:05:00:00:fa
DS2484: Bus scan complete - found 1 sensors
DS2484: Sensor 0 temp = 20.50C
```

**Failure Mode:** If `ds2484Master.begin()` fails at boot, `available.ds2484 = false`; page 9 displays error screen.

---

## 4. Serial Debug Output (FEATURE-SPEC-012: Timestamped Logging)

**Configuration:**
- Baud rate: 115200
- Port: /dev/ttyUSB1 (hardware serial via USB UART bridge)
- Purpose: Development and troubleshooting only (not user-facing)
- **Format:** All messages include millisecond-precision timestamps `[HH:MM:SS.sss]` and category tags

**Timestamp Format (FEATURE-SPEC-012):**
- Format: `[HH:MM:SS.sss]` (13 characters)
  - HH: Hours (00–23)
  - MM: Minutes (00–59)
  - SS: Seconds (00–59)
  - sss: Milliseconds (000–999)
- Time reference: Milliseconds since boot via `millis()`
- Precision: 1 millisecond
- Calculation: Integer-only arithmetic (no floating-point overhead)

**Boot Sequence Output (Verified 2026-10-22):**
```
[00:00:00.000] ========================================
[00:00:00.001] Sensor Pages Dashboard
[00:00:00.002] Version: 3.1.0
[00:00:00.003] Build Date: 2026-10-22T17:20:00Z
[00:00:00.004] Compiler: arm-none-eabi-g++
[00:00:00.005] ESP32-WROOM-32 Detected
[00:00:00.006] ========================================
[00:00:00.015] [BOOT] Starting setup...
[00:00:00.450] [BOOT] Initializing I2C...
[00:00:00.451] [BOOT] I2C bus initialized at 400 kHz
[00:00:00.456] [SENSOR] Probing DS2484 at 0x18...
[00:00:00.456] [BOOT] DS2484: FOUND
[00:00:00.456] [BOOT] DS2484: Initialized at 0x18
[00:00:01.054] [SETUP] Scanning 1-Wire bus...
[00:00:01.345] [BOOT] DS2484: Found sensor 0: 28:bb:0e:68:05:00:00:fa
[00:00:01.345] [BOOT] DS2484: Bus scan complete - found 1 sensors
[00:00:09.722] [SETUP] ========== BOOT COMPLETE ==========
```

**Logging Categories:**

| Tag | Purpose | Usage | Example |
|-----|---------|-------|---------|
| `[BOOT]` | Startup/initialization | Device setup, I2C init, sensor detection | `[BOOT] Initializing I2C...` |
| `[DIAG]` | Diagnostics/timing | Heap info, loop timing, system status | `[DIAG] Heap OK: 344748 bytes free` |
| `[SENSOR]` | Sensor reads/I2C operations | Sensor read timing, data validity checks | `[SENSOR] BME280 read time: 1975 µs` |
| `[INPUT]` | User input events | Gamepad button, encoder rotation | `[INPUT] GAMEPAD Button Y: PRESSED` |
| `[MODE]` | Display/UI changes | Page navigation, Large Display mode | `[MODE] page change: 2 → 1 (source: GAMEPAD)` |
| `[I2C]` | I2C protocol diagnostics | Communication errors, NACK, timeouts | `[I2C] WARNING: SCD30 read timeout` |
| `[ERROR]` | Error conditions | Sensor read failures, invalid data | `[ERROR] Spontaneous display switch` |
| `[VERSION]` | Version/build info | Version, build date, compiler info | `[VERSION] Version: 3.1.0` |

**Input Activity Logging Examples:**

Gamepad Navigation:
```
[00:00:15.234] [INPUT] GAMEPAD Button Y: PRESSED
[00:00:15.256] [INPUT] GAMEPAD Button Y: RELEASED
[00:00:15.271] [MODE] page change: 2 → 1 (source: GAMEPAD Button Y)
```

Encoder Navigation:
```
[00:00:17.123] [INPUT] ENCODER Rotation: position 142 → 143 (delta: +1, CLOCKWISE)
[00:00:17.145] [MODE] page change: 1 → 2 (source: ENCODER rotation CW)
```

Large Display Mode:
```
[00:00:21.567] [INPUT] ENCODER Button: PRESSED
[00:00:21.589] [MODE] Large Display enter (page 4, valueIndex 0)
```

**Notes:**
- Every log line includes `[HH:MM:SS.sss]` timestamp prepended automatically by logPrintf()
- Empty logPrintf("") calls produce no output (no wasted space)
- Serial output available only via USB connection; not stored or persisted
- Related requirements: FR-024 (Timestamped Logging), FR-025 (Display Diagnostics), FR-026 (Version Info)

---

### 4a. Timestamped Logging Function (FEATURE-SPEC-012)

#### `void logPrintf(const char* format, ...)`

**Purpose:** Output formatted message to Serial with automatic `[HH:MM:SS.sss]` timestamp prefix.

**Defined in:** `src/diagnostics.h` (line 21), implemented in `src/main.cpp` (lines ~2597-2612)

**Signature:**
```cpp
void logPrintf(const char* format, ...);
```

**Parameters:**
- `const char* format` — printf-style format string (same as Serial.printf)
- `...` — Variable arguments matching format specifiers

**Return Value:** void (no return value)

**Implementation Details:**

1. **Timestamp Calculation:**
   ```cpp
   uint32_t elapsedMs = millis();
   uint8_t h = (elapsedMs / 3600000) % 24;
   uint8_t m = (elapsedMs / 60000) % 60;
   uint8_t s = (elapsedMs / 1000) % 60;
   uint16_t ms = elapsedMs % 1000;
   // Format: [HH:MM:SS.sss]
   ```

2. **Message Formatting:**
   - Formats user message via `vsnprintf()` into 512-byte buffer
   - Skips empty messages (logPrintf("") produces no output)

3. **Output:**
   - Prepends timestamp and appends newline automatically
   - Calls `Serial.printf("[%02d:%02d:%02d.%03d] %s\n", h, m, s, ms, buffer)`

**Performance Impact (FR-027):**
- Per-message overhead: ~50-100 µs (integer arithmetic only, no floating-point)
- Memory: 50 bytes stack (format buffer), 500 bytes code
- No impact on loop cycle time (logging not on critical path)
- Meets <5% performance overhead requirement

**Usage Examples:**

```cpp
// Simple message
logPrintf("[SENSOR] BME280 read time: %lu µs", duration);
// Output: [00:00:15.342] [SENSOR] BME280 read time: 12345 µs

// Gamepad button press
logPrintf("[INPUT] GAMEPAD Button Y: PRESSED");
// Output: [00:00:15.456] [INPUT] GAMEPAD Button Y: PRESSED

// Page navigation with source
logPrintf("[MODE] page change: %d → %d (source: %s)", oldPage, newPage, "GAMEPAD");
// Output: [00:00:15.600] [MODE] page change: 2 → 3 (source: GAMEPAD)

// Multiple parameters
logPrintf("[I2C] Device at 0x%02X returned value 0x%04X", address, value);
// Output: [00:00:20.123] [I2C] Device at 0x61 returned value 0x0142
```

**Important Notes:**
- **Do NOT include `\n` in format strings** — logPrintf() appends newline automatically
- Including `\n` creates double newlines and blank output lines (incorrect behavior)
- Empty calls `logPrintf("")` are filtered and produce no output (keeps logs clean)

**Related Requirements:** FR-024 (Timestamped Logging), BR-031 (Timestamp Format & Calculation)

---

### 4b. Version & Build Constants (FEATURE-SPEC-012)

**Definitions (in src/diagnostics.h, lines 11-12):**
```cpp
#define VERSION "3.1.0"
#define BUILD_DATE "2026-10-22T17:20:00Z"
```

**Purpose:** Embedded version and build timestamp for diagnostics, release tracking, and firmware identification.

**Version Format:**
- Semantic versioning: `MAJOR.MINOR.PATCH`
- Example: `3.1.0` (version 3, feature 1, patch 0)
- Updated after each feature merge or patch release

**Build Date Format:**
- ISO 8601 UTC: `YYYY-MM-DDTHH:MM:SSZ`
- Example: `2026-10-22T17:20:00Z` (October 22, 2026 at 17:20:00 UTC)
- Set at build time; updated with each release

**Update Procedure:**

1. **Edit src/diagnostics.h** (lines 11-12)
2. **Update VERSION:** Increment MAJOR, MINOR, or PATCH per semantic versioning
3. **Update BUILD_DATE:** Set to current build timestamp in ISO 8601 format
4. **Compile firmware:** Version changes take effect at next build
5. **Deploy to device:** Run `pio run -t upload`
6. **Verify:** Connect to serial monitor; should see new version in boot banner

**Boot Output Example:**
```
[00:00:00.000] ========================================
[00:00:00.001] Sensor Pages Dashboard
[00:00:00.002] Version: 3.1.0
[00:00:00.003] Build Date: 2026-10-22T17:20:00Z
[00:00:00.004] Compiler: arm-none-eabi-g++ 9.3.1
[00:00:00.005] ESP32-WROOM-32 Detected
[00:00:00.006] ========================================
```

**Usage in Code:**
```cpp
// Output during setup() via logPrintf() (includes BR-031 timestamps)
logPrintf("Version: %s", VERSION);
logPrintf("Build Date: %s", BUILD_DATE);
```

**Accessibility:**
- Readable from serial monitor at every boot
- Available for debugging and release identification
- No API to query at runtime (future enhancement possible)

**Related Requirements:** FR-026 (Version & Build Information Display), BR-037 (Version & Build Info at Boot)

---

## 5. I2C Read Functions (Low-Level Helpers)

#### readReg16(address, reg, &value)

**Purpose:** Read 16-bit register value from any I2C device (LSB:MSB byte order).

**Parameters:**
```cpp
uint8_t address    // I2C device address
uint8_t reg        // Register offset
uint16_t &value    // Output: 16-bit value (by reference)
```

**Returns:** bool (true if successful, false on I2C error)

**Protocol:**
```
1. Wire.beginTransmission(address)
2. Wire.write(reg)
3. Wire.endTransmission(false)          // false = repeated start
4. Wire.requestFrom(address, 2)         // Request 2 bytes
5. if (bytes_received == 2):
     lsb = Wire.read()
     msb = Wire.read()
     value = (msb << 8) | lsb
     return true
   else:
     return false
```

---

#### writeReg16(address, reg, value)

**Purpose:** Write 16-bit register value to any I2C device (LSB:MSB byte order).

**Parameters:**
```cpp
uint8_t address   // I2C device address
uint8_t reg       // Register offset
uint16_t value    // 16-bit value to write
```

**Returns:** bool (true if successful, false on I2C error)

**Protocol:**
```
1. Wire.beginTransmission(address)
2. Wire.write(reg)
3. Wire.write(value & 0xFF)     // LSB first
4. Wire.write(value >> 8)       // MSB second
5. Wire.endTransmission()
6. return (result == 0)
```

---

## 6. 1-Wire Bus Protocol (via DS2484 I2C Adapter)

### Overview

**Interface:** I2C-to-1-Wire adapter (DS2484 @ 0x18)  
**Topology:** Single-wire, parasitic or external power  
**Devices:** DS18B20, DS1822, DS1820, and other compatible 1-Wire temperature sensors (max 3)  
**Addressing:** 64-bit ROM addresses (8-byte device IDs)  

### 1-Wire Temperature Sensors (via DS2484 @ 0x18)

**Purpose:** Read temperature from multiple 1-Wire sensors via I2C-to-1-Wire bridge (3 second async polling, 60 second bus scan).

**Protocol:** DallasTemperature library (OneWire abstraction over DS2484 I2C interface).

**Polling Schedule:**
- **Bus Scan (60 seconds):** Detect new sensors, verify presence of known sensors
- **Temperature Conversion (3 seconds):** Request measurements, read values asynchronously

**Supported Sensor Types:**
- DS18B20 (Family Code 0x28)
- DS1822 (Family Code 0x22)
- DS1820 (Family Code 0x10)
- Other DallasTemperature-compatible sensors

**Max Sensors:** 3 per adapter (Adafruit hardware limit).

**Resolution (Standard 12-bit):**
- Conversion time: ~750 ms per sensor
- Accuracy: 0.0625 °C
- Temperature range: -55 °C to +125 °C

### Async State Machine (readDs2484Temperatures)

**Cycle Duration:** 100 ms (empirically set, not adjustable).

**State Flow:**

```
State: waiting
  ├─ Call requestTemperatures()      (start conversion on all devices)
  ├─ Set pending=true
  ├─ Record timestamp
  └─ Return

State: converting (100 ms wait)
  ├─ Check (millis() - timestamp) < 100?
  ├─ If yes: Return (wait more)
  └─ If no: Fetch temperatures

State: fetching
  ├─ For each device i:
  │   temperatures[i] = getTempC(addresses[i])
  ├─ Set pending=false
  └─ Return (restart cycle)
```

**Temperature Sentinel:**
- If `getTempC()` returns `DEVICE_DISCONNECTED_C` (~–127.0 °C):
  - Sensor is disconnected or malfunctioning.
  - Display shows "disconnected" in red.
  - [UNKNOWN] Exact sentinel value depends on DallasTemperature library version.

### Device Enumeration & Hot-Plug Detection (rescanDs18b20)

**Frequency:** Every 3000 ms (configurable via `DS18B20_RESCAN_MS`).

**Logic:**
```cpp
1. ds18b20Sensors.begin()             // Re-scan bus
2. newCount = min(getDeviceCount(), MAX_DS18B20_SENSORS)
3. For each device:
     getAddress(index, newAddresses[index])
4. changed = (newCount != previous_count) ||
             (memcmp(newAddresses, previous_addresses, size) != 0)
5. if (changed):
     Update global addresses & count
     Zero-initialize temperatures
     Set pending=false (reset async state machine)
     Return true (trigger page redraw if on page 1)
6. Return false (no change)
```

**Device Limit:** Max 8 devices (`MAX_DS18B20_SENSORS`); excess silently capped.

**Failure Handling:** If bus enumeration fails, previous address list retained (no error indication).

---

## 7. SPI Bus Protocol

### Overview

**Master:** ESP32 (Hardware SPI)  
**Clock:** ~80 MHz (from Freenove TFT_eSPI setup header)  
**Slave:** ST7796 TFT Display Controller  
**Pins:** MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, RST=-1  

### TFT Display Interface (Write-Only)

**Purpose:** Display sensor data on 320×480 pixel screen.

**Protocol:** Hardware SPI via TFT_eSPI library (no read commands used; write-only).

**Supported Commands (TFT_eSPI subset):**

| Function | Purpose | Parameters |
|----------|---------|---|
| `tft.init()` | Initialize display | (none) |
| `tft.setRotation(3)` | Set orientation | 3 = landscape |
| `tft.fillScreen(color)` | Clear screen | 16-bit color (RGB565) |
| `tft.setTextColor(fg, bg)` | Set text color | Both 16-bit RGB565 |
| `tft.drawString(text, x, y, font)` | Draw text | Coordinates, font size 1–4 |
| `tft.drawRightString(text, x, y, font)` | Right-aligned text | Right-justified at x |
| `tft.drawFastHLine(x, y, length, color)` | Draw horizontal line | Pixels |
| `tft.setTextDatum(datum)` | Set alignment | TL_DATUM = top-left |
| `tft.setTextPadding(width)` | Clear text background | Width in pixels |

**Color Scheme:**

| Constant | RGB | Usage |
|----------|-----|-------|
| TFT_BLACK | 0x0000 | Background |
| TFT_WHITE | 0xFFFF | Default text |
| TFT_CYAN | 0x07FF | Page titles |
| TFT_DARKGREY | 0x39E7 | Dividers, footers |
| TFT_YELLOW | 0xFFE0 | I2C addresses |
| TFT_GREEN | 0x07E0 | Sensor readings (pass) |
| TFT_ORANGE | 0xFD20 | Sensor readings (warning) |
| TFT_RED | 0xF800 | Error messages |
| TFT_CYAN | 0x07FF | Computed/derived values (bright, sunlight-readable) |

**⚠️ Avoid:** TFT_MAGENTA (0xF81F) — Not readable in bright environments

**Layout Constants:**

| Coordinate | Pixel | Purpose |
|-----------|-------|---------|
| Title x | 6 | Left margin |
| Title y | 8 | Top margin |
| Divider y | 42 | Below title |
| First row y | 64 | First data row (BME280 page) |
| Row height | 24 | Spacing between rows |
| Value x | 474 | Right edge (width - 6) |
| Footer y | 294 | Bottom (480 - 186) |

**Failure Handling:** No error indication if SPI write fails; display simply doesn't update.

---

## 8. Serial UART Interface

### Overview

**Baud Rate:** 115200  
**Direction:** Unidirectional (ESP32 → Host only)  
**Purpose:** Debug output, warnings  

### Serial Output Format

**[UNKNOWN]** No structured logging framework. Only one documented output:

**Boot-Time Warning:**
```
WARNING: Gamepad QT not found at 0x50
```

**When emitted:** If `gamepad.begin()` fails at boot and `available.gamepad` becomes false.

**Side Effects:** Page navigation disabled (Y/A buttons not functional).

**No other serial output** documented in firmware.

---

## 9. What Does NOT Exist

### ❌ HTTP / REST APIs
- No HTTP server.
- No REST endpoints.
- No JSON serialization.

### ❌ Web Services
- No WebSocket support.
- No MQTT broker.
- No CoAP endpoints.

### ❌ RPC / Message Queues
- No gRPC.
- No message broker (RabbitMQ, Kafka, etc.).
- No event streams.

### ❌ Webhooks / Callbacks
- No webhook registration.
- No external event listeners.

### ❌ File APIs
- No file upload/download.
- No multipart/form-data handling.

### ❌ Authentication / Authorization
- No API keys.
- No OAuth / JWT.
- No role-based access control.

### ❌ Pagination / Filtering
- No query parameters.
- No sorting options.
- No pagination cursors.

### ❌ OpenAPI / Swagger
- No API specification document.
- No schema definition.

### ❌ Rate Limiting / Quotas
- No API rate limits.
- No usage tracking.

---

## 10. Data Models (DTOs)

All data structures are **in-memory C structs**, not serializable DTOs in the HTTP sense. However, for completeness:

### SensorStatus (Detection Flags)

```cpp
struct SensorStatus {
  bool ina219;      // INA219 @ 0x40 present
  bool veml6075;    // VEML6075 @ 0x10 present
  bool tsl2591;     // TSL2591 @ 0x29 present
  bool encoder;     // Encoder seesaw @ 0x36 present
  bool veml6070;    // VEML6070 @ 0x38/0x39 present
  bool si1145;      // SI1145 @ 0x60 present
  bool scd30;       // SCD30 @ 0x61 present
  bool bme280;      // BME280 @ 0x77 present
  bool gamepad;     // Gamepad seesaw @ 0x50 present
};
```

### Ina219Values (Power Monitor)

```cpp
struct Ina219Values {
  float shunt;           // Shunt voltage (mV), negated
  float bus;             // Bus voltage (V)
  float load;            // Load voltage = bus - shunt/1000 (V)
  float current;         // Current (mA), negated
  float power;           // Power (mW)
  float chargeMah;       // Coulomb counter (mAh) — accumulator
  bool initialized;      // First-read flag
  uint32_t lastSampleUs; // Last sample timestamp (µs)
};
```

### Veml6075Values (UV Sensor)

```cpp
struct Veml6075Values {
  uint16_t uva;       // Raw UVA ADC count
  uint16_t uvb;       // Raw UVB ADC count
  uint16_t comp1;     // Compensation factor 1
  uint16_t comp2;     // Compensation factor 2
  float uvaComp;      // Compensated UVA
  float uvbComp;      // Compensated UVB
  float index;        // UV index
};
```

### Veml6070Values (UV Sensor)

```cpp
struct Veml6070Values {
  uint16_t raw;   // Combined 16-bit raw ADC
  uint8_t lsb;    // Low byte (addr 0x38)
  uint8_t msb;    // High byte (addr 0x39)
};
```

### Scd30Values (CO₂ Sensor)

```cpp
struct Scd30Values {
  float co2;      // CO₂ (ppm)
  float temp;     // Temperature (°C)
  float humidity; // Relative humidity (%)
};
```

### EncoderValues (Rotary Encoder)

```cpp
struct EncoderValues {
  int32_t position;       // Absolute position (ticks)
  int32_t delta;          // Change since last read
  bool pressed;           // Button held?
  uint32_t presses;       // Cumulative press count
  uint8_t brightness;     // LED brightness (0–255)
  const char *color;      // Color name string
};
```

### GamepadValues (Gamepad Joystick)

```cpp
struct GamepadValues {
  int x;              // Joystick X (0–1023)
  int y;              // Joystick Y (0–1023)
  uint32_t buttons;   // Button bitmask
  uint8_t eventCount; // Pending events
  uint8_t lastEvent;  // Last keypad event (packed)
  bool hasEvent;      // Event valid?
};
```

### Ds18b20Values (Temperature Sensors)

```cpp
struct Ds18b20Values {
  DeviceAddress addresses[MAX_DS18B20_SENSORS];  // 8-byte ROM per device
  float temperatures[MAX_DS18B20_SENSORS];       // Temperature per device
  uint8_t count;                                  // Number of devices (0–8)
};
```

---

## 11. Boot Logger API (FEATURE-SPEC-013) — **NEW**

### Overview

Boot logger provides TFT display output during system initialization. Messages appear without timestamps (unlike Serial logPrintf), enabling visual confirmation of boot progress.

### API Functions

#### bootLoggerInit()

**Signature:**
```cpp
void bootLoggerInit(TFT_eSPI& tft);
```

**Purpose:**  
Activate boot terminal mode on TFT display. Called after TFT initialization completes.

**Parameters:**
- `tft`: Reference to TFT_eSPI display object

**Behavior:**
- Initializes boot log buffer (30-line circular buffer)
- Clears TFT display and draws boot terminal header
- Sets text color to TFT_WHITE on TFT_BLACK
- Ready to receive messages via bootLogPrintf()

**Return Value:** None

**Side Effects:**
- TFT display cleared and repainted
- Global boot logger state initialized

#### bootLogPrintf()

**Signature:**
```cpp
void bootLogPrintf(const char* category, const char* format, ...);
```

**Purpose:**  
Add a formatted message to boot log (TFT display). Similar to logPrintf but without timestamps.

**Parameters:**
- `category`: Log category prefix (e.g., "BOOT", "I2C", "SETUP", "SENSOR", "ERROR")
- `format`: Printf-style format string
- `...`: Variable arguments (same as printf)

**Behavior:**
- Formats message: `[CATEGORY] message text`
- Adds line to circular buffer
- Triggers TFT re-render if changed
- Maximum line length: 60 characters (truncated)
- Messages without timestamps

**Return Value:** None

**Examples:**
```cpp
bootLogPrintf("I2C", "Bus initialized at 400 kHz");
bootLogPrintf("SETUP", "Initializing sensors...");
bootLogPrintf("ERROR", "Sensor initialization failed");
```

**Differences from logPrintf():**
| Aspect | bootLogPrintf | logPrintf |
|--------|---------------|-----------|
| Output | TFT display | Serial (USB) |
| Timestamps | NO | YES ([HH:MM:SS.sss]) |
| Activation | After TFT ready | Immediate |
| Buffer | ~30 lines RAM | Unbounded (disk file) |
| Format | `[CATEGORY] text` | `[HH:MM:SS.sss] [CATEGORY] text` |

#### bootLoggerComplete()

**Signature:**
```cpp
void bootLoggerComplete();
```

**Purpose:**  
Exit boot terminal mode and transition to normal operation. Called after "BOOT COMPLETE" message and 1000ms delay.

**Behavior:**
- Disables boot logger
- Clears boot log buffer (memory free)
- Releases TFT from terminal mode
- Sets up for first normal page display

**Return Value:** None

**Side Effects:**
- Boot terminal messages discarded
- TFT display cleared for first sensor page

#### bootLoggerRender()

**Signature:**
```cpp
void bootLoggerRender();
```

**Purpose:**  
Manually render current boot log to TFT display. Called internally after each bootLogPrintf().

**Behavior:**
- Scrolls old messages off top
- Adds new line at bottom
- Updates TFT display in real-time

**Return Value:** None

**Note:** Usually called automatically; manual call not needed.

---

## 12. System Information Page API (FEATURE-SPEC-013) — **NEW**

### Overview

System information page (page 10) displays runtime diagnostics. Functions provide data updates and page rendering.

### API Functions

#### pageSystemInfo()

**Signature:**
```cpp
void pageSystemInfo();
```

**Purpose:**  
Render system information page (page 10). Called by drawCurrentPage() when currentPage == 10.

**Behavior:**
- Draws page title: "SYSTEM INFORMATION"
- Displays version, build date, uptime, heap, errors
- Updates dynamic values (uptime, heap, errors)
- Handles color coding (red errors, green labels)
- Draws navigation hint at bottom

**Return Value:** None

**Called By:** drawCurrentPage() in main loop (every 500 ms)

#### updateSystemInfoData()

**Signature:**
```cpp
void updateSystemInfoData();
```

**Purpose:**  
Refresh all system information values before rendering. Updates uptime, heap, error counts.

**Behavior:**
- Recalculates uptime from millis()
- Reads current free heap via ESP.getFreeHeap()
- Reads min free heap via ESP.getMinFreeHeap()
- Reads error counts from diagStats
- Updates all global SystemInfoData fields

**Return Value:** None

**Called By:** pageSystemInfo() at page refresh rate (500 ms)

#### getUptimeMs()

**Signature:**
```cpp
uint32_t getUptimeMs();
```

**Purpose:**  
Get current uptime in milliseconds since boot.

**Return Value:**  
Milliseconds elapsed since setup() started (from millis()).

**Used By:** updateSystemInfoData() for uptime calculation

#### formatUptimeString()

**Signature:**
```cpp
String formatUptimeString(uint32_t ms);
```

**Purpose:**  
Convert milliseconds to formatted uptime string (HH:MM:SS).

**Parameters:**
- `ms`: Milliseconds since boot

**Return Value:**  
String in format "HH:MM:SS" (e.g., "00:12:34")

**Formula:**
```cpp
hours = (ms / 1000) / 3600
minutes = ((ms / 1000) % 3600) / 60
seconds = (ms / 1000) % 60
// Return formatted string
```

---

## 12. Communication Patterns

### Read Pattern (Polling Sensors)

```
Initialize I2C (400 kHz, SDA=32, SCL=25) @ setup()
├─ Detect sensors (SensorStatus flags)
└─ For each sensor reading cycle:
    ├─ Time-gate check (500 ms, 50 Hz, etc.)
    ├─ Read I2C registers via Adafruit or custom protocol
    ├─ Validate (CRC for SCD30)
    ├─ Update global value struct
    └─ Display struct fields on TFT
```

### Write Pattern (LED Control)

```
Encoder button press detected @ readEncoderAt50Hz()
├─ Increment colorIndex (mod 4)
├─ Call updateEncoderLed()
│   ├─ encoderLed.setPixelColor(0, colors[colorIndex])
│   ├─ encoderLed.setBrightness(encoderBrightness)
│   └─ encoderLed.show()  [I2C write via seesaw]
└─ Update encoderValues.color (for display)
```

### Async Pattern (DS18B20 Non-Blocking Read)

```
Cycle 1:
  requestTemperatures()     [Start conversion; 100 ms latency]
  pending = true
  return

Cycles 2–95:
  if (elapsed < 100 ms) return   [Wait in background]

Cycle 96+:
  for each device:
    temperatures[i] = getTempC(address[i])  [Fetch result]
  pending = false
  restart cycle
```

---

## 13. Error Handling (API Level)

### I2C Read Failure
- **Symptom:** `Wire.endTransmission()` != 0 or `Wire.requestFrom()` returns fewer bytes.
- **Action:** Return false from read function.
- **Side Effect:** Value struct not updated; old value persists.
- **Display:** No change (stale data shown).

### CRC Validation Failure (SCD30)
- **Symptom:** Computed CRC8 ≠ received CRC8 for any of 3 fields.
- **Action:** `readScd30()` returns false.
- **Side Effect:** `scd30Values` not updated; old values persist.
- **Display:** CO₂/temp/humidity frozen at last valid read.

### Sensor Not Detected (Boot)
- **Symptom:** `begin()` / probe returns false.
- **Action:** `available.X = false`.
- **Side Effect:** Page for that sensor shows error screen ("Sensor nicht vorhanden", I2C address in yellow).
- **Display:** Error message + navigation help + no data rows.

### DS18B20 Device Disconnect
- **Symptom:** ROM address missing from next 3-second rescan.
- **Action:** Device removed from `addresses` array; `count` decremented.
- **Side Effect:** Temperature row for that device disappears.
- **Display:** Only remaining devices shown; no error message.

### Temperature Sentinel (DS18B20)
- **Symptom:** `getTempC()` returns `DEVICE_DISCONNECTED_C` (~–127.0 °C).
- **Action:** No explicit check in firmware; value passed through.
- **Display:** Very large negative value (or "disconnected" label if added by user code).

### Gamepad Not Present (Boot)
- **Symptom:** `gamepad.begin()` returns false.
- **Action:** `available.gamepad = false`; warning printed to Serial.
- **Side Effect:** Page navigation disabled; Y/A buttons non-functional.
- **Display:** Boot-time warning only; game pad page shows error.

---

## 13. Timing & Scheduling (API Perspective)

### Read Frequencies

| Component | Interval | Gate | Precision |
|-----------|----------|------|-----------|
| Gamepad | 25 ms | `(int32_t)(millis() - nextGamepadMs) >= 0` | millis() |
| Encoder | 20 ms | `(int32_t)(millis() - nextEncoderMs) >= 0` | millis() |
| INA219 | 20 µs | `(int32_t)(micros() - nextInaSampleUs) >= 0` | micros() |
| Sensors (VEML/SCD30) | 500 ms | `(int32_t)(millis() - nextSensorMs) >= 0` | millis() |
| DS18B20 Rescan | 3000 ms | `(int32_t)(millis() - nextDs18b20RescanMs) >= 0` | millis() |
| TFT Refresh | 20 ms (live pages), 500 ms (normal) | Composite gate | millis() |

### Signed Comparison for Rollover Safety

```cpp
// Handles uint32_t rollover every 49 days (millis) or 71 min (micros)
if ((int32_t)(now - deadline) >= 0) {
  // Execute action
  deadline = now + interval;
}
```

---

## 14. Concurrency Model

**Single-threaded, no async/await.**

All communication is synchronous, blocking, non-concurrent. No multitasking at application level (Arduino framework runs FreeRTOS internally, but firmware doesn't create tasks).

---

## 15. Data Serialization

### No Serialization to Disk
- No JSON encoding.
- No Protocol Buffers.
- No MessagePack.

### Implicit Serialization: I2C & 1-Wire
- Sensor data is deserialized FROM hardware (I2C byte reads → floats).
- Data is NOT serialized TO hardware (read-only sensor interaction).

### Display Output
- Floats converted to strings via `String(float, precision)`.
- Strings rendered as pixels on TFT via SPI.

---

## 16. Non-Blocking Rotated Sensor Reading (FEATURE-SPEC-006) — **NEW**

### Overview

Five new functions implement the non-blocking rotated sensor architecture. Sensors are read in a 7-slot rotation (200 ms per slot = 1.4 s full cycle), with async measurement patterns for long-duration sensors (TSL2591, DS18B20).

---

### `void rotatedReadSensor(uint32_t slot)`

**Purpose:** Dispatcher function that handles the current sensor rotation slot.

**Signature:**
```cpp
void rotatedReadSensor(uint32_t slot);
```

**Parameters:**
- `slot` (uint32_t): Slot number 0–6, derived from `(millis() / SENSOR_SLOT_INTERVAL_MS) % 7`

**Returns:** void

**Behavior:**
```cpp
switch (slot) {
  case 0: readBme280(bme280Values); break;           // 0 ms
  case 1: startTsl2591Measurement(); break;          // 200 ms (async start)
  case 2: readSi1145(si1145Values); break;           // 400 ms
  case 3: readVeml6075(veml6075Values); break;       // 600 ms
  case 4: readVeml6070(veml6070Values); break;       // 800 ms
  case 5: readScd30(scd30Values); break;             // 1000 ms
  case 6: startDs18b20Conversion(); break;           // 1200 ms (async start)
           checkTsl2591Async();                       // Check TSL2591 readiness
           checkDs18b20Async(); break;                // Check DS18B20 readiness
}
```

**Side Effects:** Updates global sensor cache structs; may log to Serial.

**Calling Pattern:**
```cpp
// In main loop:
static uint32_t lastSlot = 0xFFFFFFFF;
uint32_t currentSlot = (millis() / SENSOR_SLOT_INTERVAL_MS) % 7;
if (currentSlot != lastSlot) {
  rotatedReadSensor(currentSlot);
  lastSlot = currentSlot;
}
```

**Performance:** Maximum blocking per slot: 200 ms (1 sensor read duration).

---

### `void startTsl2591Measurement()`

**Purpose:** Initiate TSL2591 asynchronous measurement (start ADC integration, return immediately).

**Signature:**
```cpp
void startTsl2591Measurement();
```

**Parameters:** None

**Returns:** void

**Behavior:**
1. Guard: Return if `available.tsl2591 = false`
2. Set `tsl2591State = MEASURING`
3. Record measurement start time: `tsl2591MeasureStartMs = millis()`
4. Call Adafruit_TSL2591 setup function to start integration timer (implementation-specific)
5. Log to Serial: `"TSL2591 async START time: XXX µs"` (duration of setup call)
6. Return immediately (non-blocking)

**State Machine:**
```cpp
static enum Tsl2591State { IDLE, MEASURING, READY } tsl2591State = IDLE;
static uint32_t tsl2591MeasureStartMs = 0;
```

**Next Step:** `checkTsl2591Async()` called ~200+ ms later to read result.

**Called by:** `rotatedReadSensor()` Slot 1 (200 ms).

**Note:** Do not call `getFullLuminosity()` here; only initialize measurement.

---

### `void checkTsl2591Async()`

**Purpose:** Check if TSL2591 measurement is complete; read result if ready.

**Signature:**
```cpp
void checkTsl2591Async();
```

**Parameters:** None

**Returns:** void

**Behavior:**
1. Guard: Return if `tsl2591State != MEASURING`
2. Calculate elapsed time: `elapsedMs = millis() - tsl2591MeasureStartMs`
3. If `elapsedMs < TSL2591_INTEGRATION_TIME_MS`:
   - Return without reading (not ready, will retry next cycle)
4. If `elapsedMs >= TSL2591_INTEGRATION_TIME_MS`:
   - Call `readTsl2591(tsl2591Values)` to fetch result and cache it
   - Set `tsl2591State = IDLE`
   - Return

**Configuration:**
```cpp
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;  // Typical 200 ms (adjustable)
```

**Called by:** `rotatedReadSensor()` Slot 6 or 7 (after 200+ ms elapsed).

**Benefit:** Eliminates 200 ms blocking; measurement happens in background.

---

### `void startDs18b20Conversion()`

**Purpose:** Initiate 1-Wire asynchronous temperature conversion for all DS18B20 sensors.

**Signature:**
```cpp
void startDs18b20Conversion();
```

**Parameters:** None

**Returns:** void

**Behavior:**
1. Guard: Return if `available.ds2484 = false`
2. Set `ds18b20State = CONVERTING`
3. Record conversion start time: `ds18b20ConvertStartMs = millis()`
4. Send 1-Wire conversion command to all DS18B20 sensors on bus
   - Implementation: Use Adafruit_DS248x API (e.g., `ds2484.wireWriteBytes([0xCC, 0x44])`)
5. Log to Serial: `"DS18B20 async START time: XXX µs"` (duration of command send)
6. Return immediately (non-blocking)

**State Machine:**
```cpp
static enum Ds18b20State { IDLE, CONVERTING, READY } ds18b20State = IDLE;
static uint32_t ds18b20ConvertStartMs = 0;
```

**Next Step:** `checkDs18b20Async()` called ~750+ ms later (after conversion completes).

**Called by:** `rotatedReadSensor()` Slot 6 (1200 ms).

**Configuration:**
```cpp
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;  // 12-bit (adjustable; 8-bit=95ms, 10-bit=375ms)
```

**Note:** Do not read sensor values here; only send conversion command.

---

### `void checkDs18b20Async()`

**Purpose:** Check if DS18B20 conversion is complete; read results if ready.

**Signature:**
```cpp
void checkDs18b20Async();
```

**Parameters:** None

**Returns:** void

**Behavior:**
1. Guard: Return if `ds18b20State != CONVERTING`
2. Calculate elapsed time: `elapsedMs = millis() - ds18b20ConvertStartMs`
3. If `elapsedMs < DS18B20_CONVERSION_TIME_MS`:
   - Return without reading (not ready, will retry next cycle)
4. If `elapsedMs >= DS18B20_CONVERSION_TIME_MS`:
   - Call `readDs2484Temperatures(ds2484Values)` to fetch results and cache them
   - Set `ds18b20State = IDLE`
   - Return

**Configuration:**
```cpp
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;  // 12-bit standard (adjustable)
```

**Called by:** `rotatedReadSensor()` Slot 6 or next cycle Slot 7+ (after 750+ ms elapsed).

**Benefit:** Eliminates 750 ms blocking; 1-Wire conversion happens in background.

---

### `void updateInputStats()`

**Purpose:** Aggregate per-sensor timing statistics and prepare for serial reporting.

**Signature:**
```cpp
void updateInputStats();
```

**Parameters:** None

**Returns:** void

**Behavior:**
1. Called every `INPUT_STATS_INTERVAL_MS` (1000 ms, 1 Hz)
2. If `ENABLE_STATS_REPORTING = 0` (disabled):
   - Return immediately (no-op)
3. If `ENABLE_STATS_REPORTING = 1` (enabled):
   - Aggregate min/max/avg timing for each sensor read
   - Count number of successful reads per sensor
   - Print formatted stats table to Serial:
   ```
   === Sensor Stats (1000 ms) ===
   BME280:       min=2100 µs, max=2300 µs, avg=2156 µs, reads=5
   TSL2591:      async_starts=1, async_ready=1, avg_wait=205 ms
   SI1145:       min=650 µs, max=750 µs, avg=689 µs, reads=5
   ...
   ===================================
   ```
   - Reset all counters to zero for next interval

**Configuration:**
```cpp
#define ENABLE_STATS_REPORTING 1  // 1 = enabled (default), 0 = disabled
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;  // 1 second
```

**Called by:** Main loop, every 1000 ms.

**Side Effects:** Logs to Serial (potentially verbose).

**Performance Impact:** Minimal (only computation, no I/O except Serial write).

---

## Summary Table: All External Interfaces

| Interface | Protocol | Role | Devices | Data Direction | Failure Mode |
|-----------|----------|------|---------|---|---|
| **I2C @ 0x40** | I2C 400 kHz | Power monitoring | INA219 | Read (50 Hz) | Stale data |
| **I2C @ 0x10** | I2C 400 kHz | UV sensor | VEML6075 | Read (500 ms) | Stale data |
| **I2C @ 0x38/0x39** | I2C 400 kHz | UV sensor (dual addr) | VEML6070 | Read (500 ms) | Stale data |
| **I2C @ 0x29** | I2C 400 kHz | Light sensor | TSL2591 | Read (on-demand) | Disabled |
| **I2C @ 0x60** | I2C 400 kHz | Spectrum sensor | SI1145 | Read (on-demand) | Disabled |
| **I2C @ 0x61** | I2C 400 kHz | CO₂ sensor (CRC) | SCD30 | Read (500 ms) | Stale data (CRC fail) |
| **I2C @ 0x77** | I2C 400 kHz | Climate sensor | BME280 | Read (500 ms) | Stale data |
| **I2C @ 0x36** | I2C 400 kHz (seesaw) | Encoder + NeoPixel | Encoder | Read (50 Hz), Write (immediate) | LED freeze; stale position |
| **I2C @ 0x50** | I2C 400 kHz (seesaw) | Gamepad joystick | Gamepad | Read (40 Hz) | Navigation disabled |
| **1-Wire @ GPIO23** | 1-Wire | Temperature (async) | DS18B20 ×8 | Read (100 ms cycle) | Stale data |
| **SPI @ GPIO15** | SPI 80 MHz | Display graphics | ST7796 TFT | Write (multiple per refresh) | No visual update |
| **Serial @ 115200** | UART | Debug output | Host USB | Write (rare) | No debug output |

---

## Conclusion

This is a **low-level embedded hardware API**, not a web service API. All communication is **device-level protocols (I2C, 1-Wire, SPI)** with **no application-level abstraction, no HTTP, no REST, no RPC.**

To integrate with external systems, a developer would need to:
1. Read values from global struct instances (in-memory).
2. Add external interfaces (e.g., WiFi + MQTT, HTTP server, etc.) as new features.
3. No existing APIs support external communication.


## Appendix C: VEML Sensor Register Specifications

### VEML6075 Register Map (Vishay doc 84304)

- **Configuration (0x00):** Control register with bits for integration time, shutdown, and active-force mode.
  - **UV_IT[11:10]:** Integration time (`00` = 50 ms, `01` = 100 ms, `10` = 200 ms, `11` = 400 ms)
  - **UV_SD (bit 8):** Shutdown control
- **UVA Data (0x07):** 16-bit RO, raw UVA reading
- **UVB Data (0x09):** 16-bit RO, raw UVB reading
- **UV Compensation 1 (0x0A):** 16-bit RO, visible/IR channel 1
- **UV Compensation 2 (0x0B):** 16-bit RO, visible/IR channel 2
- **Device ID (0x0C):** 16-bit RO, expected value `0x0026`

### VEML6070 I2C Protocol (Vishay doc 84277)

- **Initialization:** Write `0x02` to address `0x38` (command write)
- **Read data:** LSB from `0x38`, MSB from `0x39`
- **Integration time options:** `0x02` = 125 ms, `0x12` = 250 ms, `0x22` = 500 ms, `0x32` = 1000 ms

## Appendix D: SCD30 I2C Protocol Specification

### Start Measurement (`0x0010`)

- **Format:** `[0x00, 0x10, pressure_MSB, pressure_LSB, CRC8]`
- **Pressure:** mbar (`0` = no compensation, `1013` mbar typical)
- **Response time:** 100 ms

### Get Data Ready Status (`0x0202`)

- **Command:** `[0x02, 0x02]`
- **Response:** `[status_MSB, status_LSB, CRC8]`
- **Ready indicator:** `status == 0x0001`

### Read Measurement (`0x0300`)

- **Command:** `[0x03, 0x00]`
- **Response:** 18 bytes with 6 CRC-protected half-words
- **Format:** `[CO2_H, CO2_L, CO2_CRC, CO2_H2, CO2_L2, CO2_CRC2, TEMP_H, TEMP_L, TEMP_CRC, TEMP_H2, TEMP_L2, TEMP_CRC2, RH_H, RH_L, RH_CRC, RH_H2, RH_L2, RH_CRC2]`
- **Float byte order:** Big-endian (MSB first)
- **Reassembly:** `bytes[3], bytes[2], bytes[1], bytes[0]` for each float

### CRC8 Algorithm

- **Polynomial:** `0x31`
- **Initial:** `0xFF`
- **Test vectors:**
  - `0xBE, 0xEF` → `0x92`
  - `0x00, 0x00` → `0x81`
  - `0x00, 0x01` → `0xB0`
  - `0x02, 0x02` → `0x3A`

---

## Appendix E: Large Display Mode Rendering API — **NEW (FEATURE-SPEC-007)**

### Unified Large Display Renderer

**Function:**
```cpp
void renderLargeDisplay(uint8_t sensorPage, uint8_t valueIndex);
```

**Purpose:**  
Render large display mode for any sensor page with single value focus, enlarged font, centered layout.

**Parameters:**
- `sensorPage` (uint8_t): Page index 0–9
- `valueIndex` (uint8_t): Value to display (0 to page-specific valueCount-1)

**Behavior:**
1. Clear TFT display (black background)
2. Retrieve sensor-specific data via combined helper function:
   ```cpp
   String value = getSensorLargeDisplayData(sensorPage, valueIndex, &label, &unit, &color);
   ```
3. Draw layout components:
   - **Title (y=80):** Sensor name (e.g., "BME280 Large Display"), Font 4, Cyan, centered
   - **Label (y=140):** Value name (e.g., "Temperatur"), Font 4, Cyan, centered
   - **Value (y=200):** Numeric/text value, Font 8 (largest), color-coded, centered
   - **Unit (y=280):** Measurement unit (e.g., "°C"), Font 2, White, centered
   - **Footer (y=350):** "B:next X:prev Y:exit", Font 1, Dark Grey, centered
4. Return (no rate limit applied here; rate limiting occurs in main loop)

**Return:** void

**Color Coding:**  
- Green (TFT_GREEN): Primary measurements, good readings
- Orange (TFT_ORANGE): Secondary measurements, caution
- Cyan (TFT_CYAN): Computed/derived values, indices
- Red (TFT_RED): Errors, out-of-range values
- **Never use Magenta** (not readable in sunlight)

**Performance:**  
Display update subject to 200 ms rate limit (FR-015) enforced in main loop.

---

### Sensor-Specific Large Display Helper Functions

**Generic Function Signature:**
```cpp
// All sensor-specific helpers follow this pattern:
String sensorType_getLargeDisplayData(uint8_t valueIndex,
                                       const char** outLabel,    // OUTPUT: e.g., "Temperatur"
                                       const char** outUnit,     // OUTPUT: e.g., "°C"
                                       uint16_t* outColor);      // OUTPUT: e.g., TFT_GREEN
// Returns: Formatted value string (e.g., "24.50")
```

**Implementation Pattern:**
1. Guard: Return empty string if `available.sensor = false`
2. Validate: Check `valueIndex` is within page-specific valueCount
3. Retrieve: Read value from global cache struct (no I2C calls)
4. Format: Convert to string with appropriate decimal places
5. Return: All four pieces of information (value, label, unit, color)

---

### Per-Page Large Display Helpers

#### Page 0: BME280
```cpp
String bme280_getLargeDisplayData(uint8_t valueIndex,
                                   const char** outLabel,
                                   const char** outUnit,
                                   uint16_t* outColor);
// Value count: 7
// Values: 0=Temperature, 1=Pressure, 2=Humidity, 3=Dew point, 4=Altitude, 5=AQI, 6=Temp trend
```

#### Page 1: Gamepad
```cpp
String gamepad_getLargeDisplayData(uint8_t valueIndex,
                                    const char** outLabel,
                                    const char** outUnit,
                                    uint16_t* outColor);
// Value count: 4
// Values: 0=Joystick X, 1=Joystick Y, 2=Button state, 3=Total presses
```

#### Page 2: INA219
```cpp
String ina219_getLargeDisplayData(uint8_t valueIndex,
                                   const char** outLabel,
                                   const char** outUnit,
                                   uint16_t* outColor);
// Value count: 7
// Values: 0=Runtime, 1=Voltage, 2=Supply, 3=Current, 4=Charge, 5=Shunt voltage, 6=Power
// NOTE: Existing implementation; refactored to use unified renderer
```

#### Page 3: Rotary Encoder
```cpp
String encoder_getLargeDisplayData(uint8_t valueIndex,
                                    const char** outLabel,
                                    const char** outUnit,
                                    uint16_t* outColor);
// Value count: 5
// Values: 0=Position, 1=Delta, 2=Presses, 3=LED color, 4=LED brightness
```

#### Page 4: SCD30 CO₂
```cpp
String scd30_getLargeDisplayData(uint8_t valueIndex,
                                 const char** outLabel,
                                 const char** outUnit,
                                 uint16_t* outColor);
// Value count: 3
// Values: 0=CO₂, 1=Temperature, 2=Humidity
```

#### Page 5: SI1145 UV/IR
```cpp
String si1145_getLargeDisplayData(uint8_t valueIndex,
                                  const char** outLabel,
                                  const char** outUnit,
                                  uint16_t* outColor);
// Value count: 4
// Values: 0=Visible light, 1=Infrared, 2=UV index, 3=UV raw
```

#### Page 6: TSL2591 Light
```cpp
String tsl2591_getLargeDisplayData(uint8_t valueIndex,
                                   const char** outLabel,
                                   const char** outUnit,
                                   uint16_t* outColor);
// Value count: 4
// Values: 0=Full spectrum, 1=Infrared, 2=Visible, 3=Lux
```

#### Page 7: VEML6070 UV
```cpp
String veml6070_getLargeDisplayData(uint8_t valueIndex,
                                    const char** outLabel,
                                    const char** outUnit,
                                    uint16_t* outColor);
// Value count: 3
// Values: 0=UV raw, 1=UV level, 2=LSB/MSB
```

#### Page 8: VEML6075 UV
```cpp
String veml6075_getLargeDisplayData(uint8_t valueIndex,
                                    const char** outLabel,
                                    const char** outUnit,
                                    uint16_t* outColor);
// Value count: 6
// Values: 0=UVA raw, 1=UVB raw, 2=UVA comp, 3=UVB comp, 4=UV index, 5=Compensation
```

#### Page 9: DS2484 1-Wire
```cpp
String ds2484_getLargeDisplayData(uint8_t valueIndex,
                                  const char** outLabel,
                                  const char** outUnit,
                                  uint16_t* outColor);
// Value count: 3–9 (3 per discovered sensor, max 3 sensors)
// Values: [Sensor 0: ROM-ID, Temp, Status], [Sensor 1: ROM-ID, Temp, Status], ...
// Dynamic based on discovered sensor count
```

#### Page 10: System Information
```cpp
void pageSystemInfo();
// Read-only system diagnostics page
// Displays: Version, Build Date, Uptime, Heap Status, Min Free Heap, Sensor Count, Error Summary
// Updates: Uptime and heap every 500ms, error counts on occurrence
// No large display mode (always shows full system view)
```

---

### Global Large Display State

```cpp
struct {
  bool active;                  // Large display mode currently active?
  uint8_t currentPage;          // Page index where mode entered (0-9)
  uint8_t valueIndex;           // Current value being displayed (0 to pageValueCount-1)
} largeDisplayMode;
```

**Initialization:**
```cpp
largeDisplayMode = {false, 0, 0};  // Boot: inactive, no page, index 0
```

---

### Integration in Page Rendering

**Example (Page 0 - BME280):**
```cpp
void pageBme280() {
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 0) {
    // Large display mode active on this page
    renderLargeDisplay(0, largeDisplayMode.valueIndex);
  } else {
    // Standard page rendering
    pageBme280Standard();  // Existing standard renderer
  }
}
```

**Every page renderer (0–9) follows same pattern.**

---

## 11. Python Serial Monitor API (FEATURE-SPEC-012) — **NEW**

### Overview

The **Python Serial Monitor** (`scripts/serial_monitor.py`) is a host-side tool that captures, analyzes, and persists firmware serial output for long-term diagnostics and post-mortem analysis.

**Location:** `scripts/serial_monitor.py` (369 lines, Python 3.6+)

**Dependency:** `pip install pyserial`

**Purpose:**
- Persistent logging to disk for 24+ hour stress testing
- Real-time terminal output with color-coding
- Intelligent I2C error categorization
- Search functionality for pattern matching
- Automatic daily log rotation

---

### Command-Line Interface

#### Basic Usage

**Normal mode (show all output):**
```bash
python3 scripts/serial_monitor.py
```

**Compact mode (show only critical errors, persist all):**
```bash
python3 scripts/serial_monitor.py --compact
```

**Background execution (recommended for long tests):**
```bash
python3 scripts/serial_monitor.py --compact &
nohup python3 scripts/serial_monitor.py --compact > /tmp/monitor.log 2>&1 &
```

**Post-mortem search:**
```bash
python3 scripts/serial_monitor.py --search "Error 263"
python3 scripts/serial_monitor.py --search "bus:locked"
python3 scripts/serial_monitor.py --search "CRITICAL"
```

#### Command-Line Flags

| Flag | Description | Default | Example |
|------|-------------|---------|---------|
| `--compact` | Filter terminal output to show only critical errors; persist all to disk | Off (normal mode) | `python3 scripts/serial_monitor.py --compact` |
| `--search TEXT` | Search logs for pattern matching TEXT; return matching lines | None | `python3 scripts/serial_monitor.py --search "Error 263"` |
| `--help` | Display usage help | — | `python3 scripts/serial_monitor.py --help` |

---

### Terminal Output Format (Normal Mode)

**Example output with timestamps and color-coding:**
```
🔌 Serial Monitor (NORMAL MODE)...
   Port: /dev/ttyUSB0
   Baudrate: 115200
   Log directory: logs/serial
   Log file: logs/serial/serial_monitor_2026-10-22.log
────────────────────────────────────────────────────────────────────────────────
✓ Connected to /dev/ttyUSB0

[23:06:05.935] [DS18B20] Results read: 1/1 sensors OK, took 38204 µs
[23:06:05.976] [DS18B20] Starting conversion for 1 sensors...
[23:06:06.456] [DS18B20] Conversion started (took 17212 µs)
[23:06:06.646] [I2C-TIMING] Sensors:value time:63 ms bus:free errors:0
[23:06:07.404] [I2C-TIMING] TSL2591:value time:240 ms bus:locked errors:0
[23:06:07.445] [DS18B20] Conversion ready, reading results...
```

**Color Codes:**
- 🔴 Red: ERROR, I2C bus errors, Critical conditions
- 🟡 Yellow: WARN, Slow I2C transactions (≥50ms)
- 🔵 Blue: INFO, Normal I2C transactions, sensor status
- ✓ Green: OK, Successful operations
- ⚪ Gray: Debug, verbose logging

---

### Terminal Output Format (Compact Mode)

**Compact mode filters terminal to show only critical errors; all data persisted to disk:**

```
🔌 Serial Monitor (COMPACT MODE)...
   Port: /dev/ttyUSB0
   Baudrate: 115200
   Log directory: logs/serial
   Log file: logs/serial/serial_monitor_2026-10-22.log
   Mode: COMPACT (only critical messages shown)
────────────────────────────────────────────────────────────────────────────────
✓ Connected to /dev/ttyUSB0

[23:06:09.423] [I2C-TIMING] TSL2591:value time:240 ms bus:locked errors:0
[23:06:09.475] [ERROR] I2C read failed for Encoder
[23:06:13.445] [I2C-TIMING] Sensors:value time:241 ms bus:free errors:0
[23:06:13] 📊 Lines: 600, Errors: 0, I2C-Errors: 1, Slow-I2C: 5
[23:06:15.437] [I2C-TIMING] TSL2591:value time:240 ms bus:locked errors:0
```

**Terminal shows:**
- ✓ Lines tagged SLOW (time ≥ 50ms)
- ✓ Lines tagged BUS_ERROR (errors > 0)
- ✓ Lines tagged CRITICAL (Error 263, "bus:locked")
- ✗ Hidden: Lines tagged OK (time < 50ms, errors:0, bus:free)
- ✓ Statistics every 200 lines showing aggregated error counts

**Statistics Metrics:**
- **Lines:** Total lines processed so far
- **Errors:** Count of lines containing "ERROR" keyword
- **I2C-Errors:** Count of [I2C-TIMING] lines with errors > 0
- **Slow-I2C:** Count of [I2C-TIMING] lines with time ≥ 50ms

---

### I2C Error Categorization Logic

The monitor automatically parses [I2C-TIMING] lines and categorizes them:

#### Categorization Rules

```
IF line contains "Error 263" OR "ERROR"
  CATEGORY = CRITICAL (bold red)
  DISPLAY_MODE = Always shown

ELSE IF line matches "bus:locked errors:N" where N > 0
  CATEGORY = BUS_ERROR (red)
  DISPLAY_MODE = Always shown

ELSE IF line matches "time:NNN ms" where NNN >= 50
  CATEGORY = SLOW (yellow)
  DISPLAY_MODE = Shown in compact mode

ELSE IF line matches "time:NNN ms" where NNN < 50 AND "errors:0" AND "bus:free"
  CATEGORY = OK (blue)
  DISPLAY_MODE = Hidden in compact mode, shown in normal mode

END IF
```

#### Performance Thresholds

| Metric | Threshold | Meaning |
|--------|-----------|---------|
| **Healthy I2C** | < 20 ms | Normal sensor read latency |
| **Slow I2C** | ≥ 50 ms | Performance degradation, watch for issues |
| **Critical Error** | Error 263, "bus:locked" | I2C bus stuck, requires intervention |
| **Bus Error** | errors > 0 | Transaction failed, sensor read incomplete |

#### Example Line Analysis

**Input line:**
```
[23:06:09.423] [I2C-TIMING] TSL2591:value time:240 ms bus:locked errors:0
```

**Categorization:**
1. Contains "bus:locked"? YES
2. Extract errors: errors:0 → error_count = 0
3. But contains "bus:locked" → CATEGORY = SLOW or higher?
4. time=240 ms ≥ 50 ms → CATEGORY = SLOW (yellow)
5. Actually: time=240 ms + bus:locked → Treat as performance issue + bus state indicator
6. Result: **SLOW category, yellow color, shown in compact mode**

---

### Log File Storage & Format

#### Directory Structure

```
logs/
└── serial/
    ├── serial_monitor_2026-10-22.log  (created on Oct 22)
    ├── serial_monitor_2026-10-23.log  (auto-rotated on Oct 23 at 00:00)
    └── serial_monitor_2026-10-24.log  (auto-rotated on Oct 24 at 00:00)
```

#### File Format

**Each line contains:**
```
[HH:MM:SS.mmm] <firmware logPrintf() output>
```

**Example:**
```
[23:06:05.935] [DS18B20] Results read: 1/1 sensors OK, took 38204 µs
[23:06:05.976] [DS18B20] Starting conversion for 1 sensors...
[23:06:06.646] [I2C-TIMING] Sensors:value time:63 ms bus:free errors:0
[23:06:09.423] [I2C-TIMING] TSL2591:value time:240 ms bus:locked errors:0
[23:06:13.445] [DS18B20] Conversion ready, reading results...
```

**Chronological Order:** Oldest entries at top, newest at bottom (opposite of typical terminal scrolling)

**Growth Rate:** 1–2 MB per hour during normal operation

#### Log Rotation

**Trigger:** Midnight UTC (00:00:00)

**Behavior:**
1. At midnight, new file created: serial_monitor_YYYY-MM-DD.log (next date)
2. Previous day's file closed (remains in logs/serial/)
3. No data loss during rotation
4. User can maintain archives separately

---

### Search Functionality

#### Search Command

```bash
python3 scripts/serial_monitor.py --search "pattern"
```

**Behavior:**
- Searches logs/serial/ directory for files matching serial_monitor_*.log
- Returns all lines containing pattern (case-insensitive)
- Shows filename and timestamp context
- Returns "No matches found" if pattern not present

#### Common Search Patterns

| Pattern | Use Case | Example Command |
|---------|----------|-----------------|
| `"Error 263"` | Find I2C timeout errors | `python3 scripts/serial_monitor.py --search "Error 263"` |
| `"bus:locked"` | Find I2C bus lockups | `python3 scripts/serial_monitor.py --search "bus:locked"` |
| `"CRITICAL"` | Find all critical errors | `python3 scripts/serial_monitor.py --search "CRITICAL"` |
| `"ERROR"` | Find any error condition | `python3 scripts/serial_monitor.py --search "ERROR"` |
| `"GAMEPAD"` | Find gamepad input events | `python3 scripts/serial_monitor.py --search "GAMEPAD"` |
| `"ENCODER"` | Find encoder input events | `python3 scripts/serial_monitor.py --search "ENCODER"` |
| `"[I2C-TIMING]"` | Find all I2C timing logs | `python3 scripts/serial_monitor.py --search "[I2C-TIMING]"` |

---

### Post-Mortem Analysis Workflow

#### Scenario: Device Hung at 03:45 UTC

**Step 1: Collect Evidence**
```bash
# Check which log file contains the time window
ls -la logs/serial/serial_monitor_*.log

# Example output shows: serial_monitor_2026-10-22.log
```

**Step 2: Search for Errors Around Time**
```bash
# Search for critical errors
python3 scripts/serial_monitor.py --search "Error 263"

# Extract lines from log manually
grep "\[03:4[0-5]" logs/serial/serial_monitor_2026-10-22.log | tail -50
```

**Step 3: Analyze I2C Transactions**
```bash
# Find slow I2C transactions before hang
grep "time:.*[5-9][0-9]\|time:.*[1-9][0-9][0-9]" logs/serial/serial_monitor_2026-10-22.log | tail -20
```

**Step 4: Correlate with Input Events**
```bash
# Find input activity around the time
grep "\[INPUT\]" logs/serial/serial_monitor_2026-10-22.log | tail -20
```

**Step 5: Diagnose Root Cause**
- If errors before hang: I2C communication failure
- If performance degradation trend: Accumulating I2C issues
- If no errors: Possible firmware bug or interrupt issue

---

### Performance Characteristics

**Disk I/O:**
- Writes: ~1–2 MB/hour (line buffered, flushed immediately)
- Operations: Non-blocking (pyserial handles buffering)
- No impact on firmware

**Memory:**
- Python process: ~20–50 MB resident
- Runs on host (not ESP32)
- No impact on firmware RAM/flash

**Serial Throughput:**
- Baud rate: 115200 = ~14.4 KB/sec theoretical max
- Expected rate: ~50–100 messages/min = 200–400 bytes/min
- Utilization: ~0.3% (well within limits)
- No data loss observed

---

### Integration with Firmware (logPrintf)

The Python monitor consumes output from firmware's `logPrintf()` function:

```
Firmware logPrintf() → Serial UART → /dev/ttyUSB0 → Python monitor
                                                   → Terminal output
                                                   → Disk log file
```

**No changes to firmware required** — Monitor is completely passive and reads from Serial output.

**Works immediately after firmware deployment and logPrintf() implementation.**

---

### Related Documentation

- **Architecture:** docs/02-architecture.md § 3.10.1 (Host-Level Monitoring)
- **Requirements:** docs/01-requirements.md § FR-024b (External Persistent Monitor)
- **Reconstruction:** docs/14-reconstruction-plan.md § Phase 5c (Python Monitor Setup)
- **User Guide:** SERIAL_MONITOR_README.md (comprehensive documentation)
- **Quick Reference:** SERIAL_MONITOR_QUICK_REFERENCE.txt (one-page guide)
