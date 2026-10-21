# Reconstruction Plan: sensor-pages-dashboard

**Purpose:** Deterministic guide for recreating the complete sensor-pages-dashboard embedded firmware application from documentation alone, without access to original source code.

**Assumptions:**
- Original `src/main.cpp` is NOT available
- Only documentation under `docs/` is available
- Target repository starts completely empty
- AI agent (GitHub Copilot) executes phases incrementally
- Hardware (ESP32 + sensors) available for testing
- PlatformIO and toolchain installed

**Last Updated:** 2026-09-25  
**Status:** ✅ **Complete** (Updated to reference FEATURE-SPEC-008 Central Parameter Definitions)

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
13. [Phase 13: Universal Large Display Mode](#phase-13-universal-large-display-mode-feature-spec-007)

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
   - Read encoder position every 20 ms (ALWAYS, not page-conditional — required for continuous LED updates and stats aggregation)
   - Read encoder position, compute delta
   - Detect button falling edge: (pressed && !lastPressed)
   - On falling edge: colorIndex = (colorIndex + 1) % 4, increment pressCount, call updateEncoderLed()
   - On dial rotation (delta != 0): brightness = constrain(brightness + delta*8, 0, 255), call updateEncoderLed()
   - Update encoderValues struct with position, delta, pressed, color name
   - Aggregate timing stats into EncoderStats struct (if ENABLE_STATS_REPORTING)
   - Source: [01-requirements.md](docs/01-requirements.md), FR-004 (Encoder Read Continuously)
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

6. **Implement `readBme280()`, `readVeml6075()`, `readVeml6070()`, `readSi1145()`, `readTsl2591()`, `readScd30()` functions (FEATURE-SPEC-005 Caching Architecture)**
   - Each reads from specific I2C sensor
   - Apply sensor-specific processing (UV compensation, CRC validation, dew point calculation, bitwise extraction, etc.)
   - Only update global struct if all reads succeed
   - Return false on I2C error (stale data persists)
   - Log timing to Serial (performance monitoring): `"SENSOR read time: XXX µs"`
   - **NEW (FEATURE-SPEC-005):** BME280, TSL2591, SI1145 now use centralized caching:
     - `readBme280()`: Compute Alduchov & Eskridge dew point (±0.35°C accuracy)
     - `readTsl2591()`: Extract channels via bitwise operations (lower 16 bits = CHAN0/fullSpectrum, upper 16 bits = CHAN1/infrared)
     - `readSi1145()`: Compute UV index as `raw / 100.0f`
   - Source: [04-data-model.md](docs/04-data-model.md), each sensor struct
   - Source: [05-api.md](docs/05-api.md), readBme280/readTsl2591/readSi1145 sections
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-SC-003 (plausibility checks), BR-SC-004 (timing measurement)
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
    - Encoder read: gate fires every 20 ms (ALWAYS, not page-conditional) → calls readEncoderAt50Hz()
    - INA219 read: gate fires every 20 µs → calls readIna219At50Hz() using micros()
    - Sensor read: gate fires every 500 ms → calls readSensors() wrapper (reads all non-Coulomb sensors)
    - DS18B20 update: called every loop cycle (self-gated via pending state)
    - DS18B20 rescan: gate fires every 3000 ms → calls rescanDs18b20()
    - Display rate limit: minimum 200 ms between consecutive page redraws (5 Hz maximum) via `nextLargeDisplayRefreshMs` gate
    - Page redraw: full (with header) when currentPage != lastPage; data-only when values change or timer fires, subject to 200 ms rate limit
    - Source: [06-business-rules.md](docs/06-business-rules.md), BR-021, BR-SC-011
    - Source: [09-configuration.md](docs/09-configuration.md), Timing Configuration table

14. **Implement `loop()` main event loop**
    - Initialize timing gate variables on first iteration (use nextXxx = 0 or = now)
    - Call updateDs18b20() every cycle (non-blocking, self-gated)
    - Check all timing gates in order (gamepad → encoder → INA219 → sensors → rescan → page redraw)
    - Update lastPage = currentPage at end of cycle
    - No delays; all operations cooperative scheduling via timing gates
    - Source: [02-architecture.md](docs/02-architecture.md), "Event Loop" section

#### Group 6: Sensor Data Wrapper (FEATURE-SPEC-005 Caching Architecture)

15. **Implement `readSensors()` wrapper function (FEATURE-SPEC-005 Caching)**
    - Call all sensor reads in order: readBme280, readTsl2591, readSi1145, readVeml6075, readVeml6070, readScd30
    - Called every 500 ms from loop timing gate (SENSOR_REFRESH_MS)
    - Updates global cache structs: bme280Values, tsl2591Values, si1145Values, veml6075Values, veml6070Values, scd30Values
    - INA219 and gamepad/encoder have separate high-frequency read cycles (50 Hz, 40 Hz respectively)
    - **Benefit (FEATURE-SPEC-005):** Centralizes I2C reads; page functions read from cache (no I2C during render)
    - **Performance:** Eliminates I2C latency during page rendering; improves display responsiveness
    - Source: [01-requirements.md](docs/01-requirements.md), FR-001
    - Source: [05-api.md](docs/05-api.md), readBme280/readTsl2591/readSi1145 sections
    - Source: [06-business-rules.md](docs/06-business-rules.md), BR-SC-004 (sensor caching pattern)

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

   **Page 0: `pageBme280()` (FEATURE-SPEC-005 Caching)**
   - Check available.bme280; if false, return unavailable(...)
   - **Data Retrieval (CACHED):** Read from global `bme280Values` struct (updated every 500ms by `readSensors()`)
   - Render: Temperature, Humidity, Pressure, Altitude, Dew point (from cache, no I2C calls)
   - Dew Point Formula: Alduchov & Eskridge (±0.35°C accuracy, valid 0-50°C / 1-100% RH):
     ```cpp
     const float a = 17.62, b = 243.12;
     alpha = ((a * T) / (b + T)) + log(RH% / 100)
     dewPoint = (b * alpha) / (a - alpha)
     ```
   - Colors: Temperature=GREEN, Pressure=GREEN, Altitude=CYAN, Dew point=CYAN
   - Benefit: Eliminates I2C latency during page rendering; improves responsiveness
   - Source: [07-ui.md](docs/07-ui.md), Page 0 BME280 (FEATURE-SPEC-005 section)
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-SC-003 (dew point formula)

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

   **Page 6: `pageSi1145()` (FEATURE-SPEC-005 Caching)**
   - Check available.si1145; if false, return unavailable(...)
   - **Data Retrieval (CACHED):** Read from global `si1145Values` struct (updated every 500ms by `readSensors()`)
   - Render: Visible spectrum (ADC count), Infrared spectrum (ADC count), UV raw (index × 100), UV index (computed)
   - UV Index Calculation: `uvIndex = uv_raw / 100.0f` (Adafruit standard formula)
   - Colors: Visible=GREEN, IR=GREEN, UV raw=ORANGE, UV index=CYAN
   - Benefit: Eliminates I2C latency during page rendering; improves responsiveness
   - Source: [07-ui.md](docs/07-ui.md), Page 5 SI1145 (FEATURE-SPEC-005 section)

   **Page 7: `pageTsl2591()` (FEATURE-SPEC-005 Caching)**
   - Check available.tsl2591; if false, return unavailable(...)
   - **Data Retrieval (CACHED):** Read from global `tsl2591Values` struct (updated every 500ms by `readSensors()`)
   - Render: Full spectrum (ADC count), Infrared (ADC count), Visible (computed: full - IR), Lux (computed)
   - Bitwise Extraction (in `readTsl2591()`): `getFullLuminosity()` returns uint32_t encoded as `[CHAN1(16-31) | CHAN0(0-15)]`
     - fullSpectrum = lower 16 bits (CHAN0, visible + infrared)
     - infrared = upper 16 bits (CHAN1, infrared only)
   - Visible Calculation: `visible = max(0, fullSpectrum - infrared)`
   - Lux Calculation: `lux = calculateLux(fullSpectrum, infrared)` (Adafruit library algorithm)
   - Colors: Lux=CYAN (computed value)
   - Benefit: Eliminates I2C latency during page rendering; improves responsiveness
   - Source: [07-ui.md](docs/07-ui.md), Page 6 TSL2591 (FEATURE-SPEC-005 section)

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

#### Group 4: Large Display Mode for INA219 (Page 2) — **NEW**

**Purpose:** Implement alternate display mode for INA219 showing individual sensor values in enlarged, centered format.

**Scope:** INA219 Page 2 only; designed for future extension to other pages.

**Implementation Steps:**

1. **Define global state variables (at file scope)**
   ```cpp
   bool largeDisplayMode = false;        // Mode active flag
   uint8_t largeDisplayIndex = 0;        // Current value index (0–6)
   ```
   - Initialize at declaration (before setup())
   - Reset to false/0 on power-up

2. **Implement helper functions (display-agnostic, reusable)**

   - **`const char* getLargeDisplayLabel(uint8_t valueIndex)`**
     - Returns German label for index 0–6 (Laufzeit, Batterie, Versorgung, Strom, Ladung, Shunt-Spannung, Power)
     - Returns "???" for invalid index
     - Source: [05-api.md](docs/05-api.md), Section 0
     - Source: [04-data-model.md](docs/04-data-model.md), Section 2.9

   - **`String getLargeDisplayValue(uint8_t valueIndex)`**
     - Reads from global `ina219Values` struct (no caching)
     - Formats with per-value decimal places (see table in [05-api.md](docs/05-api.md))
     - Returns formatted String object
     - Index 0: Runtime formatted as HH:MM:SS (special case, no decimals)
     - Index 1–6: Variable decimals based on value type (1–3 decimals; see [05-api.md](docs/05-api.md) for mapping)
     - Source: [05-api.md](docs/05-api.md), Section 3.2 (value formatting table)
     - Source: [04-data-model.md](docs/04-data-model.md), Section 2.12

   - **`const char* getLargeDisplayUnit(uint8_t valueIndex)`**
     - Returns unit label: s, V, mV, mA, mAh, V, mW
     - Returns empty string "" for out-of-range
     - Source: [05-api.md](docs/05-api.md), Section 0

   - **`uint16_t getLargeDisplayColor(uint8_t valueIndex)`**
     - Returns TFT_eSPI color constant (Cyan, Green, or Orange)
     - Match colors from standard `pageIna219()` display
     - Cyan: indices 0, 4 (computed values)
     - Green: indices 1, 2, 5 (primary measurements)
     - Orange: indices 3, 6 (secondary measurements)
     - Source: [05-api.md](docs/05-api.md), Section 0

3. **Implement main render function**

   - **`void pageLargeDisplayIna219(uint8_t valueIndex)`**
     - Called conditionally from `pageIna219()` when `largeDisplayMode == true`
     - Displays single value in large, centered format
     - Atomic screen clear: `fillScreen(TFT_BLACK)` at start
     - All text centered at x=240 (midpoint of 480 px width) using MC_DATUM alignment
     - Layout (all text centered):
       - Header "INA219 Large Display" (y=18, Font 4, Cyan)
       - Separator line (y=48, DarkGrey)
       - Label (y=70, Font 4, Cyan) — value name (e.g., "Batterie")
       - Value (y=160, Font 8, Color-coded) — focal point of display
       - Unit (y=240, Font 4, White) — measurement unit
       - Footer "B:next X:prev Y:exit" (y=295, Font 2, DarkGrey) — navigation hint
     - Calls helper functions: getLargeDisplayLabel(), getLargeDisplayValue(), getLargeDisplayUnit(), getLargeDisplayColor()
     - Resets text datum to TL_DATUM after rendering
     - Source: [05-api.md](docs/05-api.md), Section 3.1 (detailed layout table)
     - Source: [07-ui.md](docs/07-ui.md), Section 7 (Large Display Mode)

4. **Modify `pageIna219()` to support large display mode**
   ```cpp
   void pageIna219() {
     if (!available.ina219) return unavailable("INA219", "0x40");
     
     if (largeDisplayMode) {
       pageLargeDisplayIna219(largeDisplayIndex);  // Alternate layout
     } else {
       // ... standard page layout (existing implementation)
     }
   }
   ```
   - Conditional rendering based on `largeDisplayMode` flag
   - Source: [04-data-model.md](docs/04-data-model.md), Section 2.9

5. **Implement button handling in `loop()` for large display mode**
   - **Button B in large display mode:**
     - Cycle forward: `largeDisplayIndex = (largeDisplayIndex + 1) % 7`
   - **Button X in large display mode:**
     - Cycle backward: `largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7`
   - **Button Y in large display mode:**
     - Exit mode: `largeDisplayMode = false; largeDisplayIndex = 0;`
   - **Button A in large display mode:**
     - Inactive (no action)
   - **Page navigation (Y/A for prev/next page):**
     - **DISABLED** while `largeDisplayMode == true`
     - Only re-enabled after mode exit via Button Y
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-028 through BR-033

6. **Update gamepad button logic structure**
   ```cpp
   // In loop(), after reading gamepad:
   if (gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_B)) {
     if (!lastB) {  // Falling edge
       if (currentPage == 2 && !largeDisplayMode) {
         // Page 2 standard view: enter large display mode
         largeDisplayMode = true;
         largeDisplayIndex = 0;
       } else if (currentPage == 2 && largeDisplayMode) {
         // Page 2 large display view: cycle forward
         largeDisplayIndex = (largeDisplayIndex + 1) % 7;
       }
       // ... other buttons B behavior
     }
   }
   
   if (gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_X)) {
     if (!lastX) {  // Falling edge
       if (currentPage == 2 && largeDisplayMode) {
         // Page 2 large display view: cycle backward
         largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7;
       }
       // ... other buttons X behavior
     }
   }
   
   if (gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_Y)) {
     if (!lastY) {  // Falling edge
       if (currentPage == 2 && largeDisplayMode) {
         // Page 2 large display view: exit mode
         largeDisplayMode = false;
         largeDisplayIndex = 0;
       } else if (!largeDisplayMode) {
         // Normal page navigation
         currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;
       }
     }
   }
   
   if (gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_A)) {
     if (!lastA) {  // Falling edge
       if (currentPage == 2 && largeDisplayMode) {
         // Page 2 large display view: Button A inactive (no action)
       } else if (!largeDisplayMode) {
         // Normal page navigation
         currentPage = (currentPage + 1) % PAGE_COUNT;
       }
     }
   }
   ```
   - Source: [06-business-rules.md](docs/06-business-rules.md), BR-028 through BR-033

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
- [ ] **Large display mode:** Page 2 (INA219) enters large display on Button B
- [ ] **Large display mode:** All 7 values display with correct formatting and colors
- [ ] **Large display mode:** Button B cycles forward, Button X cycles backward, wrap at boundaries
- [ ] **Large display mode:** Button Y exits mode and returns to standard view
- [ ] **Large display mode:** Button A is inactive (no effect)
- [ ] **Large display mode:** Page navigation (Y/A) is disabled during mode
- [ ] **Large display mode:** Values update in real-time from `ina219Values` struct
- [ ] **Large display mode:** Footer shows "B:next X:prev Y:exit" (context-specific help)

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

## Addendum: Phase 6 Enhancement (FEATURE-SPEC-006) — Non-Blocking Rotated Sensor Architecture

### Objective

After completing Phases 1–12 (basic implementation), Phase 6 enhances the business logic and UI to implement non-blocking rotated sensor polling. This eliminates blocking I2C reads and guarantees < 200 ms input latency.

**Prerequisites:** Phase 5 (Business Logic) and Phase 7 (Frontend/UI) fully complete with FEATURE-SPEC-005 (sensor caching).

### Modifications to Phase 5: Business Logic

**Refactor `readSensors()` function:**
```cpp
// OLD (FEATURE-SPEC-005): All sensors read together every 500 ms
void readSensors() {
  readBme280(bme280Values);
  readTsl2591(tsl2591Values);
  readSi1145(si1145Values);
  // ... (blocking, up to 250+ ms total)
}

// NEW (FEATURE-SPEC-006): Rotated read every 200 ms, phase-based, 10-slot cycle
void rotatedReadSensor(uint32_t slot) {
  switch (slot) {
    // Phase 1: START ASYNC (Slots 0-1)
    case 0: startTsl2591Measurement(); break;          // TSL2591 integration starts
    case 1: startDs18b20Conversion(); break;            // DS18B20 conversion starts
    
    // Phase 2: READ SYNC (Slots 2-7)
    case 2: readBme280(bme280Values); break;
    case 3: readSi1145(si1145Values); break;
    case 4: readVeml6075(veml6075Values); break;
    case 5: readVeml6070(veml6070Values); break;
    case 6: readScd30(scd30Values); break;
    case 7: performDs2484Scan(); break;                 // 1-Wire bus scan (60s gate)
    
    // Phase 3: CHECK ASYNC (Slots 8-9)
    case 8: checkTsl2591Async(); break;                 // Read TSL2591 result (1600ms after start)
    case 9: checkDs18b20Async(); break;                 // Read DS18B20 result (1600ms after start)
  }
}
```

**New functions to implement:**
1. `void rotatedReadSensor(uint32_t slot)` - Dispatcher (lines 1, ~100 lines)
2. `void startTsl2591Measurement()` - Async start (~20 lines)
3. `void checkTsl2591Async()` - Async complete check (~15 lines)
4. `void startDs18b20Conversion()` - 1-Wire async start (~25 lines)
5. `void checkDs18b20Async()` - 1-Wire async complete check (~20 lines)
6. `void updateInputStats()` - Stats aggregation (~50 lines, only if `ENABLE_STATS_REPORTING`)

**Main loop changes:**
```cpp
// OLD:
if ((int32_t)(now - nextSensorMs) >= 0) {
  readSensors();  // All sensors in one call
  nextSensorMs = now + 500;
}

// NEW (10-slot, time-based modulo, startup-safe):
uint32_t sensorSlot = (millis() / SENSOR_SLOT_INTERVAL_MS) % SLOT_COUNT;
// With SENSOR_SLOT_INTERVAL_MS=200, SLOT_COUNT=10: cycle time = 2000ms

static uint32_t lastSensorSlot = 0xFFFFFFFF;  // Initialize to invalid
if (sensorSlot != lastSensorSlot) {
  rotatedReadSensor(sensorSlot);
  lastSensorSlot = sensorSlot;
}

// Stats reporting (every 1 second):
if ((int32_t)(now - nextStatsMs) >= 0) {
  updateInputStats();
  nextStatsMs = now + INPUT_STATS_INTERVAL_MS;
}
```

**New state machines:**
```cpp
// TSL2591 async state machine
enum class Tsl2591State { IDLE = 0, MEASURING = 1 };
static Tsl2591State tsl2591State = Tsl2591State::IDLE;
static uint32_t tsl2591MeasureStartMs = 0;

// DS18B20 async state machine
enum class Ds18b20State { IDLE = 0, CONVERTING = 1 };
static Ds18b20State ds18b20State = Ds18b20State::IDLE;
static uint32_t ds18b20ConvertStartMs = 0;
```

**New constants (in main.cpp):**
```cpp
// Timing constraints (IMMUTABLE)
#define ENABLE_STATS_REPORTING 1                       // 1=enabled (default), 0=disabled
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;     // 200ms per slot (immutable)
constexpr uint32_t SLOT_COUNT = 10;                   // 10 slots per cycle (immutable)
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;    // 1 second (immutable)

// Sensor-specific timing (ADJUSTABLE)
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;    // Default 200ms (can adjust)
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;     // Default 750ms at 12-bit (can adjust)
```

**Testing requirements:**
- TC-Rotated-001: Verify 10-slot rotation schedule (2000ms cycle, not 1400ms)
- TC-Rotated-002: Verify TSL2591 async pattern (Slot 0 START → Slot 8 CHECK, 1600ms apart)
- TC-Rotated-003: Verify DS18B20 async pattern (Slot 1 START → Slot 9 CHECK, 1600ms apart)
- TC-Rotated-004: Verify display updates only on value change
- TC-Rotated-005: Verify input latency < 100 ms (worst case)
- Serial output must show slot transitions, phase organization, and async event timing

### Modifications to Phase 7: Frontend/UI

**Update `drawCurrentPage()` to only redraw on value change:**
```cpp
// OLD (FEATURE-SPEC-005): Always redraw every cycle
void drawCurrentPage(bool pageChanged) {
  if (pageChanged) drawHeader(...);
  // Render page content (data always rendered)
}

// NEW (FEATURE-SPEC-006): Conditional redraw
static SensorCache lastSensorCache;  // Previous values

bool valuesChanged() {
  // Compare all relevant fields for current page
  // Return true if ANY value differs from lastSensorCache
  return (/* comparison logic */);
}

bool timerEventFired() {
  // Check if INA219 seconds timer incremented
  static uint32_t lastTimer = 0;
  if (ina219Values.timerSeconds != lastTimer) {
    lastTimer = ina219Values.timerSeconds;
    return true;
  }
  return false;
}

// In main loop:
if (currentPage != lastPage) {
  drawCurrentPage(true);   // Full redraw
} else if (valuesChanged()) {
  drawCurrentPage(false);  // Partial redraw
} else if (timerEventFired()) {
  drawCurrentPage(false);  // Partial redraw
}
// Otherwise: no drawCurrentPage() call (no redraw)

// Update cache after any redraw:
lastSensorCache = getCurrentSensorCache();
lastPage = currentPage;
```

**Performance benefit:**
- Display updates only when necessary (eliminates flicker)
- Reduced SPI bandwidth (fewer display writes per second)
- Lower CPU load (skip rendering if unchanged)
- Potential power savings (display idle more often)

**Testing requirements:**
- TC-Rotated-004: Verify display updates only on value change
- TC-Rotated-005: Verify input latency < 200 ms (worst case)
- Visual inspection: Page should not flicker with stable sensor values

### Integration Checklist

After completing Phase 6 modifications:

1. **Code Review Verification:**
   - [ ] `rotatedReadSensor()` dispatches all 10 slots correctly
   - [ ] Slot 0-1: async START phase working
   - [ ] Slot 2-7: sync READ phase working
   - [ ] Slot 8-9: async CHECK phase working
   - [ ] `startTsl2591Measurement()` and `checkTsl2591Async()` implement two-phase pattern
   - [ ] `startDs18b20Conversion()` and `checkDs18b20Async()` implement two-phase pattern with three-strike lockout
   - [ ] State machines transition correctly (IDLE → MEASURING/CONVERTING → IDLE)
   - [ ] Main loop uses time-based modulo (not state counting): `(millis() / 200) % 10`
   - [ ] Display refresh logic gates on value changes, not time

2. **Compilation:**
   - [ ] Clean build: `pio run -t clean && pio run`
   - [ ] No compiler warnings or errors
   - [ ] Verify 10 slots used, not 7: Check rotatedReadSensor() has cases 0-9

3. **Hardware Testing (10–15 minutes):**
   - [ ] TC-Rotated-001: Sensor rotation scheduling (10-slot, 2000ms cycle)
   - [ ] TC-Rotated-002: TSL2591 async measurement (Slot 0 → Slot 8, 1600ms)
   - [ ] TC-Rotated-003: DS18B20 async conversion (Slot 1 → Slot 9, 1600ms)
   - [ ] TC-Rotated-004: Display updates only on value change
   - [ ] TC-Rotated-005: Input latency < 100 ms guaranteed
   - [ ] Verify three-strike lockout: Disconnect DS18B20, verify "Disconnected" after 3 errors
   - [ ] Visual inspection: Display responsive, no flicker, pages transition smoothly

4. **Regression Testing:**
   - [ ] All 10 page renderers still work
   - [ ] Page navigation (Y/A buttons) still works
   - [ ] Gamepad page (page 1) responsive
   - [ ] Encoder page (page 4) responsive
   - [ ] DS2484 page (page 9) displays sensors correctly
   - [ ] Large display mode (INA219 page 2) still works
   - [ ] Error pages (missing sensors) still display correctly

5. **Performance Validation:**
   - [ ] Max blocked time per slot < 100 ms (measured via Serial output)
   - [ ] Input response latency < 100 ms (button press to page change)
   - [ ] Sensor freshness = 2000 ms per full cycle (consistent)
   - [ ] Display refresh rate on value change (event-driven, not constant)
   - [ ] Verify slot timing at every 200ms interval ±10ms

6. **Documentation Updates:**
   - [ ] README.md: Note FEATURE-SPEC-006 is implemented (10-slot)
   - [ ] 01-requirements.md: FR-014 references 10-slot, 2000ms design
   - [ ] 02-architecture.md: Section 15.1 describes 10-slot phases and timing
   - [ ] 05-api.md: Document rotatedReadSensor() and async functions
   - [ ] 06-business-rules.md: BR-SC-005 through BR-SC-010 use 10-slot design
   - [ ] 09-configuration.md: SLOT_COUNT=10, cycle 2000ms documented
   - [ ] 10-testing.md: TC-Rotated tests updated for 10-slot, correct slot timing
   - [ ] FEATURE-SPEC-006.md: Updated to 10-slot design (completed in this session)

### Estimated Effort

- **Code Implementation:** 5–7 hours
  - Refactor `readSensors()` into 10-slot rotation: 1.5 hours
  - Implement async state machines (TSL2591, DS18B20) with margins: 2 hours
  - Update display logic (value change detection): 1 hour
  - Stats reporting (configurable): 1 hour
  - Three-strike lockout for DS18B20: 0.5 hour

- **Testing:** 2–3 hours
  - Unit tests (timing gates, state machines, slot calculation): 1 hour
  - Hardware integration tests (all 6 test cases): 1.5 hours
  - Regression testing (existing features): 0.5 hour

- **Documentation:** 1–2 hours
  - Update all 7 documentation files: 1 hour
  - Rebuild consistency audit: 0.5–1 hour

- **Documentation:** 1 hour
  - Update existing docs with FEATURE-SPEC-006 details (already completed)

**Total Effort:** ~7–10 hours (1 day of focused development + testing)

## Phase 13: Universal Large Display Mode (FEATURE-SPEC-007) (FEATURE-SPEC-007) — **NEW**

### Objective
Extend large display mode from INA219 (Page 2 only) to all 10 sensor pages (Pages 0–9). Implement unified rendering architecture that supports all pages with consistent UI/UX.

### Dependencies
- ✅ Phase 7 completed (page renderers exist)
- ✅ Phase 10 completed (test infrastructure ready)
- ✅ FEATURE-SPEC-006 completed (display rate limiting in place)
- ✅ Documentation complete (FR-016-019, BR-SP-001-010, test cases defined)

### Implementation Sequence

#### Sub-Phase 13.1: Core Infrastructure (2–3 hours)

**Objective:** Implement unified `renderLargeDisplay()` function and global state management.

**Tasks:**
1. Define global state struct in main.cpp:
   ```cpp
   struct {
     bool active;
     uint8_t currentPage;
     uint8_t valueIndex;
   } largeDisplayMode = {false, 0, 0};
   ```

2. Implement unified renderer function:
   - `void renderLargeDisplay(uint8_t pageIndex, uint8_t valueIndex)`
   - Draw layout: title (Font 4, Cyan), label (Font 4, Cyan), value (Font 8, color-coded), unit (Font 2, White), footer (Font 1, Grey)
   - Call sensor-specific helper to retrieve label/value/unit/color
   - Respect display rate limiting (200 ms minimum)

3. Modify all 10 page renderers to support conditional rendering:
   ```cpp
   void pageXxx() {
     if (largeDisplayMode.active && largeDisplayMode.currentPage == pageIndex) {
       renderLargeDisplay(pageIndex, largeDisplayMode.valueIndex);
     } else {
       pageXxxStandard();  // Existing standard renderer
     }
   }
   ```

4. Update button handlers:
   - Button B: Mode entry/forward cycling (context-aware)
   - Button X: Backward cycling (context-aware)
   - Button Y: Mode exit (context-aware)
   - Button A: Inactive in large display mode

5. Add main loop check for page navigation:
   - Auto-exit large display mode if user navigates away via Y/A

**Acceptance Criteria:**
- [ ] Unified renderer function compiles without errors
- [ ] Global state initialized correctly
- [ ] All 10 page renderers support conditional rendering
- [ ] Button handlers route correctly based on mode
- [ ] No compilation warnings or errors
- [ ] Firmware compiles and loads to ESP32 (FEATURE-SPEC-006 code should still work)

**Testing:**
- Compile check
- Load to device; boot to Page 0
- Manually verify each button's behavior in standard and large display modes

---

#### Sub-Phase 13.2: INA219 Large Display Refactoring (1 hour)

**Objective:** Refactor existing INA219 large display mode (FR-012, working since FEATURE-SPEC-006) to use unified renderer and single combined helper function.

**Current State:**
- INA219 large display mode exists and works (Page 2, Button B entry)
- Has its own renderer function: `pageLargeDisplayIna219()`
- Has individual value retrieval logic (4 separate functions or inline code)

**Tasks:**
1. Create single combined helper function:
   ```cpp
   String ina219_getLargeDisplayData(uint8_t valueIndex,
                                     const char** outLabel,
                                     const char** outUnit,
                                     uint16_t* outColor);
   ```

2. Implement logic for all 7 values:
   - Index 0: Runtime (HH:MM:SS format, Green)
   - Index 1: Voltage (V format, Green)
   - Index 2: Supply (V format, Green)
   - Index 3: Current (A format, Orange)
   - Index 4: Charge (mAh format, Cyan)
   - Index 5: Shunt voltage (mV format, Orange)
   - Index 6: Power (W format, Green)

3. Integrate with unified renderer:
   - Remove old `pageLargeDisplayIna219()` function
   - Update `pageIna219()` to use conditional rendering pattern

4. Verify backward compatibility:
   - Button B entry should still work
   - All 7 values should cycle correctly
   - Display should remain flicker-free (rate limit still active)

**Acceptance Criteria:**
- [ ] INA219 large display mode still works (backward compatible)
- [ ] All 7 values cycle correctly (B/X buttons)
- [ ] Exit via Y returns to standard page
- [ ] No display flicker
- [ ] No crashes or anomalies

**Testing:**
- TC-LDM-001: Entry on Page 2
- TC-LDM-002: Forward cycling (all 7 values)
- TC-LDM-003: Backward cycling (wrap-around)
- TC-LDM-004: Exit via Y
- Regression: Standard INA219 page still displays correctly

---

#### Sub-Phase 13.3: High-Priority Pages (2–3 hours)

**Objective:** Implement large display mode for Pages 0 (BME280) and 4 (SCD30).

**Page 0 (BME280):**
- 7 values: Temperature, Pressure, Humidity, Dew point, Altitude, AQI, Temp trend
- Colors: Green (primary), Orange (secondary), Cyan (derived)
- Units: °C, hPa, %, °C, m, index, °C/min

**Page 4 (SCD30):**
- 3 values: CO₂, Temperature, Humidity
- Colors: Green (primary), Green (temp), Orange (humidity)
- Units: ppm, °C, %

**Tasks:**
1. Implement combined helper functions:
   ```cpp
   String bme280_getLargeDisplayData(uint8_t valueIndex, ...);
   String scd30_getLargeDisplayData(uint8_t valueIndex, ...);
   ```

2. Update page renderers (Pages 0 and 4) to use unified rendering

3. Update page renderer entries in `renderLargeDisplay()` switch statement:
   ```cpp
   case 0:  // BME280
     value = bme280_getLargeDisplayData(valueIndex, &label, &unit, &color);
     break;
   case 4:  // SCD30
     value = scd30_getLargeDisplayData(valueIndex, &label, &unit, &color);
     break;
   // ... etc
   ```

**Acceptance Criteria:**
- [ ] Pages 0 and 4 both support large display mode
- [ ] All values (7 for BME280, 3 for SCD30) cycle correctly
- [ ] Colors and units correct for each value
- [ ] Display readable from 5+ meters
- [ ] No crashes or flicker

**Testing:**
- TC-LDM-001: Entry on Pages 0 and 4
- TC-LDM-002/003: Cycling forward/backward
- TC-LDM-004: Exit via Y
- Verify standard pages still work

---

#### Sub-Phase 13.4: Remaining Pages (2–3 hours)

**Objective:** Implement large display mode for Pages 1, 3, 5, 6, 7, 8.

**Value Sets:**
- Page 1 (Gamepad): 4 values
- Page 3 (Encoder): 5 values
- Page 5 (SI1145): 4 values
- Page 6 (TSL2591): 4 values
- Page 7 (VEML6070): 3 values
- Page 8 (VEML6075): 6 values

**Tasks:**
1. Implement combined helper functions for all 6 pages (follow BME280/SCD30 pattern)

2. Register all 6 pages in `renderLargeDisplay()` switch statement

3. Update page renderers (1, 3, 5, 6, 7, 8) to use unified rendering

**Acceptance Criteria:**
- [ ] All 6 pages support large display mode
- [ ] All values cycle correctly per page
- [ ] Colors and units match specification
- [ ] No crashes across all page transitions

**Testing:**
- TC-LDM-001: Entry on each of 6 pages
- TC-LDM-002/003: Cycling verification
- TC-LDM-008: Full 10-page sweep

---

#### Sub-Phase 13.5: DS2484 1-Wire Support (1–2 hours, Optional)

**Objective:** Implement large display mode for Page 9 (DS2484) with dynamic sensor count (max 3).

**Challenge:** Value count not fixed; depends on discovered 1-Wire sensors.

**Tasks:**
1. Implement helper function with dynamic value count:
   ```cpp
   String ds2484_getLargeDisplayData(uint8_t valueIndex, ...);
   // Must handle 0–9 values based on discovered sensor count
   ```

2. Get dynamic value count:
   ```cpp
   uint8_t getPageValueCount(uint8_t pageIndex) {
     if (pageIndex == 9) {
       return 3 * ds2484SensorCount;  // 0, 3, 6, or 9
     }
     // ... other pages have fixed counts
   }
   ```

3. Handle edge cases:
   - No sensors discovered: Error screen (large display cannot enter)
   - 1 sensor: 3 values
   - 2 sensors: 6 values
   - 3 sensors: 9 values
   - Wrap-around at current sensor count

**Acceptance Criteria:**
- [ ] Page 9 supports large display if sensors discovered
- [ ] Dynamic value count correct (3 × sensorCount)
- [ ] All values cycle correctly
- [ ] Wrap-around works at boundary
- [ ] Error handling (no sensors) correct

**Testing:**
- TC-LDM-010: 1, 2, 3 sensors discovered
- Cycle through all values
- Wrap-around verification

---

### Files to Modify

**src/main.cpp:**
- Add global `largeDisplayMode` struct (~3 lines)
- Add `renderLargeDisplay()` function (~50–80 lines)
- Add per-page helper functions (~10–15 lines each, 10 pages = 100–150 lines total)
- Modify all 10 page renderers (~2–3 lines each, 10 pages = 20–30 lines)
- Modify button handlers (~20–30 lines)
- Add `getPageValueCount()` helper (~20 lines)
- **Total additions:** ~200–300 lines of new code

**docs/01-requirements.md:**
- Add FR-016, FR-017, FR-018, FR-019 (already done)

**docs/06-business-rules.md:**
- Add BR-SP-001 through BR-SP-010 (already done)

**docs/05-api.md:**
- Add large display rendering API section (already done)

**docs/07-ui.md:**
- Add large display mode specifications for all 10 pages (already done)

**docs/10-testing.md:**
- Add test cases TC-LDM-001 through TC-LDM-010 (already done)

### Estimated Effort

**Development:**
- Sub-Phase 13.1 (Core): 2–3 hours
- Sub-Phase 13.2 (INA219 refactor): 1 hour
- Sub-Phase 13.3 (BME280, SCD30): 2–3 hours
- Sub-Phase 13.4 (Remaining 6 pages): 2–3 hours
- Sub-Phase 13.5 (DS2484 optional): 1–2 hours
- **Total:** 8–12 hours (~1 day of development)

**Testing:**
- Manual integration tests (TC-LDM-001–010): 2–3 hours
- Regression testing (all 10 pages standard mode): 1 hour
- **Total:** 3–4 hours

**Documentation:**
- Already complete (FR-016-019, BR-SP-001-010, API docs, test cases)
- **Total:** 0 hours

**Overall Effort:** 11–16 hours (~1.5 days)

### Success Criteria

- [ ] All 10 pages support large display mode via Button B
- [ ] Value cycling works correctly (B=forward, X=backward, Y=exit)
- [ ] Display readable from 5+ meters away (Font 8 centered layout)
- [ ] Display rate limiting still active (no flicker)
- [ ] Existing INA219 mode backward compatible
- [ ] DS2484 supports 0–3 sensors with dynamic value count
- [ ] All 10 pages render correctly in standard and large display modes
- [ ] No crashes or memory leaks
- [ ] Passes TC-LDM-001 through TC-LDM-010

---

## Phase 14: Post-Implementation Display Optimizations (FEATURE-SPEC-006 & FEATURE-SPEC-007)

**Status:** ✅ **COMPLETED** in v2.7+

This phase documents display rendering optimizations implemented after Large Display Mode to eliminate flicker and improve responsiveness.

### Optimization 14.1: Incremental Rendering (needsFullRedraw Flag)

**Problem:** `renderLargeDisplay()` was calling `tft.fillScreen()` every frame (~50ms refresh), causing visible flicker (~5 redraws/sec on TFT I2C interface).

**Solution:** Introduced incremental rendering:
```cpp
bool doFullRedraw = largeDisplayMode.needsFullRedraw || 
                    value != largeDisplayMode.lastDisplayedValue;

if (doFullRedraw) {
    tft.fillScreen(TFT_BLACK);        // Full clear (~450ms)
    largeDisplayMode.lastDisplayedValue = value;
    // Draw title, label, unit, footer
} else {
    tft.fillRect(30, 130, 420, 60, TFT_BLACK);  // Partial clear (~50ms)
}

tft.drawString(value, 240, 160, 8);   // Always update value
largeDisplayMode.needsFullRedraw = false;
```

**Behavior:**
- Full redraw: Mode entry, value change, or button press (B/X)
- Partial redraw: Same value displayed unchanged
- Display load: ~5 redraws/sec → ~0.5 redraws/sec when stable

**Implementation Details:**
- `largeDisplayMode.needsFullRedraw`: Set true on B (enter), X/B (value change)
- `largeDisplayMode.lastDisplayedValue`: Stores previous value string for comparison
- Flag cleared after each render; set again on user input

**Data Model Changes:**
- Added `bool needsFullRedraw` to `largeDisplayMode` struct (1 byte)
- Added `String lastDisplayedValue` to `largeDisplayMode` struct (~60 bytes, dynamic)

**Performance Impact:**
- Typical sensor update cycle (500ms): 10 redraws (full) vs. 500 redraws (full every frame)
- Each frame: 1 display update vs. ~25 display operations
- I2C bandwidth reduction: ~95%
- User perception: Smooth, no flicker

**Testing:**
- TC-OPT-001: Observe Large Display mode for 60 seconds with stable sensor input
- Expected: No flicker, smooth value updates every 500ms
- TC-OPT-002: Change value with B/X buttons
- Expected: Clean transition with single full redraw, then smooth incremental updates

**Documentation Updated:**
- docs/04-data-model.md: New fields in largeDisplayMode struct
- docs/07-ui.md: Section 10.7 "Render Optimization"
- docs/06-business-rules.md: BR-DISPLAY-FLICKER-001

---

### Optimization 14.2: Conditional Header Rendering (pageChanged Parameter)

**Problem:** SCD30 page header (`drawHeaderWithBadge()`) calls `tft.fillScreen(TFT_BLACK)` every refresh, causing flicker even in standard display mode.

**Solution:** Gate header redraw to page transitions only:
```cpp
void pageScd30(bool pageChanged) {
  if (pageChanged) {
      drawHeaderWithBadge("SCD30 (0x61)", scd30Sensor.getAutoSelfCalibration());
  }
  // Sensor values update every frame regardless
}
```

**Behavior:**
- Header drawn: Page navigation (A/Y buttons), Large Display mode entry
- Header skipped: Standard display refresh with same page active
- SCD30 values: Update every 500ms without blocking header operation

**Implementation Details:**
- `drawCurrentPage(bool pageChanged)` passes flag to all 10 page functions
- Set `pageChanged=true` on page change or Large Display exit
- Set `pageChanged=false` during standard display refresh
- Only SCD30 (Page 4) currently uses this pattern

**Performance Impact:**
- SCD30 page refresh: ~100ms (header) → ~50ms (values only)
- Frame rate improvement: ~5 fps → ~10 fps on SCD30 page
- No impact on other pages

**Scope:** Page 4 (SCD30) only. Other pages may benefit from same pattern (FR-UPDATE).

**Testing:**
- TC-OPT-003: Navigate to SCD30 page (Page 4)
- Expected: Header displays once on page change, values update smoothly every 500ms
- TC-OPT-004: Stay on SCD30 page for 60 seconds
- Expected: No flicker, smooth updates, no visible header redraws

**Documentation Updated:**
- docs/07-ui.md: Section "SCD30 Display Optimization"
- docs/06-business-rules.md: BR-SCD30-004

---

### Optimization 14.3: Library Migration for UV Sensors (FEATURE-SPEC-010)

**Completed:** SparkFun VEML6075 and Adafruit VEML6070 libraries integrated.

**Details:** See Phase 11 (Library Integration) in this document.

**Impact on Reconstruction:**
- Eliminates raw I2C `Wire` calls from main.cpp for UV sensors
- Makes code maintainable and vendor-supported
- Already documented in Phase 11

---

## Final Notes

This reconstruction plan defines a **deterministic, incremental approach** to recreate sensor-pages-dashboard from documentation alone. The plan:

1. **Separates concerns** into 14 distinct phases
2. **Identifies dependencies** between phases
3. **Specifies acceptance criteria** for each phase
4. **Provides testing validation** before proceeding
5. **Flags blocked items** requiring external information
6. **Documents all decisions** with source references

An AI coding agent (GitHub Copilot) can follow this plan incrementally, verifying completion at each phase before proceeding to the next. The resulting firmware should be **functionally identical** to the original application, with all 26 business rules, 10 UI pages, 9 I2C sensors, and embedded real-time constraints properly implemented.

**Estimated Total Effort:** 3–5 days of development + 1 day integration testing
**Estimated Compilation Time:** <1 minute per build
**Estimated Test Runtime:** <5 minutes (unit tests), 10–20 minutes (hardware tests)

