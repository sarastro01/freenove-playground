# Dependencies: sensor-pages-dashboard

**Last Updated:** 2026-09-21  
**Project Type:** Embedded ESP32 Firmware  
**Dependency Management:** PlatformIO package manager

---

## Overview

sensor-pages-dashboard depends on:
- **1 microcontroller platform** (Espressif ESP32)
- **1 framework** (Arduino)
- **7 PlatformIO-managed libraries** (sensor drivers + protocols)
- **2 custom local libraries** (TFT_eSPI, TFT_Touch from Freenove)
- **Hardware sensor devices** (9 I2C sensors, 8 1-Wire temperature sensors, 1 TFT display)

**No external services, databases, or cloud dependencies.**

---

## Runtime Environment

### Target Microcontroller

| Aspect | Specification |
|--------|----------------|
| **MCU** | Tensilica Xtensa dual-core 32-bit processor |
| **Model** | ESP32 (or variants: ESP32-S2, ESP32-S3, etc.) |
| **Manufacturer** | Espressif Systems |
| **Flash Memory** | 4 MB (typical; configurable via fuses) |
| **SRAM** | 520 KB (shared; ~320 KB available for heap) |
| **CPU Speed** | 240 MHz (default; configurable to 80 MHz) |
| **Voltage** | 3.3V logic, USB 5V tolerant (with current limit) |
| **Interfaces** | 2× SPI, 2× I2C, 2× UART, ADC, RTC, Bluetooth/WiFi (not used) |
| **Datasheet** | [Espressif ESP32 Reference Manual](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/) |

### Operating System

**None.** Firmware runs bare-metal (no RTOS in application layer, though Arduino core uses FreeRTOS internally).

**Arduino Core Runtime:**
- Arduino core for ESP32 (typically v2.x or v3.x)
- FreeRTOS kernel (internal; application uses superloop only)
- No multithreading in user code

---

## Platform & Framework

### PlatformIO Platform: espressif32

| Property | Value | Notes |
|----------|-------|-------|
| **Platform Name** | `espressif32` | Espressif ESP32 platform for PlatformIO |
| **Version** | Latest (auto-managed by PlatformIO) | Typically v6.x or v7.x as of 2026 |
| **Toolchain** | xtensa-esp32-elf-gcc | GNU C++ compiler for Xtensa ISA |
| **Build System** | CMake (idf-based) | Arduino sketch integration via PlatformIO |
| **Repository** | [platformio/platform-espressif32](https://github.com/platformio/platform-espressif32) | Open-source platform definition |

### Arduino Framework

| Property | Value | Notes |
|----------|-------|-------|
| **Framework** | `arduino` | Arduino language + library abstraction |
| **Core Version** | v3.0.3 | Auto-installed by espressif32 platform v6.5.0 |
| **Pinning** | platformio.ini: `platform = espressif32 @ 6.5.0` | Locks Arduino Core v3.0.3 |
| **Tested Versions** | v3.0.0, v3.0.1, v3.0.2, v3.0.3 | All compatible; v2.x untested |
| **Language Standard** | C++11 | Arduino sketches compiled as C++ |
| **Startup** | Arduino `setup()` + `loop()` paradigm | Single-threaded superloop (cooperative scheduling) |
| **Key Functions** | `millis()`, `micros()`, `digitalWrite()`, `analogRead()`, etc. | Standard Arduino API |

**Breaking Changes in v3.0+:**
- **SPIFFS → LittleFS:** Core v3.x removes SPIFFS support
  - **Mitigation:** include/SPIFFS.h shim provides compatibility layer
  - Shim defines: `#define SPIFFS LittleFS` to remap calls
- **Wire library timeout:** Default 50 ms (same in v2.x, v3.x)
- **Timer overflow:** millis() wraps every 49 days (consistent across versions)

---

## Primary Dependencies: Libraries

All libraries are **pinned to compatible versions** in `platformio.ini`. Version specifiers use semantic versioning:
- `^X.Y.Z` = Compatible with X.Y.Z (allows minor/patch updates, not major)

### Sensor Libraries (Adafruit)

#### 1. Adafruit INA219 Library

| Property | Value |
|----------|-------|
| **Package** | `adafruit/Adafruit INA219` |
| **Version** | `^1.2.3` (compatible with 1.2.3, 1.2.4, etc.; not 1.3.0+) |
| **Repository** | [adafruit/Adafruit_INA219](https://github.com/adafruit/Adafruit_INA219) |
| **Purpose** | I2C power monitor driver (voltage, current, power) |
| **Hardware** | INA219 @ 0x40 (configurable via resistor ladder) |
| **Key Functions** | `begin()`, `getBusVoltage_V()`, `getShuntVoltage_mV()`, `getCurrent_mA()`, `getPower_mW()` |
| **Dependency** | Adafruit_BusIO (auto-installed) |
| **Flash Size** | ~10 KB |
| **Heap Size** | ~200 bytes (struct only) |

#### 2. Adafruit TSL2591 Library

| Property | Value |
|----------|-------|
| **Package** | `adafruit/Adafruit TSL2591 Library` |
| **Version** | `^1.4.5` |
| **Repository** | [adafruit/Adafruit_TSL2591_Library](https://github.com/adafruit/Adafruit_TSL2591_Library) |
| **Purpose** | I2C light sensor driver (lux, IR spectrum) |
| **Hardware** | TSL2591 @ 0x29 |
| **Key Functions** | `begin()`, `setGain()`, `setTiming()`, `getFullSpectrum()`, `getInfrared()` |
| **Configuration** | GAIN_MED, INTEGRATIONTIME_200MS (set at boot) |
| **Flash Size** | ~8 KB |
| **Heap Size** | ~200 bytes |

#### 3. Adafruit SI1145 Library

| Property | Value |
|----------|-------|
| **Package** | `adafruit/Adafruit SI1145 Library` |
| **Version** | `^1.2.0` |
| **Repository** | [adafruit/Adafruit_SI1145_Library](https://github.com/adafruit/Adafruit_SI1145_Library) |
| **Purpose** | I2C multi-spectrum sensor (visible, IR, UV) |
| **Hardware** | SI1145 @ 0x60 |
| **Key Functions** | `begin()`, `readVisible()`, `readIR()`, `readUV()` |
| **Flash Size** | ~12 KB |
| **Heap Size** | ~200 bytes |

#### 4. Adafruit BME280 Library

| Property | Value |
|----------|-------|
| **Package** | `adafruit/Adafruit BME280 Library` |
| **Version** | `^2.3.0` |
| **Repository** | [adafruit/Adafruit_BME280_Library](https://github.com/adafruit/Adafruit_BME280_Library) |
| **Purpose** | I2C environmental sensor (temperature, humidity, pressure) |
| **Hardware** | BME280 @ 0x77 |
| **Key Functions** | `begin()`, `readTemperature()`, `readHumidity()`, `readPressure()` |
| **Computed Values** | Dew point (calculated in firmware) |
| **Dependency** | Adafruit_Sensor (auto-installed) |
| **Flash Size** | ~14 KB |
| **Heap Size** | ~300 bytes |

#### 5. Adafruit seesaw Library

| Property | Value |
|----------|-------|
| **Package** | `adafruit/Adafruit seesaw Library` |
| **Version** | `^1.7.9` |
| **Repository** | [adafruit/Adafruit_seesaw](https://github.com/adafruit/Adafruit_seesaw) |
| **Purpose** | I2C interface to seesaw devices (encoder, gamepad, NeoPixel controller) |
| **Hardware** | Encoder seesaw @ 0x36, Gamepad seesaw @ 0x50 |
| **Key Functions** | `begin()`, `digitalRead()`, `digitalReadBulk()`, `analogRead()`, `getKeypadCount()`, `readKeypad()` |
| **Sub-libraries** | seesaw_neopixel (NeoPixel LED control) |
| **Flash Size** | ~20 KB |
| **Heap Size** | ~200 bytes |

#### 6. OneWire Library

| Property | Value |
|----------|-------|
| **Package** | `paulstoffregen/OneWire` |
| **Version** | `^2.3.8` |
| **Repository** | [PaulStoffregen/OneWire](https://github.com/PaulStoffregen/OneWire) |
| **Purpose** | 1-Wire serial protocol driver (Dallas temperature sensors) |
| **Hardware** | GPIO 23 (DS18B20 devices) |
| **Key Functions** | `begin()`, `search()`, `crc8()`, `reset()`, `write()`, `read()` |
| **Communication** | Synchronous bit-banging (not real hardware 1-Wire) |
| **Dependency** | None (low-level) |
| **Flash Size** | ~6 KB |
| **Heap Size** | ~100 bytes |

#### 7. DallasTemperature Library

| Property | Value |
|----------|-------|
| **Package** | `milesburton/DallasTemperature` |
| **Version** | `^4.0.4` |
| **Repository** | [milesburton/Arduino-Temperature-Control-Library](https://github.com/milesburton/Arduino-Temperature-Control-Library) |
| **Purpose** | DS18B20 temperature sensor abstraction (uses OneWire) |
| **Hardware** | DS18B20 sensors on GPIO 23 (via OneWire) |
| **Key Functions** | `begin()`, `requestTemperatures()`, `getTempC()`, `getDeviceCount()`, `getAddress()`, `setResolution()` |
| **Configuration** | 9-bit resolution (95 ms conversion time) |
| **Async Support** | `setWaitForConversion(false)` for non-blocking reads |
| **Dependency** | OneWire library |
| **Flash Size** | ~8 KB |
| **Heap Size** | ~100 bytes |
| **Sentinel Value** | `DEVICE_DISCONNECTED_C` (~–127 °C) [UNKNOWN - library version dependent] |

### Sub-Dependency: Adafruit_BusIO

| Property | Value |
|----------|-------|
| **Package** | Auto-installed by Adafruit libraries |
| **Version** | Latest compatible |
| **Purpose** | Unified I2C/SPI abstraction for all Adafruit sensors |
| **Usage** | Called by sensor libraries; not directly used in application |
| **Flash Size** | ~4 KB |

### Sub-Dependency: Adafruit_Sensor

| Property | Value |
|----------|-------|
| **Package** | Auto-installed by Adafruit BME280 |
| **Version** | Latest compatible |
| **Purpose** | Unified sensor event/axis abstraction |
| **Usage** | Required by BME280; not directly used |
| **Flash Size** | ~2 KB |

### Sub-Dependency: seesaw_NeoPixel

| Property | Value |
|----------|-------|
| **Package** | Included in Adafruit seesaw library |
| **Purpose** | NeoPixel RGB LED control via seesaw I2C interface |
| **Usage** | Encoder LED brightness + color control |
| **Functions** | `begin()`, `Color()`, `setBrightness()`, `setPixelColor()`, `show()` |
| **Flash Size** | ~4 KB |

---

## Custom Local Libraries

### TFT_eSPI (Freenove Variant)

| Property | Value |
|----------|-------|
| **Location** | `../lib/Freenove/TFT_eSPI/` |
| **Version** | 2.5.0-freenove (based on bodmer/TFT_eSPI v2.5.0) |
| **Version File** | lib/Freenove/TFT_eSPI/VERSION.txt |
| **Base Upstream** | bodmer/TFT_eSPI v2.5.0 |
| **Original** | [bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) |
| **Purpose** | SPI-based TFT display driver (ST7796, ILI9341, etc.) |
| **Hardware** | ST7796 display @ 320×480 pixels (FNK0114N) |
| **Key Functions** | `init()`, `fillScreen()`, `drawFastHLine()`, `setTextColor()`, `setTextSize()`, `drawString()`, `setRotation()` |
| **Configuration** | User_Setup.h selects display model + GPIO pins |
| **Flash Size** | ~60 KB (large library) |
| **Heap Size** | ~50 KB frame buffer (if enabled; typically not used in this firmware) |
| **SPI Clock** | 40 MHz (display), 2.5 MHz (touch, if enabled) |
| **Custom Setup** | FNK0114N_3.5_320x480_ST7796.h (Freenove-specific) |
| **Status** | ⚠️ Static copy (locked to local version, no auto-updates) |
| **Compatibility** | Tested with Arduino Core v3.0.0–v3.0.3 |

**Files:**
- `TFT_eSPI.h/cpp` — Main driver
- `TFT_config.h` — Display configuration
- `User_Setup_Select.h` — Setup file selector
- `User_Setup.h` — Selected setup (FNK0114N in this project)
- `TFT_Drivers/` — Driver-specific code (ST7796.c, ILI9341.c, etc.)
- `Fonts/` — Built-in bitmap fonts
- `Extensions/` — Display extensions (touch, SD card, etc.)
- `VERSION.txt` — Fork version and tracking information

### TFT_Touch (Optional)

| Property | Value |
|----------|-------|
| **Location** | `../lib/Freenove/TFT_Touch/` |
| **Purpose** | Touch screen calibration + event handling |
| **Status** | **Not used** in current firmware (touch-draw demo only) |
| **Hardware** | 3.5" resistive touch panel (optional, GPIO 33 CS) |

### Freenove TFT_eSPI_Setups

| Property | Value |
|----------|-------|
| **Location** | `../lib/Freenove/TFT_eSPI_Setups/` |
| **Content** | Hardware-specific setup headers (.h files) |
| **Used** | `FNK0114N_3.5_320x480_ST7796.h` (Freenove FNK0114N board) |
| **Defines** | Display controller (ST7796), SPI pins, resolution (320×480), color order |

---

## Hardware Dependencies

### Sensors (I2C Bus)

| Sensor | Manufacturer | Part Number | I2C Address | Function | Datasheet | Optional |
|--------|--------------|-------------|-------------|----------|-----------|----------|
| **Power Monitor** | Texas Instruments | INA219 | 0x40 | Voltage/current/power measurement | [TI INA219 Datasheet](https://www.ti.com/lit/ds/symlink/ina219.pdf) | No (core feature) |
| **Temperature/Humidity/Pressure** | Bosch | BME280 | 0x77 | Environmental sensing | [Bosch BME280 Datasheet](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/) | Yes |
| **Light Sensor (Lux)** | AMS/OSRAM | TSL2591 | 0x29 | Full spectrum light measurement | [AMS TSL2591 Datasheet](https://ams.osram-ws.com/products/environmental-sensors/environmental-sensors/tsl2591/) | Yes |
| **UV Index** | Vishay/OSRAM | VEML6075 | 0x10 | UVA/UVB measurement | [Vishay VEML6075 Datasheet](https://www.vishay.com/docs/84339/veml6075.pdf) | Yes |
| **UV Index (Legacy)** | OSRAM | VEML6070 | 0x38/0x39 | UV intensity (16-bit, dual-address) | [OSRAM VEML6070 Datasheet](https://www.osram-opto.com/ecat/filter/de/en/VEML6070,+UV+Sensor) | Yes |
| **Multi-Spectrum** | Vishay/Soleil | SI1145 | 0x60 | Visible/IR/UV spectrum measurement | [Vishay SI1145 Datasheet](https://www.vishay.com/docs/80277/si1145.pdf) | Yes |
| **CO₂ Sensor** | SenseAir | SCD30 (K70) | 0x61 | CO₂ concentration (NDIR) | [SenseAir SCD30 Datasheet](https://sensirion.com/products/catalog/SCD30) | Yes |
| **Rotary Encoder** | Adafruit | seesaw + NeoPixel | 0x36 | Encoder input + RGB LED | [Adafruit Seesaw Datasheet](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-seesaw-atsamd09-breakout.pdf) | Yes |
| **Gamepad** | Adafruit | seesaw + Joystick | 0x50 | Analog joystick + 6 buttons | [Adafruit Seesaw Datasheet](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-seesaw-atsamd09-breakout.pdf) | Yes |

### Temperature Sensors (1-Wire)

| Sensor | Manufacturer | Part Number | Protocol | Pin | Count | Optional |
|--------|--------------|-------------|----------|-----|-------|----------|
| **Temperature** | Maxim Integrated (now Analog Devices) | DS18B20 | 1-Wire (Dallas) | GPIO 23 | Up to 8 | Yes |

### Display

| Component | Manufacturer | Model | Interface | Resolution | Color Depth | Pin | Purpose |
|-----------|--------------|-------|-----------|------------|------------|-----|---------|
| **Display** | Freenove/BOE | FNK0114N | SPI | 320×480 | 16-bit RGB565 | GPIO 15 (CS), GPIO 2 (DC) | Main user interface |
| **Display Controller** | Sitronix | ST7796 | SPI (write-only) | 320×480 | 16-bit | — | Display IC |
| **Backlight** | — | LED array | GPIO PWM | — | — | GPIO 27 | Screen illumination |
| **Touch** | Resistive | FT5436 [UNKNOWN] | I2C / GPIO | 320×480 | — | GPIO 33 (CS) | Optional touch input (not used) |

---

## Connectivity & Communication Protocols

### I2C Bus (Inter-Integrated Circuit)

| Property | Value |
|----------|-------|
| **Frequency** | 400 kHz (standard mode) |
| **Data Line (SDA)** | GPIO 32 (ESP32) |
| **Clock Line (SCL)** | GPIO 25 (ESP32) |
| **Pull-ups** | 4.7 kΩ resistors (external hardware) |
| **Voltage** | 3.3V logic (5V tolerant with resistive divider) |
| **Max Devices** | 127 (I2C limit); 9 used in this project |
| **Implementation** | Arduino `Wire` library (hardware I2C) |
| **Timeout** | [UNKNOWN] (handled by Wire library internals) |

**Devices on bus:**
- INA219 @ 0x40
- VEML6075 @ 0x10
- TSL2591 @ 0x29
- SI1145 @ 0x60
- VEML6070 @ 0x38, 0x39 (dual-address)
- SCD30 @ 0x61
- BME280 @ 0x77
- Encoder seesaw @ 0x36
- Gamepad seesaw @ 0x50

### 1-Wire (Dallas/Maxim Single-Wire Protocol)

| Property | Value |
|----------|-------|
| **Data Line** | GPIO 23 (ESP32) |
| **Pull-up** | 4.7 kΩ resistor (external hardware) |
| **Voltage** | 3.3V logic |
| **Bit Rate** | ~60 µs per bit (software-bit-banged; slow) |
| **Max Devices** | 8 DS18B20 (capped in firmware) |
| **Power Mode** | Parasitic (powered from data line) |
| **Implementation** | OneWire library (software bit-banging) |

### SPI (Serial Peripheral Interface)

| Property | Value |
|----------|-------|
| **Clock (SCLK)** | GPIO 14 @ ~80 MHz |
| **Data Out (MOSI)** | GPIO 13 |
| **Data In (MISO)** | GPIO 12 (not used; display is write-only) |
| **Chip Select (CS)** | GPIO 15 (display) |
| **Data/Command** | GPIO 2 |
| **Implementation** | ESP32 hardware SPI (VSPI/SPI3) |
| **Voltage** | 3.3V logic |

### UART (Serial Monitor)

| Property | Value |
|----------|-------|
| **Baud Rate** | 115200 |
| **Port** | /dev/ttyUSB0 (Linux), COM3+ (Windows), /dev/cu.usbserial-* (macOS) |
| **Purpose** | Serial monitor output for debugging |
| **Pins** | TX = GPIO 1, RX = GPIO 3 (not directly used in application; USB-UART bridge on board) |

---

## Build & Development Tools

### PlatformIO CLI

| Tool | Purpose | Version | Installation |
|------|---------|---------|--------------|
| **pio** | PlatformIO command-line interface | Latest (auto-managed) | `pip install platformio` |
| **pio run** | Build firmware binary | — | Invokes toolchain |
| **pio run -t upload** | Flash firmware to board | — | Invokes esptool.py |
| **pio run -t monitor** | Serial monitor (debug output) | — | Wraps miniterm or screen |
| **pio device list** | Enumerate connected devices | — | Lists COM ports + USB devices |

### Toolchain Components (Auto-Installed by PlatformIO)

| Tool | Purpose | Vendor | Language |
|------|---------|--------|----------|
| **xtensa-esp32-elf-gcc** | C/C++ compiler for ESP32 | GNU Embedded Toolchain | C++11 |
| **esptool.py** | Firmware flashing utility | Espressif | Python 3 |
| **cmake** | Build system | Kitware | Cross-platform |
| **miniterm.py** | Serial monitor fallback | pySerial | Python 3 |

**Installation:** Automatic when PlatformIO first runs on a new board.

---

## Dependency Resolution & Version Management

### Dependency Lock File

**Not used.** PlatformIO does not generate a lock file by default.

**Version pinning strategy:**
- Each library pinned to compatible version range in `platformio.ini` (e.g., `^1.2.3`)
- Caret `^` means: Major version locked, minor/patch auto-updated
- To freeze exact versions: Replace `^` with `==` (e.g., `adafruit/Adafruit INA219 @ ==1.2.3`)

### Dependency Update Process

**Check for updates:**
```bash
pio pkg outdated
```

**Update all libraries:**
```bash
pio pkg update
```

**Update specific library:**
```bash
pio pkg install --save "adafruit/Adafruit INA219 @ ^1.3.0"
```

**Verify no breaking changes:** Manual testing required (embedded firmware, no automated tests).

---

## Known Dependency Constraints

| Constraint | Impact | Workaround |
|-----------|--------|-----------|
| Arduino Core @ v2.x vs v3.x | Potential compatibility issues with library versions | Explicitly pin espressif32 platform version |
| DallasTemperature sentinel value | Exact DEVICE_DISCONNECTED_C value depends on library version | Check library docs for library version used |
| TFT_eSPI Freenove fork | Freenove customizations may diverge from upstream | Use local copy; test against original if needed |
| OneWire bit-banging | Slow; CPU busy during 1-Wire transactions | Use external 1-Wire master (future work) |
| seesaw library version | Encoder/gamepad behavior depends on seesaw firmware version | Assume seesaw firmware ≥3.x; runtime check recommended |

---

## Missing/Optional Dependencies

### Not Used (Available But Disabled)

| Feature | Library | Reason |
|---------|---------|--------|
| **SD Card** | SD.h | No persistent storage needed |
| **SPIFFS** | SPIFFS.h | No configuration storage (all compile-time) |
| **WiFi** | WiFi.h | No network connectivity (offline device) |
| **Bluetooth** | BluetoothSerial.h | No wireless comms (offline device) |
| **NVS (Non-Volatile Storage)** | nvs_flash.h | No persistent settings (all compile-time) |
| **Touch Screen** | Touch drivers in TFT_Touch | Touch capability available but not used in current firmware |
| **Smooth/Glyph Fonts** | Disabled in User_Setup.h | Not needed for numeric display; saves flash |

### Recommended for Future Work

| Feature | Library | Purpose | Status |
|---------|---------|---------|--------|
| **SPIFFS Config** | SPIFFS.h | Runtime configuration storage | Future enhancement |
| **OTA Updates** | ArduinoOTA.h | Wireless firmware updates | Future enhancement |
| **Telemetry** | ArduinoJSON.h | Data logging to cloud | Future enhancement |
| **Real-Time Clock** | DS3231 library | Timestamped data (requires RTC hardware) | Future enhancement |

---

## Dependency Health & Maintenance

### Library Status (as of 2026-09-21)

| Library | Status | Last Update | Maintenance |
|---------|--------|-------------|------------|
| **Adafruit INA219** | ✅ Active | 2024 | Actively maintained by Adafruit |
| **Adafruit TSL2591** | ✅ Active | 2023 | Actively maintained |
| **Adafruit SI1145** | ✅ Active | 2022 | Maintained; less frequent updates |
| **Adafruit BME280** | ✅ Active | 2024 | Actively maintained |
| **Adafruit seesaw** | ✅ Active | 2023 | Actively maintained |
| **OneWire (PaulStoffregen)** | ✅ Active | 2021 | Maintained; stable, no major changes |
| **DallasTemperature** | ✅ Active | 2023 | Actively maintained by community |
| **TFT_eSPI (Bodmer)** | ✅ Active | 2024 | Highly active; frequent updates |
| **TFT_eSPI (Freenove fork)** | ⚠️ Static | [UNKNOWN] | Freenove's fork may lag behind upstream |
| **Arduino Core ESP32** | ✅ Active | 2024 | Actively maintained by Espressif |
| **espressif32 platform** | ✅ Active | 2024 | Actively maintained by PlatformIO |

### Update Risk Assessment

**Low Risk (Safe to update):**
- Adafruit sensor libraries (well-tested, backward-compatible)
- OneWire (stable, no major API changes)

**Medium Risk (Test before updating):**
- TFT_eSPI (frequent updates; may include breaking changes)
- DallasTemperature (API stable; but sentinel value may change with version)

**High Risk (Recommend against updating):**
- Arduino Core ESP32 (major version changes may break build)
- espressif32 platform (major version changes may affect toolchain)
- TFT_eSPI Freenove fork (custom modifications; test thoroughly)

### Recommended Update Strategy

1. **Pin all library versions initially:** Use `==X.Y.Z` in platformio.ini
2. **Update one at a time:** Change to `^X.Y.Z`, rebuild, test
3. **Keep release notes:** Document why each version was chosen
4. **Test on hardware:** Embedded firmware requires real device testing
5. **Keep local backup:** Archive working build configuration

---

## Dependency Installation from Clean Environment

### Prerequisites

1. **Python 3.6+** (for PlatformIO + esptool)
2. **USB drivers** for ESP32 board (typically auto-installed on Linux/macOS; manual on Windows)
3. **Git** (optional, for retrieving custom libraries)

### Step-by-Step Setup

**1. Install PlatformIO:**
```bash
pip install platformio
```

**2. Clone/download repository:**
```bash
git clone <repo-url> freenove-playground
cd freenove-playground/sensor-pages-dashboard
```

**3. Install dependencies:**
```bash
pio pkg install
```
(Automatically downloads and installs all libraries listed in `lib_deps`)

**4. Connect ESP32 board via USB**

**5. Identify serial port:**
```bash
pio device list
```

**6. Update platformio.ini with your port:**
```ini
monitor_port = /dev/ttyUSB0  # or your port
upload_port = /dev/ttyUSB0
```

**7. Build firmware:**
```bash
pio run
```

**8. Upload to board:**
```bash
pio run -t upload
```

**9. Monitor serial output:**
```bash
pio run -t monitor
```

### Troubleshooting Installation

| Problem | Cause | Solution |
|---------|-------|----------|
| **esptool.py: "port not found"** | Serial port not recognized | Install USB driver; check `pio device list` |
| **"lib not found"** | Libraries not downloaded | Run `pio pkg install` again |
| **Compilation error: "Unknown SPI pins"** | Wrong board selected | Verify `board = esp32dev` in platformio.ini |
| **Upload fails; board not detected** | USB connection unstable | Try different USB cable; check baud rate |
| **Monitor shows garbage (wrong baud)** | Serial port baud mismatch | Ensure `monitor_speed = 115200` in platformio.ini |

---

## Summary: Dependency Matrix

| Category | Type | Count | Required | Status |
|----------|------|-------|----------|--------|
| **Platform** | Microcontroller | 1 | ✅ Yes | ESP32 |
| **Framework** | Runtime | 1 | ✅ Yes | Arduino |
| **Sensor Libraries** | I2C drivers | 5 | ✅ Yes (but optional for individual sensors) | All Adafruit |
| **Seesaw Library** | Encoder/gamepad | 1 | ✅ Optional | Adafruit |
| **Protocol Libraries** | 1-Wire driver | 2 | ✅ Optional | OneWire + DallasTemperature |
| **Display Library** | TFT driver | 1 | ✅ Yes | TFT_eSPI (Freenove) |
| **Sub-dependencies** | Auto-installed | 3 | ✅ Yes | Adafruit_BusIO, Adafruit_Sensor, seesaw_NeoPixel |
| **Custom Libraries** | Local | 2 | ✅ Yes | TFT_eSPI, TFT_Touch (local Freenove copy) |
| **Hardware Sensors** | I2C/1-Wire | 9 I2C + 8 1-Wire | Mostly Optional | All connected to same I2C/1-Wire buses |
| **External Services** | Cloud/Network | 0 | ✗ No | None |
| **Databases** | Persistent Storage | 0 | ✗ No | None (RAM-only) |

---

## Appendix: Sensor Library APIs Quick Reference

### INA219 (Power Monitor)

```cpp
Adafruit_INA219 ina219(0x40);
ina219.begin();
float busV = ina219.getBusVoltage_V();
float shuntV = ina219.getShuntVoltage_mV();
float current = ina219.getCurrent_mA();
float power = ina219.getPower_mW();
```

### BME280 (Temperature/Humidity/Pressure)

```cpp
Adafruit_BME280 bme280;
bme280.begin(0x77, &Wire);
float temp = bme280.readTemperature();
float humidity = bme280.readHumidity();
float pressure = bme280.readPressure();
```

### DS18B20 (Temperature)

```cpp
DallasTemperature sensors(&oneWire);
sensors.begin();
sensors.requestTemperatures();
float tempC = sensors.getTempC(address);
if (tempC <= DEVICE_DISCONNECTED_C) { /* disconnected */ }
```

### Seesaw Encoder/Gamepad

```cpp
Adafruit_seesaw encoder;
encoder.begin(0x36);
int32_t position = encoder.getEncoderPosition();
if (encoder.digitalRead(24) == LOW) { /* button pressed */ }
uint32_t buttons = encoder.digitalReadBulk(BUTTON_MASK);
uint16_t x = encoder.analogRead(14);
uint16_t y = encoder.analogRead(15);
```

### TFT Display

```cpp
TFT_eSPI tft(320, 480);
tft.init();
tft.setRotation(3);
tft.fillScreen(TFT_BLACK);
tft.drawString("Hello", 10, 10, 2);
tft.setTextColor(TFT_GREEN);
tft.drawFastHLine(10, 50, 300, TFT_DARKGREY);
```

