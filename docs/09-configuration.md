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
| **Page redraw (live)** | 20 ms | `PAGE_REDRAW_LIVE_MS` | Timing gate (loop) | Gamepad (page 1), Encoder (page 3) pages |
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

## Display Rendering Configuration

### Colors (TFT RGB565 16-bit)

| Use | Color Value | RGB | Hex |
|-----|-------------|-----|-----|
| **Primary data** | TFT_GREEN | (0, 255, 0) | 0x07E0 |
| **Secondary data** | TFT_ORANGE | (255, 165, 0) | 0xFD20 |
| **Titles/computed** | TFT_CYAN | (0, 255, 255) | 0x07FF |
| **Accumulated values** | TFT_MAGENTA | (255, 0, 255) | 0xF81F |
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

**Available colors:** TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, etc.

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

## Known Configuration Limitations

| Limitation | Impact | Workaround |
|-----------|--------|-----------|
| No runtime reconfiguration | Cannot change timing/colors without rebuild | Edit constants, rebuild, re-upload |
| No EEPROM/NVS storage | Settings lost on reboot | Implement SPIFFS-backed config (future work) |
| I2C addresses hardcoded | Cannot use different hardware variants | Edit constants for new hardware |
| Serial port hardcoded | Cannot switch ports without rebuild | Edit platformio.ini for each platform |
| No configuration file | No persistent settings | Add JSON config in SPIFFS (future work) |

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

