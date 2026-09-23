# Reconstruction Plan: sensor-pages-dashboard

**Purpose:** Deterministic guide for recreating the complete sensor-pages-dashboard embedded firmware application from documentation alone, without access to original source code.

**Assumptions:**
- Original `src/main.cpp` is NOT available
- Only documentation under `docs/` is available
- Target repository starts completely empty
- AI agent (GitHub Copilot) executes phases incrementally
- Hardware (ESP32 + sensors) available for testing
- PlatformIO and toolchain installed

**Last Updated:** 2026-09-21

---

## Table of Contents

1. [Phase 1: Project Initialization](#phase-1-project-initialization)
2. [Phase 2: Dependencies and Tooling](#phase-2-dependencies-and-tooling)
3. [Phase 3: Domain Model](#phase-3-domain-model)
4. [Phase 4: Database/Persistence](#phase-4-databasepersistence)
5. [Phase 5: Business Logic](#phase-5-business-logic)
6. [Phase 6: API/Backend](#phase-6-apibackend)
7. [Phase 7: Frontend/UI](#phase-7-frontendui)
8. [Phase 8: Authentication/Authorization](#phase-8-authenticationauthorization)
9. [Phase 9: External Integrations](#phase-9-external-integrations)
10. [Phase 10: Tests](#phase-10-tests)
11. [Phase 11: Build](#phase-11-build)
12. [Phase 12: Deployment](#phase-12-deployment)

---

## Phase 1: Project Initialization

### Objective
Set up the basic project structure, directory hierarchy, version control, and documentation scaffold. Establish foundation for subsequent phases.

### Files/Components to Create

```
sensor-pages-dashboard/
├── .gitignore
├── README.md
├── platformio.ini
├── docs/
│   ├── README.md (index of all docs)
│   └── (copy existing docs: 01-requirements.md through 12-dependencies.md)
├── include/
│   ├── SPIFFS.h
│   └── User_Setup.h
├── src/
│   └── main.cpp (created in later phases)
├── lib/
│   └── Freenove/
│       ├── TFT_eSPI/
│       ├── TFT_eSPI_Setups/
│       └── TFT_Touch/
└── test/
    └── README
```

### Dependencies
- **None** (Phase 1 is foundational)

### Implementation Order

1. **Initialize Git repository**
   - Create `.gitignore` (exclude `.pio/`, `.vscode/`, build artifacts)
   - See [09-configuration.md](docs/09-configuration.md) for exact patterns

2. **Create directory structure**
   - `mkdir -p include src lib/Freenove test docs`

3. **Create `platformio.ini` (skeleton)**
   - Define environment: `[env:freenove-fnk0114n-sensor-pages]`
   - Set platform, board, framework (see [09-configuration.md](docs/09-configuration.md))
   - Set monitor/upload ports (TODO: make platform-configurable)
   - NOTE: Library dependencies added in Phase 2

4. **Create `include/SPIFFS.h`**
   - Compatibility header for ESP32 Core 3.x (LittleFS replaces SPIFFS)
   - Define macro: `#define SPIFFS LittleFS`
   - Source: [TFT_eSPI/ILI9341 with ESP32](../README.md) user notes

5. **Create `include/User_Setup.h`**
   - Minimal stub that includes display setup from lib/Freenove
   - Disable LOAD_GFXFF and SMOOTH_FONT (see [09-configuration.md](docs/09-configuration.md))
   - Purpose: Allow TFT_eSPI to find hardware configuration

6. **Create `lib/Freenove/` directory structure**
   - TFT_eSPI library (source: copy from Freenove GitHub or local mirror)
   - TFT_eSPI_Setups with FNK0114N_3.5_320x480_ST7796.h
   - Rationale: Custom Freenove board requires specific TFT setup
   - [BLOCKED BY MISSING INFORMATION]: Exact Freenove TFT_eSPI version not pinned in documentation

7. **Create `README.md` (project root)**
   - Overview, hardware list, quick start
   - Link to docs/01-requirements.md for full specifications

8. **Create `docs/README.md` (documentation index)**
   - Links to all documentation files
   - Recommended reading order: 01, 04, 02, 06, 07, 09, 12, 10

9. **Create `test/README.md`**
   - Placeholder for test strategy (populated in Phase 10)

### Acceptance Criteria

- [ ] All directories created and empty
- [ ] `.gitignore` contains PlatformIO artifacts (`.pio`, `.vscode`, build outputs)
- [ ] `platformio.ini` parses without errors: `pio project config`
- [ ] `include/User_Setup.h` and `include/SPIFFS.h` both exist
- [ ] `lib/Freenove/TFT_eSPI/` directory structure matches Freenove layout
- [ ] `README.md` and `docs/README.md` are readable and contain correct links
- [ ] Git repo initialized: `git log` shows initial commit

### Tests/Validation Before Proceeding

```bash
# Verify structure
find . -type f -name "*.md" | head -20
ls -la include/ src/ lib/Freenove/

# Verify platformio.ini syntax
pio project config --env=freenove-fnk0114n-sensor-pages

# Verify no build errors yet (should fail on missing libraries)
pio run 2>&1 | grep -E "fatal|error" | head -5
```

### Dependencies on Other Phases
- **Blocking:** None (foundational)
- **Blocked by:** None

---

## Phase 2: Dependencies and Tooling

### Objective
Set up all build-time dependencies, configure PlatformIO, verify toolchain, and ensure all external libraries are available.

### Files/Components to Create

**Modifications to existing files:**
- Update `platformio.ini` with complete `lib_deps` section
- No new source files; only configuration updates

### Dependencies

**Required by Phase 2:**
- Phase 1 (project structure)
- Internet access (download libraries from PlatformIO package manager)
- PlatformIO CLI installed: `pio --version`

**Enables Phase(s):**
- Phase 3, 5, 9 (all phases depend on libraries being available)

### Implementation Order

1. **Add library dependencies to `platformio.ini`**
   - Source: [12-dependencies.md](docs/12-dependencies.md), "Primary Dependencies" section
   - Add all 5 core sensor libraries:
     - `adafruit/Adafruit INA219 @ ^1.2.3`
     - `adafruit/Adafruit TSL2591 Library @ ^1.4.5`
     - `adafruit/Adafruit SI1145 Library @ ^1.2.0`
     - `adafruit/Adafruit BME280 Library @ ^2.3.0`
     - `adafruit/Adafruit seesaw Library @ ^1.7.9`
   - Rationale: These are pinned to compatible versions (caret ^) to allow patch updates

2. **Add build flags to `platformio.ini`**
   - `-Iinclude` (include project headers)
   - `-D USER_SETUP_LOADED` (enable custom TFT setup)
   - `-include include/User_Setup.h` (force User_Setup inclusion)
   - Source: [09-configuration.md](docs/09-configuration.md)

3. **Add serial port configuration to `platformio.ini`**
   - `monitor_speed = 115200`
   - `monitor_port = /dev/ttyUSB0` (platform-dependent; see note below)
   - `upload_speed = 921600`
   - `upload_port = /dev/ttyUSB0`
   - **NOTE:** Ports are platform-specific; document as TODO with instructions

4. **Verify toolchain availability**
   - Run: `pio run --target=compiledb 2>&1 | head -20`
   - Expected: Downloads xtensa-esp32-elf-gcc, cmake, esptool.py if first time
   - Expected output: "Processing espressif32 (platform: espressif32; board: esp32dev; framework: arduino)"

5. **Download and verify all libraries**
   - Run: `pio pkg install`
   - Expected: All 7 libraries + 3 sub-dependencies download to `.pio/libdeps/`
   - Verify: `ls .pio/libdeps/freenove-fnk0114n-sensor-pages/` | wc -l` should show 10+ libraries

6. **Document TFT_eSPI library status**
   - Custom Freenove version required (not available via package manager)
   - Status: Already in `lib/Freenove/TFT_eSPI/` (from Phase 1)
   - Validation: Check `lib/Freenove/TFT_eSCI/library.properties` exists

7. **Verify build configuration**
   - Run: `pio project config --env=freenove-fnk0114n-sensor-pages`
   - Expected: TOML output showing platform, board, framework, all libraries listed

### Acceptance Criteria

- [ ] `platformio.ini` contains all 7 lib_deps entries
- [ ] All build_flags present: `-Iinclude`, `-D USER_SETUP_LOADED`, `-include include/User_Setup.h`
- [ ] Serial port configuration present (even if platform-dependent)
- [ ] `pio project config` shows all dependencies resolved
- [ ] `.pio/libdeps/` directory contains 10+ libraries
- [ ] No errors in `pio pkg list`

### Tests/Validation Before Proceeding

```bash
# Verify configuration syntax
pio project config --env=freenove-fnk0114n-sensor-pages > /dev/null && echo "Config OK"

# Count downloaded libraries
ls .pio/libdeps/freenove-fnk0114n-sensor-pages/ 2>/dev/null | wc -l

# Verify toolchain available
xtensa-esp32-elf-gcc --version | head -1

# Check TFT_eSPI custom setup file
ls lib/Freenove/TFT_eSPI_Setups/FNK0114N_3.5_320x480_ST7796.h && echo "Display setup OK"
```

### Compilation Test (Expected to Fail)

```bash
# This WILL fail because src/main.cpp doesn't exist yet
# But it verifies toolchain + dependencies are functional
pio run 2>&1 | grep -E "Compiling|error" | head -10
```

Expected error: `fatal error: main.cpp: No such file or directory`

This is OK; Phase 3 will create the source file.

### Dependencies on Other Phases
- **Blocking:** Phase 3, 5, 7, 9 (all depend on libraries being available)
- **Blocked by:** Phase 1

---

## Phase 3: Domain Model

### Objective
Define all data structures, global variables, constants, and type definitions that represent the application's data model. Establish the "shape" of all application state.

### Files/Components to Create

**Primary file:**
- `src/main.cpp` (skeleton with includes, #define, struct definitions, global variables)

**Secondary files:**
- None (all type definitions go in main.cpp or as inline comments)

### Dependencies

**Required by Phase 3:**
- Phase 1 (project structure)
- Phase 2 (libraries available)
- [02-architecture.md](docs/02-architecture.md) (architecture diagram)
- [04-data-model.md](docs/04-data-model.md) (struct field-by-field specification)
- [01-requirements.md](docs/01-requirements.md) (business requirements for fields)

**Enables Phase(s):**
- Phase 5 (business logic depends on struct definitions)
- Phase 7 (UI depends on data struct definitions)

### Implementation Order

1. **Create `src/main.cpp` with include statements**
   - All standard Arduino includes: `#include <Arduino.h>`, `#include <Wire.h>`
   - All sensor library includes (7 Adafruit + 2 protocol libraries)
   - All standard C++ includes: `<math.h>`, `<string.h>`
   - Source: [04-data-model.md](docs/04-data-model.md), "Struct Definitions" section; [12-dependencies.md](docs/12-dependencies.md)

2. **Define all #define constants**
   - I2C addresses (9 sensors): 0x40, 0x10, 0x29, etc. (see [09-configuration.md](docs/09-configuration.md))
   - GPIO pins: SDA=32, SCL=25, GAMEPAD_ADDR=0x50, ENCODER_ADDR=0x36, etc.
   - Register addresses: VEML6075_REG_*, SCD30_CMD_*
   - Button constants: GAMEPAD_BUTTON_Y, GAMEPAD_BUTTON_A, etc.
   - Timing intervals: SENSOR_REFRESH_MS=500, INPUT_REFRESH_MS=20
   - Page count: PAGE_COUNT=9
   - Source: [01-requirements.md](docs/01-requirements.md), [09-configuration.md](docs/09-configuration.md)

3. **Define all struct types (in order of dependency)**
   - **SensorStatus**: 9 bool flags (ina219, veml6075, veml6070, tsl2591, si1145, scd30, bme280, encoder, gamepad)
     - Source: [04-data-model.md](docs/04-data-model.md) > SensorStatus
   - **Ina219Values**: float shunt/bus/load/current/power, float chargeMah, bool initialized, uint32_t lastSampleUs
   - **Veml6075Values**: uint16_t uva/uvb/comp1/comp2, float uvaComp/uvbComp, float index
   - **Veml6070Values**: uint16_t raw (combined MSB:LSB), uint8_t lsb/msb
   - **Scd30Values**: float co2/temp/humidity
   - **EncoderValues**: int32_t position/delta, bool pressed, uint32_t presses, uint8_t brightness, const char* color
   - **GamepadValues**: uint16_t x/y, uint32_t buttons, uint8_t eventCount/hasEvent/lastEvent
   - **Ds18b20Values**: DeviceAddress addresses[8], float temperatures[8], uint8_t count

4. **Initialize global instances for all libraries**
   - `TFT_eSPI tft(320, 480);`
   - `Adafruit_INA219 ina219(0x40);`
   - `Adafruit_TSL2591 tsl2591(2591);`
   - `Adafruit_SI1145 si1145;`
   - `Adafruit_BME280 bme280;`
   - `Adafruit_seesaw gamepad;`
   - `Adafruit_seesaw encoder;`
   - `seesaw_NeoPixel encoderLed;`


5. **Instantiate all global data structures**
   - `SensorStatus available{};` (zero-initialized)
   - `Ina219Values ina219Values{};`
   - `Veml6075Values veml6075Values{};`
   - `Veml6070Values veml6070Values{};`
   - `Scd30Values scd30Values{};`
   - `EncoderValues encoderValues{};`
   - `GamepadValues gamepadValues{};`


6. **Define auxiliary global variables**
   - Current page: `uint8_t currentPage = 0;`
   - Last page (for change detection): `uint8_t lastPage = 0xFF;`
   - Encoder state: `int32_t encoderLastPosition = 0;`, `uint32_t encoderPressCount = 0;`, `bool encoderLastPressed = false;`, `uint8_t encoderColorIndex = 0;`, `uint8_t encoderBrightness = 64;`
   - Timing gate next-execution times: `uint32_t nextInaS, nextEncoderMs, nextGamepadMs, nextSensorMs;`
   - Source: [04-data-model.md](docs/04-data-model.md)

7. **Define LED color palette**
   - `const uint32_t encoderColors[4]` = {blue, green, red, purple} RGB values
   - `const char* encoderColorNames[4]` = {"blue", "green", "red", "purple"}
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-010

8. **Add stub function declarations** (actual implementation in Phase 5)
   - Sensor read functions: `void readIna219At50Hz()`, `void readEncoderAt50Hz()`, `void readGamepadAt40Hz()`, etc.
   - Display functions: `void drawHeader()`, `void row()`, `void footer()`, `void unavailable()`, `void drawCurrentPage()`, etc.
   - Page renderers: `void pageBme280()`, `void pageGamepad()`, ..., `void pageVeml6075()`
   - Initialization: `void detectSensors()`, `void setup()`, `void loop()`

### Acceptance Criteria

- [ ] `src/main.cpp` compiles without errors: `pio run` produces object file
- [ ] All 8 structs defined with correct fields (can verify with `sizeof()` in compile error messages if structs not used)
- [ ] All 9 I2C addresses #define'd correctly
- [ ] All GPIO pin assignments #define'd
- [ ] All timing intervals #define'd with correct values (ms units)
- [ ] PAGE_COUNT = 9
- [ ] All global variables declared and zero-initialized
- [ ] All library instances created with correct parameters
- [ ] Encoder color arrays have exactly 4 entries
- [ ] No undefined symbol errors in compilation

### Tests/Validation Before Proceeding

```bash
# Verify compilation (will fail on missing function definitions, but structs valid)
pio run 2>&1 | grep -E "struct|Ina219Values|SensorStatus" || echo "Structs OK"

# Check struct sizes (via compiler errors, which is OK for now)
pio run 2>&1 | grep "undefined reference" | head -5

# Verify no include errors
pio run 2>&1 | grep -c "fatal error" && echo "FAILED: Include error" || echo "Includes OK"
```

### Dependencies on Other Phases
- **Blocking:** Phase 5, 7, 9 (all depend on data model)
- **Blocked by:** Phase 1, 2

---

## Phase 4: Database/Persistence

### Objective
Document that this phase is NOT applicable to the project and explain the design rationale.

### Design Decision

**No persistence layer is implemented in sensor-pages-dashboard.**

**Rationale:**
1. **RAM-only data model** — All state resides in global structs (~300 bytes)
2. **No reboot requirement** — Embedded device runs continuously; data loss on reboot is acceptable
3. **No configuration storage** — All settings are compile-time constants (hardcoded in source)
4. **No event logging** — No history or audit trail needed
5. **Embedded constraints** — SPIFFS/NVS would add complexity for limited benefit

**Source:** [04-data-model.md](docs/04-data-model.md), "Lifecycle" section

### Files/Components: None

No files to create. No persistence code required.

### Implementation Order: N/A

### Acceptance Criteria

- [ ] Document affirms no persistence required
- [ ] Code review confirms no SPIFFS/NVS/EEPROM operations in firmware
- [ ] All state is ephemeral (lost on reboot)

### Tests/Validation Before Proceeding

```bash
# Verify no persistence APIs used in main.cpp
grep -i "spiffs\|eeprom\|nvs\|littlefs" src/main.cpp || echo "No persistence code found (expected)"
```

### Dependencies on Other Phases
- **Blocking:** None
- **Blocked by:** None (Phase 3 can confirm no persistence structs needed)

---

## Phase 5: Business Logic

### Objective
Implement all sensor polling, data processing, event handling, state machines, and algorithmic rules. This is the largest and most complex phase.

### Files/Components to Create

**Modifications to `src/main.cpp`:**
- Sensor read functions (9 total)
- Helper functions (CRC, register I/O, float conversion)
- State machine functions (DS18B20 async)
- Timing gate logic
- Main `setup()` and `loop()` functions

### Dependencies

**Required by Phase 5:**
- Phase 1 (project structure)
- Phase 2 (libraries available)
- Phase 3 (data model defined)
- [01-requirements.md](docs/01-requirements.md) (functional requirements FR-004 through FR-011)
- [06-business-rules.md](docs/06-business-rules.md) (all 26 business rules)
- [09-configuration.md](docs/09-configuration.md) (timing intervals, I2C addresses, pin assignments)

**Enables Phase(s):**
- Phase 7 (UI depends on sensor data being read and stored)
- Phase 10 (tests depend on business logic being testable)

### Implementation Order

#### Group 1: Sensor Detection & Initialization

1. **Implement `detectSensors()` function**
   - Probe each of 9 I2C sensors at their addresses (0x40, 0x10, 0x29, etc.)
   - Check for SCD30 ready status via command 0x0202
   - Scan DS18B20 bus and count devices (max 8)
   - Set `available.X` flags based on detection results
   - Print warnings to Serial if optional sensors missing (e.g., gamepad)
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-001
   - Source: [09-configuration.md](docs/09-configuration.md), Device Addresses table

2. **Implement `setup()` function**
   - Initialize Serial @ 115200 baud
   - Initialize I2C @ 400 kHz (Wire.begin(SDA_PIN, SCL_PIN, 400000))
   - Initialize 1-Wire bus
   - Initialize TFT display (init, setRotation(3), fillScreen BLACK)
   - Call `detectSensors()`
   - Perform initial sensor reads (one cycle for each sensor)
   - Configure sensor settings: TSL2591 gain/timing, VEML6075 config write, DS18B20 9-bit resolution
   - Initialize encoder LED brightness (64)
   - Print "Boot complete" to Serial
   - Display page 0 (BME280) on TFT
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-024, BR-025, BR-026

#### Group 2: Sensor Read Functions (9 total)

3. **Implement `readIna219At50Hz()` function**
   - Check timing gate: `(int32_t)(nowUs - nextInaSampleUs) >= 0` (every 20 µs)
   - Read shunt, bus, current, power via Adafruit library
   - Compute load = bus - shunt/1000
   - Apply Coulomb accumulation if `initialized == true`:
     - elapsedUs = nowUs - lastSampleUs
     - If elapsedUs < 200000 ms: chargeMah += current * elapsedUs / 3.6e9
     - Else: skip (gap too large)
   - Negate current/shunt (sign convention)
   - Set initialized = true on first read (but don't accumulate)
   - Store nowUs in lastSampleUs
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-004, BR-022
   - Source: [05-api.md](docs/05-api.md), INA219 section

4. **Implement `readEncoderAt50Hz()` function**
   - Check timing gate: (currentPage == 4 || currentPage != lastPage) AND time gate fires
   - Read encoder position, compute delta
   - Detect button falling edge: (pressed && !lastPressed)
   - On falling edge: colorIndex = (colorIndex + 1) % 4, increment pressCount, call updateEncoderLed()
   - On dial rotation (delta != 0): brightness = constrain(brightness + delta*8, 0, 255), call updateEncoderLed()
   - Update encoderValues struct with position, delta, pressed, color name
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-010, BR-011, BR-018

5. **Implement `readGamepadAt40Hz()` function**
   - Check timing gate: (int32_t)(now - nextGamepadMs) >= 0 (every 25ms)
   - Save previous button state
   - Read analog X, Y (invert: 1023 - raw)
   - Read digital button bitmask
   - Detect page navigation: Y falling edge → page = (page - 1 + 10) % 10, A falling edge → page = (page + 1) % 10
   - Read keypad events (for display only; not used for navigation)
   - Store values in gamepadValues struct
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-012, BR-013, BR-019, BR-020

6. **Implement `readBme280()`, `readVeml6075()`, `readVeml6070()`, `readSi1145()`, `readTsl2591()`, `readScd30()` functions**
   - Each reads from specific I2C sensor
   - Apply sensor-specific processing (UV compensation, CRC validation, etc.)
   - Only update global struct if all reads succeed
   - Return false on I2C error (stale data persists)
   - Source: [04-data-model.md](docs/04-data-model.md), each sensor struct
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-005 (CRC), BR-006 (UV comp), BR-007 (VEML6070 levels)
   - [BLOCKED BY MISSING INFORMATION]: Exact register addresses for VEML sensors not fully specified; refer to sensor datasheets

#### Group 3: Helper Functions

7. **Implement CRC8 validation for SCD30**
   - Function: `uint8_t scd30Crc(uint8_t* data, uint8_t len)` with polynomial 0x31, initial value 0xFF
   - Used to validate 3 fields × 2 data bytes + 1 CRC byte = 18 bytes total
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-005

8. **Implement register read/write helpers**
   - `bool readReg16(uint8_t addr, uint8_t reg, uint16_t& value)` — Read 16-bit register via I2C
   - `bool writeReg16(uint8_t addr, uint8_t reg, uint16_t value)` — Write 16-bit register via I2C
   - Used by VEML sensors and other custom register access
   - Source: [05-api.md](docs/05-api.md), I2C Device section

9. **Implement UV compensation formula**
   - `void applyVeml6075Compensation(Veml6075Values& v)` — Apply 2.22×comp1 + 1.33×comp2 subtraction with max(0, ...) clamping
   - Compute UV index: ((uvaComp * 0.001461) + (uvbComp * 0.002591)) * 0.5
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-006

10. **Implement SCD30 command sender**
    - `bool scd30Command(uint16_t cmd)` — Send command to SCD30
    - `bool scd30Ready()` — Check data ready status via command 0x0202
    - Source: [05-api.md](docs/05-api.md), SCD30 section

#### Group 4: 1-Wire Temperature Polling

11. **Implement `performDs2484Scan()` 1-Wire bus scan**
    - Initialize Adafruit_DS248x adapter via I2C (0x18)
    - Call OneWireSearchReset() to start device enumeration
    - Loop: OneWireSearch() fills 8-byte ROM address array
    - Validate CRC8 of each address using OneWire::crc8()
    - Store discovered sensors in ds2484Values.sensors[0..2] (max 3)
    - Timing: First scan forced on boot, then every 60 seconds
    - Non-blocking: completed within single loop cycle
    - Source: [05-api.md](docs/05-api.md), Device 10; [04-data-model.md](docs/04-data-model.md), Section 2.8

12. **Implement `readDs2484Temperatures()` async temperature polling**
    - Stage 1 (every 3 seconds): Broadcast Convert T command (0xCC 0x44) to all devices
    - Wait 850 ms for 12-bit conversion (750 ms typical + margin)
    - Stage 2 (same cycle): For each discovered sensor, use Match ROM (0x55) + Read Scratchpad (0xBE)
    - Read 2 bytes (LSB, MSB) and convert: temp_C = (int16_t)((MSB << 8) | LSB) * 0.0625
    - Error handling: 3-strike lockout per sensor; recovery on success
    - Non-blocking: async conversion via DS2484 hardware timer
    - Source: [05-api.md](docs/05-api.md), Device 10; [04-data-model.md](docs/04-data-model.md), Section 2.8

#### Group 5: Timing & Main Loop

13. **Implement timing gates in `loop()` function**
    - All timing gates use signed comparison for rollover safety: `(int32_t)(now - deadline) >= 0`
    - Gamepad read: gate fires every 25 ms → calls readGamepadAt40Hz()
    - Encoder read: gate fires every 20 ms when page 4 OR page changing → calls readEncoderAt50Hz()
    - INA219 read: gate fires every 20 µs → calls readIna219At50Hz() using micros()
    - Sensor read: gate fires every 500 ms → calls readSensors() wrapper (reads all non-Coulomb sensors)
    - DS18B20 update: called every loop cycle (self-gated via pending state)
    - DS18B20 rescan: gate fires every 3000 ms → calls rescanDs18b20()
    - Page redraw: full (with header) when currentPage != lastPage; data-only every 20 or 500 ms
    - Source: [06-business-rules.md](docs/06-business-rules.md), BR-021
    - Source: [09-configuration.md](docs/09-configuration.md), Timing Configuration table

14. **Implement `loop()` main event loop**
    - Initialize timing gate variables on first iteration (use nextXxx = 0 or = now)
    - Call updateDs18b20() every cycle (non-blocking, self-gated)
    - Check all timing gates in order (gamepad → encoder → INA219 → sensors → rescan → page redraw)
    - Update lastPage = currentPage at end of cycle
    - No delays; all operations cooperative scheduling via timing gates
    - Source: [02-architecture.md](docs/02-architecture.md), "Event Loop" section

#### Group 6: Sensor Data Wrapper

15. **Implement `readSensors()` wrapper function**
    - Call all non-Coulomb sensor reads: readBme280, readVeml*, readSi1145, readTsl2591, readScd30
    - Called every 500 ms from loop timing gate
    - INA219 and gamepad/encoder have separate read cycles
    - Source: [01-requirements.md](docs/01-requirements.md), FR-001

### Acceptance Criteria

- [ ] All 9 sensor read functions implemented (5 Adafruit + 2 seesaw + 2 seesaw special case)
- [ ] `detectSensors()` probes all 9 addresses and sets available[] flags correctly
- [ ] `setup()` calls detectSensors, initializes all buses, configures display, prints boot messages
- [ ] `loop()` compiles without errors and doesn't block (uses timing gates only)
- [ ] CRC8 algorithm matches SCD30 datasheet polynomial 0x31
- [ ] UV compensation formulas apply exact hardcoded coefficients (2.22, 1.33, etc.)
- [ ] Coulomb accumulation formula matches specification: current * elapsed / 3.6e9
- [ ] Timing gates use signed comparison: `(int32_t)(now - deadline) >= 0`
- [ ] DS18B20 state machine uses 3 stages (request → wait 100ms → fetch)
- [ ] Hot-plug detection compares ROM addresses via memcmp()
- [ ] All I2C addresses match [09-configuration.md](docs/09-configuration.md) Device Addresses table
- [ ] All GPIO pins match [09-configuration.md](docs/09-configuration.md) GPIO Pin Assignments table
- [ ] No blocking operations in loop() (all gates, no delay())
- [ ] Serial output on startup (debug information)

### Tests/Validation Before Proceeding

```bash
# Verify compilation succeeds
pio run 2>&1 | tail -5 | grep -E "Compiling|Linking"

# Check for blocking operations (delay, millis in while loop, etc.)
grep -n "delay(" src/main.cpp && echo "WARNING: delay() found" || echo "No delay() calls"
grep -n "while" src/main.cpp && echo "WARNING: while loop found" || echo "No while loops"

# Verify timing gate pattern appears 5+ times
grep -c "(int32_t)(.*now.*-.*next.*) >= 0" src/main.cpp

# Verify all 9 sensor addresses present
grep -c "0x40\|0x10\|0x29\|0x36\|0x38\|0x39\|0x60\|0x61\|0x77\|0x50" src/main.cpp

# Check for Serial.begin and Wire.begin
grep "Serial.begin\|Wire.begin" src/main.cpp | wc -l  # Should be 2 or more
```

### Dependencies on Other Phases
- **Blocking:** Phase 7 (UI depends on sensor data), Phase 10 (tests depend on algorithms)
- **Blocked by:** Phase 1, 2, 3

---

## Phase 6: API/Backend

### Objective
Document that this phase is NOT applicable and explain the embedded firmware architecture.

### Design Decision

**No API or backend exists in sensor-pages-dashboard.**

**Rationale:**
1. **Embedded firmware** — Runs on ESP32 microcontroller directly; no HTTP/REST/RPC APIs
2. **Offline device** — No network connectivity (WiFi/Bluetooth not used)
3. **Serial debug only** — Serial UART @ 115200 baud outputs text for monitoring; not machine-readable API
4. **Single-board deployment** — No multi-device coordination or cloud integration

**Serial Monitor Output:**
- Sensor detection status (on boot)
- Optional I2C error messages (if implemented)
- No continuous telemetry

**Source:** [01-requirements.md](docs/01-requirements.md), "Functional Requirements"; [02-architecture.md](docs/02-architecture.md)

### Files/Components: None

No API server, no REST endpoints, no RPC interfaces required.

### Implementation Order: N/A

### Acceptance Criteria

- [ ] Firmware runs offline (no network connections attempted)
- [ ] Code contains no WiFi.h, BluetoothSerial.h, or HTTP client imports
- [ ] Serial output is informational text only (no JSON/protocol encoding)

### Tests/Validation Before Proceeding

```bash
# Verify no network library includes
grep -i "wifi\|bluetooth\|httpclient" src/main.cpp && echo "ERROR: Network code found" || echo "OK: No network code"
```

### Dependencies on Other Phases
- **Blocking:** None
- **Blocked by:** None (Phase 5 can confirm no API needed)

---

## Phase 7: Frontend/UI

### Objective
Implement all user-facing display rendering, page layouts, navigation logic, and visual feedback. Create all 10 sensor pages and supporting UI components.

### Files/Components to Create

**Modifications to `src/main.cpp`:**
- Display helper functions (writeText, row, footer, unavailable)
- Page renderer functions (10 total: pageBme280, pageDs18b20, ..., pageVeml6075)
- drawCurrentPage() dispatcher
- drawHeader() function

### Dependencies

**Required by Phase 7:**
- Phase 1 (project structure)
- Phase 2 (TFT_eSPI library available)
- Phase 3 (data model with sensor structs)
- Phase 5 (sensor data being read and stored)
- [07-ui.md](docs/07-ui.md) (page layouts, pixel coordinates, colors, refresh rates)
- [09-configuration.md](docs/09-configuration.md) (display colors, layout constants, TFT pins)

**Enables Phase(s):**
- Phase 10 (UI tests verify pages render without crashes)
- Phase 11 (build system includes display assets)

### Implementation Order

#### Group 1: Display Initialization

1. **Initialize TFT display in `setup()`**
   - Call `tft.init()`
   - Set rotation: `tft.setRotation(3)` (landscape, 90° rotated)
   - Clear screen: `tft.fillScreen(TFT_BLACK)`
   - Set default text datum and color
   - Source: [09-configuration.md](docs/09-configuration.md), Display Hardware Configuration

#### Group 2: Display Helper Functions

2. **Implement `writeText(x, y, text, color, size)` function**
   - Set text color: `tft.setTextColor(color)`
   - Set text size: `tft.setTextSize(size)`
   - Set position: `tft.setCursor(x, y)` or `tft.setTextDatum(TL_DATUM)` + `tft.drawString()`
   - Write text to display
   - Used by all page renderers for labels and titles
   - Source: [07-ui.md](docs/07-ui.md), Components section

3. **Implement `drawHeader(title)` function**
   - Clear entire screen: `tft.fillScreen(TFT_BLACK)`
   - Write cyan title @ (6, 8), font size 4
   - Draw dark grey horizontal line @ y=42 with length 468 pixels
   - Source: [07-ui.md](docs/07-ui.md), Header component
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-015

4. **Implement `row(y, label, value, color)` function**
   - Write white label text (left-aligned) @ (6, y), size 2
   - Write colored value text (right-aligned) @ (474, y), size 2
   - Source: [07-ui.md](docs/07-ui.md), Data Row component

5. **Implement `footer(text)` function**
   - Draw dark grey horizontal line @ y=290
   - Write text @ (6, 294), size 1, white or yellow
   - Source: [07-ui.md](docs/07-ui.md), Footer component

6. **Implement `unavailable(name, address, addressLabel)` function**
   - Write red "Sensor nicht vorhanden" @ (6, 64), size 2
   - Write yellow address @ (96, addressLabel)
   - Write white navigation hints @ (142, 166)
   - Write "Keine Daten verfuegbar" footer
   - Called when sensor not detected at boot
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-017

#### Group 3: Page Layout & Rendering

7. **Implement `drawCurrentPage(bool pageChanged)` dispatcher**
   - If pageChanged == true: call drawHeader() (clears screen)
   - Else: skip header (data-only refresh)
   - Switch on currentPage (0-8) and call appropriate page renderer
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-015

8. **Implement 9 page renderer functions** (in order of [01-requirements.md](docs/01-requirements.md))

   **Page 0: `pageBme280()`**
   - Check available.bme280; if false, return unavailable(...)
   - Render: Temperature, Humidity, Pressure, Altitude, Dew point
   - Colors: Temperature=GREEN, Pressure=GREEN, etc.
   - Formulas: Dew point = standard meteorological formula (if specified in data model)
   - [BLOCKED BY MISSING INFORMATION]: Exact dew point calculation formula not in documentation

   **Page 1: `pageDs18b20()`**
   - Check available.ds18b20; if false, return unavailable(...) [but DS18B20 unlikely to report as unavailable; check hot-plug count instead]
   - Loop through ds18b20Values.count devices
   - For each device: render ROM address (yellow) and temperature (green or red if disconnected)
   - Detect disconnect sentinel: tempC <= DEVICE_DISCONNECTED_C → show "disconnected" in red
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-014

   **Page 2: `pageGamepad()`**
   - Check available.gamepad; if false, return unavailable(...)
   - Render: Joystick X, Joystick Y, 6 button states (PRESSED/released), last keypad event
   - Joystick values: 0-1023 (already inverted in readGamepadAt40Hz)
   - Button states: "PRESSED" or "released" (based on active-low bits)
   - Source: [07-ui.md](docs/07-ui.md), Page 2 Gamepad QT

   **Page 3: `pageIna219()`**
   - Check available.ina219; if false, return unavailable(...)
   - Render: Bus Voltage, Shunt Voltage, Load Voltage, Current, Power, Coulomb counter (mAh)
   - Coulomb counter: display chargeMah (accumulates since boot)
   - Colors: Voltage=GREEN, Current=GREEN, Power=CYAN
   - Precision: 3 decimals for voltages, 1-2 for currents
   - Source: [07-ui.md](docs/07-ui.md), Page 3 INA219

   **Page 4: `pageEncoder()`**
   - Check available.encoder; if false, return unavailable(...)
   - Render: Position, Delta, Button state, Press count, LED color, LED brightness
   - Position: current encoder position (int32_t)
   - Delta: position change since last read
   - Button: "PRESSED" or "released"
   - Press count: total presses since boot
   - Color: name from encoderColorNames[colorIndex]
   - Brightness: 0-255 with "/255" suffix
   - Source: [07-ui.md](docs/07-ui.md), Page 4 Rotary Encoder

   **Page 5: `pageScd30()`**
   - Check available.scd30; if false, return unavailable(...)
   - Render: CO₂ (ppm), Temperature (°C), Humidity (%)
   - All values: 2 decimals
   - Colors: CO₂=CYAN (computed), Temp=GREEN, Humidity=GREEN
   - Data from scd30Values struct (CRC-validated)
   - Source: [07-ui.md](docs/07-ui.md), Page 5 SCD30

   **Page 6: `pageSi1145()`**
   - Check available.si1145; if false, return unavailable(...)
   - Render: Visible spectrum, Infrared spectrum, UV index
   - Colors: Visible=GREEN, IR=ORANGE, UV=CYAN
   - Source: [07-ui.md](docs/07-ui.md), Page 6 SI1145

   **Page 7: `pageTsl2591()`**
   - Check available.tsl2591; if false, return unavailable(...)
   - Render: Full spectrum, IR, Visible, Calculated lux
   - Colors: Lux=CYAN (computed value)
   - Source: [07-ui.md](docs/07-ui.md), Page 7 TSL2591

   **Page 8: `pageVeml6070()`**
   - Check available.veml6070; if false, return unavailable(...)
   - Render: Raw ADC value, UV level (classified into 5 bins)
   - UV level classification: 5 thresholds (200, 500, 1000, 2000) → "Very low", "Low", "Moderate", "High", "Very high"
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-007
   - Source: [07-ui.md](docs/07-ui.md), Page 8 VEML6070

   **Page 9: `pageVeml6075()`**
   - Check available.veml6075; if false, return unavailable(...)
   - Render: UVA raw, UVB raw, COMP1, COMP2, UVA compensated, UVB compensated, UV index
   - Compensated values: computed via formula in Phase 5
   - UV index: derived from compensated values
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-006
   - Source: [07-ui.md](docs/07-ui.md), Page 9 VEML6075

### Acceptance Criteria

- [ ] All 9 page renderers implemented with correct layout (pixel coordinates match [07-ui.md](docs/07-ui.md))
- [ ] All page titles displayed in cyan
- [ ] All labels displayed in white
- [ ] All sensor values displayed in correct colors (green/orange/cyan/red as specified; never magenta)
- [ ] Header drawn with divider line @ y=38
- [ ] Footer drawn with navigation hints
- [ ] "Sensor nicht vorhanden" error screen displays for missing sensors
- [ ] All error screens allow navigation (Y/A buttons still work)
- [ ] Coulomb counter displays on INA219 page (mAh)
- [ ] UV level classification bins display correctly (5 categories)
- [ ] Encoder color shows one of: blue, green, red, purple
- [ ] Encoder brightness shows 0-255 range
- [ ] Gamepad button states show "PRESSED" or "released"
- [ ] Data precision matches spec (2-3 decimals for floats)
- [ ] Page redraw performance acceptable (no visible flicker)

### Tests/Validation Before Proceeding

```bash
# Verify all 9 page functions present
grep -c "void page" src/main.cpp  # Should show 9+

# Verify header/footer/row helpers present
grep -c "void drawHeader\|void row\|void footer\|void unavailable\|void writeText" src/main.cpp  # Should show 5+

# Check all I2C addresses used in error screens
grep "unavailable" src/main.cpp | grep "0x"  # Should show all addresses

# Verify page titles match [07-ui.md](docs/07-ui.md)
grep -E "BME280|DS18B20|Gamepad|INA219|Rotary|SCD30|SI1145|TSL2591|VEML6070|VEML6075" src/main.cpp | wc -l
```

### Manual Testing on Hardware

- [ ] All 9 pages render without crashes
- [ ] Page titles visible and correct
- [ ] Data displays in correct colors
- [ ] Y/A buttons navigate pages (modulo wraps 0↔9)
- [ ] "Sensor nicht vorhanden" displays for missing sensors
- [ ] No screen tearing or flickering

### Dependencies on Other Phases
- **Blocking:** Phase 10 (tests verify UI rendering)
- **Blocked by:** Phase 1, 2, 3, 5 (all dependencies must be ready)

---

## Phase 8: Authentication/Authorization

### Objective
Document that this phase is NOT applicable to embedded firmware without authentication.

### Design Decision

**No authentication or authorization system exists.**

**Rationale:**
1. **Embedded device** — Runs in trusted laboratory/industrial environment
2. **No multi-user support** — Single operator, direct access
3. **No remote access** — No network APIs; physical access only
4. **Physical security model** — Device assumes trusted handler
5. **USB programming unrestricted** — Anyone with USB access can reprogram

**Security Model:**
- Assume trusted environment (lab, manufacturing floor, etc.)
- No passwords, tokens, or API keys
- Physical access = full control
- Serial monitor outputs debugging info (assume trusted viewer)

**Source:** [09-configuration.md](docs/09-configuration.md), Security Considerations

### Files/Components: None

No authentication code, no permission checks, no session management.

### Implementation Order: N/A

### Acceptance Criteria

- [ ] No login/password prompts
- [ ] No user roles or permissions checks
- [ ] No encryption of serial data
- [ ] Documentation affirms embedded device model

### Tests/Validation Before Proceeding

```bash
# Verify no authentication code
grep -i "auth\|password\|login\|token\|session" src/main.cpp && echo "ERROR: Auth code found" || echo "OK"
```

### Dependencies on Other Phases
- **Blocking:** None
- **Blocked by:** None

---

## Phase 9: External Integrations

### Objective
Implement all hardware protocol layers and communication with external devices (I2C sensors, 1-Wire devices, SPI display, serial monitor).

### Files/Components to Create

**Modifications to `src/main.cpp`:**
- I2C initialization and error handling
- 1-Wire protocol initialization
- SPI display initialization (in setup)
- Serial UART initialization (in setup)
- All register read/write operations (helpers from Phase 5)

**No new files** — All integration code in main.cpp

### Dependencies

**Required by Phase 9:**
- Phase 1 (project structure)
- Phase 2 (libraries available)
- Phase 3 (data model defined)
- Phase 5 (sensor read functions implemented)
- [09-configuration.md](docs/09-configuration.md) (GPIO pins, I2C addresses, baud rates)
- [12-dependencies.md](docs/12-dependencies.md) (library APIs)

**Enables Phase(s):**
- Phase 10 (integration tests verify hardware communication)
- Phase 11 (build includes all protocols)

### Implementation Order

#### Group 1: I2C Bus Initialization

1. **Initialize I2C bus in `setup()`**
   - `Wire.begin(SDA_PIN, SCL_PIN, 400000)` — Initialize on GPIO 32, 25 @ 400 kHz
   - Assume 4.7 kΩ external pull-ups on hardware (not configured in software)
   - Source: [09-configuration.md](docs/09-configuration.md), I2C Configuration

2. **Implement I2C error handling (optional)**
   - Wire library uses internal timeouts (exact values [UNKNOWN], see [12-dependencies.md](docs/12-dependencies.md))
   - Detect I2C errors via Wire.endTransmission() return code:
     - 0 = success
     - 1 = data too long
     - 2 = address NACK
     - 3 = data NACK
     - 4 = other error
   - On error: print to Serial (if debug enabled), return false from read function
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-002

#### Group 2: I2C-to-1-Wire Bus Initialization

3. **Initialize DS2484 I2C-to-1-Wire adapter in `setup()`**
   - `Adafruit_DS248x ds2484Master` — Create adapter object
   - `ds2484Master.begin(&Wire, DS2484_ADDR)` — Initialize at I2C address 0x18
   - `initDs2484()` — Guard function that checks initialization success
   - Set `ds2484Values.lastScanMs = 0` to force immediate bus scan on first loop
   - Source: [05-api.md](docs/05-api.md), Device 10; [04-data-model.md](docs/04-data-model.md), Section 2.8

#### Group 3: SPI Display Initialization

4. **Initialize TFT display in `setup()`**
   - Already documented in Phase 7 (Group 1)
   - TFT_eSPI automatically uses SPI pins (GPIO 12/13/14) configured in User_Setup.h
   - Source: [09-configuration.md](docs/09-configuration.md), Display Hardware Configuration

#### Group 4: Serial UART Initialization

5. **Initialize Serial monitor in `setup()`**
   - `Serial.begin(115200)` — Standard Arduino Serial on UART0
   - Used for debug output only (sensor detection messages, optional error logs)
   - Baud rate: 115200 (matches platformio.ini monitor_speed)
   - Print boot messages: "Detecting sensors...", sensor detection results, "Boot complete"
   - Source: [09-configuration.md](docs/09-configuration.md), Serial Monitor Output

#### Group 5: Sensor-Specific Protocol Handlers

6. **Implement I2C sensor begin() calls in `detectSensors()`**
   - Each sensor library provides `begin()` method with specific parameters
   - Example: `bme280.begin(0x77, &Wire)` — Specify address and Wire instance
   - Example: `ina219.begin(&Wire)` — Pass Wire object
   - Rationale: Allows multiple I2C buses (though only one used here)
   - Source: [12-dependencies.md](docs/12-dependencies.md), Library APIs quick reference

7. **Implement SCD30 custom I2C protocol**
   - SCD30 uses proprietary protocol (not Adafruit library; custom implementation)
   - Command format: `scd30Command(uint16_t cmd)` sends 2-byte big-endian command
   - Response format: 18-byte array with CRC8 validation
   - Start measurement: `scd30Command(0x0010)`
   - Check ready: `scd30Command(0x0202)` — reads 2-byte status
   - Read data: `scd30Command(0x0300)` — reads 18 bytes (3 floats with CRC8)
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-005
   - [BLOCKED BY MISSING INFORMATION]: Exact SCD30 command protocol details; refer to SCD30 datasheet

8. **Implement VEML sensor register access**
   - Use `readReg16()` and `writeReg16()` helpers (defined in Phase 5)
   - VEML6075: read UVA/UVB/COMP1/COMP2 from registers 0x07/0x09/0x0A/0x0B
   - VEML6070: read dual-address LSB (0x38) + MSB (0x39)
   - VEML6075 config write: `writeReg16(0x10, 0x00, 0x0000)` at boot
   - Source: [09-configuration.md](docs/09-configuration.md), Device Addresses table
   - [BLOCKED BY MISSING INFORMATION]: Exact register addresses; refer to VEML datasheets

9. **Implement Gamepad/Encoder seesaw protocol**
   - Use Adafruit seesaw library: `digitalRead()`, `digitalReadBulk()`, `analogRead()`, `getKeypadCount()`, `readKeypad()`
   - Library abstracts I2C protocol details
   - Source: [12-dependencies.md](docs/12-dependencies.md), seesaw Library section

### Acceptance Criteria

- [ ] I2C bus initialized @ 400 kHz on correct GPIO pins (32, 25)
- [ ] All 9 I2C sensors probed at correct addresses (0x40, 0x10, 0x29, 0x36, 0x38/0x39, 0x60, 0x61, 0x77, 0x50)
- [ ] 1-Wire bus initialized on GPIO 23 with 9-bit resolution and non-blocking mode
- [ ] DS18B20 devices enumerated and count stored
- [ ] TFT display initialized and clears to black
- [ ] Serial monitor @ 115200 baud outputs boot messages
- [ ] I2C error handling returns false on Wire errors (stale data persists)
- [ ] SCD30 custom protocol implemented (command format correct)
- [ ] Register read/write helpers handle all VEML/SI1145 accesses
- [ ] Seesaw devices communicate via library (no custom protocol needed)

### Tests/Validation Before Proceeding

```bash
# Verify Wire.begin present
grep "Wire.begin" src/main.cpp && echo "I2C OK"

# Verify all 10 I2C addresses in code (includes DS2484)
grep -o "0x[0-9a-fA-F][0-9a-fA-F]" src/main.cpp | sort -u | wc -l  # Should be 10+

# Verify DS2484 1-Wire adapter initialization
grep "Adafruit_DS248x\|performDs2484Scan\|readDs2484Temperatures" src/main.cpp | wc -l

# Verify Serial initialization
grep "Serial.begin" src/main.cpp && echo "Serial OK"

# Verify TFT initialization
grep "tft.init\|tft.fillScreen" src/main.cpp && echo "Display OK"
```

### Hardware Testing

- [ ] Connect all 9 I2C sensors and verify detection messages on Serial monitor
- [ ] Connect DS18B20 devices and verify enumeration
- [ ] Connect TFT display and verify boot screen renders
- [ ] Connect gamepad/encoder and verify input reading
- [ ] I2C bus stable (no errors on 50 Hz polling)

### Dependencies on Other Phases
- **Blocking:** Phase 10 (integration tests verify hardware comms)
- **Blocked by:** Phase 1, 2, 3, 5

---

## Phase 10: Tests

### Objective
Implement automated test suite for all business logic, verify hardware integration, and establish regression prevention framework.

### Files/Components to Create

- `test/test_crc8.cpp` — CRC8 algorithm
- `test/test_coulomb.cpp` — Coulomb accumulation formula
- `test/test_navigation.cpp` — Page navigation modulo arithmetic
- `test/test_timing.cpp` — Timing gate rollover safety
- `test/test_led_color.cpp` — LED color cycling
- `test/test_uv_compensation.cpp` — UV compensation formulas
- `test/test_gamepad_edge.cpp` — Button falling-edge detection
- Plus integration tests (require hardware)

**Updates to existing files:**
- `platformio.ini` — Add test environment configuration

### Dependencies

**Required by Phase 10:**
- Phase 1 (project structure)
- Phase 3 (data model)
- Phase 5 (business logic algorithms)
- Phase 7 (UI renderering, if testing visual output)
- Phase 9 (hardware integration, if hardware tests)
- [10-testing.md](docs/10-testing.md) (complete test strategy and test cases)

**Enables Phase(s):**
- Phase 11 (build system includes tests)
- Phase 12 (deployment requires passing tests)

### Implementation Order

#### Group 1: Unit Test Infrastructure

1. **Configure PlatformIO for testing**
   - Add to `platformio.ini`:
     ```ini
     [env:esp32dev-test]
     platform = espressif32
     board = esp32dev
     framework = arduino
     test_framework = unity
     ```
   - Rationale: Unity is PlatformIO's default embedded test framework
   - Source: [10-testing.md](docs/10-testing.md), Phase 1: Unit Tests

2. **Create test boilerplate template**
   - Standard test structure:
     ```cpp
     #include <unity.h>
     #include "../src/main.cpp"  // Include firmware code
     
     void setUp(void) { /* init */ }
     void tearDown(void) { /* cleanup */ }
     void test_name() { TEST_ASSERT_EQUAL(...); }
     int main() { UNITY_BEGIN(); RUN_TEST(...); UNITY_END(); }
     ```
   - Source: [10-testing.md](docs/10-testing.md), Appendix

#### Group 2: Unit Tests (7 Test Files)

3. **Create `test/test_crc8.cpp`**
   - Test 1: Valid SCD30 18-byte response passes CRC
   - Test 2: Single bit flip in CRC byte detected
   - Test 3: All 6 CRC fields validated (all-or-nothing behavior)
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-005
   - Source: [10-testing.md](docs/10-testing.md), Unit Tests table

4. **Create `test/test_coulomb.cpp`**
   - Test 1: Accumulation formula exact match (100mA × 1s = 0.02778 mAh)
   - Test 2: Time gap > 200ms skipped
   - Test 3: First read doesn't accumulate
   - Test 4: IEEE 754 precision limits respected
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-004, BR-022

5. **Create `test/test_navigation.cpp`**
   - Test 1: Forward wrap (page 9 + 1 = 0)
   - Test 2: Backward wrap (page 0 - 1 = 9)
   - Test 3: Middle navigation (page 5 + 1 = 6)
   - Test 4: Falling edge detection (prev=released, curr=pressed triggers)
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-012

6. **Create `test/test_timing.cpp`**
   - Test 1: Timer rollover safety at 32-bit boundary
   - Test 2: Timing gate fires when deadline reached
   - Test 3: Timing gate blocks before deadline
   - Test 4: Signed comparison handles wrap-around correctly
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-021

7. **Create `test/test_led_color.cpp`**
   - Test 1: Color cycles 0→1→2→3→0 via modulo 4
   - Test 2: Brightness clamped to 0–255 range
   - Test 3: Delta × 8 formula applied
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-010, BR-011

8. **Create `test/test_uv_compensation.cpp`**
   - Test 1: Negative compensation clamped to 0
   - Test 2: Formula coefficients applied correctly (2.22×comp1, etc.)
   - Test 3: UV index calculation matches specification
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-006

9. **Create `test/test_gamepad_edge.cpp`**
   - Test 1: Falling edge detected (0→1 transition)
   - Test 2: Rising edge NOT detected
   - Test 3: Stable high state NOT detected as edge
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-019

#### Group 3: Integration Tests (Hardware Required)

10. **Create integration test environment**
    - Separate board config: `[env:esp32dev-hw]` in platformio.ini
    - Requires: ESP32 board connected to all 9 I2C sensors + DS18B20 + display
    - Run: `pio test -e esp32dev-hw`
    - Source: [10-testing.md](docs/10-testing.md), Phase 2: Integration Tests

11. **Create hardware test suite**
    - `test/test_i2c_enumeration_hw.cpp` — All sensors detected (or gracefully skip if missing)
    - `test/test_ds18b20_async_hw.cpp` — Async state machine completes, temperature reads
    - `test/test_page_rendering_hw.cpp` — All 9 pages render without crash
    - `test/test_encoder_latency_hw.cpp` — LED updates < 20 ms
    - `test/test_gamepad_latency_hw.cpp` — Page navigation < 25 ms
    - Source: [10-testing.md](docs/10-testing.md), Integration Tests table

#### Group 4: Test Configuration & Documentation

12. **Create `test/README.md` with test execution instructions**
    - How to run unit tests: `pio test -e esp32dev`
    - How to run hardware tests: `pio test -e esp32dev-hw`
    - Expected output examples
    - Troubleshooting guide
    - Source: [10-testing.md](docs/10-testing.md)

### Acceptance Criteria

- [ ] All 7 unit test files compile without errors
- [ ] All 7 unit tests PASS (100% pass rate)
- [ ] Unit tests execute in < 1 second total
- [ ] Integration tests compile (if hardware available)
- [ ] All integration tests PASS on hardware (5+ device types)
- [ ] No regressions detected (baseline established)
- [ ] Test execution documented in `test/README.md`
- [ ] Minimal Viable Testing checklist from [10-testing.md](docs/10-testing.md) passes

### Tests/Validation Before Proceeding

```bash
# Run unit tests (no hardware required)
pio test -e esp32dev 2>&1 | tail -10  # Should show "passed"

# Count test passes
pio test -e esp32dev 2>&1 | grep -c "PASS"

# Verify no test failures
pio test -e esp32dev 2>&1 | grep "FAIL" && echo "ERROR: Test failed" || echo "All tests passed"

# Check test coverage
grep -c "void test_" test/*.cpp  # Should be 7+

# Verify hardware tests present (if hardware config exists)
grep -c "env:esp32dev-hw" platformio.ini
```

### Dependencies on Other Phases
- **Blocking:** Phase 11 (build system must pass all tests)
- **Blocked by:** Phase 1, 3, 5, 7, 9

---

## Phase 11: Build

### Objective
Configure build system, verify compilation, create release artifacts, and establish reproducible build process.

### Files/Components to Create

**Updates to existing files:**
- `platformio.ini` — Add build flags, optimization settings, artifact configuration

**Build artifacts (auto-generated):**
- `.pio/build/freenove-fnk0114n-sensor-pages/firmware.elf` — ELF binary
- `.pio/build/freenove-fnk0114n-sensor-pages/firmware.bin` — Binary image for upload
- `.pio/build/freenove-fnk0114n-sensor-pages/firmware.map` — Symbol map for debugging

### Dependencies

**Required by Phase 11:**
- Phase 1 (project structure)
- Phase 2 (dependencies configured)
- Phase 3 (data model compiles)
- Phase 5 (business logic compiles)
- Phase 7 (UI compiles)
- Phase 9 (integrations compile)
- Phase 10 (all tests pass)
- [09-configuration.md](docs/09-configuration.md) (build flags)

**Enables Phase(s):**
- Phase 12 (deployment requires build artifacts)

### Implementation Order

1. **Verify build flags in `platformio.ini`**
   - `-Iinclude` (include search path)
   - `-D USER_SETUP_LOADED` (enable custom TFT setup)
   - `-include include/User_Setup.h` (force header inclusion)
   - Source: [09-configuration.md](docs/09-configuration.md), Build Flags

2. **Verify all compilation targets**
   - Primary: `pio run` — Build firmware
   - Test: `pio run -t upload` — Prepare for upload
   - Monitor: `pio run -t monitor` — Serial monitor
   - Clean: `pio run -t clean` — Remove build artifacts

3. **Set optimization level (optional)**
   - Default: `-O2` (balance speed/size)
   - Alternative: `-Os` (optimize for size, ~5% smaller binary)
   - Source: [09-configuration.md](docs/09-configuration.md)

4. **Verify no compiler warnings treated as errors**
   - Allow warnings (e.g., unused variables)
   - Rationale: Embedded code often uses incomplete Adafruit stubs
   - If desired: add `-Wall -Wextra -Werror` to treat warnings as errors

5. **Verify release configuration**
   - Binary size: Should be < 512 KB (ESP32 typical flash)
   - RAM at startup: Should use < 50 KB (320 KB available)
   - No errors: Exit code 0 on `pio run`

6. **Document build process**
   - Create `docs/BUILD.md` with:
     - Prerequisites (Python 3.6+, PlatformIO CLI)
     - Build steps: `pio run`
     - Expected output
     - Artifact locations
     - Troubleshooting (compiler errors, missing libraries)

### Acceptance Criteria

- [ ] `pio run` completes successfully with exit code 0
- [ ] Firmware binary created: `.pio/build/freenove-fnk0114n-sensor-pages/firmware.bin` exists
- [ ] Binary size < 512 KB (typically 400-500 KB)
- [ ] No compiler errors (warnings acceptable)
- [ ] All unit tests pass before/after build
- [ ] Build is reproducible (same source → same binary)
- [ ] Build artifacts ignored by git (in `.gitignore`)
- [ ] `docs/BUILD.md` documents complete process

### Tests/Validation Before Proceeding

```bash
# Clean and rebuild
pio run -t clean && pio run 2>&1 | tail -5

# Check exit code
pio run > /dev/null 2>&1 && echo "Build OK" || echo "Build FAILED"

# Verify firmware size
ls -lh .pio/build/freenove-fnk0114n-sensor-pages/firmware.bin | awk '{print $5}'  # Should be < 512K

# Run tests before deployment
pio test -e esp32dev 2>&1 | grep "passed"
```

### Clean Build Verification

```bash
# Simulate fresh clone
rm -rf .pio .git build
git init
git add .
pio run  # Should succeed
```

### Dependencies on Other Phases
- **Blocking:** Phase 12 (deployment requires build artifacts)
- **Blocked by:** Phase 1, 2, 3, 5, 7, 9, 10

---

## Phase 12: Deployment

### Objective
Verify firmware on real hardware, establish deployment procedures, create troubleshooting guide, and document operational procedures.

### Files/Components to Create

**Documentation only (no code changes):**
- Update `docs/BUILD.md` with deployment procedures
- Create `docs/DEPLOYMENT.md` with:
  - Hardware setup checklist
  - Serial port configuration (platform-specific)
  - Upload procedure
  - Boot verification
  - Troubleshooting guide
  - Safety guidelines
- Create `docs/OPERATIONS.md` with:
  - Operating instructions (page navigation, sensor readings)
  - Expected behavior
  - Error recovery
  - Maintenance procedures

### Dependencies

**Required by Phase 12:**
- Phase 11 (build artifacts available)
- Hardware: ESP32 board, all 9 I2C sensors, display, gamepad, encoder
- USB cable + drivers for ESP32 programming
- [09-configuration.md](docs/09-configuration.md) (serial port config)

**Enables Phase(s):**
- Post-deployment support and maintenance

### Implementation Order

1. **Prepare hardware**
   - Connect ESP32 board via USB to development machine
   - Verify all sensors connected (9 I2C, 8 DS18B20 optional, display)
   - Document connection diagram if not already provided

2. **Identify serial port**
   - Linux: `ls /dev/tty* | grep USB` → `/dev/ttyUSB0` (typical)
   - macOS: `ls /dev/cu.usbserial-*` → `/dev/cu.usbserial-AB0DJ4V3` (example)
   - Windows: Device Manager → Ports (COM) → `COM3` (typical)
   - Update `platformio.ini` with correct port

3. **Flash firmware to board**
   - Command: `pio run -t upload`
   - Expected: "Writing at 0x..." progress, then "Hash of data verified"
   - If fails: Check USB connection, verify port, try different cable

4. **Verify boot on serial monitor**
   - Command: `pio run -t monitor`
   - Expected output (example):
     ```
     [startup] Detecting sensors...
     [INFO] INA219 detected @ 0x40
     [INFO] BME280 detected @ 0x77
     [WARNING] Gamepad QT not found at 0x50
     [INFO] Boot complete. Starting main loop.
     ```
   - Exit: Ctrl+C

5. **Verify display rendering**
   - Expected: TFT shows page 0 (BME280) with sensor data
   - Test: Press Y/A buttons → page navigation
   - Test: Connect encoder → dial rotates, LED changes color/brightness
   - Test: Connect gamepad → joystick axes, button presses appear on page 2

6. **Verify all 9 pages render**
   - Navigate through pages 0–9 using Y/A buttons
   - Each page should display data or "Sensor nicht vorhanden" if missing
   - No crashes or blank screens

7. **Verify timing constraints**
   - INA219 updates every 20 µs (hard to observe; check loop runs without lag)
   - Gamepad/Encoder updates every 20 ms (observe responsive button/dial)
   - Sensor updates every 500 ms (observe data changes)
   - DS18B20 rescan every 3000 ms (hot-plug detection)

8. **Stress test (optional)**
   - Run for 10+ minutes continuously
   - Monitor for: crashes, stuck pages, data corruption, heat
   - Verify Coulomb counter accumulates correctly (INA219 page)
   - Verify no memory leaks (RAM usage stable)

9. **Create deployment checklist**
   - Hardware wiring verified
   - All sensors connected (optional ones can be absent)
   - Serial port configured correctly
   - Firmware uploaded successfully
   - Boot messages appear on Serial monitor
   - All 9 pages render correctly
   - Navigation (Y/A buttons) works
   - No crashes over 10-minute test run

10. **Create troubleshooting guide** (if deployment fails)
    - "Board not detected" → Check USB cable, drivers
    - "Upload failed" → Verify baud rate, try slower (115200 upload_speed)
    - "Garbage on serial monitor" → Wrong baud rate (should be 115200)
    - "Display blank" → Check SPI pins (GPIO 13/14/15/2/27)
    - "Sensors not detected" → Check I2C pins (GPIO 32/25), pull-ups
    - "Page navigation stuck" → Check gamepad connection, re-flash firmware
    - Source: [10-testing.md](docs/10-testing.md), Troubleshooting Installation

### Acceptance Criteria

- [ ] Firmware uploads to board without errors
- [ ] Serial monitor shows boot messages (no garbage)
- [ ] Display renders page 0 correctly (sensor data visible)
- [ ] All 9 pages navigate correctly (Y/A buttons work, pages 0–8)
- [ ] Navigation wraps correctly (page 0 Y → page 9, page 9 A → page 0)
- [ ] Sensor data updates at expected intervals (20ms live, 500ms slow)
- [ ] No crashes or resets over 10-minute continuous run
- [ ] Coulomb counter accumulates (INA219 page)
- [ ] LED responds to encoder (color cycle, brightness adjust)
- [ ] Gamepad input works (joystick/buttons on page 2)
- [ ] DS18B20 devices detected and temperatures displayed (if present)
- [ ] "Sensor nicht vorhanden" displays for missing optional sensors
- [ ] Deployment documentation complete

### Hardware Compatibility Matrix

| Component | Required | Status | Notes |
|-----------|----------|--------|-------|
| **ESP32 board** | ✅ Yes | Verified | esp32dev configuration |
| **USB cable** | ✅ Yes | [VERIFY] | Micro-USB or USB-C per board |
| **3.3V TFT display** | ✅ Yes | Verified | ST7796, 320×480, SPI |
| **INA219** | ✅ Optional* | Verified | Core feature; one page dedicated |
| **BME280** | ✅ Optional | Verified | Temperature/humidity/pressure |
| **DS18B20** | ✅ Optional | Verified | 1-Wire temperature; up to 8 devices |
| **Gamepad seesaw** | ✅ Optional | Verified | Joystick + 6 buttons |
| **Encoder seesaw** | ✅ Optional | Verified | Dial + button + NeoPixel LED |
| **VEML/SI1145/TSL2591** | ✅ Optional | Verified | Various light/UV sensors |
| **SCD30** | ✅ Optional | Verified | CO₂ sensor |

*Core feature: INA219 recommended (central to Coulomb counter functionality)

### Post-Deployment Operations

1. **Continuous Operation**
   - Device assumes 24/7 operation (no sleep modes)
   - Reboot via hardware reset or power cycle
   - Data lost on reboot (no persistence)

2. **Maintenance**
   - Firmware updates: Re-flash via USB + `pio run -t upload`
   - Sensor calibration: None (firmware uses sensor library defaults)
   - Display cleaning: Standard LCD care (no abrasive cleaners)

3. **Firmware Updates** (if needed)
   - Modify source code
   - Rebuild: `pio run`
   - Re-upload: `pio run -t upload`
   - No OTA (over-the-air) updates implemented

### Manual Testing Checklist (Final Verification)

Before declaring deployment complete:

- [ ] All 9 pages display correctly (no corruption)
- [ ] Page 0 (BME280): Temperature, humidity, pressure visible
- [ ] Page 1 (DS18B20): Device list and temperatures (if sensors present)
- [ ] Page 2 (Gamepad): Joystick X/Y and button states update live
- [ ] Page 3 (INA219): Voltage, current, power, Coulomb counter accumulate
- [ ] Page 4 (Encoder): Position, button press count, LED color/brightness control
- [ ] Pages 5–9 (other sensors): Display data if sensors present, or "Sensor nicht vorhanden"
- [ ] Y button navigation (prev): Each press goes back one page
- [ ] A button navigation (next): Each press goes forward one page
- [ ] Page wrap-around: Page 0 Y → 9, Page 9 A → 0
- [ ] Encoder button press: Press → LED color changes (cycles blue→green→red→purple)
- [ ] Encoder dial rotation: Rotate → LED brightness changes (0–255 visual)
- [ ] Gamepad joystick: Move stick → X/Y values change on page 2
- [ ] No crashes after 30-minute continuous operation
- [ ] Serial monitor output clean (no error messages unless sensors missing)
- [ ] Coulomb counter on INA219 page increases monotonically (if load applied)

### Expected Behavior on Missing Sensors

| Sensor | Page | Display | Navigation |
|--------|------|---------|-----------|
| BME280 | 0 | "Sensor nicht vorhanden" | Y/A still work |
| Gamepad | 2 | "Sensor nicht vorhanden" | Y/A still work |
| INA219 | 3 | "Sensor nicht vorhanden" | Y/A still work |
| (others) | 5–9 | "Sensor nicht vorhanden" | Y/A still work |

**Note:** If ALL sensors missing, firmware still operates (blank pages, but no crashes).

### Security Considerations (Post-Deployment)

- No credentials stored in firmware
- No network connectivity (offline by default)
- Serial monitor outputs debug info (assume trusted access)
- USB programming unrestricted (assume trusted environment)
- Physical tampering not detected (no secure boot)

### Performance Metrics (Baseline)

| Metric | Target | Typical | Notes |
|--------|--------|---------|-------|
| **Loop frequency** | ~1000 Hz+ | [TBD] | Main loop must handle all gates |
| **Page transition latency** | <25 ms | [TBD] | Navigation responsiveness |
| **LED update latency** | <20 ms | [TBD] | Encoder LED feedback |
| **Sensor read latency** | 500 ms | Fixed | Sensor polling interval |
| **INA219 sampling** | 20 µs | Fixed | High-precision Coulomb sampling |
| **Memory usage** | <50 KB RAM | ~300 bytes data + buffers | Monitor via ESP.getFreeHeap() |
| **Flash usage** | <512 KB | Typically 400–500 KB | Firmware binary size |

### Dependencies on Other Phases
- **Blocking:** None (final phase)
- **Blocked by:** Phase 11 (build must succeed first)

---

## Cross-Phase Dependency Graph

```
Phase 1 (Init)
    ↓
Phase 2 (Dependencies) ← → Phase 3 (Domain Model)
    ↓                        ↓
    ├→ Phase 5 (Business Logic)
    ├→ Phase 7 (UI)           
    └→ Phase 9 (Integrations)
       ↓
    Phase 10 (Tests)
       ↓
    Phase 11 (Build)
       ↓
    Phase 12 (Deployment)

Phase 4 (Persistence): N/A
Phase 6 (API): N/A
Phase 8 (Auth): N/A

All phases must complete in sequence: 1→2/3→5/7/9→10→11→12
```

## Blocked Items Summary

| Item | Phase | Resolution |
|------|-------|-----------|
| Freenove TFT_eSPI version pinning | 2 | Copy from Freenove GitHub or local mirror; version [UNKNOWN] |
| Dew point calculation formula | 7 (Page 0) | Consult meteorological standards or BME280 library docs |
| SCD30 command protocol details | 9 | Refer to SCD30 datasheet (Sensirion) |
| VEML sensor register addresses | 9 | Refer to VEML6075/VEML6070 datasheets |
| DS2484 I2C address conflicts | 4 | Run I2C address scanner to detect conflicts |
| I2C Wire library timeout values | 5, 9 | Check Arduino Wire library source or datasheet |
| Seesaw firmware version requirements | 2, 9 | Assume ≥3.x; consider runtime version check |

## Success Criteria (Overall)

Phase 12 deployment is complete when:

1. ✅ **Build succeeds** — `pio run` exit code 0
2. ✅ **Tests pass** — All unit + integration tests pass
3. ✅ **Firmware uploads** — No errors on `pio run -t upload`
4. ✅ **Hardware boots** — Serial monitor shows "Boot complete"
5. ✅ **Display renders** — All 9 pages show correctly
6. ✅ **Navigation works** — Y/A buttons navigate all pages, wrap correctly
7. ✅ **Sensors read** — Data displays (or "Sensor nicht vorhanden" if missing)
8. ✅ **Stability verified** — 30-minute continuous operation without crashes
9. ✅ **Documentation complete** — BUILD.md, DEPLOYMENT.md, OPERATIONS.md

---

## Appendix: File Structure After Phase 12

```
sensor-pages-dashboard/
├── .git/
├── .gitignore
├── .pio/
│   ├── build/
│   │   └── freenove-fnk0114n-sensor-pages/
│   │       ├── firmware.bin
│   │       ├── firmware.elf
│   │       └── firmware.map
│   └── libdeps/
│       └── freenove-fnk0114n-sensor-pages/
│           ├── Adafruit_INA219/
│           ├── Adafruit_TSL2591_Library/
│           ├── ... (7 libraries total)
│
├── .vscode/
│   └── (VS Code settings, if used)
│
├── docs/
│   ├── 01-requirements.md ✓
│   ├── 02-architecture.md ✓
│   ├── 04-data-model.md ✓
│   ├── 05-api.md ✓
│   ├── 06-business-rules.md ✓
│   ├── 07-ui.md ✓
│   ├── 09-configuration.md ✓
│   ├── 10-testing.md ✓
│   ├── 12-dependencies.md ✓
│   ├── 14-reconstruction-plan.md (this file)
│   ├── BUILD.md (created Phase 11)
│   ├── DEPLOYMENT.md (created Phase 12)
│   ├── OPERATIONS.md (created Phase 12)
│   └── README.md
│
├── include/
│   ├── SPIFFS.h (Phase 1)
│   └── User_Setup.h (Phase 1)
│
├── lib/
│   └── Freenove/
│       ├── TFT_eSPI/ (Phase 1, from Freenove fork)
│       ├── TFT_eSPI_Setups/ (FNK0114N setup)
│       └── TFT_Touch/ (optional)
│
├── src/
│   └── main.cpp (Phases 3-12)
│
├── test/
│   ├── README.md
│   ├── test_crc8.cpp (Phase 10)
│   ├── test_coulomb.cpp (Phase 10)
│   ├── test_navigation.cpp (Phase 10)
│   ├── test_timing.cpp (Phase 10)
│   ├── test_led_color.cpp (Phase 10)
│   ├── test_uv_compensation.cpp (Phase 10)
│   ├── test_gamepad_edge.cpp (Phase 10)
│   ├── test_*_hw.cpp (integration tests, Phase 10)
│   └── (13+ test files total)
│
├── platformio.ini (Phases 1, 2, 11)
├── README.md (Phase 1)
└── LICENSE (optional)
```

---

## Final Notes

This reconstruction plan defines a **deterministic, incremental approach** to recreate sensor-pages-dashboard from documentation alone. The plan:

1. **Separates concerns** into 12 distinct phases
2. **Identifies dependencies** between phases
3. **Specifies acceptance criteria** for each phase
4. **Provides testing validation** before proceeding
5. **Flags blocked items** requiring external information
6. **Documents all decisions** with source references

An AI coding agent (GitHub Copilot) can follow this plan incrementally, verifying completion at each phase before proceeding to the next. The resulting firmware should be **functionally identical** to the original application, with all 26 business rules, 10 UI pages, 9 I2C sensors, and embedded real-time constraints properly implemented.

**Estimated Total Effort:** 3–5 days of development + 1 day integration testing
**Estimated Compilation Time:** <1 minute per build
**Estimated Test Runtime:** <5 minutes (unit tests), 10–20 minutes (hardware tests)

