# Configuration: sensor-pages-dashboard

**Last Updated:** 2026-09-21  
**Configuration Type:** Embedded Firmware (PlatformIO + Arduino)  
**Environment Support:** Development (hardware-in-the-loop)

---

## Overview

sensor-pages-dashboard is an embedded ESP32 firmware project. Configuration is defined via:
1. **Build-time configuration** (platformio.ini, User_Setup.h)
2. **Compile-time constants** (src/main.cpp #define statements)
3. **Hardware pin assignments** (GPIO mappings in headers)
4. **Serial port configuration** (monitor/upload ports)

**No runtime configuration or environment variables exist** — all settings are compile-time constants.

---

## Build Configuration File: platformio.ini

**Location:** `sensor-pages-dashboard/platformio.ini`

```ini
[env:freenove-fnk0114n-sensor-pages]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
monitor_port = /dev/ttyUSB0
upload_speed = 921600
upload_port = /dev/ttyUSB0

lib_extra_dirs =
  ../lib/Freenove

lib_deps =
  adafruit/Adafruit INA219 @ ^1.2.3
  adafruit/Adafruit TSL2591 Library @ ^1.4.5
  adafruit/Adafruit SI1145 Library @ ^1.2.0
  adafruit/Adafruit BME280 Library @ ^2.3.0
  adafruit/Adafruit seesaw Library @ ^1.7.9

build_flags =
  -Iinclude
  -D USER_SETUP_LOADED
  -include include/User_Setup.h
```

### Build Configuration Parameters

| Parameter | Value | Purpose | Modifiable |
|-----------|-------|---------|-----------|
| **platform** | `espressif32` | ESP32 platform for PlatformIO | No (hardware constraint) |
| **board** | `esp32dev` | ESP32 development board target | No (hardware constraint) |
| **framework** | `arduino` | Arduino framework for ESP32 | No (framework choice) |
| **monitor_speed** | `115200` baud | Serial monitor baud rate (debug output) | Yes (if UART config changed) |
| **monitor_port** | `/dev/ttyUSB0` | Serial port for debug monitoring (Linux) | **Yes** (platform-dependent) |
| **upload_speed** | `921600` baud | SPI upload baud rate (firmware flashing) | Yes (slower for stability) |
| **upload_port** | `/dev/ttyUSB0` | Serial port for firmware upload (Linux) | **Yes** (platform-dependent) |
| **lib_extra_dirs** | `../lib/Freenove` | Custom library directory (TFT_eSPI library) | No (library location fixed) |

### Serial Port Configuration (Platform-Specific)

**Linux/macOS:**
- Monitor port: `/dev/ttyUSB0` (or `/dev/ttyUSB1`, `/dev/ttyACM0`, etc.)
- Upload port: Same as monitor port
- To discover port: `ls /dev/tty*` or `lsusb`

**Windows:**
- Monitor port: `COM3` (or `COM4`, `COM5`, etc. — check Device Manager)
- Upload port: Same as monitor port

**macOS Alternative:**
- Monitor port: `/dev/cu.usbserial-<ID>` or `/dev/cu.usbmodem<ID>`

### Build Flags

| Flag | Purpose |
|------|---------|
| `-Iinclude` | Add `include/` directory to compiler search path (for User_Setup.h) |
| `-D USER_SETUP_LOADED` | Define macro to enable TFT_eSPI custom setup |
| `-include include/User_Setup.h` | Force inclusion of User_Setup.h before compilation |

---

## Display Hardware Configuration: User_Setup.h

**Location:** `sensor-pages-dashboard/include/User_Setup.h`

```cpp
#pragma once

#include "../../lib/Freenove/TFT_eSPI_Setups/FNK0114N_3.5_320x480_ST7796.h"

#undef LOAD_GFXFF
#undef SMOOTH_FONT
```

This file:
1. Includes the Freenove hardware setup file (FNK0114N board)
2. Disables GFX fonts (LOAD_GFXFF) to save flash space
3. Disables smooth fonts (SMOOTH_FONT) to save flash space

### Display Setup: FNK0114N_3.5_320x480_ST7796.h

**Board:** Freenove FNK0114N 3.5-inch TFT display  
**Controller:** ST7796 (SPI-based, write-only)  
**Resolution:** 320 × 480 pixels (landscape, rotated 90°)

#### Display SPI Pin Configuration (ESP32)

| Function | GPIO Pin | Configuration |
|----------|----------|----------------|
| **MISO** (Master In, Slave Out) | GPIO 12 | Input from display (not used; display is write-only) |
| **MOSI** (Master Out, Slave In) | GPIO 13 | Output to display data |
| **SCLK** (Serial Clock) | GPIO 14 | SPI clock signal |
| **CS** (Chip Select) | GPIO 15 | Display select (pulled LOW to enable) |
| **DC** (Data/Command) | GPIO 2 | Data=HIGH, Command=LOW |
| **RST** (Reset) | -1 (disabled) | Connected to ESP32 board RST pin or 3.3V |
| **TOUCH_CS** (Touch Select) | GPIO 33 | Touch controller chip select (if present) |
| **BL** (Backlight) | GPIO 27 | LED backlight control (HIGH=ON) |

#### Display SPI Timing

| Parameter | Value | Notes |
|-----------|-------|-------|
| **SPI Clock** | ~80 MHz | Standard ESP32 SPI speed; may be lower for stability |
| **Mode** | SPI Mode 0 | CPOL=0, CPHA=0 |
| **Endianness** | Big-endian | 16-bit RGB565 color values |

#### Display Memory

| Aspect | Value |
|--------|-------|
| **Color Depth** | 16-bit RGB565 (5R, 6G, 5B bits) |
| **Frame Buffer** | Managed by TFT_eSPI library; not resident in firmware |
| **Resolution** | 320 × 480 = 153,600 pixels |
| **Frame Size (RGB565)** | 307.2 KB per frame (if buffered; not done in this firmware) |

---

## Microcontroller Configuration: ESP32

### GPIO Pin Assignments (Summary)

| Use | GPIO Pin(s) | Direction | Function |
|-----|-------------|-----------|----------|
| **I2C SDA** | GPIO 32 | Bidirectional | I2C data line (all sensors) |
| **I2C SCL** | GPIO 25 | Bidirectional | I2C clock line (all sensors) |
| **SPI MOSI** | GPIO 13 | Output | Display data |
| **SPI SCLK** | GPIO 14 | Output | Display clock |
| **SPI MISO** | GPIO 12 | Input | Display input (not used) |
| **Display CS** | GPIO 15 | Output | Display chip select (active LOW) |
| **Display DC** | GPIO 2 | Output | Display data/command control |
| **Display BL** | GPIO 27 | Output | Backlight control (PWM capable) |
| **Touch CS** | GPIO 33 | Output | Touch controller chip select |

### I2C Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Bus Clock** | 400 kHz | Standard mode; sufficient for all sensors |
| **SDA Pin** | GPIO 32 | Pulled HIGH by 4.7 kΩ resistors (hardware) |
| **SCL Pin** | GPIO 25 | Pulled HIGH by 4.7 kΩ resistors (hardware) |
| **Max Devices** | 9 I2C addresses (see Device Addresses below) |
| **Timeout** | 50 ms (default Arduino Wire) | All transactions must complete within 50 ms; on timeout, Wire returns error (false or 0 bytes); sensor data from previous read persists (stale data) |
| **Error Recovery** | None (implicit) | If I2C transaction fails, next polling cycle retries; no exponential backoff |

**Wire Library Behavior (Arduino Core v3.x):**
```cpp
Wire.begin(32, 25);          // SDA, SCL
Wire.setClock(400000);        // 400 kHz
// Timeout = 50 ms (default; not configurable in this project)
// Call Wire.setTimeOut(msec) to change if needed
```

**Timeout Scenarios:**
1. **Bus jammed** (stuck LOW): All reads fail; timeout error every 50 ms
2. **Device not responding**: Timeout after 50 ms; returns false
3. **Slow device** (conversion delay): May timeout if previous read still in progress

**No explicit recovery implemented** — sensors re-probed at boot; thereafter no recovery if bus fails.

### Memory Configuration

| Aspect | Size | Notes |
|--------|------|-------|
| **Heap Available** | ~320 KB | ESP32 default SRAM |
| **Data Structs Total** | ~300 bytes | 8 global sensor value structs (see docs/04-data-model.md) |
| **Flash (Program)** | ~500 KB (typical) | Firmware binary size; variable with library versions |
| **SPIFFS/NVS** | Not used | No persistent storage; data lost on reboot |

---

## Device Addresses: I2C Sensors

All sensors operate on I2C bus @ 400 kHz, GPIO 32 (SDA) + GPIO 25 (SCL).

| Sensor | I2C Address | Quantity | Notes |
|--------|-------------|----------|-------|
| **INA219** | 0x40 | 1 | Power monitor; must be present for INA219 page |
| **VEML6075** | 0x10 | 1 | UV sensor (UVA/UVB); optional |
| **TSL2591** | 0x29 | 1 | Light sensor (lux); optional |
| **Encoder seesaw** | 0x36 | 1 | Rotary encoder + NeoPixel LED; optional |
| **VEML6070** | 0x38, 0x39 | 1 | UV sensor (dual-address); optional |
| **SI1145** | 0x60 | 1 | Multi-spectrum sensor; optional |
| **SCD30** | 0x61 | 1 | CO₂ sensor; optional |
| **BME280** | 0x77 | 1 | Climate sensor (temperature/humidity/pressure); optional |
| **Gamepad seesaw** | 0x50 | 1 | Joystick + buttons; optional |

### I2C Address Conflicts

No address conflicts exist if all devices are connected (no two share the same address). However, I2C bus scanning is **not implemented** — detection relies on boot-time probes.

---

## Device Addresses: Seesaw Buttons & Encoder

**Encoder seesaw** (address 0x36):
- Digital output: NeoPixel LED (RGB, PWM-controlled brightness)
- Digital input: Rotary encoder dial (position tracking, delta calculation)
- Digital input: Encoder button (falling-edge triggered)
- Register: `ENCODER_NEOPIXEL_PIN = 6` (GPIO pin on seesaw)

**Gamepad seesaw** (address 0x50):
- Analog inputs: Joystick X (GPIO 14, 0–1023) and Y (GPIO 15, 0–1023)
- Digital inputs: 6 buttons (Y=2, A=5, X=6, B=1, SELECT=0, START=16) — active-low
- Keypad event queue: Stores button press/release events (hardware FIFO)

---

## Timing Configuration: Refresh Rates & Polling Intervals

All timing intervals are compile-time constants (no runtime reconfiguration).

| Component | Interval | Constant Name | Location | Notes |
|-----------|----------|---------------|----------|-------|
| **Sensor polling** | 500 ms | `SENSOR_REFRESH_MS` | Timing gate (loop ~line 47) | BME280, VEML, SCD30, etc. |
| **Gamepad input** | 25 ms | `GAMEPAD_REFRESH_MS` | Timing gate (loop) | 40 Hz = 1000ms/40 = 25 ms ✓ |
| **Encoder input** | 20 ms | `ENCODER_REFRESH_MS` | Timing gate (loop) | 50 Hz = 1000ms/50 = 20 ms ✓ |
| **INA219 sampling** | 20 µs | `INA219_SAMPLE_US` | Timing gate (micros) | 50 kHz = 1e6µs/50 = 20 µs ✓ |
| **INA219 time-gap sanity** | 200 ms | `INA219_GAP_SANITY_MS` | Coulomb check | Max acceptable gap before discarding |
| **Page redraw (live)** | 20 ms | `PAGE_REDRAW_LIVE_MS` | Timing gate (loop) | Gamepad (page 1), Encoder (page 3) pages (but encoder reads CONTINUOUSLY regardless of page) |
| **Page redraw (slow)** | 500 ms | `PAGE_REDRAW_SLOW_MS` | Timing gate (loop) | All sensor pages (0, 2, 4–8) |

### Timing Constants (Compilation Reference)

All timing intervals **must be defined as named constants** to avoid confusion:

```cpp
// Millisecond-based intervals
#define SENSOR_REFRESH_MS 500       // Sensor polling cycle
#define GAMEPAD_REFRESH_MS 25       // Gamepad input (40 Hz)
#define ENCODER_REFRESH_MS 20       // Encoder input (50 Hz)
// (Note: 1-Wire/DS18B20 support removed in v2.0)
#define PAGE_REDRAW_LIVE_MS 20      // Fast page redraw (input pages)
#define PAGE_REDRAW_SLOW_MS 500     // Slow page redraw (sensor pages)

// Microsecond-based intervals
#define INA219_SAMPLE_US 20         // INA219 high-precision (50 kHz)
#define INA219_GAP_SANITY_MS 200    // Max gap before discarding Coulomb sample
```

### Timing Gate Implementation

All timing gates use signed comparison for rollover safety:
```cpp
if ((int32_t)(now - deadline) >= 0) { /* execute */ }
```

This handles ESP32 timer wrap-around correctly (millis wraps every ~49 days, micros every ~71 minutes).

---

## Non-Blocking Rotated Sensor Architecture Configuration (FEATURE-SPEC-006)

### Slot Dispatcher Configuration

**Immutable Constraints (do not change without architectural redesign):**
```cpp
#define ENABLE_STATS_REPORTING 1                       // 1=enabled (default), 0=disabled
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;     // Slot interval (200ms per slot)
constexpr uint32_t SLOT_COUNT = 10;                   // Number of slots per cycle
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;    // Stats reporting interval (1s)
```

**Rotation Cycle:** 10 slots × 200 ms = **2000 ms per full cycle**

**Slot Schedule (Phase-Based Organization):**

**Phase 1: START ASYNC (Slots 0-1, 0-200ms)**

| Slot | Time | Operation | Max Duration | State Change |
|------|------|-----------|--------------|--------------|
| 0 | 0 ms | startTsl2591Measurement() | ~0 ms | tsl2591State: IDLE → MEASURING |
| 1 | 200 ms | startDs18b20Conversion() | ~0 ms | ds18b20State: IDLE → CONVERTING |

**Phase 2: READ SYNC (Slots 2-7, 400-1400ms)**

| Slot | Time | Sensor | Function | Max Duration |
|------|------|--------|----------|--------------|
| 2 | 400 ms | BME280 | readBme280() | ~2 ms |
| 3 | 600 ms | SI1145 | readSi1145() | ~1 ms |
| 4 | 800 ms | VEML6075 | readVeml6075() | ~1 ms |
| 5 | 1000 ms | VEML6070 | readVeml6070() | ~1 ms |
| 6 | 1200 ms | SCD30 | readScd30() | ~5-10 ms |
| 7 | 1400 ms | DS2484 Scan | performDs2484Scan() (60s interval) | ~10 ms |

**Phase 3: CHECK ASYNC (Slots 8-9, 1600-1800ms)**

| Slot | Time | Operation | Timing Guarantee |
|------|------|-----------|------------------|
| 8 | 1600 ms | checkTsl2591Async() | Started at 0ms, needs 200ms → 1400ms margin ✓ |
| 9 | 1800 ms | checkDs18b20Async() | Started at 200ms, needs 750ms → 850ms margin ✓ |

### Sensor-Specific Timing Configuration

**TSL2591 Integration Time (Async Measurement Duration):**
```cpp
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;  // Adjustable (default 200ms)
```

**Description:**
- Measurement triggered in Slot 0 (0 ms)
- Result checked in Slot 8 (1600 ms)
- Elapsed time: 1600 ms > 200 ms integration ✓ (1400ms margin)

**Typical values:**
- 100 ms (faster, less sensitivity)
- 200 ms (standard, balanced, default)
- 400 ms (slower, higher sensitivity)

**Effect on timing:** Does not affect cycle time (measurement happens in background). Only affects how long integration runs.

**Configuration:** Edit value in `src/main.cpp`, rebuild, upload.

---

**DS18B20 Conversion Time (1-Wire Async Conversion Duration):**
```cpp
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;  // Adjustable (12-bit default)
```

**Description:**
- Conversion triggered in Slot 1 (200 ms)
- Result checked in Slot 9 (1800 ms)
- Elapsed time: 1600 ms > 750 ms conversion ✓ (850ms margin)

**Resolution options (change to trade precision for speed):**
- 8-bit: 95 ms (±0.5°C, fastest)
- 10-bit: 375 ms (±0.25°C, medium)
- 12-bit: 750 ms (±0.0625°C, slowest, default)

**Effect on timing:** Changing this value does NOT change cycle time (remains 2000ms). Only affects margin available for conversion to complete.

**Configuration:** Edit value in `src/main.cpp` (search for `DS18B20_CONVERSION_TIME_MS`), rebuild, upload.

**Warning:** If conversion time reduced below ~95ms, may get incomplete reads or CRC errors. If increased above ~750ms, may get timeout errors (should still work, but less margin).

### Input Statistics & Reporting

**Stats Aggregation Interval:**
```cpp
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;  // 1 second (immutable)
```

**Feature Toggle:**
```cpp
#define ENABLE_STATS_REPORTING 1  // 1=enabled (default), 0=disabled (compile-time)
```

**When ENABLED (default):**
- Per-sensor timing logged to Serial after each read
- Aggregated statistics printed every 1 second
- Useful for performance monitoring and debugging

**Example output:**
```
[STATS] Gamepad Stats (1s): Count=40, MinTime=4800, MaxTime=5234, SumTime=201456
[STATS] Encoder Stats (1s): Count=50, MinTime=987, MaxTime=1156, SumTime=50123
[STATS] INA219 Stats (1s): Count=50, MinTime=1100, MaxTime=1289, SumTime=61234
[STATS] Slot 0 (TSL2591 START): avg=45µs
[STATS] Slot 1 (DS18B20 START): avg=23µs
[STATS] Slot 2 (BME280): avg=2156µs
[STATS] Slot 8 (TSL2591 CHECK): avg=1045µs
[STATS] Slot 9 (DS18B20 CHECK): avg=1234µs
```

**When DISABLED:**
- No per-sensor timing output
- Slightly reduced Serial I/O load
- Performance measurement still happens internally (not printed)

---

## DS2484 1-Wire Adapter Configuration

### Hardware Setup

**DS2484 I2C-to-1-Wire Bridge:**
```cpp
#define DS2484_ADDR 0x18              // I2C address (Adafruit breakout, hardcoded)
#define DS2484_MAX_SENSORS 3          // Hardware limit (QWIIC connector)
```

**Library:**
```
Adafruit_DS248x @ 1.2.0 (automatically via lib_deps)
OneWire library @ compatible version
```

**I2C Bus Configuration:**
- Address: 0x18 (fixed, matches Adafruit DS2484 breakout board)
- Bus: Same I2C as all other sensors (SDA=GPIO32, SCL=GPIO25, 400kHz)
- Detection: Boot-time only via `available.ds2484` flag

### 1-Wire Temperature Sensor Discovery

**Discovery Mechanism:**
```cpp
// At boot (setup()):
void detectSensors() {
  available.ds2484 = ds2484Master.begin(&Wire, 0x18);
  if (available.ds2484) {
    performDs2484Scan();  // Discover all attached sensors
  }
}

// Periodic scanning (every 60 seconds or on demand):
void performDs2484Scan() {
  // Runs in Slot 7 (every cycle, gated to 60s via internal counter)
  // Resets sensor array and runs OneWireSearch()
  // Validates ROM addresses via CRC7
  // Updates ds2484Values.sensorCount
}
```

**Discovery Frequency:**
- **At boot:** Immediate scan in `setup()`
- **In loop:** Every 2000ms in Slot 7, but only executes scan every 60 seconds (internal gate)
- **On demand:** If `ds2484Values.sensorCount == 0`, scan is re-triggered immediately

**Supported Sensor Types (1-Wire):**
- DS18B20 (Family Code 0x28) — Standard digital thermometer
- DS1822 (Family Code 0x22) — Digital thermometer variant
- DS1820 (Family Code 0x10) — Legacy digital thermometer
- Other Dallas Semiconductor temperature sensors with compatible protocol

**Maximum Sensors:** 3 per adapter (hardware limitation of Adafruit breakout QWIIC connector)

### Temperature Conversion Configuration

**Polling Frequency:**
- Conversion initiated: Every Slot 1 (once per 2000ms cycle)
- Results checked: Every Slot 9 (once per 2000ms cycle, ~1600ms after start)
- Update rate: 2000ms per sensor (consistent with other sensors)

**Conversion Precision:**
```cpp
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;  // Adjustable
```

Configure in `src/main.cpp`. Options:
- **95 ms** (8-bit, ±0.5°C) — Fast but coarse
- **375 ms** (10-bit, ±0.25°C) — Medium precision
- **750 ms** (12-bit, ±0.0625°C, default) — Full precision

**Cycle time remains 2000ms regardless of conversion time.**

### Sensor Availability & Error Handling

**Online/Offline Status:**
```cpp
struct Ds18b20Sensor {
  uint8_t address[8];        // 1-Wire ROM address
  float temperature;         // Latest reading in °C
  bool available;            // Online flag
  uint8_t errorCount;        // Error counter (0-3)
  uint32_t lastReadMs;       // Last successful read timestamp
};
```

**Three-Strike Lockout:**
- After 3 consecutive read failures: `available = false`
- Locked-out sensors no longer polled
- Page 9 (DS2484) displays "Disconnected" status
- Recovery: Restart system to reset flags and re-scan bus

**Example Display (Page 9):**
```
DS2484 1-Wire (Page 9):
────────────────────────
Sensor 0: 28 AB CD EF 01 23 45 67  ← ROM ID (hex)
          Temperature: 23.45°C

Sensor 1: 28 12 34 56 78 9A BC DE
          Disconnected (locked out after 3 errors)

Sensor 2: [Not found]
```

### 1-Wire ROM Address Validation

**CRC7 Checksum:**
- ROM address is 8 bytes: Family (1) + Serial (6) + CRC (1)
- CRC7 validates ROM address integrity
- Failed CRC: Sensor rejected from discovery
- Passed CRC: Sensor added to poll list

**Example ROM Address:**
```
28 AB CD EF 01 23 45 67
├─ 28: Family code (DS18B20)
├─ AB CD EF 01 23 45: Serial number (unique per sensor)
└─ 67: CRC7 checksum
```

### Temperature Data Format

**Raw Value (16-bit):**
```
Byte 0 (temp MSB): Whole °C (-55 to +125)
Byte 1 (temp LSB): Fractional °C (0, 0.0625, 0.125, ... 0.9375)
```

**Conversion Formula:**
```cpp
float temperature = ((int16_t)(msb << 8 | lsb)) * 0.0625;
// Example: raw 0x0140 = (0x14 << 8 | 0x00) * 0.0625 = 20.0°C
```

**Precision:**
- 12-bit default: Resolution = 0.0625°C, Range = -55 to +125°C
- 10-bit: Resolution = 0.25°C
- 8-bit: Resolution = 1°C

---
```
BME280 read time: 2156 µs
TSL2591 async START time: 45 µs
SI1145 read time: 689 µs
...
=== Sensor Stats (1000 ms) ===
BME280:       min=2100 µs, max=2300 µs, avg=2156 µs, reads=5
TSL2591:      async_starts=1, async_ready=1, avg_wait=205 ms
SI1145:       min=650 µs, max=750 µs, avg=689 µs, reads=5
VEML6075:     min=550 µs, max=650 µs, avg=600 µs, reads=5
VEML6070:     min=450 µs, max=550 µs, avg=500 µs, reads=5
SCD30:        min=300 µs, max=400 µs, avg=350 µs, reads=1
DS18B20:      async_starts=1, async_ready=0 (pending)
===================================
```

**When DISABLED (#define ENABLE_STATS_REPORTING 0):**
- Per-sensor timing still logged (always on)
- Aggregated stats section suppressed (cleaner serial output)
- Suitable for production/field use

**Configuration:** Edit `#define` in src/main.cpp, rebuild, upload.

**Memory Impact:** Negligible (stats counters only, ~100 bytes).

### Display Update Behavior

**Display Refresh Strategy:**
- **Page changed:** Full redraw (header + data) immediately
- **Sensor values changed:** Partial redraw (data only, skips header clear)
- **Timer event:** Partial redraw (INA219 seconds counter updated)
- **No change:** No redraw (skip drawing, save CPU/power)

**Implementation:**
```cpp
if (currentPage != lastPage) {
  drawCurrentPage(true);   // Full redraw
} else if (valuesChanged()) {
  drawCurrentPage(false);  // Partial redraw
} else if (timerEventFired()) {
  drawCurrentPage(false);  // Partial redraw
}
// Otherwise: no call to drawCurrentPage() (no redraw)
```

**Benefit:** Eliminates flicker, reduces display SPI load, responsive UI, potential power savings.

**Performance Impact:** Display updates only when necessary; typical loop iteration < 1 ms (vs 20+ ms if redrawing every cycle).

---



### Colors (TFT RGB565 16-bit)

| Use | Color Value | RGB | Hex |
|-----|-------------|-----|-----|
| **Primary data** | TFT_GREEN | (0, 255, 0) | 0x07E0 |
| **Secondary data** | TFT_ORANGE | (255, 165, 0) | 0xFD20 |
| **Titles/computed** | TFT_CYAN | (0, 255, 255) | 0x07FF |
| **Derived/computed values** | TFT_CYAN | (0, 255, 255) | 0x07FF |
| **Labels** | TFT_WHITE | (255, 255, 255) | 0xFFFF |
| **Background** | TFT_BLACK | (0, 0, 0) | 0x0000 |
| **Error/disconnect** | TFT_RED | (255, 0, 0) | 0xF800 |
| **Dividers/footer** | TFT_DARKGREY | (64, 64, 64) | 0x4208 |
| **Sensor addresses** | TFT_YELLOW | (255, 255, 0) | 0xFFE0 |

### Layout Constants (Pixels)

| Element | Y-Coordinate | Height | Notes |
|---------|--------------|--------|-------|
| **Header** | y=0 to y=42 | 42 px | Cyan title + divider line |
| **Data rows** | y=52 to y=268 | ~216 px | 9 rows × 24 px spacing |
| **Footer** | y=294 | 24 px | Navigation hints + sensor name |
| **Divider lines** | y=42, y=290 | 1 px | TFT_DARKGREY horizontal lines |

### Text Rendering

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Font size** | 2–4 | Size 4 for title, 2 for labels, 1 for values |
| **Default font** | TFT_eSPI internal (bitmap) | GFX fonts disabled (LOAD_GFXFF undefined) |
| **Smooth rendering** | Disabled | Smooth fonts not used (SMOOTH_FONT undefined) |
| **Text alignment** | Left (labels), Right (values) | TL_DATUM (top-left), TR_DATUM (top-right) |

---

## Compilation Configuration

### Include Directories

| Path | Purpose |
|------|---------|
| `include/` | Project headers (User_Setup.h, SPIFFS.h) |
| `lib/Freenove/` | Custom Freenove libraries (TFT_eSPI, TFT_Touch) |
| Default Arduino paths | Standard Arduino + Adafruit libraries |

### Preprocessor Defines (Build Flags)

| Define | Purpose |
|--------|---------|
| `USER_SETUP_LOADED` | Enable custom TFT_eSPI setup (don't use default) |

### Disabled Features (via #undef in User_Setup.h)

| Feature | Disabled | Reason |
|---------|----------|--------|
| `LOAD_GFXFF` | ✓ | Save flash space (firmware binary < 512 KB) |
| `SMOOTH_FONT` | ✓ | Save flash space; not needed for monospace data display |

---

## Environment-Specific Configuration

### Development Environment (Current)

**Use Case:** Direct hardware testing, debugging via serial monitor

**Configuration:**
- Serial monitor: `/dev/ttyUSB0` @ 115200 baud
- Upload: `/dev/ttyUSB0` @ 921600 baud
- All 9 I2C sensors recommended (but optional; system graceful with missing sensors)
- All 9 sensors are I2C-based with fixed addresses
- Build flags: Default (all features enabled)

**Setup Steps:**
1. Connect ESP32 board via USB to `/dev/ttyUSB0`
2. Run: `pio run -t upload`
3. Monitor: `pio run -t monitor` or `screen /dev/ttyUSB0 115200`

### Production Environment (Not Implemented)

**Hypothetical** production deployment would require:
- Persistent configuration (SPIFFS/NVS) — currently not used
- Over-the-air (OTA) firmware updates — not implemented
- Telemetry logging — not implemented
- Stateful data across reboots — not implemented

**Current status:** Firmware assumes single-board deployment; no multi-device coordination.

---

## Serial Monitor Output

### Debug/Startup Output

**Baud Rate:** 115200  
**Format:** ASCII text, 8 data bits, 1 stop bit, no parity

Example startup sequence:
```
[boot] Detecting sensors...
[INFO] INA219 detected @ 0x40
[INFO] BME280 detected @ 0x77
[WARNING] Gamepad QT not found at 0x50
[INFO] Boot complete. Starting main loop.
```

(Note: Exact output depends on which library functions are instrumented with Serial.print())

### Debug Information Available

- Sensor detection status (on boot)
- I2C errors (if any)
- No continuous telemetry (data-heavy; would stress serial link)

---

## Configuration Modification Guide

### Adding a New Sensor (I2C)

1. **Allocate I2C address** (must not conflict with existing)
2. **Add #define constants** in main.cpp lines 14–35:
   ```cpp
   #define NEW_SENSOR_ADDR 0x??
   #define NEW_SENSOR_REG_CFG 0x??
   ```
3. **Add detection** in `detectSensors()` function (lines 553–574)
4. **Add read function** (see lines 189–206 for examples)
5. **Add page renderer** (see lines 398–442 for page structure)
6. **Update PAGE_COUNT** (line 46) if adding new page

### Changing Timing Intervals

**Modify constants** (lines 46–48):
```cpp
constexpr uint32_t SENSOR_REFRESH_MS = 500;  // Slower = less power, less responsive
constexpr uint32_t INPUT_REFRESH_MS = 20;    // Faster = more responsive (20ms = 50Hz)
// (Note: DS18B20 support removed in v2.0)
```

**Rebuild:** `pio run -t upload`

### Changing Display Colors

**Modify page renderers** (e.g., lines 346–365 in `pageIna219()`):
```cpp
row(52, "Voltage (V)", String(busV, 3), TFT_GREEN);  // Change TFT_GREEN to another color
```

**Available colors:** TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN (use CYAN for derived values; never use MAGENTA — not readable in sunlight)

### Changing Serial Port

**Linux/macOS:**
Edit `platformio.ini`:
```ini
monitor_port = /dev/ttyUSB1  # or /dev/ttyACM0, /dev/cu.usbserial-..., etc.
upload_port = /dev/ttyUSB1
```

**Windows:**
```ini
monitor_port = COM4
upload_port = COM4
```

**Discover port:** `pio device list`

### Changing SPI Clock Speed

**In User_Setup.h or TFT driver file:** (default ~80 MHz)
```cpp
// Add to main.cpp after tft.init():
tft.setSPISpeed(40000000);  // 40 MHz (slower for stability)
```

---

## Configuration Validation

### Checklist Before Deployment

- [ ] Serial port in `platformio.ini` matches target device
- [ ] All sensor I2C addresses configured and non-conflicting
- [ ] Display GPIO pins match hardware (User_Setup.h)
- [ ] I2C GPIO pins (32, 25) match schematic
- [ ] All 9 I2C sensor addresses configured correctly
- [ ] Timing intervals appropriate (500ms for sensors, 20ms for input)
- [ ] All required libraries installed (see docs/12-dependencies.md)
- [ ] Build compiles without errors: `pio run`
- [ ] Firmware uploads successfully: `pio run -t upload`
- [ ] Serial monitor shows startup messages: `pio run -t monitor`
- [ ] Display renders; pages navigate correctly

---

## Large Display Mode Configuration (FR-012 — Page 2 INA219) — **NEW**

### Layout Parameters

Large Display Mode uses hardcoded pixel coordinates and font sizes for centered rendering. To customize layout, edit these constants in `sensor-pages-dashboard/src/main.cpp`:

#### Display Coordinates (pixels)

| Element | Variable | Value | Notes |
|---------|----------|-------|-------|
| Header Y | — | 18 | Header "INA219 Large Display" |
| Separator Y | — | 48 | DarkGrey horizontal divider line |
| Label Y | — | 70 | Value name (e.g., "Batterie") |
| Value Y | — | 160 | Enlarged value display (focal point) |
| Unit Y | — | 240 | Unit string (e.g., "V") |
| Footer Y | — | 295 | Navigation footer "B:next X:prev Y:exit" |
| All X | — | 240 | Horizontal center (480 ÷ 2) |

**Adjustment Strategy:**
- Y coordinates are fixed by display resolution and layout goals (320 px vertical height)
- To reposition: Edit y values in `pageLargeDisplayIna219()` function (lines 263–312)
- X coordinate (240) should remain centered; do not modify unless display resolution changes

#### Font Sizes

| Element | Font | Size | Purpose |
|---------|------|------|---------|
| Header | 4 | Medium | Section title; readable, not too large |
| Label | 4 | Medium | Value name; contrast with footer |
| Value | 8 | Large | Focal point; maximized readability |
| Unit | 4 | Medium | Secondary info; readable with value |
| Footer | 2 | Small | Navigation hint; doesn't dominate screen |

**Font Adjustment:**
- Standard TFT_eSPI fonts: 1–8 (increasing size)
- Font 8 is maximum practical size for value display on 320 px height
- To change: Edit font number in `pageLargeDisplayIna219()` calls (lines ~280–310)
- Test on hardware after any change; verify text fits within screen bounds

#### Color Scheme

| Element | Color | TFT_eSPI Constant | RGB565 | Rationale |
|---------|-------|-------------------|--------|-----------|
| Header | Cyan | `TFT_CYAN` | 0x07FF | High contrast; derived value indicator |
| Label | Cyan | `TFT_CYAN` | 0x07FF | Matches header; visual coherence |
| Value (Cyan) | Cyan | `TFT_CYAN` | 0x07FF | Laufzeit, Ladung (computed values) |
| Value (Green) | Green | `TFT_GREEN` | 0x07E0 | Batterie, Versorgung, Shunt (raw measurements) |
| Value (Orange) | Orange | `TFT_ORANGE` | 0xFD20 | Strom, Power (secondary values) |
| Unit | White | `TFT_WHITE` | 0xFFFF | Maximum contrast; supplement to value |
| Footer | DarkGrey | `TFT_DARKGREY` | 0x39E7 | Subtle; navigation hint (not focal) |
| Background | Black | `TFT_BLACK` | 0x0000 | Contrast base; power efficiency |

**Color Assignment per Index:**
- See [07-ui.md Section 7.2](07-ui.md#72-value-index--display-mapping) for exact index-to-color mapping

**Customization:**
- Edit color constants in `pageLargeDisplayIna219()` (lines ~285–310)
- See [TFT_eSPI documentation](https://github.com/Bodmer/TFT_eSPI) for available color names
- Test on actual display; colors may appear different under different lighting

### Value Formatting Configuration

Large Display Mode value formatting is inherited from the standard INA219 page via helper functions:

| Index | Value Source | Decimal Places | Unit | Function |
|-------|--------------|-----------------|------|----------|
| 0 | Runtime counter | N/A | — | `formatRuntime()` (HH:MM:SS) |
| 1 | Load voltage | 3 | V | `getLargeDisplayValue(1)` |
| 2 | Bus voltage | 3 | V | `getLargeDisplayValue(2)` |
| 3 | Current | 1 | mA | `getLargeDisplayValue(3)` |
| 4 | Charge (mAh) | 3 | mAh | `getLargeDisplayValue(4)` |
| 5 | Shunt voltage | 2 | mV | `getLargeDisplayValue(5)` |
| 6 | Power | 0 | mW | `getLargeDisplayValue(6)` |

**To Modify Decimal Places:**
- Edit cases in `getLargeDisplayValue()` function (lines ~200–210)
- Change `sprintf()` format string (e.g., `"%.1f"` for 1 decimal)
- Rebuild and test: `pio run -t upload`

**Consistency Requirement:**
- Decimal places in large display must match standard page exactly
- Verify both pages after any decimal change

### Refresh Rate Configuration

| Parameter | Current | Location | Notes |
|-----------|---------|----------|-------|
| `SENSOR_REFRESH_MS` | 500 | Line 55 | INA219 page base refresh (includes large display) |
| `INPUT_REFRESH_MS` | 10 | Line 55 | Gamepad polling (100 Hz for button responsiveness) |

**Large Display Mode inherits Page 2 refresh rate (500 ms).** To change:
1. Edit `SENSOR_REFRESH_MS` constant in src/main.cpp
2. Recompile: `pio run`
3. Upload: `pio run -t upload`
4. Verify timing on hardware

**Performance Constraint:** Render time must fit within refresh window (~450 ms out of 500 ms available).

---

## Embedded Compilation Constants (src/main.cpp)

All timing and behavioral constants are defined at compile-time in `src/main.cpp` lines 50-190. These are immutable at runtime and control performance, feature availability, and logging behavior.

| Constant | Value | Type | Configurable | Purpose |
|----------|-------|------|--------------|---------|
| **ENABLE_STATS_REPORTING** | 1 | #define | Yes | Enable per-second aggregated input device stats (Gamepad, Encoder, INA219); 0=disable |
| **SENSOR_REFRESH_MS** | 500 | constexpr | Yes | Sensor polling cycle interval (milliseconds) |
| **INPUT_REFRESH_MS** | 10 | constexpr | Yes | Gamepad/Encoder/INA219 polling frequency (milliseconds); 10ms = 100Hz |
| **SENSOR_SLOT_INTERVAL_MS** | 200 | constexpr | No | Rotated sensor slot width (FEATURE-SPEC-006); do not change |
| **TSL2591_INTEGRATION_TIME_MS** | 200 | static | Yes | TSL2591 measurement wait time (100/200/400 ms options) |
| **DS18B20_CONVERSION_TIME_MS** | 750 | static | Yes | 1-Wire temperature conversion time (95/375/750 ms for 8/10/12-bit) |
| **INPUT_STATS_INTERVAL_MS** | 1000 | constexpr | No | Input stats reporting interval (1 second; immutable) |

### Modification Guide

**To Enable/Disable Stats Reporting:**
```cpp
#define ENABLE_STATS_REPORTING 1  // Line 184: Change to 0 for silent mode
```

**To Change Sensor Polling Rate (Trade-off: freshness vs power):**
```cpp
constexpr uint32_t SENSOR_REFRESH_MS = 500;  // Line 54: Slower = less power, less responsive
```

**To Change Input Polling Rate (Game controllers, buttons):**
```cpp
constexpr uint32_t INPUT_REFRESH_MS = 10;    // Line 55: Faster = more responsive (lower ms = higher Hz)
// Practical range: 5-25 ms (200-20 Hz)
```

**To Change TSL2591 Integration Time (for different light conditions):**
```cpp
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;  // Line ~180: Change to 100/200/400
// 100 ms = faster, less light sensitivity
// 200 ms = balanced (default)
// 400 ms = slower, more light sensitivity
```

**To Change DS18B20 Resolution (Temperature precision vs speed):**
```cpp
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;   // Line ~181: Change to 95/375/750
// 95 ms = 8-bit, ±0.5°C
// 375 ms = 10-bit, ±0.25°C
// 750 ms = 12-bit, ±0.0625°C (default, most accurate)
```

**After modifying any constant:**
1. Edit `src/main.cpp` at the specified line
2. Recompile: `pio run --environment freenove-fnk0114n-sensor-pages`
3. Upload: `pio run --environment freenove-fnk0114n-sensor-pages --target upload`
4. Verify on hardware: `pio run --environment freenove-fnk0114n-sensor-pages --target monitor`

---

## Encoder Navigation Configuration (FEATURE-SPEC-011)

### Overview

Encoder navigation parameters are **compile-time constants** defined in `src/main.cpp`. No runtime configuration available. The encoder provides alternative input for page navigation (standard mode) and parameter value scrolling (Large Display mode), with visual feedback via NeoPixel LED.

### Compile-Time Constants

| Constant | Value | Location | Purpose |
|----------|-------|----------|---------|
| `ENCODER_DELTA_THRESHOLD` | `1` | src/main.cpp:2684 | Minimum encoder steps to trigger page navigation |
| `pageColors[10]` | See table below | src/main.cpp:1260-1273 | 10-color map for encoder LED feedback by page |
| `LED Brightness (Standard Mode)` | `255` | src/main.cpp:1276 | Full brightness in standard page navigation mode |
| `LED Brightness (Large Display Mode)` | 128-255 (scaled) | src/main.cpp:1277-1285 | Scaled brightness in Large Display Mode (50%-100% range) |
| `INPUT_REFRESH_MS` | `10` | src/main.cpp:60 | Polling frequency (100 Hz) for responsive encoder input |

### Encoder LED Color Map (Unchangeable at Runtime)

**The following 10-color map is hardcoded and provides visual feedback of the current page:**

| Page | Sensor Title | Color Name | Hex Value | RGB | Notes |
|------|--------------|-----------|-----------|-----|-------|
| 0 | BME280 Climate Sensor | Red | 0xFF0000 | (255, 0, 0) | Primary climate data |
| 1 | Gamepad QT Input | Orange | 0xFF7F00 | (255, 127, 0) | Game input controller |
| 2 | INA219 Battery Monitor | Yellow | 0xFFFF00 | (255, 255, 0) | Power measurement |
| 3 | Rotary Encoder & LED | Green | 0x00FF00 | (0, 255, 0) | Encoder control page |
| 4 | SCD30 CO₂ Sensor | Cyan | 0x00FFFF | (0, 255, 255) | Gas measurement |
| 5 | SI1145 UV/IR Sensor | Blue | 0x0000FF | (0, 0, 255) | Spectrum sensor |
| 6 | TSL2591 Light Sensor | Purple | 0x7F00FF | (127, 0, 255) | Illumination sensor |
| 7 | VEML6070 UV Sensor | White | 0xFFFFFF | (255, 255, 255) | UV-A index |
| 8 | VEML6075 UV Sensor | Pink | 0xFF1493 | (255, 20, 147) | UV-A/B index |
| 9 | DS2484 1-Wire Adaptor | Light Blue | 0x87CEEB | (135, 206, 235) | Temperature sensor hub |

### Behavior Configuration

**Delta Threshold (Navigation Sensitivity):**
```cpp
// Line 2684 in handleButtons() - encoder navigation block
const int ENCODER_DELTA_THRESHOLD = 1;
// 1 = most sensitive (default): each encoder step = one page change
// 2+ = less sensitive: requires larger rotation for page change
// Recommended: keep at 1 for intuitive 1:1 response
```

**LED Brightness in Standard Mode:**
```cpp
// Line 1276 in updateEncoderLed()
uint8_t ledBrightness = 255;  // Full brightness (100%)
// Range: 0-255, where 128 ≈ 50%, 255 = 100%
// Increase for brighter LED (higher power consumption)
// Decrease for dimmer LED (lower power consumption)
```

**LED Brightness in Large Display Mode:**
```cpp
// Lines 1281 in updateEncoderLed()
ledBrightness = 128 + (largeDisplayMode.valueIndex * 127 / paramCount);
// Linear scaling from 50% (128) to 100% (255)
// Value 0: 50% brightness (visual start marker)
// Value N (last): 100% brightness (visual end marker)
// To change range: edit "128" (minimum) and "127" (scale factor)
```

### How to Customize Encoder Behavior

**To Change Delta Threshold (Rotation Sensitivity):**
```cpp
// src/main.cpp, line 2684
const int ENCODER_DELTA_THRESHOLD = 1;  // Change this to 2 or higher
// 1 = 1 encoder step = 1 page; very responsive (default)
// 2 = 2 encoder steps = 1 page; less responsive
// 3+ = more steps required; least responsive
```

**To Change LED Page Color Mapping:**
```cpp
// src/main.cpp, lines 1260-1273, in updateEncoderLed()
const uint32_t pageColors[] = {
    0xFF0000,  // Page 0: change Red to any color (e.g., 0x00FF00 for green)
    0xFF7F00,  // Page 1: Orange
    0xFFFF00,  // Page 2: Yellow
    // ... modify any hex value
    0x87CEEB   // Page 9: Light Blue
};
// Hex format: 0xRRGGBB (e.g., 0xFF0000 = red, 0x00FF00 = green)
```

**To Change LED Brightness Range (Large Display Mode):**
```cpp
// src/main.cpp, line 1281, in updateEncoderLed()
ledBrightness = 128 + (largeDisplayMode.valueIndex * 127 / paramCount);
// Current: 50% (128) to 100% (255)
// To change to 20% (51) to 100% (255): edit "128" to "51"
// To change to 50% (128) to 80% (204): edit "127" to "76"
```

### Rebuild & Upload Required

Any changes to these compile-time constants require recompilation and device reprogramming:

```bash
# Edit src/main.cpp constants (see sections above)

# Recompile firmware
pio run --environment freenove-fnk0114n-sensor-pages

# Upload to device
pio run --environment freenove-fnk0114n-sensor-pages --target upload

# Monitor serial output (optional)
pio run --environment freenove-fnk0114n-sensor-pages --target monitor
```

**Upload Duration:** ~30 seconds  
**Device Reset:** Automatic after upload  
**Settings Persistence:** New constants take effect immediately on boot  

### No Runtime Configuration Available

- ❌ No settings menu on device
- ❌ No persistent EEPROM storage of preferences
- ❌ No Over-The-Air (OTA) updates of constants
- ✅ All configuration is compile-time only (consistent with project design: "no menus, no hierarchies")

This design choice ensures:
- Predictable, deterministic behavior
- No risk of corrupted runtime settings
- Minimal RAM overhead (all constants in flash)
- Simple, auditable code (no hidden state)

---

## Known Configuration Limitations

| Limitation | Impact | Workaround |
|-----------|--------|-----------|
| No runtime reconfiguration | Cannot change timing/colors without rebuild | Edit constants, rebuild, re-upload |
| No EEPROM/NVS storage | Settings lost on reboot | Implement SPIFFS-backed config (future work) |
| I2C addresses hardcoded | Cannot use different hardware variants | Edit constants for new hardware |
| Serial port hardcoded | Cannot switch ports without rebuild | Edit platformio.ini for each platform |
| No configuration file | No persistent settings | Add JSON config in SPIFFS (future work) |
| Encoder parameters compile-time only | Cannot adjust sensitivity at runtime | Rebuild firmware for different thresholds |

---

## Security Considerations

**No credentials/secrets in this firmware:**
- No WiFi SSID/password (offline device)
- No API keys (no cloud connectivity)
- No authentication (embedded device, trusted environment)
- I2C bus unencrypted (local only; assume secure laboratory/industrial environment)

**Physical security:**
- USB programming port unrestricted (anyone can reprogram ESP32)
- Serial monitor outputs debugging info (assume trusted access)
- No secure boot or firmware encryption implemented

---

## Automatic Versioning Configuration

**Feature:** Auto-Update VERSION and BUILD_DATE (implemented 2026-10-22)

**Overview:** Every firmware build automatically increments the patch version and updates the build date to current UTC time. This ensures `VERSION` and `BUILD_DATE` are always in sync with the actual compilation.

### Versioning Components

**Files:**
- `scripts/update_build_info.py` — Python script to update version/date
- `extra_scripts/pre_build.py` — PlatformIO pre-build hook
- `src/diagnostics.h` — Contains VERSION and BUILD_DATE constants
- `platformio.ini` — Includes extra_scripts directive

### Version Format

**Semantic Versioning:** `major.minor.patch`
- Example: `3.1.0`, `3.1.1`, `3.1.2`, etc.
- **Patch component** increments by 1 on every build
- **Major/minor components** updated manually (requires edit to script)

### Build Date Format

**ISO 8601 UTC:** `YYYY-MM-DDTHH:MM:SSZ`
- Example: `2026-10-22T22:41:29Z`
- Updated to current UTC time on every build

### Build Workflow

```bash
$ platformio run

🔨 Running pre-build: Updating build information...
======================================================================
   ✅ Updated diagnostics.h:
      VERSION: 3.1.3 → 3.1.4
      BUILD_DATE: 2026-10-22T22:41:29Z
======================================================================

[Compilation proceeds...]
[Build succeeds]
```

### Configuration

**PlatformIO configuration (platformio.ini):**
```ini
extra_scripts = extra_scripts/pre_build.py
```

This directive tells PlatformIO to execute `extra_scripts/pre_build.py` before compilation. The script:
1. Calls `scripts/update_build_info.py`
2. Updates `src/diagnostics.h` with new VERSION and BUILD_DATE
3. Continues with normal compilation

### Disabling Auto-Update (Temporary)

If you need to disable automatic versioning:

1. **Option 1:** Comment out extra_scripts in platformio.ini:
   ```ini
   # extra_scripts = extra_scripts/pre_build.py
   ```

2. **Option 2:** Manually edit VERSION/BUILD_DATE in src/diagnostics.h:
   ```c
   #define VERSION "3.1.2"
   #define BUILD_DATE "2026-10-22T00:00:00Z"
   ```

3. Re-enable by uncommenting platformio.ini or re-running `python3 scripts/update_build_info.py`

### Manual Version Update

To manually update major or minor version, edit `scripts/update_build_info.py` and modify the initial version baseline. The next build will patch-increment from the new value.

### Verification

Check current version and build date in firmware:
```bash
# In diagnostics.h:
grep "^#define VERSION\|^#define BUILD_DATE" src/diagnostics.h

# Output example:
#define VERSION "3.1.4"
#define BUILD_DATE "2026-10-22T22:41:29Z"
```

Or observe on the device:
- **Page 1** (Boot Banner): Displays VERSION and BUILD_DATE during startup
- **Page 10** (System Info): Displays current VERSION and uptime

### Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| VERSION not updating | Pre-build hook disabled | Check `extra_scripts` line in platformio.ini |
| BUILD_DATE stale | Script not executed | Verify Python 3 is available in PATH |
| Build fails after script | Script error | Check diagnostics.h for syntax errors; revert if needed |
| Import error on Windows | Path handling | extra_scripts assumes Unix paths; may need adjustment |

### Related Documentation

See [BUILD-VERSIONING.md](/docs/BUILD-VERSIONING.md) for complete technical details of the versioning system.

---

