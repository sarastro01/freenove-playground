# API & External Interfaces: sensor-pages-dashboard

**Last Updated:** 2026-09-23  
**Project Type:** Embedded Firmware (No HTTP/REST APIs)

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

## 1. I2C Bus Protocol

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

**Purpose:** Read CO₂/temperature/humidity with CRC8 validation (500 ms cycle).

**Protocol:** Sensirion proprietary command + CRC8 checksums (not Adafruit library).

**Commands:**

| Command | Hex | Purpose |
|---------|-----|---------|
| START | 0x0010 | Start measurement |
| READY | 0x0202 | Poll measurement ready status |
| READ | 0x0300 | Fetch CO₂/temp/humidity (18 bytes) |

**Response Format (18 bytes, 6 CRC checks):**
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

#### Device 8: Encoder Seesaw (Adafruit QT Rotary Encoder) @ 0x36

**Purpose:** Read rotary encoder position, button state; control NeoPixel RGB LED (50 Hz when page==4).

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

## 2. Serial Debug Output

**Configuration:**
- Baud rate: 115200
- Port: /dev/ttyUSB0 (hardware serial via USB UART bridge)
- Purpose: Development and troubleshooting only (not user-facing)

**DS2484-Related Messages:**

| Message | Meaning | When |
|---------|---------|------|
| `Probing DS2484 at 0x18...` | Attempting to initialize adapter | Boot (detectSensors) |
| `DS2484: FOUND` | Adapter detected and responding | Boot success |
| `DS2484: NOT FOUND` | Adapter not detected on I2C bus | Boot failure |
| `DS2484: Initialized at 0x18` | Adapter initialization complete | initDs2484() success |
| `DS2484: Initialization FAILED` | Adapter initialization error | initDs2484() failure |
| `DS2484: Scanning 1-Wire bus...` | Starting 60-second bus scan | performDs2484Scan() |
| `DS2484: Found sensor 0: 28:bb:0e:68:05:00:00:fa` | Sensor discovered | performDs2484Scan() per sensor |
| `DS2484: Bus scan complete - found 1 sensors` | Bus scan finished | performDs2484Scan() end |
| `DS2484: Sensor 0 temp = 20.50C` | Temperature reading | readDs2484Temperatures() per sensor |

**Example Boot Sequence Output:**
```
[  1940][E][Wire.cpp:513] requestFrom(): i2cRead returned Error 263
[  2964][E][Wire.cpp:513] requestFrom(): i2cRead returned Error 263
Probing DS2484 at 0x18...
DS2484: FOUND
DS2484: Initialized at 0x18
DS2484: Scanning 1-Wire bus...
DS2484: Found sensor 0: 28:bb:0e:68:05:00:00:fa
DS2484: Bus scan complete - found 1 sensors
DS2484: Sensor 0 temp = 20.50C
```

**Notes:**
- Wire.cpp error 263 messages are expected (probe of missing sensors like SCD30 @ 0x61)
- Serial output available only via USB connection; not stored or persisted

---

## 3. I2C Read Functions (Low-Level Helpers)

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

## 2. 1-Wire Bus Protocol (via DS2484 I2C Adapter)

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

## 3. SPI Bus Protocol

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
| TFT_MAGENTA | 0xF81F | Computed values |

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

## 4. Serial UART Interface

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

## 5. What Does NOT Exist

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

## 6. Data Models (DTOs)

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

## 7. Communication Patterns

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

## 8. Error Handling (API Level)

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

## 9. Timing & Scheduling (API Perspective)

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

## 10. Concurrency Model

**Single-threaded, no async/await.**

All communication is synchronous, blocking, non-concurrent. No multitasking at application level (Arduino framework runs FreeRTOS internally, but firmware doesn't create tasks).

---

## 11. Data Serialization

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
