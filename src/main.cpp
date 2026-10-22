#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_INA219.h>
#include <Adafruit_TSL2591.h>
#include <Adafruit_SI1145.h>
#include <Adafruit_BME280.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>
#include <Adafruit_DS248x.h>
#include <OneWire.h>
#include <SparkFun_SCD30_Arduino_Library.h>  // FEATURE-SPEC-009: Upgraded to SparkFun library
#include <SparkFun_VEML6075_Arduino_Library.h>  // FEATURE-SPEC-010: Upgraded to SparkFun library
// PHASE-4: Removed direct Adafruit_VEML6070.h include to avoid redefinition (included via sensors.h)
// #include <Adafruit_VEML6070.h>  // FEATURE-SPEC-010: Upgraded to Adafruit library
#include <math.h>
#include <string.h>

// PHASE-4: Include sensor registry and all sensor classes
#include "sensors/sensors.h"
#include "diagnostics.h"

#define SDA_PIN 32
#define SCL_PIN 25
#define GAMEPAD_ADDR 0x50
#define ENCODER_ADDR 0x36
#define GAMEPAD_BUTTON_Y 2
#define GAMEPAD_BUTTON_A 5
#define GAMEPAD_BUTTON_X 6
#define GAMEPAD_BUTTON_B 1
#define GAMEPAD_BUTTON_SELECT 0
#define GAMEPAD_BUTTON_START 16
#define ENCODER_BUTTON_PIN 24
#define ENCODER_NEOPIXEL_PIN 6

// SD card pins for VSPI (same as Freenove sketches)
#define SD_SCK   18
#define SD_MISO  19
#define SD_MOSI  23
#define SD_CS    5
#define AUDIO_EN 4

#define VEML6075_ADDR 0x10
#define VEML6070_ADDR_LSB 0x38
#define VEML6070_ADDR_MSB 0x39
#define SI1145_ADDR 0x60
#define SCD30_ADDR 0x61
#define BME280_ADDR 0x77
#define DS2484_ADDR 0x18

#define VEML6075_REG_CONF 0x00
#define VEML6075_REG_UVA 0x07
#define VEML6075_REG_UVB 0x09
#define VEML6075_REG_COMP1 0x0A
#define VEML6075_REG_COMP2 0x0B
#define VEML6075_REG_ID 0x0C
#define SCD30_CMD_START 0x0010
#define SCD30_CMD_READY 0x0202
#define SCD30_CMD_READ 0x0300
#define SCD30_CMD_GET_ASC 0x5301
#define SCD30_CMD_SET_ASC 0x53DA

// ============================================================================
// PHASE 1: CONFIGURATION SECTION
// ============================================================================
// Central collection of all constants, thresholds, and tuning parameters
// Organized by functional area for easy location and modification

// Page and Display Configuration
constexpr uint8_t PAGE_COUNT = 10;
constexpr uint8_t CONFIG_PAGE_TITLE_MAX_LEN = 20;

// Timing Configuration (milliseconds)
constexpr uint32_t SENSOR_REFRESH_MS = 500;
constexpr uint32_t INPUT_REFRESH_MS = 10;        // Legacy shared input timing
constexpr uint32_t GAMEPAD_REFRESH_MS = 25;     // 40 Hz (25 ms interval)
constexpr uint32_t ENCODER_REFRESH_MS = 20;      // 50 Hz (20 ms interval)
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;  // 200 ms per slot (10 slots = 2000 ms cycle)
constexpr uint32_t SLOT_COUNT = 10;

// Encoder Configuration
constexpr int ENCODER_DELTA_THRESHOLD = 1;
constexpr uint8_t ENCODER_LED_DEFAULT_BRIGHTNESS = 255;
constexpr uint8_t ENCODER_LED_LARGE_DISPLAY_MIN = 128;   // 50% brightness
constexpr uint8_t ENCODER_LED_LARGE_DISPLAY_MAX = 255;   // 100% brightness

// Sensor Configuration
constexpr uint32_t TSL2591_INTEGRATION_TIME_DEFAULT_MS = 200;
constexpr uint32_t DS18B20_CONVERSION_TIME_DEFAULT_MS = 750;
constexpr uint8_t DS18B20_ERROR_THRESHOLD = 3;    // Consecutive errors before sensor disabled

// Button Configuration
constexpr uint32_t BUTTON_MASK = (1UL << GAMEPAD_BUTTON_X) | (1UL << GAMEPAD_BUTTON_Y) |
                                  (1UL << GAMEPAD_BUTTON_A) | (1UL << GAMEPAD_BUTTON_B) |
                                  (1UL << GAMEPAD_BUTTON_SELECT) | (1UL << GAMEPAD_BUTTON_START);

// Display Refresh Rate Limiting
constexpr uint32_t LARGE_DISPLAY_REFRESH_INTERVAL_MS = 200;  // Max 5 Hz

// Statistics Reporting
constexpr bool ENABLE_STATS_REPORTING_DEFAULT = true;

// ============================================================================
// Page Title Lookup Table (Eliminates String Duplication)
// ============================================================================
const char* const PAGE_TITLES[PAGE_COUNT] = {
  "BME280",      // Page 0
  "Gamepad",     // Page 1
  "INA219",      // Page 2
  "Encoder",     // Page 3
  "SCD30",       // Page 4
  "SI1145",      // Page 5
  "TSL2591",     // Page 6
  "VEML6070",    // Page 7
  "VEML6075",    // Page 8
  "DS2484"       // Page 9
};

// ============================================================================
// SECTION 1: HARDWARE DISPLAY
// ============================================================================
// Display Configuration: FNK0114N 3.5" TFT with ILI9341 controller
// Physical Resolution: 320×480 pixels
// Display Mode: LANDSCAPE (rotation=3 via User_Setup.h)
// Effective Coordinate System: x=0-480 (horizontal), y=0-320 (vertical)
// Header: y=0-38, Content Area: y=40-320
TFT_eSPI tft = TFT_eSPI(320, 480);

// ============================================================================
// SECTION 2: HARDWARE DEVICES (Adafruit Library Instances)
// ============================================================================
Adafruit_INA219 ina219(0x40);
Adafruit_TSL2591 tsl2591(2591);
Adafruit_SI1145 si1145;
Adafruit_BME280 bme280;
Adafruit_seesaw gamepad;
Adafruit_seesaw encoder;
seesaw_NeoPixel encoderLed(1, ENCODER_NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800, &Wire);
SCD30 scd30Sensor;                           // SparkFun SCD30 library instance
VEML6075 veml6075Sensor;                     // SparkFun VEML6075 library instance
Adafruit_VEML6070 veml6070Sensor;            // Adafruit VEML6070 library instance
Adafruit_DS248x ds2484Master;                // DS2484 I2C-to-1-Wire adapter

// ============================================================================
// SECTION 3: SENSOR WRAPPERS & REGISTRY
// ============================================================================
SensorRegistry sensorRegistry;               // Registry manages all 8 sensor wrappers polymorphically
DiagnosticStats diagStats = {0, 0, 0xFFFFFFFF, 0, 0, 0, 0, 0};

// Cooperative I2C bus serialization. The loop is single-threaded, but this
// guard prevents a scheduled read from starting while another read is active.
static bool i2cBusLocked = false;
static uint32_t i2cLockTimeout = 0;

static bool tryLockI2cBus(const char* requester)
{
#if I2C_SCHEDULING_ENABLED
  const uint32_t now = millis();
  if (i2cBusLocked && (int32_t)(now - i2cLockTimeout) >= 0) {
    i2cBusLocked = false;  // Recover from an interrupted/overlong read.
  }
  if (i2cBusLocked) {
    logPrintf("[I2C-SCHED] %s skipped: bus locked", requester);
    return false;
  }
  i2cBusLocked = true;
  i2cLockTimeout = now + I2C_LOCK_TIMEOUT_MS;
#endif
  return true;
}

static void unlockI2cBus()
{
#if I2C_SCHEDULING_ENABLED
  i2cBusLocked = false;
#endif
}

static void logI2cTiming(const char* sensor, uint32_t durationUs)
{
  logPrintf("[I2C-TIMING] %s:value time:%lu ms bus:%s errors:%lu",
            sensor, durationUs / 1000UL, i2cBusLocked ? "locked" : "free",
            diagStats.i2cErrors);
}

Ina219Sensor ina219Sensor;
Bme280Sensor bme280Sensor;
Si1145Sensor si1145Sensor;
Tsl2591Sensor tsl2591Sensor;
Veml6075Sensor veml6075Sensor_new;
Veml6070Sensor veml6070Sensor_class;
Scd30Sensor scd30Sensor_class;
Ds2484Sensor ds2484Sensor;

// ============================================================================
// SECTION 4: SENSOR STATUS & AVAILABILITY
// ============================================================================
struct SensorStatus {
  bool ina219;
  bool veml6075;
  bool tsl2591;
  bool encoder;
  bool veml6070;
  bool si1145;
  bool scd30;
  bool bme280;
  bool gamepad;
  bool ds2484;
};
SensorStatus available{};

// ============================================================================
// SECTION 5: SENSOR DATA CACHES (Global)
// ============================================================================
// Struct definitions moved to sensor headers; included via sensors.h
// PHASE 3a (INA219), 3b (BME280), 3c (SI1145), 3d (TSL2591), 3e (VEML6075), 3f (VEML6070), 3g (SCD30)
#include "sensors/Ds2484Types.h"

Ina219Values ina219Values{};
Bme280Values bme280Values{};
Si1145Values si1145Values{};
Tsl2591Values tsl2591Values{};
Veml6075Values veml6075Values{};
Veml6070Values veml6070Values{};
Scd30Values scd30Values{};
Ds2484Values ds2484Values{};

// Change detection caches for display refresh optimization
static Ina219Values lastIna219Values{};
static Bme280Values lastBme280Values{};
static Tsl2591Values lastTsl2591Values{};
static Si1145Values lastSi1145Values{};
static Veml6075Values lastVeml6075Values{};
static Veml6070Values lastVeml6070Values{};
static Scd30Values lastScd30Values{};
static Ds2484Values lastDs2484Values{};

// ============================================================================
// SECTION 6: DISPLAY & UI STATE
// ============================================================================
uint8_t currentPage = 2;                     // Start with INA219 page (Page 2)
uint8_t lastPage = 255;
uint32_t nextLargeDisplayRefreshMs = 0;      // Rate limiting: max 5 Hz (200ms interval)

struct {
  bool active;                               // Large display mode currently active
  uint8_t currentPage;                       // Page index where mode was entered (0-9)
  uint8_t valueIndex;                        // Current value being displayed (0 to pageValueCount-1)
  bool needsFullRedraw;                      // Redraw entire screen (not just incremental update)
  String lastDisplayedValue;                 // Track last displayed value to detect changes
} largeDisplayMode = {false, 0, 0, false, ""};

static uint32_t lastTimerSeconds = 0xFFFFFFFF;

// ============================================================================
// SECTION 7: INPUT DEVICE STATE - ENCODER
// ============================================================================
struct EncoderValues { 
  int32_t position; int32_t delta; bool pressed; 
  uint32_t presses; uint8_t brightness; const char *color; 
};
EncoderValues encoderValues{};
static EncoderValues lastEncoderValues{};

int32_t encoderLastPosition = 0;
uint32_t encoderPressCount = 0;
uint32_t encoderPressedSince = 0;
bool encoderLastPressed = false;
uint8_t encoderColorIndex = 0;
uint8_t encoderBrightness = 64;

const uint32_t encoderColors[] = {
  seesaw_NeoPixel::Color(0, 96, 255), seesaw_NeoPixel::Color(0, 255, 32),
  seesaw_NeoPixel::Color(255, 48, 0), seesaw_NeoPixel::Color(180, 0, 255)
};
const char *const encoderColorNames[] = {"blue", "green", "red", "purple"};

// ============================================================================
// SECTION 8: INPUT DEVICE STATE - GAMEPAD
// ============================================================================
struct GamepadValues { 
  int x; int y; uint32_t buttons; 
  uint8_t eventCount; uint8_t lastEvent; bool hasEvent; 
};
GamepadValues gamepadValues{};
static GamepadValues lastGamepadValues{};

// ============================================================================
// SECTION 9: SCD30 STATE MACHINE (ASC - Auto Self Calibration)
// ============================================================================
struct Scd30State {
  uint32_t lastAscQuery;                     // Timestamp of last ASC status query (ms)
  bool pendingAscWrite;                      // Whether an ASC toggle is in-flight
};
Scd30State scd30State{};

// ============================================================================
// SECTION 10: ASYNC SENSOR STATE MACHINES
// ============================================================================
enum class Tsl2591State { IDLE, MEASURING };
static Tsl2591State tsl2591State = Tsl2591State::IDLE;
static uint32_t tsl2591MeasureStartMs = 0;

enum class Ds18b20State { IDLE, CONVERTING };
static Ds18b20State ds18b20State = Ds18b20State::IDLE;
static uint32_t ds18b20ConvertStartMs = 0;

// Track current sensor slot for rotation dispatch
static uint32_t lastSensorSlot = 0xFFFFFFFF;

// ============================================================================
// SECTION 11: CONFIGURATION & TUNING
// ============================================================================
static uint32_t TSL2591_INTEGRATION_TIME_MS = TSL2591_INTEGRATION_TIME_DEFAULT_MS;
static uint32_t DS18B20_CONVERSION_TIME_MS = DS18B20_CONVERSION_TIME_DEFAULT_MS;

uint32_t fwVersion = 0;
uint32_t optionsMask = 0;
float seesawTemperature = 0.0f;

// ============================================================================
// SECTION 12: STATISTICS & MONITORING
// ============================================================================
struct SensorReadStats {
  uint32_t minTime;
  uint32_t maxTime;
  uint32_t totalTime;
  uint16_t readCount;
};

struct GamepadStats {
  uint32_t minTime;
  uint32_t maxTime;
  uint32_t totalTime;
  uint32_t readCount;
};

struct EncoderStats {
  uint32_t minTime;
  uint32_t maxTime;
  uint32_t totalTime;
  uint32_t readCount;
};

struct Ina219Stats {
  uint32_t minTime;
  uint32_t maxTime;
  uint32_t totalTime;
  uint32_t readCount;
};

static SensorReadStats bme280Stats, tsl2591Stats, si1145Stats, veml6075Stats, veml6070Stats, scd30Stats, ds18b20Stats;
static GamepadStats gamepadStats{};
static EncoderStats encoderStats{};
static Ina219Stats ina219Stats{};
static uint32_t lastStatsUpdateMs = 0;
static uint32_t lastGamepadStatsLogMs = 0;
static uint32_t lastEncoderStatsLogMs = 0;
static uint32_t lastIna219StatsLogMs = 0;


// ============================================================================
// FEATURE-SPEC-008: CENTRAL PARAMETER TRIPLE DEFINITIONS
// ============================================================================
// Each sensor defines its parameter metadata once (Label, Unit, Color)
// Both Large Display and Sensor Page use the same definition
// This guarantees consistency by design, eliminates redundancy
// ============================================================================

struct ParameterTriple {
  const char* label;
  const char* unit;
  uint16_t color;
};

// BME280: 7 Parameters
const ParameterTriple bme280_params[] = {
  {"Temperature", "C", TFT_GREEN},
  {"Pressure", "hPa", TFT_GREEN},
  {"Humidity", "%", TFT_ORANGE},
  {"Dew Point", "C", TFT_CYAN},
  {"Altitude", "m", TFT_CYAN},
  {"Air Quality", "AQI", TFT_ORANGE},
  {"Temp Trend", "C/min", TFT_CYAN}
};
const uint8_t BME280_PARAM_COUNT = 7;

// Gamepad: 8 Parameters
const ParameterTriple gamepad_params[] = {
  {"Joystick X", "ADC", TFT_GREEN},
  {"Joystick Y", "ADC", TFT_GREEN},
  {"Button X", "State", TFT_ORANGE},
  {"Button Y", "State", TFT_ORANGE},
  {"Button A", "State", TFT_ORANGE},
  {"Button B", "State", TFT_ORANGE},
  {"Select/Start", "State", TFT_ORANGE},
  {"Button Mask", "Hex", TFT_CYAN}
};
const uint8_t GAMEPAD_PARAM_COUNT = 8;

// INA219: 7 Parameters
const ParameterTriple ina219_params[] = {
  {"Runtime", "", TFT_CYAN},
  {"Battery", "V", TFT_GREEN},
  {"Supply", "V", TFT_GREEN},
  {"Current", "mA", TFT_ORANGE},
  {"Charge", "mAh", TFT_CYAN},
  {"Shunt Voltage", "mV", TFT_GREEN},
  {"Power", "mW", TFT_ORANGE}
};
const uint8_t INA219_PARAM_COUNT = 7;

// Encoder: 7 Parameters
const ParameterTriple encoder_params[] = {
  {"Position", "Steps", TFT_GREEN},
  {"Delta", "Steps", TFT_ORANGE},
  {"Button", "State", TFT_ORANGE},
  {"Presses", "Count", TFT_CYAN},
  {"LED Color", "Name", TFT_CYAN},
  {"LED Brightness", "/255", TFT_GREEN},
  {"Seesaw Temp", "C", TFT_ORANGE}
};
const uint8_t ENCODER_PARAM_COUNT = 7;

// SCD30: 3 Parameters
const ParameterTriple scd30_params[] = {
  {"CO2 Level", "ppm", TFT_GREEN},
  {"Temperature", "C", TFT_GREEN},
  {"Humidity", "%", TFT_ORANGE}
};
const uint8_t SCD30_PARAM_COUNT = 3;

// SI1145: 4 Parameters
const ParameterTriple si1145_params[] = {
  {"Visible Light", "ADC", TFT_GREEN},
  {"Infrared", "ADC", TFT_GREEN},
  {"UV Index", "Index", TFT_CYAN},
  {"UV Raw", "ADC", TFT_ORANGE}
};
const uint8_t SI1145_PARAM_COUNT = 4;

// TSL2591: 4 Parameters
const ParameterTriple tsl2591_params[] = {
  {"Full Spectrum", "counts", TFT_GREEN},
  {"Infrared", "counts", TFT_GREEN},
  {"Visible", "counts", TFT_ORANGE},
  {"Lux", "lux", TFT_CYAN}
};
const uint8_t TSL2591_PARAM_COUNT = 4;

// VEML6070: 3 Parameters
const ParameterTriple veml6070_params[] = {
  {"UV Raw", "ADC", TFT_GREEN},
  {"UV Level", "Level", TFT_ORANGE},
  {"LSB/MSB", "Hex", TFT_CYAN}
};
const uint8_t VEML6070_PARAM_COUNT = 3;

// VEML6075: 6 Parameters
const ParameterTriple veml6075_params[] = {
  {"UVA Raw", "ADC", TFT_GREEN},
  {"UVB Raw", "ADC", TFT_GREEN},
  {"UVA Compensated", "Calc", TFT_CYAN},
  {"UVB Compensated", "Calc", TFT_CYAN},
  {"UV Index", "Index", TFT_ORANGE},
  {"Compensation", "Factors", TFT_CYAN}
};
const uint8_t VEML6075_PARAM_COUNT = 6;

// ============================================================================
// FORWARD DECLARATIONS - Page Handler Functions (for PageHandler array)
// ============================================================================
void pageBme280();
void pageGamepad();
void pageIna219();
void pageEncoder();
void pageScd30(bool pageChanged);
void pageSi1145();
void pageTsl2591();
void pageVeml6070();
void pageVeml6075();
void pageDs2484();

// ============================================================================
// PHASE 5: PAGE HANDLER ARRAY (Data-Driven Page Dispatch)
// ============================================================================
struct PageHandler {
  const char* name;                  // Page title (e.g., "BME280")
  void (*renderPage)(bool changed);  // Function pointer to page render function
  uint16_t color;                    // LED indicator color for this page
};

// Array maps page index (0-9) to handler (replaces switch statement)
const PageHandler pageHandlers[PAGE_COUNT] = {
  {"BME280",   [](bool changed) { pageBme280(); }, 0x07E0},      // Page 0: Green
  {"Gamepad",  [](bool changed) { pageGamepad(); }, 0xF81F},     // Page 1: Magenta
  {"INA219",   [](bool changed) { pageIna219(); }, 0xF800},      // Page 2: Red
  {"Encoder",  [](bool changed) { pageEncoder(); }, 0xFC00},     // Page 3: Orange
  {"SCD30",    [](bool changed) { pageScd30(changed); }, 0x0810},// Page 4: Blue
  {"SI1145",   [](bool changed) { pageSi1145(); }, 0xFFE0},      // Page 5: Yellow
  {"TSL2591",  [](bool changed) { pageTsl2591(); }, 0x07FF},     // Page 6: Cyan
  {"VEML6070", [](bool changed) { pageVeml6070(); }, 0xFC00},    // Page 7: Orange
  {"VEML6075", [](bool changed) { pageVeml6075(); }, 0xF81F},    // Page 8: Magenta
  {"DS2484",   [](bool changed) { pageDs2484(); }, 0xFBE0}       // Page 9: Light Gray
};

// ============================================================================
// FORWARD DECLARATIONS - Required for Value Getters to use helper functions
// ============================================================================
String formatRuntime(uint32_t elapsedUs);

// ============================================================================
// CENTRAL VALUE GETTERS - Return value string for each sensor parameter
// Used by both Large Display and Sensor Page rendering (FEATURE-SPEC-008)
// ============================================================================

String bme280_getValue(uint8_t index) {
  if (index >= BME280_PARAM_COUNT || !available.bme280) return "";
  switch (index) {
    case 0: return String(bme280Values.temperature, 2);
    case 1: return String(bme280Values.pressure, 2);
    case 2: return String(bme280Values.humidity, 2);
    case 3: return String(bme280Values.dewPoint, 2);
    case 4: return String(bme280Values.altitude, 1);
    case 5: return String(0.95, 2);  // Placeholder AQI
    case 6: return String(0.5, 2);   // Placeholder trend
    default: return "";
  }
}

String gamepad_getValue(uint8_t index) {
  if (index >= GAMEPAD_PARAM_COUNT || !available.gamepad) return "";
  switch (index) {
    case 0: return String((int)gamepadValues.x);
    case 1: return String((int)gamepadValues.y);
    case 2: return (gamepadValues.buttons & GAMEPAD_BUTTON_X) ? "PRESSED" : "released";
    case 3: return (gamepadValues.buttons & GAMEPAD_BUTTON_Y) ? "PRESSED" : "released";
    case 4: return (gamepadValues.buttons & GAMEPAD_BUTTON_A) ? "PRESSED" : "released";
    case 5: return (gamepadValues.buttons & GAMEPAD_BUTTON_B) ? "PRESSED" : "released";
    case 6: return (gamepadValues.buttons & GAMEPAD_BUTTON_SELECT) ? "SEL" : (gamepadValues.buttons & GAMEPAD_BUTTON_START) ? "SRT" : "--";
    case 7: return String("0x") + String((unsigned long)gamepadValues.buttons, HEX);
    default: return "";
  }
}

String ina219_getValue(uint8_t index) {
  if (index >= INA219_PARAM_COUNT || !available.ina219) return "";
  switch (index) {
    case 0: {  // Runtime
      const uint32_t runtimeUs = micros() - ina219Sensor.getValues().startTimeUs;
      return formatRuntime(runtimeUs);
    }
    case 1: return String(ina219Sensor.getValues().load, 3);
    case 2: return String(ina219Sensor.getValues().bus, 3);
    case 3: return String(ina219Sensor.getValues().current, 1);
    case 4: return String(ina219Sensor.getValues().chargeMah, 3);
    case 5: return String(ina219Sensor.getValues().shunt, 2);
    case 6: return String((uint16_t)ina219Sensor.getValues().power);
    default: return "";
  }
}

String encoder_getValue(uint8_t index) {
  if (index >= ENCODER_PARAM_COUNT || !available.encoder) return "";
  switch (index) {
    case 0: return String((long)encoderValues.position);
    case 1: return String((long)encoderValues.delta);
    case 2: return encoderValues.pressed ? "PRESSED" : "released";
    case 3: return String((unsigned long)encoderValues.presses);
    case 4: return encoderValues.color;
    case 5: return String((int)encoderValues.brightness);
    case 6: return String(seesawTemperature, 1);
    default: return "";
  }
}

String scd30_getValue(uint8_t index) {
  if (index >= SCD30_PARAM_COUNT || !available.scd30) return "";
  switch (index) {
    case 0: return String((uint16_t)scd30Values.co2);
    case 1: return String(scd30Values.temp, 1);
    case 2: return String(scd30Values.humidity, 1);
    default: return "";
  }
}

String si1145_getValue(uint8_t index) {
  if (index >= SI1145_PARAM_COUNT || !available.si1145) return "";
  switch (index) {
    case 0: return String(si1145Values.visible);
    case 1: return String(si1145Values.infrared);
    case 2: return String(si1145Values.uvIndex, 2);
    case 3: return String(si1145Values.uv);
    default: return "";
  }
}

String tsl2591_getValue(uint8_t index) {
  if (index >= TSL2591_PARAM_COUNT || !available.tsl2591) return "";
  switch (index) {
    case 0: return String(tsl2591Values.fullSpectrum);
    case 1: return String(tsl2591Values.infrared);
    case 2: return String(tsl2591Values.visible);
    case 3: return String(tsl2591Values.lux, 2);
    default: return "";
  }
}

String veml6070_getValue(uint8_t index) {
  if (index >= VEML6070_PARAM_COUNT || !available.veml6070) return "";
  switch (index) {
    case 0: return String(veml6070Values.raw);
    case 1: return String(veml6070Values.raw > 200 ? "High" : (veml6070Values.raw > 100 ? "Medium" : "Low"));
    case 2: return String("0x") + String(veml6070Values.msb, HEX) + String("/0x") + String(veml6070Values.lsb, HEX);
    default: return "";
  }
}

String veml6075_getValue(uint8_t index) {
  if (index >= VEML6075_PARAM_COUNT || !available.veml6075) return "";
  switch (index) {
    case 0: return String(veml6075Values.uva);
    case 1: return String(veml6075Values.uvb);
    case 2: return String(veml6075Values.uvaComp, 1);
    case 3: return String(veml6075Values.uvbComp, 1);
    case 4: return String(veml6075Values.index, 2);
    case 5: return String(veml6075Values.comp1) + "/" + String(veml6075Values.comp2);
    default: return "";
  }
}

// ============================================================================
// PHASE 2: HELPER FUNCTIONS & FORWARD DECLARATIONS
// ============================================================================
// Note: Forward declarations placed BEFORE helper functions that use them

// Forward declarations needed by helper functions
void drawCurrentPage(bool pageChanged = false);
uint8_t getPageValueCount(uint8_t pageIndex);
String formatRuntime(uint32_t elapsedUs);

// ============================================================================
// PHASE 2: HELPER FUNCTIONS
// ============================================================================
// Extracted functions to reduce code duplication and improve readability

/// @brief Check if sensor on the given page is available
/// Eliminates 2+ duplicate switch statements throughout the codebase
/// (FEATURE-SPEC-011: Large Display Mode entry validation)
bool isSensorAvailable(uint8_t pageIndex) {
  switch (pageIndex) {
    case 0: return available.bme280;
    case 1: return available.gamepad;
    case 2: return available.ina219;
    case 3: return available.encoder;
    case 4: return available.scd30;
    case 5: return available.si1145;
    case 6: return available.tsl2591;
    case 7: return available.veml6070;
    case 8: return available.veml6075;
    case 9: return available.ds2484;
    default: return false;
  }
}

/// @brief Get the title for a page by index
/// Replaces repeated string literals (11 instances in original code)
const char* getPageTitle(uint8_t pageIndex) {
  if (pageIndex < PAGE_COUNT) {
    return PAGE_TITLES[pageIndex];
  }
  return "";
}

/// @brief Check if a specific gamepad button is currently pressed
/// Helper to reduce verbosity of button state checking
bool isGamepadButtonPressed(uint8_t buttonMask, uint32_t currentButtons, uint32_t lastButtons) {
  const bool currentPressed = !(currentButtons & (1UL << buttonMask));
  const bool lastPressed = !(lastButtons & (1UL << buttonMask));
  return currentPressed && !lastPressed;  // Rising edge detection
}

/// @brief Enter Large Display Mode for the given page
/// Centralizes Mode entry logic (eliminates 2+ duplicate blocks in gamepad/encoder handlers)
/// Validates sensor availability before entry
void enterLargeDisplayMode(uint8_t pageIndex) {
  if (!isSensorAvailable(pageIndex)) return;
  
  uint8_t valueCount = getPageValueCount(pageIndex);
  if (valueCount > 0) {
    largeDisplayMode.active = true;
    largeDisplayMode.currentPage = pageIndex;
    largeDisplayMode.valueIndex = 0;
    largeDisplayMode.needsFullRedraw = true;
    largeDisplayMode.lastDisplayedValue = "";
  }
}

/// @brief Exit Large Display Mode and redraw the current page
/// Centralizes Mode exit logic to prevent display artifacts
inline void exitLargeDisplayMode() {
  largeDisplayMode.active = false;
  largeDisplayMode.valueIndex = 0;
  drawCurrentPage(true);  // Force full redraw
}

void writeText(int x, int y, const String &text, uint16_t color, uint8_t font = 2)
{
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, x, y, font);
}

void drawHeader(const char *title)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  writeText(6, 8, title, TFT_CYAN, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawFastHLine(6, 38, 468, TFT_DARKGREY);
}

// FEATURE-SPEC-009: Draw header with optional status badge (e.g., ASC status for SCD30)
void drawHeaderWithBadge(const char *title, const char *badgeText, uint16_t badgeColor)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  writeText(6, 8, title, TFT_CYAN, 4);
  
  // Draw ASC badge in top-right corner
  if (badgeText && badgeText[0]) {
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(badgeColor, TFT_BLACK);
    tft.drawString(String("ASC: ") + badgeText, 468, 12, 2);
  }
  
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawFastHLine(6, 38, 468, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
}

void row(int y, const char *label, const String &value, uint16_t color = TFT_WHITE)
{
  writeText(6, y, label, TFT_WHITE);
  tft.setTextPadding(320);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawRightString(value, 474, y, 2);
}

/// @brief Display a parameter triple: (Name, Value, Unit)
/// Renders: "Name: value unit" with color-coded value
/// This ensures consistent triple structure across all pages (FEATURE-SPEC-007)
void rowTriple(int y, const char *name, const String &value, const String &unit, uint16_t color = TFT_WHITE)
{
  // Display: "Name:" (left, white) | "value unit" (right, color-coded)
  writeText(6, y, name, TFT_WHITE);
  tft.setTextPadding(320);
  tft.setTextColor(color, TFT_BLACK);
  String displayValue = value + (unit.length() > 0 ? String(" ") + unit : "");
  tft.drawRightString(displayValue, 474, y, 2);
}

/// @brief Display all sensor parameters using central parameter definition
/// Eliminates redundancy: one definition, used by both Large Display and Sensor Page (FEATURE-SPEC-008)
void displaySensorPage(
  const ParameterTriple* params,
  uint8_t paramCount,
  String (*getValueFunc)(uint8_t)
) {
  int displayY = 56;
  const int lineHeight = 24;
  
  for (uint8_t i = 0; i < paramCount && displayY < 280; i++) {
    String value = getValueFunc(i);
    // Add colon to label for display
    String label = String(params[i].label) + ":";
    rowTriple(displayY, label.c_str(), value, params[i].unit, params[i].color);
    displayY += lineHeight;
  }
  tft.setTextPadding(0);
}

void footer(const char *status)
{
  writeText(6, 294, status, TFT_DARKGREY);
  tft.setTextPadding(0);
}

// Error display for missing sensors (title address handled by drawHeader)
void unavailable(const char *name, const char *address, const char *addressLabel)
{
  row(52, addressLabel, address, TFT_YELLOW);
  writeText(6, 64, "Sensor nicht vorhanden", TFT_RED, 2);
  writeText(6, 142, "Y: vorherige Seite", TFT_WHITE);
  writeText(6, 166, "A: naechste Seite", TFT_WHITE);
  footer("No data available");
}

// ==================== FEATURE-SPEC-007: UNIVERSAL LARGE DISPLAY MODE INFRASTRUCTURE ====================

// Forward declarations for combined helper functions (implemented below)
String ina219_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String bme280_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String gamepad_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String encoder_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String scd30_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String si1145_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String tsl2591_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String veml6070_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String veml6075_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);
String ds2484_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor);

// FEATURE-SPEC-009: Forward declarations for ASC control functions
bool setAsc(bool enable);
bool getAscStatus(bool& outEnabled);
String scd30_getAscStatus();

/// @brief Get the number of values available in large display mode for a given page
/// @param pageIndex Page index (0-9)
/// @return Number of displayable values for that page (3-9)
uint8_t getPageValueCount(uint8_t pageIndex) {
  switch (pageIndex) {
    case 0:  return BME280_PARAM_COUNT;     // BME280
    case 1:  return GAMEPAD_PARAM_COUNT;    // Gamepad
    case 2:  return INA219_PARAM_COUNT;     // INA219
    case 3:  return ENCODER_PARAM_COUNT;    // Encoder
    case 4:  return SCD30_PARAM_COUNT;      // SCD30
    case 5:  return SI1145_PARAM_COUNT;     // SI1145
    case 6:  return TSL2591_PARAM_COUNT;    // TSL2591
    case 7:  return VEML6070_PARAM_COUNT;   // VEML6070
    case 8:  return VEML6075_PARAM_COUNT;   // VEML6075
    case 9:  {                              // DS2484: Dynamic
      if (!available.ds2484 || ds2484Values.sensorCount == 0) return 0;
      return 3 * ds2484Values.sensorCount;
    }
    default: return 0;
  }
}

/// @brief Unified large display renderer for all pages (FEATURE-SPEC-007)
/// Displays a single sensor value in large format: title, label, huge value (Font 8), unit, footer
/// @param pageIndex Page to render (0-9)
/// @param valueIndex Value to display within that page (0 to pageValueCount-1)
void renderLargeDisplay(uint8_t pageIndex, uint8_t valueIndex) {
  // Get page-specific data via combined helper function
  const char* label = nullptr;
  const char* unit = nullptr;
  uint16_t color = TFT_WHITE;
  String value = "";
  
  switch (pageIndex) {
    case 0:  value = bme280_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 1:  value = gamepad_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 2:  value = ina219_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 3:  value = encoder_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 4:  value = scd30_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 5:  value = si1145_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 6:  value = tsl2591_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 7:  value = veml6070_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 8:  value = veml6075_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 9:  value = ds2484_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    default: return;
  }
  
  // Track if full redraw needed (before modifying the flag)
  bool doFullRedraw = largeDisplayMode.needsFullRedraw || value != largeDisplayMode.lastDisplayedValue;
  
  // Only clear screen if value changed or full redraw requested
  if (doFullRedraw) {
    tft.fillScreen(TFT_BLACK);
    largeDisplayMode.lastDisplayedValue = value;
  } else {
    // Value unchanged - only update the value text area
    // Clear just the value area to avoid flicker
    tft.fillRect(30, 130, 420, 60, TFT_BLACK);
  }
  
  tft.setTextDatum(MC_DATUM);  // Middle-center alignment
  
  // Get page title for display
  const char* pageTitle = nullptr;
  switch (pageIndex) {
    case 0: pageTitle = "BME280 Climate Sensor"; break;
    case 1: pageTitle = "Gamepad QT Controller"; break;
    case 2: pageTitle = "INA219 Power Monitor"; break;
    case 3: pageTitle = "Rotary Encoder Input"; break;
    case 4: pageTitle = "SCD30 CO2 Sensor"; break;
    case 5: pageTitle = "SI1145 UV/IR Sensor"; break;
    case 6: pageTitle = "TSL2591 Light Sensor"; break;
    case 7: pageTitle = "VEML6070 UV Sensor"; break;
    case 8: pageTitle = "VEML6075 UV Sensor"; break;
    case 9: pageTitle = "DS2484 1-Wire Adapter"; break;
    default: pageTitle = "Unknown"; break;
  }
  
  // Draw layout components (on full redraw)
  if (doFullRedraw) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(pageTitle, 240, 18, 4);          // Title (Font 4, Cyan, top)
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawFastHLine(6, 48, 468, TFT_DARKGREY);   // Separator line
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(label ? label : "???", 240, 70, 4);  // Label (Font 4, Cyan)
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(unit ? unit : "", 240, 240, 4); // Unit (Font 4, White)
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("B:next X:prev Y:exit", 240, 295, 2);  // Footer (Font 2, Dark Grey)
  }
  
  // Always update the value (might change color)
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, 240, 160, 8);             // Value (Font 8, color-coded, focal point)
  
  // Clear needsFullRedraw after render
  largeDisplayMode.needsFullRedraw = false;
  
  tft.setTextDatum(TL_DATUM);  // Reset to default
}

// ==================== END LARGE DISPLAY MODE INFRASTRUCTURE ====================

// ==================== END LARGE DISPLAY MODE INFRASTRUCTURE ====================

// ==================== LARGE DISPLAY MODE COMBINED HELPER FUNCTIONS ====================

/// Page 0: BME280 - Uses central parameter definitions (FEATURE-SPEC-008)
String bme280_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= BME280_PARAM_COUNT) return "";
  *outLabel = bme280_params[valueIndex].label;
  *outUnit = bme280_params[valueIndex].unit;
  *outColor = bme280_params[valueIndex].color;
  return bme280_getValue(valueIndex);
}

/// Page 1: Gamepad - Uses central parameter definitions (FEATURE-SPEC-008)
String gamepad_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= GAMEPAD_PARAM_COUNT) return "";
  *outLabel = gamepad_params[valueIndex].label;
  *outUnit = gamepad_params[valueIndex].unit;
  *outColor = gamepad_params[valueIndex].color;
  return gamepad_getValue(valueIndex);
}

/// Page 2: INA219 - Uses central parameter definitions (FEATURE-SPEC-008)
String ina219_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= INA219_PARAM_COUNT) return "";
  *outLabel = ina219_params[valueIndex].label;
  *outUnit = ina219_params[valueIndex].unit;
  *outColor = ina219_params[valueIndex].color;
  return ina219_getValue(valueIndex);
}

/// Page 3: Encoder - Uses central parameter definitions (FEATURE-SPEC-008)
String encoder_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= ENCODER_PARAM_COUNT) return "";
  *outLabel = encoder_params[valueIndex].label;
  *outUnit = encoder_params[valueIndex].unit;
  *outColor = encoder_params[valueIndex].color;
  return encoder_getValue(valueIndex);
}

/// Page 4: SCD30 - Uses central parameter definitions (FEATURE-SPEC-008)
String scd30_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= SCD30_PARAM_COUNT) return "";
  *outLabel = scd30_params[valueIndex].label;
  *outUnit = scd30_params[valueIndex].unit;
  *outColor = scd30_params[valueIndex].color;
  return scd30_getValue(valueIndex);
}

/// Page 5: SI1145 - Uses central parameter definitions (FEATURE-SPEC-008)
String si1145_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= SI1145_PARAM_COUNT) return "";
  *outLabel = si1145_params[valueIndex].label;
  *outUnit = si1145_params[valueIndex].unit;
  *outColor = si1145_params[valueIndex].color;
  return si1145_getValue(valueIndex);
}

/// Page 6: TSL2591 - Uses central parameter definitions (FEATURE-SPEC-008)
String tsl2591_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= TSL2591_PARAM_COUNT) return "";
  *outLabel = tsl2591_params[valueIndex].label;
  *outUnit = tsl2591_params[valueIndex].unit;
  *outColor = tsl2591_params[valueIndex].color;
  return tsl2591_getValue(valueIndex);
}

/// Page 7: VEML6070 - Uses central parameter definitions (FEATURE-SPEC-008)
String veml6070_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= VEML6070_PARAM_COUNT) return "";
  *outLabel = veml6070_params[valueIndex].label;
  *outUnit = veml6070_params[valueIndex].unit;
  *outColor = veml6070_params[valueIndex].color;
  return veml6070_getValue(valueIndex);
}

/// Page 8: VEML6075 - Uses central parameter definitions (FEATURE-SPEC-008)
String veml6075_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (valueIndex >= VEML6075_PARAM_COUNT) return "";
  *outLabel = veml6075_params[valueIndex].label;
  *outUnit = veml6075_params[valueIndex].unit;
  *outColor = veml6075_params[valueIndex].color;
  return veml6075_getValue(valueIndex);
}

/// Page 9: DS2484 1-Wire Temperature Sensor Large Display Helper (FEATURE-SPEC-007)
String ds2484_getLargeDisplayData(uint8_t valueIndex, const char** outLabel, const char** outUnit, uint16_t* outColor) {
  if (!available.ds2484 || ds2484Values.sensorCount == 0 || valueIndex >= (3 * ds2484Values.sensorCount)) return "";
  
  uint8_t sensorIdx = valueIndex / 3;
  uint8_t fieldIdx = valueIndex % 3;
  
  if (sensorIdx >= ds2484Values.sensorCount) return "";
  
  const Ds18b20Sensor &sensor = ds2484Values.sensors[sensorIdx];
  
  switch (fieldIdx) {
    case 0: {  // ROM-ID
      *outLabel = "ROM-ID";
      *outUnit = "Hex";
      *outColor = TFT_CYAN;
      String romId = "";
      for (int i = 0; i < 8; i++) {
        if (i > 0) romId += ":";
        romId += String(sensor.address[i], HEX);
      }
      return romId;
    }
    case 1: {  // Temperature
      *outLabel = "Temperature";
      *outUnit = "°C";
      if (sensor.temperature == -127.0f) {
        *outColor = TFT_RED;
        return "Disconnected";
      }
      *outColor = TFT_GREEN;
      return String(sensor.temperature, 2);
    }
    case 2: {  // Status
      *outLabel = "Status";
      *outUnit = "Text";
      if (sensor.temperature == -127.0f) {
        *outColor = TFT_RED;
        return "Not Found";
      }
      *outColor = TFT_GREEN;
      return "OK";
    }
    default: return "";
  }
}

// ==================== END COMBINED HELPER FUNCTIONS ====================

// ==================== OLD INA219 HELPERS (KEPT FOR COMPATIBILITY - TO BE REMOVED) ====================

// ==================== END LARGE DISPLAY MODE HELPERS ====================

// ==================== END LARGE DISPLAY MODE HELPERS ====================

// FEATURE-SPEC-010: Helper functions removed - now using libraries for UV sensors


// FEATURE-SPEC-010: Legacy SCD30 functions removed (scd30Crc, scd30Command, scd30Ready, scd30Float)
// - SparkFun library handles all I2C protocol and CRC validation internally

bool readScd30(Scd30Values &values)
{
  uint32_t startTime = micros();
  
  // PHASE 3g: Delegate to sensor class wrapper
  scd30Sensor_class.update();
  values = scd30Sensor_class.getValues();
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  logI2cTiming("SCD30", duration);
  return true;
}

// FEATURE-SPEC-009: Toggle ASC (Automatic Self Calibration) mode on SCD30
// Now using SparkFun SCD30 library for robust I2C communication with builtin delays
bool setAsc(bool enable)
{
  if (!available.scd30) return false;
  scd30Sensor.setAutoSelfCalibration(enable);
  scd30State.pendingAscWrite = true;
  return true;
}

// FEATURE-SPEC-009: Query ASC status from SCD30
// SparkFun library handles I2C timing and CRC validation internally
bool getAscStatus(bool& outEnabled)
{
  if (!available.scd30) return false;
  outEnabled = scd30Sensor.getAutoSelfCalibration();
  scd30State.lastAscQuery = millis();
  scd30State.pendingAscWrite = false;
  return true;
}

// FEATURE-SPEC-009: Get ASC status string for display
String scd30_getAscStatus()
{
  if (!available.scd30) return "--";
  bool ascEnabled = scd30Sensor.getAutoSelfCalibration();
  return ascEnabled ? "ON" : "OFF";
}

bool readVeml6075(Veml6075Values &values)
{
  uint32_t startTime = micros();
  
  // FEATURE-SPEC-010: Use SparkFun VEML6075 Library for robust I2C communication
  values.uva = veml6075Sensor.rawUva();
  values.uvb = veml6075Sensor.rawUvb();
  values.comp1 = veml6075Sensor.uvComp1();
  values.comp2 = veml6075Sensor.uvComp2();
  values.uvaComp = veml6075Sensor.uva();
  values.uvbComp = veml6075Sensor.uvb();
  values.index = veml6075Sensor.index();
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  logI2cTiming("VEML6075", duration);
  return true;
}

bool readVeml6070(Veml6070Values &values)
{
  uint32_t startTime = micros();
  
  // PHASE 3f: Delegate to sensor class wrapper
  veml6070Sensor_class.update();
  values = veml6070Sensor_class.getValues();
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  logI2cTiming("VEML6070", duration);
  return true;
}

// New sensor read functions (FEATURE-SPEC-005: Unified Sensor Caching Architecture)

void readBme280(Bme280Values &values)
{
  if (!available.bme280) return;
  
  uint32_t startTime = micros();
  
  // PHASE 3: Delegate to sensor class
  bme280Sensor.update();
  
  // Copy from sensor accessor to global struct
  values = bme280Sensor.getValues();
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  logI2cTiming("BME280", duration);
}

void readTsl2591(Tsl2591Values &values)
{
  if (!available.tsl2591) return;
  const uint32_t startTime = micros();
  // PHASE 3: Delegate to sensor class
  tsl2591Sensor.update();
  // Copy from sensor accessor to global struct
  values = tsl2591Sensor.getValues();
  logI2cTiming("TSL2591", micros() - startTime);
}

void readSi1145(Si1145Values &values)
{
  if (!available.si1145) return;
  
  uint32_t startTime = micros();
  
  // PHASE 3: Delegate to sensor class
  si1145Sensor.update();
  
  // Copy from sensor accessor to global struct
  values = si1145Sensor.getValues();
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  logI2cTiming("SI1145", duration);
}

// DS2484 1-Wire temperature sensor functions
String formatRomId(const uint8_t *address)
{
  String result = "";
  for (uint8_t i = 0; i < 8; i++) {
    if (i > 0) result += ":";
    if (address[i] < 0x10) result += "0";
    result += String(address[i], HEX);
  }
  return result;
}

bool initDs2484()
{
  if (!available.ds2484) return false;
  // Initialize Adafruit DS248x (parameter order: TwoWire*, uint8_t address)
  bool success = ds2484Master.begin(&Wire, DS2484_ADDR);
  if (success) {
    // logPrintf("DS2484: Initialized at 0x18");
  } else {
    // logPrintf("DS2484: Initialization FAILED");
  }
  return success;
}

// DS2484 1-Wire operations via Adafruit library

bool ds2484ResetBus()
{
  // DS2484 reset bus via Adafruit library
  return ds2484Master.reset();
}

// PHASE 3h: Sync DS2484 wrapper values to global cache for display layer
void syncDs2484Values()
{
  if (!available.ds2484) return;
  
  // Get current values from sensor wrapper (updated via registry)
  const Ds2484Values &wrapperValues = ds2484Sensor.getValues();
  
  // Sync to global cache for backward compatibility
  ds2484Values = wrapperValues;
}

void performDs2484Scan()
{
  if (!available.ds2484) return;
  
  const uint32_t nowMs = millis();
  
  // Bus scan at 60-second interval (or on first scan when sensorCount == 0)
  bool firstScan = ds2484Values.sensorCount == 0;
  if (!firstScan && (int32_t)(nowMs - ds2484Values.lastScanMs) < 60000) return;
  
  logPrintf("[DS2484] Scanning 1-Wire bus...");
  
  // Reset sensor count and clear all sensors
  ds2484Values.sensorCount = 0;
  for (uint8_t i = 0; i < 3; i++) {
    ds2484Values.sensors[i].available = false;
    ds2484Values.sensors[i].errorCount = 0;
  }
  
  // Scan for all devices on 1-Wire bus via DS2484
  uint8_t address[8];
  ds2484Master.OneWireSearchReset();
  
  while (ds2484Master.OneWireSearch(address) && ds2484Values.sensorCount < 3) {
    // Verify CRC of address
    if (OneWire::crc8(address, 7) != address[7]) {
      continue;  // CRC mismatch, skip
    }
    
    Ds18b20Sensor &sensor = ds2484Values.sensors[ds2484Values.sensorCount];
    memcpy(sensor.address, address, 8);
    sensor.familyCode = address[0];
    sensor.available = true;
    sensor.errorCount = 0;
    sensor.temperature = 0.0f;
    sensor.lastReadMs = nowMs;
    
    logPrintf("[DS2484] Found sensor %d: Family=%02X, Address=%02X%02X%02X%02X%02X%02X%02X%02X", 
      ds2484Values.sensorCount, address[0], address[1], address[2], address[3], address[4], address[5], address[6], address[7]);
    ds2484Values.sensorCount++;
  }
  
  logPrintf("[DS2484] Bus scan complete - found %d sensors", ds2484Values.sensorCount);
  ds2484Values.lastScanMs = nowMs;
}

void readDs2484Temperatures()
{
  if (!available.ds2484 || ds2484Values.sensorCount == 0) return;
  
  const uint32_t nowMs = millis();
  
  // Temperature polling at 3-second interval
  if ((int32_t)(nowMs - ds2484Values.lastConversionMs) < 3000) return;
  
  // Send Convert T command to all devices (0x44 via Skip ROM 0xCC)
  if (!ds2484Master.OneWireReset()) {
    ds2484Values.lastConversionMs = nowMs;
    return;
  }
  
  if (!ds2484Master.OneWireWriteByte(0xCC)) {  // Skip ROM
    ds2484Values.lastConversionMs = nowMs;
    return;
  }
  
  if (!ds2484Master.OneWireWriteByte(0x44)) {  // Convert T
    ds2484Values.lastConversionMs = nowMs;
    return;
  }
  
  delay(850);  // Wait for conversion (12-bit resolution = 750ms typical)
  
  // Measure only the actual I2C read time, not the conversion wait time
  uint32_t startTime = micros();
  
  // Read temperature for each discovered sensor
  for (uint8_t i = 0; i < ds2484Values.sensorCount; i++) {
    Ds18b20Sensor &sensor = ds2484Values.sensors[i];
    
    // Skip if already offline after 3 errors
    if (sensor.errorCount >= 3) {
      sensor.available = false;
      continue;
    }
    
    // Reset and select device using Match ROM
    if (!ds2484Master.OneWireReset()) {
      sensor.errorCount++;
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Match ROM (0x55) command
    if (!ds2484Master.OneWireWriteByte(0x55)) {
      sensor.errorCount++;
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Send 8-byte ROM address
    bool romWriteOk = true;
    for (uint8_t j = 0; j < 8; j++) {
      if (!ds2484Master.OneWireWriteByte(sensor.address[j])) {
        romWriteOk = false;
        break;
      }
    }
    
    if (!romWriteOk) {
      sensor.errorCount++;
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Read Scratchpad command (0xBE)
    if (!ds2484Master.OneWireWriteByte(0xBE)) {
      sensor.errorCount++;
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Read 2 bytes of temperature data (LSB, MSB)
    uint8_t tempLsb, tempMsb;
    if (!ds2484Master.OneWireReadByte(&tempLsb) || !ds2484Master.OneWireReadByte(&tempMsb)) {
      sensor.errorCount++;
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Convert raw bytes to temperature
    // DS18B20: 12-bit resolution, 0.0625°C per LSB
    int16_t rawTemp = (int16_t)((tempMsb << 8) | tempLsb);
    sensor.temperature = rawTemp * 0.0625f;
    sensor.errorCount = 0;
    sensor.available = true;
    sensor.lastReadMs = nowMs;
    
    // logPrintf("DS2484: Sensor %d temp = %.2f°C", i, sensor.temperature);
  }
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  logPrintf("DS2484 read time: %lu µs", duration);
  
  ds2484Values.lastConversionMs = nowMs;
}

/// @brief Update encoder LED color and brightness based on display mode (FEATURE-SPEC-011)
/// Standard mode: LED color reflects current page (10-color map), full brightness (255)
/// Large Display mode: LED color reflects page, brightness scales 50-100% with value index
void updateEncoderLed()
{
  if (!available.encoder) return;

  // FEATURE-SPEC-011: 10-color page map for encoder LED feedback
  const uint32_t pageColors[] = {
    0xFF0000,  // Page 0 (BME280): Red
    0xFF7F00,  // Page 1 (Gamepad): Orange
    0xFFFF00,  // Page 2 (INA219): Yellow
    0x00FF00,  // Page 3 (Encoder): Green
    0x00FFFF,  // Page 4 (SCD30): Cyan
    0x0000FF,  // Page 5 (SI1145): Blue
    0x7F00FF,  // Page 6 (TSL2591): Purple
    0xFFFFFF,  // Page 7 (VEML6070): White
    0xFF1493,  // Page 8 (VEML6075): Pink
    0x87CEEB   // Page 9 (DS2484): Sky Blue
  };

  uint32_t ledColor = pageColors[currentPage];
  uint8_t ledBrightness = 255;  // Default: full brightness in standard mode

  // FEATURE-SPEC-011: Adjust brightness in Large Display Mode
  if (largeDisplayMode.active) {
    uint8_t paramCount = getPageValueCount(largeDisplayMode.currentPage);
    // Linear brightness scale: 50% (128) to 100% (255) based on value index
    if (paramCount > 0) {
      ledBrightness = 128 + (largeDisplayMode.valueIndex * 127 / paramCount);
    } else {
      ledBrightness = 128;  // Safety fallback to 50%
    }
  }

  encoderLed.setBrightness(ledBrightness);
  encoderLed.setPixelColor(0, ledColor);
  encoderLed.show();
}

void readEncoderAt50Hz()
{
  if (!available.encoder) return;
  if (!tryLockI2cBus("Encoder")) return;

  uint32_t startTime = micros();
  const int32_t previousPosition = encoderLastPosition;
  static uint32_t consecutiveI2cErrors = 0;
  int32_t position = previousPosition;
  bool readSucceeded = false;
  logPrintf("[ENCODER] Reading position (previous: %ld)", (long)previousPosition);
  for (uint8_t attempt = 1; attempt <= I2C_MAX_RETRY_ATTEMPTS; ++attempt) {
    // A timeout is reported by the ESP32 I2C driver as error 263.  Probe the
    // device before the normal read so a failed transaction can be recovered.
    Wire.beginTransmission(ENCODER_ADDR);
    const uint8_t i2cError = Wire.endTransmission();
    if (i2cError == 0) {
      position = encoder.getEncoderPosition();
      encoderValues.pressed = encoder.digitalRead(ENCODER_BUTTON_PIN) == LOW;
      readSucceeded = true;
      consecutiveI2cErrors = 0;
      break;
    }
    ++consecutiveI2cErrors;
    logPrintf("[ENCODER] I2C error 263 (attempt %u/%u, timestamped)", attempt, I2C_MAX_RETRY_ATTEMPTS);
    recoverI2cBus("encoder", ENCODER_ADDR, consecutiveI2cErrors, SDA_PIN, SCL_PIN);
  }
  if (!readSucceeded) {
    logPrintf("[ENCODER] I2C read failed after %u attempts", I2C_MAX_RETRY_ATTEMPTS);
  }
  encoderValues.delta = position - previousPosition;
  encoderLastPosition = position;
  encoderValues.position = position;
  logPrintf("[ENCODER] Got position: %ld (previous: %ld, delta: %+ld)",
            (long)position, (long)previousPosition, (long)encoderValues.delta);
  if (labs(encoderValues.delta) > ENCODER_POSITION_MAX_DELTA) {
    logPrintf("[ENCODER] WARNING: Position delta exceeded limit (previous: %ld, current: %ld)",
              (long)previousPosition, (long)position);
  } else {
    logPrintf("[ENCODER] Position valid");
  }

  // FEATURE-SPEC-011: Encoder button press tracking (debounce via rising-edge detection)
  // Note: LED updates are now handled by updateEncoderLed() in navigation context,
  // and navigation logic is in handleButtons() in the main loop.
  // This function only tracks state; state changes are processed in handleButtons().

  if (encoderValues.pressed && !encoderPressedSince) encoderPressedSince = millis();
  if (!encoderValues.pressed) encoderPressedSince = 0;
  // NOTE: encoderLastPressed is NOT updated here!
  // It is maintained separately in the encoder navigation handler to ensure
  // rising-edge detection works correctly for mode toggling.
  encoderValues.presses = encoderPressCount;
  encoderValues.brightness = encoderBrightness;
  encoderValues.color = encoderColorNames[encoderColorIndex];
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  
  // Aggregate stats for 1-second reporting
  #if ENABLE_STATS_REPORTING
  encoderStats.readCount++;
  encoderStats.totalTime += duration;
  if (encoderStats.readCount == 1) {
    encoderStats.minTime = duration;
    encoderStats.maxTime = duration;
  } else {
    if (duration < encoderStats.minTime) encoderStats.minTime = duration;
    if (duration > encoderStats.maxTime) encoderStats.maxTime = duration;
  }
  #endif
  unlockI2cBus();
  logI2cTiming("Encoder", duration);
}

void readSensors()
{
  if (!tryLockI2cBus("Sensors")) return;
  const uint32_t startTime = micros();
  // PHASE-4: Update all sensors via registry
  sensorRegistry.updateAll();
  
  // Legacy sensor reads (kept for backwards compatibility during transition)
  if (available.bme280) readBme280(bme280Values);       // New: BME280 caching (FEATURE-SPEC-005)
  // NOTE: TSL2591 is async - starts measurement in rotatedReadSensor(), checked later
  // if (available.tsl2591) readTsl2591(tsl2591Values);  // Removed: async via startTsl2591Measurement()
  if (available.si1145) readSi1145(si1145Values);       // New: SI1145 caching (FEATURE-SPEC-005)
  if (available.veml6075) readVeml6075(veml6075Values);
  if (available.veml6070) readVeml6070(veml6070Values);
  // FEATURE-SPEC-010: Use SparkFun SCD30 dataAvailable() instead of scd30Ready()
  if (available.scd30 && scd30Sensor.dataAvailable()) readScd30(scd30Values);
  // PHASE 3h: Sync DS2484 wrapper values to global cache
  if (available.ds2484) {
    syncDs2484Values();       // Sync wrapper updates to global cache for display layer
  }
  unlockI2cBus();
  logI2cTiming("Sensors", micros() - startTime);
}

// ============================================================================
// FEATURE-SPEC-006: Non-Blocking Asynchronous Sensor Functions
// ============================================================================

/// @brief Start TSL2591 asynchronous measurement (non-blocking, ~0 ms)
/// Sets state to MEASURING and records start time; measurement happens in background.
/// The result must be checked later via checkTsl2591Async() after TSL2591_INTEGRATION_TIME_MS.
void startTsl2591Measurement()
{
  if (!available.tsl2591) return;
  
  uint32_t startTime = micros();
  
  // Set state machine to MEASURING
  tsl2591State = Tsl2591State::MEASURING;
  tsl2591MeasureStartMs = millis();
  
  // Note: The actual ADC integration setup would happen here if using low-level Adafruit API.
  // For now, we rely on getFullLuminosity() blocking call to be split into:
  // 1. This function: setup integration (minimal time)
  // 2. checkTsl2591Async(): call getFullLuminosity() after delay
  
  uint32_t endTime = micros();
  // logPrintf("TSL2591 async START time: %lu µs", endTime - startTime);
}

/// @brief Check if TSL2591 measurement is complete; read result if ready (non-blocking if not ready)
/// If measurement has been integrating for >= TSL2591_INTEGRATION_TIME_MS, reads the result
/// and updates the cache. Otherwise, returns immediately without reading.
void checkTsl2591Async()
{
  // Guard: Only proceed if currently MEASURING
  if (tsl2591State != Tsl2591State::MEASURING) return;
  if (!available.tsl2591) return;
  
  uint32_t elapsedMs = millis() - tsl2591MeasureStartMs;
  
  // Not ready yet; will retry next cycle
  if (elapsedMs < TSL2591_INTEGRATION_TIME_MS) {
    // logPrintf("TSL2591 async WAITING: %lu ms elapsed, need %lu ms", elapsedMs, TSL2591_INTEGRATION_TIME_MS);
    return;
  }
  
  // Ready to read; measure ONLY the actual I2C read time (NOT library overhead)
  // FEATURE-SPEC-010: Use Adafruit TSL2591 library getFullLuminosity() instead of direct Wire calls
  uint32_t startTime = micros();
  
  // Read luminosity data using library method
  uint32_t lum = tsl2591.getFullLuminosity();
  uint16_t data0 = (uint16_t)(lum >> 16);
  uint16_t data1 = (uint16_t)(lum & 0xFFFF);
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  
  // Calculate luminosity values (same as Adafruit does)
  uint32_t fullSpectrum = data0;
  uint32_t infrared = data1;
  uint32_t visible = (fullSpectrum > infrared) ? (fullSpectrum - infrared) : 0;
  float lux = tsl2591.calculateLux(fullSpectrum, infrared);
  
  // Get gain and integration time from cached state (already set at init)
  uint16_t gain = tsl2591.getGain();
  uint16_t integrationTime = tsl2591.getTiming();
  
  tsl2591Values.fullSpectrum = fullSpectrum;
  tsl2591Values.infrared = infrared;
  tsl2591Values.visible = visible;
  tsl2591Values.lux = lux;
  tsl2591Values.gain = gain;
  tsl2591Values.integrationTime = integrationTime;
  
  // State machine back to IDLE
  tsl2591State = Tsl2591State::IDLE;
  
  logI2cTiming("TSL2591", duration);
}

/// @brief Start DS18B20 asynchronous temperature conversion (non-blocking, ~1 ms)
/// Sends 1-Wire conversion command to all DS18B20 sensors on the bus via DS2484.
/// Includes: Reset, Skip ROM (0xCC), Convert T (0x44).
/// Result must be checked later via readDs18b20Results() after DS18B20_CONVERSION_TIME_MS.
void startDs18b20Conversion()
{
  if (!available.ds2484 || ds2484Values.sensorCount == 0) return;
  
  uint32_t startTime = micros();
  logPrintf("[DS18B20] Starting conversion for %d sensors...", ds2484Values.sensorCount);
  
  // Send Convert T command to all devices (0x44 via Skip ROM 0xCC)
  if (!ds2484Master.OneWireReset()) {
    logPrintf("[DS18B20] ERROR: OneWire Reset failed");
    return;
  }
  
  if (!ds2484Master.OneWireWriteByte(0xCC)) {  // Skip ROM
    logPrintf("[DS18B20] ERROR: Skip ROM write failed");
    return;
  }
  
  if (!ds2484Master.OneWireWriteByte(0x44)) {  // Convert T
    logPrintf("[DS18B20] ERROR: Convert T write failed");
    return;
  }
  
  // Set state machine to CONVERTING (750ms conversion time will elapse)
  ds18b20State = Ds18b20State::CONVERTING;
  ds18b20ConvertStartMs = millis();
  
  uint32_t endTime = micros();
  logPrintf("[DS18B20] Conversion started (took %lu µs)", endTime - startTime);
}

/// @brief Read DS18B20 temperature results from all discovered sensors (blocking, ~X ms)
/// Assumes conversion was started earlier via startDs18b20Conversion().
/// Does NOT check timing - caller must enforce minimum delay of DS18B20_CONVERSION_TIME_MS.
/// Updates sensor.temperature and error counters; marks offline after 3 failures.
void readDs18b20Results()
{
  if (!available.ds2484 || ds2484Values.sensorCount == 0) return;
  
  const uint32_t nowMs = millis();
  uint32_t startTime = micros();
  logPrintf("[DS18B20] Reading results from %d sensors...", ds2484Values.sensorCount);
  
  uint8_t successCount = 0;
  // Read temperature for each discovered sensor
  for (uint8_t i = 0; i < ds2484Values.sensorCount; i++) {
    Ds18b20Sensor &sensor = ds2484Values.sensors[i];
    
    // Skip if already offline after 3 errors
    if (sensor.errorCount >= 3) {
      sensor.available = false;
      logPrintf("[DS18B20] Sensor %d: OFFLINE (3+ errors)", i);
      continue;
    }
    
    // Reset and select device using Match ROM
    if (!ds2484Master.OneWireReset()) {
      sensor.errorCount++;
      logPrintf("[DS18B20] Sensor %d: Reset failed (error %d/3)", i, sensor.errorCount);
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Match ROM (0x55) command
    if (!ds2484Master.OneWireWriteByte(0x55)) {
      sensor.errorCount++;
      logPrintf("[DS18B20] Sensor %d: Match ROM failed (error %d/3)", i, sensor.errorCount);
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Send 8-byte ROM address
    bool romWriteOk = true;
    for (uint8_t j = 0; j < 8; j++) {
      if (!ds2484Master.OneWireWriteByte(sensor.address[j])) {
        romWriteOk = false;
        break;
      }
    }
    
    if (!romWriteOk) {
      sensor.errorCount++;
      logPrintf("[DS18B20] Sensor %d: ROM address write failed (error %d/3)", i, sensor.errorCount);
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Read Scratchpad command (0xBE)
    if (!ds2484Master.OneWireWriteByte(0xBE)) {
      sensor.errorCount++;
      logPrintf("[DS18B20] Sensor %d: Read Scratchpad failed (error %d/3)", i, sensor.errorCount);
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Read 2 bytes of temperature data (LSB, MSB)
    uint8_t tempLsb, tempMsb;
    if (!ds2484Master.OneWireReadByte(&tempLsb) || !ds2484Master.OneWireReadByte(&tempMsb)) {
      sensor.errorCount++;
      logPrintf("[DS18B20] Sensor %d: Temperature read failed (error %d/3)", i, sensor.errorCount);
      if (sensor.errorCount >= 3) sensor.available = false;
      continue;
    }
    
    // Convert raw bytes to temperature
    // DS18B20: 12-bit resolution, 0.0625°C per LSB
    int16_t rawTemp = (int16_t)((tempMsb << 8) | tempLsb);
    sensor.temperature = rawTemp * 0.0625f;
    sensor.errorCount = 0;
    sensor.available = true;
    sensor.lastReadMs = nowMs;
    successCount++;
    
    logPrintf("[DS18B20] Sensor %d: TEMP = %.2f°C", i, sensor.temperature);
  }
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  logPrintf("[DS18B20] Results read: %d/%d sensors OK, took %lu µs", successCount, ds2484Values.sensorCount, duration);
  
  // Reset state machine to IDLE for next conversion cycle
  ds18b20State = Ds18b20State::IDLE;
}

/// @brief Check if DS18B20 conversion is complete; read results if ready (non-blocking if not ready)
/// If conversion has been running for >= DS18B20_CONVERSION_TIME_MS, reads the results
/// and updates the cache. Otherwise, returns immediately without reading.
void checkDs18b20Async()
{
  // Guard: Only proceed if currently CONVERTING
  if (ds18b20State != Ds18b20State::CONVERTING) return;
  if (!available.ds2484) return;
  
  uint32_t elapsedMs = millis() - ds18b20ConvertStartMs;
  
  // Not ready yet; will retry next cycle
  if (elapsedMs < DS18B20_CONVERSION_TIME_MS) {
    logPrintf("[DS18B20] Waiting: %lu ms elapsed (need %lu ms)", elapsedMs, DS18B20_CONVERSION_TIME_MS);
    return;
  }
  
  logPrintf("[DS18B20] Conversion ready, reading results...");
  // Ready to read; call blocking read function
  uint32_t startTime = micros();
  readDs18b20Results();
  uint32_t endTime = micros();
  
  // State machine back to IDLE
  ds18b20State = Ds18b20State::IDLE;
  
  logPrintf("[DS18B20] Complete: read took %lu µs, total wait was %lu ms", endTime - startTime, elapsedMs);
}

/// @brief Rotated sensor read dispatcher (FEATURE-SPEC-006 non-blocking architecture)
/// Call this once per SENSOR_SLOT_INTERVAL_MS with slot number (0-6) to distribute sensor reads.
/// Each slot handles specific sensors; async sensors start in one slot and result is read in a later slot.
///
/// Slot schedule (200 ms intervals):
///   Slot 0 (0 ms):   Read BME280
///   Slot 1 (200 ms): Start TSL2591 measurement (async)
///   Slot 2 (400 ms): Read SI1145
///   Slot 3 (600 ms): Read VEML6075
///   Slot 4 (800 ms): Read VEML6070
///   Slot 5 (1000 ms): Read SCD30
///   Slot 6 (1200 ms): Start DS18B20 conversion (async) + check TSL2591 + check DS18B20
void rotatedReadSensor(uint32_t slot)
{
  if (!tryLockI2cBus("Sensors")) return;
  const uint32_t startTime = micros();
  // Clamp slot to valid range
  slot = slot % SLOT_COUNT;
  
  // FEATURE-SPEC-006: Optimized slot schedule (200 ms intervals, 2000 ms total cycle with 10 slots)
  // Structure: START async → READ sync → CHECK async (when conversion is guaranteed complete)
  //
  //   Phase 1: START ASYNC (Slots 0-1, 0-200ms)
  //   Slot 0 (0 ms):    Start TSL2591 measurement (async, needs ~200ms)
  //   Slot 1 (200 ms):  Start DS18B20 conversion (async, needs ~750ms)
  //
  //   Phase 2: READ SYNC (Slots 2-7, 400-1400ms)
  //   Slot 2 (400 ms):  BME280 read (synchronous, ~2ms)
  //   Slot 3 (600 ms):  SI1145 read (synchronous, ~1ms)
  //   Slot 4 (800 ms):  VEML6075 read (synchronous, ~1ms)
  //   Slot 5 (1000 ms): VEML6070 read (synchronous, ~1ms)
  //   Slot 6 (1200 ms): SCD30 read (synchronous, ~5-10ms)
  //   Slot 7 (1400 ms): DS2484 bus scan (synchronous, ~10ms, runs every 60 seconds)
  //
  //   Phase 3: CHECK ASYNC (Slots 8-9, 1600-1800ms)
  //   Slot 8 (1600 ms): Check TSL2591 result (started 1600ms ago, guaranteed complete at 200ms ✓)
  //   Slot 9 (1800 ms): Check DS18B20 result (started 1800ms ago, guaranteed complete at 750ms ✓)
  //
  // Cycle Time: 10 slots × 200ms = 2000ms
  
  switch (slot) {
    // ========== PHASE 1: START ASYNC MEASUREMENTS ==========
    case 0:
      // Start TSL2591 asynchronous light measurement
      // Will be checked in Slot 8 (~1600ms later, well after 200ms needed)
      startTsl2591Measurement();
      break;
      
    case 1:
      // Start DS18B20 asynchronous temperature conversion on all discovered sensors
      // Will be checked in Slot 9 (~1800ms later, well after 750ms needed)
      if (available.ds2484 && ds2484Values.sensorCount > 0) {
        startDs18b20Conversion();
      }
      break;
      
    // ========== PHASE 2: READ SYNCHRONOUS SENSORS ==========
    case 2:
      // BME280: Fast synchronous read (~2 ms)
      if (available.bme280) readBme280(bme280Values);
      break;
      
    case 3:
      // SI1145: Fast synchronous read (~1 ms)
      if (available.si1145) readSi1145(si1145Values);
      break;
      
    case 4:
      // VEML6075: Fast synchronous read (~1 ms)
      if (available.veml6075) readVeml6075(veml6075Values);
      break;
      
    case 5:
      // VEML6070: Fast synchronous read (~1 ms)
      if (available.veml6070) readVeml6070(veml6070Values);
      break;
      
    case 6:
      // SCD30: Moderate synchronous read (~5-10 ms) + optional ASC status query
      // FEATURE-SPEC-010: Use SparkFun SCD30 Library - always read to prevent flickering
      if (available.scd30) readScd30(scd30Values);
      
      // FEATURE-SPEC-009: Query ASC status with adaptive timing
      // SparkFun library handles I2C timing and delays internally
      // After toggle: query immediately (pendingAscWrite = true)
      // After boot: first query after 3+ seconds, then every 2 seconds
      if (available.scd30) {
        uint32_t timeSinceLastQuery = millis() - scd30State.lastAscQuery;
        bool shouldQuery = scd30State.pendingAscWrite ||  // After toggle
                          (scd30State.lastAscQuery == 0 && millis() >= 3000);  // After boot, wait 3s
        if (!shouldQuery && !scd30State.pendingAscWrite) {
          shouldQuery = (timeSinceLastQuery >= 2000);  // Regular polling every 2s
        }
        
        if (shouldQuery) {
          // SparkFun library handles I2C bus settling internally
          bool ascEnabled;
          getAscStatus(ascEnabled);  // Query ASC status via library
        }
      }
      break;
      
    case 7:
      // DS2484 periodic bus scan (60-second interval) to detect new/removed 1-Wire sensors
      if (available.ds2484) {
        performDs2484Scan();
      }
      break;
      
    // ========== PHASE 3: CHECK ASYNC RESULTS ==========
    case 8:
      // Check TSL2591 result (started 1600ms ago in Slot 0)
      // TSL2591 needs ~200ms, so definitely complete now
      checkTsl2591Async();
      break;
      
    case 9:
      // Check DS18B20 result (started 1800ms ago in Slot 1)
      // DS18B20 needs ~750ms, so definitely complete now
      checkDs18b20Async();
      break;
  }
  unlockI2cBus();
  logI2cTiming("Sensors", micros() - startTime);
}

/// @brief Check if any sensor value has changed compared to last cached snapshot
/// Used for display refresh optimization: only redraw if data actually changed.
/// Compares all relevant sensor cache fields with their last known values.
bool valuesChanged()
{
  // INA219 changed?
  if (ina219Sensor.getValues().load != lastIna219Values.load ||
      ina219Sensor.getValues().bus != lastIna219Values.bus ||
      ina219Sensor.getValues().current != lastIna219Values.current ||
      ina219Sensor.getValues().power != lastIna219Values.power ||
      ina219Sensor.getValues().shunt != lastIna219Values.shunt ||
      ina219Sensor.getValues().chargeMah != lastIna219Values.chargeMah) {
    return true;
  }
  
  // BME280 changed?
  if (bme280Values.temperature != lastBme280Values.temperature ||
      bme280Values.humidity != lastBme280Values.humidity ||
      bme280Values.pressure != lastBme280Values.pressure ||
      bme280Values.altitude != lastBme280Values.altitude) {
    return true;
  }
  
  // TSL2591 changed?
  if (tsl2591Values.lux != lastTsl2591Values.lux ||
      tsl2591Values.visible != lastTsl2591Values.visible ||
      tsl2591Values.infrared != lastTsl2591Values.infrared) {
    return true;
  }
  
  // SI1145 changed?
  if (si1145Values.visible != lastSi1145Values.visible ||
      si1145Values.infrared != lastSi1145Values.infrared ||
      si1145Values.uvIndex != lastSi1145Values.uvIndex) {
    return true;
  }
  
  // VEML6075 changed?
  if (veml6075Values.uva != lastVeml6075Values.uva ||
      veml6075Values.uvb != lastVeml6075Values.uvb ||
      veml6075Values.uvaComp != lastVeml6075Values.uvaComp ||
      veml6075Values.uvbComp != lastVeml6075Values.uvbComp) {
    return true;
  }
  
  // VEML6070 changed?
  if (veml6070Values.raw != lastVeml6070Values.raw) {
    return true;
  }
  
  // SCD30 changed? Use tolerance to avoid flickering from tiny sensor variations
  // Tolerance: CO2 ±1 ppm, Temp ±0.1°C, Humidity ±0.5%
  if (fabs(scd30Values.co2 - lastScd30Values.co2) > 1.0f ||
      fabs(scd30Values.temp - lastScd30Values.temp) > 0.1f ||
      fabs(scd30Values.humidity - lastScd30Values.humidity) > 0.5f) {
    return true;
  }
  
  // DS2484 changed?
  if (ds2484Values.sensorCount != lastDs2484Values.sensorCount) {
    return true;  // Sensor count changed
  }
  for (uint8_t i = 0; i < ds2484Values.sensorCount && i < 3; i++) {
    if (ds2484Values.sensors[i].temperature != lastDs2484Values.sensors[i].temperature) {
      return true;  // Any DS18B20 temperature changed
    }
  }
  
  // Gamepad changed?
  if (gamepadValues.x != lastGamepadValues.x ||
      gamepadValues.y != lastGamepadValues.y ||
      gamepadValues.buttons != lastGamepadValues.buttons) {
    return true;
  }
  
  // Encoder changed?
  if (encoderValues.position != lastEncoderValues.position ||
      encoderValues.pressed != lastEncoderValues.pressed ||
      encoderValues.brightness != lastEncoderValues.brightness) {
    return true;
  }
  
  return false;  // No values changed
}

/// @brief Check if the SPECIFIC value displayed in Large Display Mode has changed (FEATURE-SPEC-007)
/// In Large Display Mode, only the value at largeDisplayMode.valueIndex should trigger a redraw.
/// This prevents unnecessary updates when other sensors change.
/// Returns false if NOT in large display mode.
bool largeDisplayValueChanged()
{
  // Only apply this optimization if in large display mode
  if (!largeDisplayMode.active) {
    return false;  // Not in large display mode; use regular valuesChanged()
  }
  
  uint8_t pageIndex = largeDisplayMode.currentPage;
  uint8_t valueIndex = largeDisplayMode.valueIndex;
  
  // Page-specific checks - for now, always return true to ensure display updates
  // In the future, this can be optimized with per-page value change tracking
  switch (pageIndex) {
    case 0:  // BME280 - runtime changes every second
      return true;
    case 1:  // Gamepad - button states change frequently
      return true;
    case 2:  // INA219 - runtime changes every second
      return true;
    case 3:  // Encoder - position/delta change frequently
      return true;
    case 4:  // SCD30 - CO2/temp/humidity change gradually
      return true;
    case 5:  // SI1145 - light levels change gradually
      return true;
    case 6:  // TSL2591 - light levels change gradually
      return true;
    case 7:  // VEML6070 - UV levels change gradually
      return true;
    case 8:  // VEML6075 - UV levels change gradually
      return true;
    case 9:  // DS2484 - temperatures change gradually
      return true;
    default:
      return false;
  }
}

/// @brief Update cached values snapshot after display redraw (for next comparison)
void updateCachedValues()
{
  lastIna219Values = ina219Sensor.getValues();
  lastBme280Values = bme280Values;
  lastTsl2591Values = tsl2591Values;
  lastSi1145Values = si1145Values;
  lastVeml6075Values = veml6075Values;
  lastVeml6070Values = veml6070Values;
  lastScd30Values = scd30Values;
  lastDs2484Values = ds2484Values;
  lastGamepadValues = gamepadValues;
  lastEncoderValues = encoderValues;
}

/// @brief Update input statistics (optional, configurable via ENABLE_STATS_REPORTING)
/// Called every INPUT_STATS_INTERVAL_MS (1000 ms) to aggregate and print sensor timing stats.
/// When disabled (ENABLE_STATS_REPORTING=0), this function returns immediately.
void updateInputStats()
{
#if !ENABLE_STATS_REPORTING
  return;  // Stats reporting disabled
#else
  uint32_t now = millis();
  
  // Only aggregate once per interval
  if (now - lastStatsUpdateMs < INPUT_STATS_INTERVAL_MS) {
    return;
  }
  
  lastStatsUpdateMs = now;
  
  // Print aggregated statistics to Serial
  // logPrintf("\n=== Sensor Stats (1000 ms interval) ===");
  
  if (bme280Stats.readCount > 0) {
    /* // logPrintf("BME280:       min=%lu µs, max=%lu µs, avg=%lu µs, reads=%u\n",
      bme280Stats.minTime, bme280Stats.maxTime,
      bme280Stats.totalTime / bme280Stats.readCount,
      bme280Stats.readCount); */
  }
  
  if (tsl2591Stats.readCount > 0) {
    /* // logPrintf("TSL2591:      min=%lu µs, max=%lu µs, avg=%lu µs, reads=%u (async)\n",
      tsl2591Stats.minTime, tsl2591Stats.maxTime,
      tsl2591Stats.totalTime / tsl2591Stats.readCount,
      tsl2591Stats.readCount); */
  }
  
  if (si1145Stats.readCount > 0) {
    /* // logPrintf("SI1145:       min=%lu µs, max=%lu µs, avg=%lu µs, reads=%u\n",
      si1145Stats.minTime, si1145Stats.maxTime,
      si1145Stats.totalTime / si1145Stats.readCount,
      si1145Stats.readCount); */
  }
  
  if (veml6075Stats.readCount > 0) {
    /* // logPrintf("VEML6075:     min=%lu µs, max=%lu µs, avg=%lu µs, reads=%u\n",
      veml6075Stats.minTime, veml6075Stats.maxTime,
      veml6075Stats.totalTime / veml6075Stats.readCount,
      veml6075Stats.readCount); */
  }
  
  if (veml6070Stats.readCount > 0) {
    /* // logPrintf("VEML6070:     min=%lu µs, max=%lu µs, avg=%lu µs, reads=%u\n",
      veml6070Stats.minTime, veml6070Stats.maxTime,
      veml6070Stats.totalTime / veml6070Stats.readCount,
      veml6070Stats.readCount); */
  }
  
  if (scd30Stats.readCount > 0) {
    /* // logPrintf("SCD30:        min=%lu µs, max=%lu µs, avg=%lu µs, reads=%u\n",
      scd30Stats.minTime, scd30Stats.maxTime,
      scd30Stats.totalTime / scd30Stats.readCount,
      scd30Stats.readCount); */
  }
  
  if (ds18b20Stats.readCount > 0) {
    /* // logPrintf("DS18B20:      min=%lu µs, max=%lu µs, avg=%lu µs, reads=%u (async)\n",
      ds18b20Stats.minTime, ds18b20Stats.maxTime,
      ds18b20Stats.totalTime / ds18b20Stats.readCount,
      ds18b20Stats.readCount); */
  }
  
  // logPrintf("=====================================");
  
  // Reset counters for next interval
  bme280Stats = {};
  tsl2591Stats = {};
  si1145Stats = {};
  veml6075Stats = {};
  veml6070Stats = {};
  scd30Stats = {};
  ds18b20Stats = {};
#endif
}

/// @brief Print aggregated gamepad statistics once per second
/// Shows: readCount, min/max/total read times for gamepad in the last 1000ms
void printGamepadStats()
{
  #if !ENABLE_STATS_REPORTING
  return;
  #endif
  
  const uint32_t nowMs = millis();
  
  // Print stats every 1000ms
  if ((int32_t)(nowMs - lastGamepadStatsLogMs) < 1000) return;
  
  if (gamepadStats.readCount > 0) {
    logPrintf("Gamepad stats (1000ms): reads=%lu, min=%lu µs, max=%lu µs, total=%lu µs",
      gamepadStats.readCount,
      gamepadStats.minTime,
      gamepadStats.maxTime,
      gamepadStats.totalTime);
  }
  
  // Reset for next interval
  gamepadStats = {};
  lastGamepadStatsLogMs = nowMs;
}

/// @brief Print aggregated encoder statistics once per second
/// Shows: readCount, min/max/total read times for encoder in the last 1000ms
void printEncoderStats()
{
  #if !ENABLE_STATS_REPORTING
  return;
  #endif
  
  const uint32_t nowMs = millis();
  
  // Print stats every 1000ms (synchronized with gamepad)
  if ((int32_t)(nowMs - lastEncoderStatsLogMs) < 1000) return;
  
  if (encoderStats.readCount > 0) {
    logPrintf("Encoder stats (1000ms): reads=%lu, min=%lu µs, max=%lu µs, total=%lu µs",
      encoderStats.readCount,
      encoderStats.minTime,
      encoderStats.maxTime,
      encoderStats.totalTime);
  }
  
  // Reset for next interval
  encoderStats = {};
  lastEncoderStatsLogMs = nowMs;
}

/// @brief Print aggregated INA219 statistics once per second
/// Shows: readCount, min/max/total read times for INA219 in the last 1000ms
void printIna219Stats()
{
  #if !ENABLE_STATS_REPORTING
  return;
  #endif
  
  const uint32_t nowMs = millis();
  
  // Print stats every 1000ms (synchronized with gamepad)
  if ((int32_t)(nowMs - lastIna219StatsLogMs) < 1000) return;
  
  if (ina219Stats.readCount > 0) {
    logPrintf("INA219  stats (1000ms): reads=%lu, min=%lu µs, max=%lu µs, total=%lu µs",
      ina219Stats.readCount,
      ina219Stats.minTime,
      ina219Stats.maxTime,
      ina219Stats.totalTime);
  }
  
  // Reset for next interval
  ina219Stats = {};
  lastIna219StatsLogMs = nowMs;
}

void readGamepadAt40Hz()
{
  if (!available.gamepad) return;
  if (!tryLockI2cBus("Gamepad")) return;

  uint32_t startTime = micros();
  const uint32_t previousButtons = gamepadValues.buttons;
  static uint32_t consecutiveI2cErrors = 0;
  bool readSucceeded = false;
  logPrintf("[GAMEPAD] Reading state (previous buttons: 0x%08lX)", (unsigned long)previousButtons);
  for (uint8_t attempt = 1; attempt <= I2C_MAX_RETRY_ATTEMPTS; ++attempt) {
    Wire.beginTransmission(GAMEPAD_ADDR);
    const uint8_t i2cError = Wire.endTransmission();
    if (i2cError == 0) {
      gamepadValues.x = 1023 - gamepad.analogRead(14);
      gamepadValues.y = 1023 - gamepad.analogRead(15);
      gamepadValues.buttons = gamepad.digitalReadBulk(BUTTON_MASK);
      readSucceeded = true;
      consecutiveI2cErrors = 0;
      break;
    }
    ++consecutiveI2cErrors;
    logPrintf("[GAMEPAD] I2C error 263 (attempt %u/%u)", attempt, I2C_MAX_RETRY_ATTEMPTS);
    recoverI2cBus("gamepad", GAMEPAD_ADDR, consecutiveI2cErrors, SDA_PIN, SCL_PIN);
  }
  if (!readSucceeded) {
    logPrintf("[GAMEPAD] I2C read failed after %u attempts", I2C_MAX_RETRY_ATTEMPTS);
    gamepadValues.buttons = previousButtons;
  }
  if (gamepadValues.buttons != previousButtons) {
    logPrintf("[GAMEPAD] Button state changed: 0x%08lX -> 0x%08lX",
              (unsigned long)previousButtons, (unsigned long)gamepadValues.buttons);
  }
  gamepadValues.hasEvent = false;
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  
  // Aggregate stats for 1-second reporting
  #if ENABLE_STATS_REPORTING
  gamepadStats.readCount++;
  gamepadStats.totalTime += duration;
  if (gamepadStats.readCount == 1) {
    gamepadStats.minTime = duration;
    gamepadStats.maxTime = duration;
  } else {
    if (duration < gamepadStats.minTime) gamepadStats.minTime = duration;
    if (duration > gamepadStats.maxTime) gamepadStats.maxTime = duration;
  }
  #endif
  
  // Skip getKeypadCount() as it causes I2C blocking with other sensors
  // Button states are read directly via digitalReadBulk() above
  unlockI2cBus();
  logI2cTiming("Gamepad", duration);
}


void readIna219At50Hz()
{
  if (!available.ina219) return;
  if (!tryLockI2cBus("INA219")) return;
  const uint32_t startTime = micros();

  // PHASE 3a: Use ina219Sensor for hardware reads (single I2C instance)
  // Then copy to global ina219Values for backward compatibility
  ina219Sensor.update();
  
  // Populate global ina219Values from sensor
  ina219Values = ina219Sensor.getValues();
  unlockI2cBus();
  logI2cTiming("INA219", micros() - startTime);
}

// Format runtime as HH:MM:SS
String formatRuntime(uint32_t elapsedUs) {
  uint32_t seconds = elapsedUs / 1000000;
  uint16_t hours = seconds / 3600;
  uint16_t minutes = (seconds % 3600) / 60;
  uint16_t secs = seconds % 60;
  char buffer[12];
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, secs);
  return String(buffer);
}

void pageIna219()
{
  if (!available.ina219) return unavailable("INA219", "0x40", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 2) {
    renderLargeDisplay(2, largeDisplayMode.valueIndex);
    return;
  }
  
  // Display all INA219 parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(ina219_params, INA219_PARAM_COUNT, ina219_getValue);
  footer("Y/A: Change page | INA219");
}

void pageVeml6075()
{
  if (!available.veml6075) return unavailable("VEML6075", "0x10", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 8) {
    renderLargeDisplay(8, largeDisplayMode.valueIndex);
    return;
  }
  
  // Display all VEML6075 parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(veml6075_params, VEML6075_PARAM_COUNT, veml6075_getValue);
  footer("Y/A: Change page | VEML6075");
}

void pageTsl2591()
{
  if (!available.tsl2591) return unavailable("TSL2591", "0x29", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 6) {
    renderLargeDisplay(6, largeDisplayMode.valueIndex);
    return;
  }
  
  // Display all TSL2591 parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(tsl2591_params, TSL2591_PARAM_COUNT, tsl2591_getValue);
  footer("Y/A: Change page | TSL2591");
}

void pageEncoder()
{
  if (!available.encoder) return unavailable("Rotary Encoder", "0x36", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 3) {
    renderLargeDisplay(3, largeDisplayMode.valueIndex);
    return;
  }
  
  // Display all Encoder parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(encoder_params, ENCODER_PARAM_COUNT, encoder_getValue);
  footer("Y/A: Change page | Encoder");
}

void pageVeml6070()
{
  if (!available.veml6070) return unavailable("VEML6070", "0x38/0x39", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 7) {
    renderLargeDisplay(7, largeDisplayMode.valueIndex);
    return;
  }
  
  // Display all VEML6070 parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(veml6070_params, VEML6070_PARAM_COUNT, veml6070_getValue);
  footer("Y/A: Change page | VEML6070");
}

void pageSi1145()
{
  if (!available.si1145) return unavailable("SI1145", "0x60", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 5) {
    renderLargeDisplay(5, largeDisplayMode.valueIndex);
    return;
  }
  
  // Display all SI1145 parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(si1145_params, SI1145_PARAM_COUNT, si1145_getValue);
  footer("Y/A: Change page | SI1145");
}

void pageScd30(bool pageChanged)
{
  if (!available.scd30) return unavailable("SCD30", "0x61", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 4) {
    renderLargeDisplay(4, largeDisplayMode.valueIndex);
    return;
  }

  // BUGFIX: Only redraw header when page changes to prevent screen flicker
  // The header includes fillScreen() which causes visible flicker if done every frame
  if (pageChanged) {
    String ascStatus = scd30_getAscStatus();
    bool ascEnabled = scd30Sensor.getAutoSelfCalibration();
    uint16_t badgeColor = ascEnabled ? TFT_GREEN : TFT_ORANGE;
    drawHeaderWithBadge("SCD30 (0x61)", ascStatus.c_str(), badgeColor);
  }

  // Display all SCD30 parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(scd30_params, SCD30_PARAM_COUNT, scd30_getValue);
  footer("X: Toggle ASC | Y/A: Change page | SCD30");
}

void pageBme280()
{
  if (!available.bme280) return unavailable("BME280", "0x77", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 0) {
    renderLargeDisplay(0, largeDisplayMode.valueIndex);
    return;
  }
  
  // Display all BME280 parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(bme280_params, BME280_PARAM_COUNT, bme280_getValue);
  footer("Y/A: Change page | BME280");
}

String gamepadButtonState(uint8_t pin)
{
  return (gamepadValues.buttons & (1UL << pin)) ? "released" : "PRESSED";
}

String gamepadEventText()
{
  if (!gamepadValues.hasEvent) return "-";
  const uint8_t edge = gamepadValues.lastEvent & 0x03;
  const uint8_t key = gamepadValues.lastEvent >> 2;
  const char *edgeName = edge == SEESAW_KEYPAD_EDGE_FALLING ? "down" :
                         edge == SEESAW_KEYPAD_EDGE_RISING ? "up" : "other";
  return String((int)key) + " " + edgeName;
}

void pageGamepad()
{
  if (!available.gamepad) return unavailable("Gamepad QT", "0x50", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 1) {
    renderLargeDisplay(1, largeDisplayMode.valueIndex);
    return;
  }
  
  // Display all Gamepad parameters using central definition (FEATURE-SPEC-008)
  displaySensorPage(gamepad_params, GAMEPAD_PARAM_COUNT, gamepad_getValue);
  footer("Y/A: Change page | Gamepad QT");
}

void pageDs2484()
{
  if (!available.ds2484) return unavailable("DS2484 1-Wire", "0x18", "I2C address:");
  
  if (largeDisplayMode.active && largeDisplayMode.currentPage == 9) {
    renderLargeDisplay(9, largeDisplayMode.valueIndex);
    return;
  }
  
  // Status line: adapter present and sensor count
  const char *statusMsg = ds2484Values.sensorCount == 0 ? "OK - No Sensors Found" :
                          ds2484Values.sensorCount == 1 ? "OK - 1 Sensor" :
                          ds2484Values.sensorCount == 2 ? "OK - 2 Sensors" :
                          "OK - 3 Sensors";
  // Display values as triples: (Name, Value, Unit) for consistency (FEATURE-SPEC-007)
  rowTriple(56, "Status:", String(statusMsg), "", TFT_GREEN);
  
  // Display up to 3 sensors with their ROM-IDs and temperatures
  int displayY = 90;
  for (uint8_t i = 0; i < ds2484Values.sensorCount && i < 3; i++) {
    const Ds18b20Sensor &sensor = ds2484Values.sensors[i];
    
    // Sensor ID line (ROM-ID as HEX)
    String romId = formatRomId(sensor.address);
    writeText(6, displayY, String("Sensor ") + String(i + 1) + " ID:", TFT_WHITE);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextPadding(320);
    tft.drawRightString(romId, 474, displayY, 2);  // Font 2 for consistency
    displayY += 26;
    
    // Temperature line
    String tempStr;
    uint16_t tempColor;
    if (!sensor.available) {
      tempStr = "Disconnected";
      tempColor = TFT_RED;
    } else {
      tempStr = String(sensor.temperature, 2) + " °C";
      tempColor = TFT_CYAN;
    }
    writeText(6, displayY, "Temperature:", TFT_WHITE);
    tft.setTextColor(tempColor, TFT_BLACK);
    tft.setTextPadding(320);
    tft.drawRightString(tempStr, 474, displayY, 2);  // Font 2 for consistency
    displayY += 42;  // Spacing between sensor blocks
  }
  
  tft.setTextPadding(0);
  footer("Y/A: Change page | DS2484");
}

void drawCurrentPage(bool pageChanged)
{
  if (pageChanged) {
    const char *titles[] = {
      "BME280 (0x77)",
      "Gamepad QT (0x50)",
      "INA219 (0x40)",
      "Rotary Encoder (0x36)",
      "SCD30 (0x61)",
      "SI1145 (0x60)",
      "TSL2591 (0x29)",
      "VEML6070 (0x38/0x39)",
      "VEML6075 (0x10)",
      "DS2484 1-Wire (0x18)"
    };
    // FEATURE-SPEC-009: Draw header for SCD30 with ASC badge only when page changes
    if (currentPage == 4) {
      String ascStatus = scd30_getAscStatus();
      bool ascEnabled = available.scd30 ? scd30Sensor.getAutoSelfCalibration() : false;
      uint16_t badgeColor = available.scd30 ? (ascEnabled ? TFT_GREEN : TFT_ORANGE) : TFT_DARKGREY;
      drawHeaderWithBadge("SCD30 (0x61)", ascStatus.c_str(), badgeColor);
    } else {
      drawHeader(titles[currentPage]);
    }
  }
  
  // PHASE 5: Data-driven page dispatch using PageHandler array (replaces switch statement)
  if (currentPage < PAGE_COUNT) {
    pageHandlers[currentPage].renderPage(pageChanged);
  }
}

/// @brief Scan I2C bus and display results on screen
void scanI2CBus()
{
  logPrintf("[SETUP] ========== I2C BUS SCAN ==========");
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("I2C Bus Scan:");
  tft.println("");
  
  uint8_t count = 0;
  uint8_t addresses[128];
  
  // Scan all 127 possible I2C addresses
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      addresses[count++] = addr;
      logPrintf("[SETUP] Found device at 0x%02X", addr);
      
      // Display on screen (max 14 lines on 320x240 display with text size 2)
      if (count <= 14) {
        tft.printf("0x%02X ", addr);
        if (count % 3 == 0) tft.println("");
      }
    }
  }
  
  tft.setTextSize(1);
  if (count == 0) {
    tft.println("\nNo devices found!");
    logPrintf("[SETUP] No I2C devices found!");
  } else {
    tft.printf("\nTotal: %d device(s)", count);
    logPrintf("[SETUP] Total devices found: %d", count);
  }
  
  // Map found devices to known sensors
  logPrintf("[SETUP] Device identification:");
  tft.println("\nIdentified:");
  
  for (uint8_t i = 0; i < count; i++) {
    uint8_t addr = addresses[i];
    const char* name = "Unknown";
    
    switch (addr) {
      case 0x10: name = "VEML6075 (UV)"; break;
      case 0x18: name = "DS2484 (1-Wire)"; break;
      case 0x29: name = "TSL2591 (Light)"; break;
      case 0x36: name = "Encoder/LED"; break;
      case 0x38: name = "VEML6070 LSB (UV)"; break;
      case 0x39: name = "VEML6070 MSB (UV)"; break;
      case 0x3C: name = "SCD30 (CO2)"; break;
      case 0x40: name = "INA219 (Power)"; break;
      case 0x50: name = "Gamepad"; break;
      case 0x60: name = "SI1145 (Light/UV)"; break;
      case 0x77: name = "BME280 (Env)"; break;
    }
    
    logPrintf("[SETUP]   0x%02X: %s", addr, name);
    tft.printf("0x%02X: %s\n", addr, name);
  }
  
  logPrintf("[SETUP] ========== SCAN COMPLETE ==========");
  delay(3000);  // Show scan results for 3 seconds
}

void detectSensors()
{
  logPrintf("[SETUP] INA219: Probing...");
  available.ina219 = ina219.begin(&Wire);
  logPrintf("[SETUP] INA219: %s", available.ina219 ? "FOUND" : "NOT FOUND");
  
  // FEATURE-SPEC-010: Use SparkFun VEML6075 Library
  logPrintf("[SETUP] VEML6075: Probing...");
  if (veml6075Sensor.begin(Wire) == VEML6075_ERROR_SUCCESS) {
    available.veml6075 = true;
    logPrintf("[SETUP] VEML6075: FOUND, using SparkFun library");
  } else {
    available.veml6075 = false;
    logPrintf("[SETUP] VEML6075: NOT FOUND");
  }
  
  logPrintf("[SETUP] TSL2591: Probing...");
  available.tsl2591 = tsl2591.begin();
  if (available.tsl2591) {
    tsl2591.setGain(TSL2591_GAIN_MED);
    tsl2591.setTiming(TSL2591_INTEGRATIONTIME_200MS);
    logPrintf("[SETUP] TSL2591: FOUND");
  } else {
    logPrintf("[SETUP] TSL2591: NOT FOUND");
  }
  
  logPrintf("[SETUP] Encoder: Probing...");
  available.encoder = encoder.begin(ENCODER_ADDR);
  if (available.encoder) {
    encoder.pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
    encoderLastPosition = encoder.getEncoderPosition();
    encoderLed.begin(ENCODER_ADDR);
    encoderLed.setBrightness(encoderBrightness);
    updateEncoderLed();
    logPrintf("[SETUP] Encoder: FOUND");
  } else {
    logPrintf("[SETUP] Encoder: NOT FOUND");
  }
  
  // FEATURE-SPEC-010: Use Adafruit VEML6070 Library
  logPrintf("[SETUP] VEML6070: Probing...");
  veml6070Sensor.begin(VEML6070_HALF_T, &Wire);
  Wire.beginTransmission(VEML6070_ADDR_LSB);
  available.veml6070 = Wire.endTransmission() == 0;
  logPrintf("[SETUP] VEML6070: %s, using Adafruit library", available.veml6070 ? "FOUND" : "NOT FOUND");
  
  logPrintf("[SETUP] SI1145: Probing...");
  available.si1145 = si1145.begin();
  logPrintf("[SETUP] SI1145: %s", available.si1145 ? "FOUND" : "NOT FOUND");
  
  // FEATURE-SPEC-009: Initialize SCD30 with SparkFun library
  logPrintf("[SETUP] SCD30: Probing...");
  available.scd30 = scd30Sensor.begin();
  logPrintf("[SETUP] SCD30: %s", available.scd30 ? "FOUND" : "NOT FOUND");
  if (available.scd30) {
    scd30Sensor.beginMeasuring();  // Start continuous measurement
    scd30State.lastAscQuery = 0;    // Will be queried by polling loop after 3+ seconds
    scd30State.pendingAscWrite = false;
    logPrintf("[SETUP] SCD30: FOUND, using SparkFun library");
  }
  
  available.bme280 = bme280.begin(BME280_ADDR, &Wire);
  available.gamepad = gamepad.begin(GAMEPAD_ADDR);
  if (available.gamepad) gamepad.pinModeBulk(BUTTON_MASK, INPUT_PULLUP);
  if (available.gamepad) gamepad.setGPIOInterrupts(BUTTON_MASK, true);
  
  // Probe DS2484 I2C-to-1-Wire adapter at 0x18
  logPrintf("[SETUP] Probing DS2484 at 0x18...");
  available.ds2484 = ds2484Master.begin(&Wire, DS2484_ADDR);
  if (available.ds2484) {
    logPrintf("[SETUP] DS2484: FOUND, initializing...");
    initDs2484();  // Initialize DS2484
    ds2484Values.masterPresent = true;
    ds2484Values.lastScanMs = 0;  // Trigger immediate bus scan
    ds2484Values.lastConversionMs = 0;
    
    // HOTFIX 2026-10-22: Skip initial DS2484 bus scan - causes I2C bus hang at boot
    // The scan will be triggered periodically in the main loop (every 60 seconds)
    // See: https://github.com/freenove/freenove-playground/issues/XXX
    logPrintf("[SETUP] DS2484: Initial bus scan deferred to main loop (prevents I2C hang)");
    // performDs2484Scan();  // DISABLED - causes boot hang
  } else {
    logPrintf("[SETUP] DS2484: NOT FOUND at 0x18");
  }
}

// Initialize sensor caches with NaN values for floats (FEATURE-SPEC-005)
// Distinguishes "uninitialized" from "real measurement = 0"
void initializeSensorCaches()
{
  // BME280 cache
  bme280Values.temperature = NAN;
  bme280Values.humidity = NAN;
  bme280Values.pressure = NAN;
  bme280Values.altitude = NAN;
  bme280Values.dewPoint = NAN;
  
  // TSL2591 cache (only float fields get NaN; uint32_t fields initialized to 0)
  tsl2591Values.fullSpectrum = 0;       // uint32_t, initialized to 0
  tsl2591Values.infrared = 0;           // uint32_t, initialized to 0
  tsl2591Values.visible = 0;            // uint32_t, initialized to 0
  tsl2591Values.lux = NAN;              // float, initialized to NaN
  tsl2591Values.gain = 0;               // uint16_t, initialized to 0
  tsl2591Values.integrationTime = 0;    // uint16_t, initialized to 0
  
  // SI1145 cache (only float fields get NaN; uint16_t fields initialized to 0)
  si1145Values.visible = 0;   // uint16_t, initialized to 0
  si1145Values.infrared = 0;  // uint16_t, initialized to 0
  si1145Values.uv = 0;        // uint16_t, initialized to 0
  si1145Values.uvIndex = NAN; // float, initialized to NaN
  
  // DS2484 sensor cache initialization
  for (uint8_t i = 0; i < 3; i++) {
    ds2484Values.sensors[i].temperature = 0.0f;
    ds2484Values.sensors[i].available = false;
    ds2484Values.sensors[i].errorCount = 0;
  }
}

// ============================================================================
// LOGGING INFRASTRUCTURE (FEATURE-SPEC-012)
// ============================================================================

void logPrintf(const char* format, ...)
{
  // Calculate timestamp from millis() using integer-only arithmetic
  uint32_t elapsedMs = millis();
  uint8_t h = (elapsedMs / 3600000) % 24;
  uint8_t m = (elapsedMs / 60000) % 60;
  uint8_t s = (elapsedMs / 1000) % 60;
  uint16_t ms = elapsedMs % 1000;
  
  // Format message with timestamp prefix and the user's message
  char buffer[512];
  va_list args;
  va_start(args, format);
  
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  // Print timestamp + formatted message + newline
  // Skip empty messages (no output for blank lines)
  if (len > 0 && buffer[0] != '\0') {
    ::Serial.printf("[%02d:%02d:%02d.%03d] %s\n", h, m, s, ms, buffer);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);  // Give serial time to init
  
  // Boot banner with version and build date (FEATURE-SPEC-012)
  logPrintf("========================================");
  logPrintf("Sensor Pages Dashboard");
  logPrintf("Version: %s", VERSION);
  logPrintf("Build Date: %s", BUILD_DATE);
  logPrintf("Compiler: arm-none-eabi-g++");
  logPrintf("ESP32-WROOM-32 Detected");
  logPrintf("========================================");
  logPrintf("[BOOT] Starting setup...");
  
  logPrintf("[SETUP] ========== BOOT START ==========");
  
  // Initialize diagnostics FIRST
  initDiagnostics();
  
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  
  // FEATURE-SPEC-010: Set I2C timeout to prevent hangs on missing sensors
  // Default is 20ms, set to 100ms for slower sensors and long distances
  Wire.setTimeOut(100);
  
  tft.init();
  tft.setRotation(3);
  
  // HOTFIX 2026-10-22: Disable I2C bus scan - causes I2C bus hang at boot
  // The bus scan iterates through all 127 I2C addresses and can deadlock
  // if any device is in an error state (e.g., DS2484 after overflow event)
  // logPrintf("[SETUP] Scanning I2C bus...");
  // scanI2CBus();  // DISABLED - causes boot hang
  
  logPrintf("[SETUP] I2C bus scan disabled (boot safety)");
  drawHeader("Sensor Dashboard");
  
  logPrintf("[SETUP] Detecting sensors...");
  detectSensors();
  
  logPrintf("[SETUP] Initializing sensor caches...");
  initializeSensorCaches();
  
  // PHASE-4: Initialize the Sensor Registry with all 8 working sensors
  logPrintf("[SETUP] Initializing Sensor Registry...");
  sensorRegistry.registerSensor(0, &ina219Sensor, "INA219", 0xF800);  // Red
  // sensorRegistry.registerSensor(1, &gamepadSensor, "Gamepad", 0xF81F);  // DISABLED - Magenta
  // sensorRegistry.registerSensor(2, &encoderSensor, "Encoder", 0xFC00);  // DISABLED - Orange
  sensorRegistry.registerSensor(3, &bme280Sensor, "BME280", 0x07E0);  // Green
  sensorRegistry.registerSensor(4, &si1145Sensor, "SI1145", 0xFFE0);  // Yellow
  sensorRegistry.registerSensor(5, &tsl2591Sensor, "TSL2591", 0x07FF);  // Cyan
  sensorRegistry.registerSensor(6, &veml6075Sensor_new, "VEML6075", 0xF81F);  // Magenta
  sensorRegistry.registerSensor(7, &veml6070Sensor_class, "VEML6070", 0xFC00);  // Orange
  sensorRegistry.registerSensor(8, &scd30Sensor_class, "SCD30", 0x0810);  // Blue
  sensorRegistry.registerSensor(9, &ds2484Sensor, "DS2484", 0xFBE0);  // Light Gray
  
  // PHASE-4: Set device pointers for sensors that use global device instances
  veml6070Sensor_class.setDevice(&veml6070Sensor);  // Pass global device instance to wrapper
  
  // Initialize all sensors
  logPrintf("[SETUP] Initializing all sensors via registry...");
  sensorRegistry.initializeAll();
  
  logPrintf("[SETUP] Performing initial sensor reads...");
  readSensors();
  
  // Start initial DS18B20 conversion if sensors were found
  if (available.ds2484 && ds2484Values.sensorCount > 0) {
    logPrintf("[SETUP] Starting initial DS18B20 conversion (%d sensors)...", ds2484Values.sensorCount);
    startDs18b20Conversion();
    
    // Wait for conversion to complete (750ms + buffer)
    logPrintf("[SETUP] Waiting 1000ms for initial DS18B20 conversion...");
    delay(1000);
    
    // Read the results
    logPrintf("[SETUP] Reading initial DS18B20 results...");
    readDs18b20Results();
  }
  
  logPrintf("[SETUP] Drawing initial page...");
  drawCurrentPage(true);
  
  logPrintf("[SETUP] ========== BOOT COMPLETE ==========");
}

void loop()
{
  static uint32_t nextGamepadMs = 0;
  static uint32_t nextEncoderMs = 0;
  static uint32_t nextSensorMs = 0;
  static uint32_t nextDisplayMs = 0;
  static uint32_t nextInaSampleUs = 0;
  uint32_t now = millis();

  // Monitor heap every 10 seconds
  monitorHeap(now);

  // PRIORITY 1: Read gamepad input FIRST (no drawing, just read state)
  if (available.gamepad && (int32_t)(now - nextGamepadMs) >= 0) {
    const uint32_t buttonsBefore = gamepadValues.buttons;
    readGamepadAt40Hz();
    const bool bPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_B));
    const bool xPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_X));
    const bool yPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_Y));
    const bool aPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_A));
    const bool lastB = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_B));
    const bool lastX = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_X));
    const bool lastY = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_Y));
    const bool lastA = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_A));
    
    // Large Display Mode Logic (All Pages) - FEATURE-SPEC-007
    // Button B: Enter mode or cycle forward
    if (bPressed && !lastB) {
      logPrintf("[INPUT] GAMEPAD Button B: PRESSED");
      if (!largeDisplayMode.active) {
        enterLargeDisplayMode(currentPage);
      } else if (largeDisplayMode.active && largeDisplayMode.currentPage == currentPage) {
        // Cycle forward in large display mode
        uint8_t valueCount = getPageValueCount(currentPage);
        largeDisplayMode.valueIndex = (largeDisplayMode.valueIndex + 1) % valueCount;
        largeDisplayMode.needsFullRedraw = true;
      }
    }
    
    // Button X: Toggle ASC (page 4, normal mode) or cycle backward (large display mode)
    if (xPressed && !lastX) {
      logPrintf("[INPUT] GAMEPAD Button X: PRESSED");
      // FEATURE-SPEC-009: Toggle ASC on Page 4 in normal mode
      if (currentPage == 4 && !largeDisplayMode.active && available.scd30) {
        bool currentAsc = scd30Sensor.getAutoSelfCalibration();
        setAsc(!currentAsc);
      }
      // Cycle backward in large display mode
      else if (largeDisplayMode.active && largeDisplayMode.currentPage == currentPage) {
        uint8_t valueCount = getPageValueCount(currentPage);
        largeDisplayMode.valueIndex = (largeDisplayMode.valueIndex - 1 + valueCount) % valueCount;
        largeDisplayMode.needsFullRedraw = true;
      }
    }
    
    // Button Y: Exit mode or navigate to previous page
    if (yPressed && !lastY) {
      logPrintf("[INPUT] GAMEPAD Button Y: PRESSED");
      if (largeDisplayMode.active && largeDisplayMode.currentPage == currentPage) {
        // Exit large display mode
        exitLargeDisplayMode();
      } else {
        // Normal page navigation
        uint8_t oldPage = currentPage;
        currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;
        logPrintf("[MODE] page change: %d → %d (source: GAMEPAD Button Y)", oldPage, currentPage);
      }
    }
    
    // Button A: Inactive in large display mode, or navigate to next page
    if (aPressed && !lastA) {
      logPrintf("[INPUT] GAMEPAD Button A: PRESSED");
      if (!largeDisplayMode.active || largeDisplayMode.currentPage != currentPage) {
        // Normal page navigation
        uint8_t oldPage = currentPage;
        currentPage = (currentPage + 1) % PAGE_COUNT;
        logPrintf("[MODE] page change: %d → %d (source: GAMEPAD Button A)", oldPage, currentPage);
      }
      // else: Button A is inactive in large display mode (do nothing)
    }
    
    nextGamepadMs = now + GAMEPAD_REFRESH_MS;
  }

  // FEATURE-SPEC-011: Encoder Navigation Handler
  // Processes encoder rotation for page navigation (standard mode) or value scrolling (Large Display mode)
  // Processes encoder button press for Large Display mode toggling
  // Note: Encoder continues to poll at 50 Hz via readEncoderAt50Hz() below
  if (available.encoder) {
    // We need to track the last delta-processed position separately to avoid reprocessing
    static int32_t lastEncoderNavigationPosition = 0;
    int32_t currentNavigationDelta = encoderValues.position - lastEncoderNavigationPosition;

    // Standard Mode: Page Navigation via Encoder Rotation
    if (!largeDisplayMode.active) {
      if (currentNavigationDelta >= ENCODER_DELTA_THRESHOLD) {
        // Clockwise rotation: next page
        uint8_t oldPage = currentPage;
        currentPage = (currentPage + 1) % PAGE_COUNT;
        logPrintf("[INPUT] ENCODER Rotation: position %d → %d (delta: +%d, CLOCKWISE)", 
                  lastEncoderNavigationPosition, encoderValues.position, currentNavigationDelta);
        logPrintf("[MODE] page change: %d → %d (source: ENCODER rotation CW)", oldPage, currentPage);
        lastEncoderNavigationPosition = encoderValues.position;
      } else if (currentNavigationDelta <= -ENCODER_DELTA_THRESHOLD) {
        // Counter-clockwise rotation: previous page
        uint8_t oldPage = currentPage;
        currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;
        logPrintf("[INPUT] ENCODER Rotation: position %d → %d (delta: %d, CCW)", 
                  lastEncoderNavigationPosition, encoderValues.position, currentNavigationDelta);
        logPrintf("[MODE] page change: %d → %d (source: ENCODER rotation CCW)", oldPage, currentPage);
        lastEncoderNavigationPosition = encoderValues.position;
      }
    }
    // Large Display Mode: Value Scrolling via Encoder Rotation
    else {
      uint8_t paramCount = getPageValueCount(largeDisplayMode.currentPage);
      if (paramCount > 0) {
        if (currentNavigationDelta >= ENCODER_DELTA_THRESHOLD) {
          // Clockwise rotation: next parameter value
          logPrintf("[INPUT] ENCODER Rotation: position %d → %d (delta: +%d, CLOCKWISE)", 
                    lastEncoderNavigationPosition, encoderValues.position, currentNavigationDelta);
          largeDisplayMode.valueIndex = (largeDisplayMode.valueIndex + 1) % paramCount;
          largeDisplayMode.needsFullRedraw = true;
          lastEncoderNavigationPosition = encoderValues.position;
        } else if (currentNavigationDelta <= -ENCODER_DELTA_THRESHOLD) {
          // Counter-clockwise rotation: previous parameter value
          logPrintf("[INPUT] ENCODER Rotation: position %d → %d (delta: %d, CCW)", 
                    lastEncoderNavigationPosition, encoderValues.position, currentNavigationDelta);
          largeDisplayMode.valueIndex = (largeDisplayMode.valueIndex - 1 + paramCount) % paramCount;
          largeDisplayMode.needsFullRedraw = true;
          lastEncoderNavigationPosition = encoderValues.position;
        }
      }
    }

    // Encoder Button Press: Toggle Large Display Mode (Rising-Edge Debounce)
    bool encoderPressed = encoderValues.pressed == 0;  // active-low
    if (encoderPressed && !encoderLastPressed) {
      // Rising edge: button just pressed
      logPrintf("[INPUT] ENCODER Button: PRESSED");
      if (!largeDisplayMode.active) {
        // Enter Large Display Mode for current page
        enterLargeDisplayMode(currentPage);
        logPrintf("[MODE] Large Display enter (page %d, valueIndex 0)", currentPage);
      } else if (largeDisplayMode.currentPage == currentPage) {
        // Exit Large Display Mode - return to standard view
        exitLargeDisplayMode();
        logPrintf("[MODE] Large Display exit (returning to page %d)", currentPage);
      }
    }
    if (!encoderPressed && encoderLastPressed) {
      // Falling edge: button just released
      logPrintf("[INPUT] ENCODER Button: RELEASED");
    }
    encoderLastPressed = encoderPressed;
  }

  // PRIORITY 2: Read encoder input (always, for stats aggregation)
  if (available.encoder && (int32_t)(now - nextEncoderMs) >= 0) {
    readEncoderAt50Hz();
    // FEATURE-SPEC-011: Update LED based on current mode and page every 50 Hz
    updateEncoderLed();
    nextEncoderMs = now + ENCODER_REFRESH_MS;
  }

  // PRIORITY 3: Read INA219 at independent schedule
  const uint32_t nowUs = micros();
  if (available.ina219 && (int32_t)(nowUs - nextInaSampleUs) >= 0) {
    readIna219At50Hz();
    nextInaSampleUs = nowUs + 20000;
  }

  // PRIORITY 4: Rotated sensor read dispatch (FEATURE-SPEC-006 non-blocking architecture)
  // Every SENSOR_SLOT_INTERVAL_MS (200 ms), dispatch the appropriate sensor(s) for current slot
  // With 10 slots, cycle = 10 × 200ms = 2000ms, giving DS18B20 1600ms between start and check
  static uint32_t lastSensorSlotDispatched = 0xFFFFFFFF;
  uint32_t currentSensorSlot = (now / SENSOR_SLOT_INTERVAL_MS) % SLOT_COUNT;
  if (currentSensorSlot != lastSensorSlotDispatched) {
    // logPrintf("[LOOP] Slot %lu starting", currentSensorSlot);
    rotatedReadSensor(currentSensorSlot);
    lastSensorSlotDispatched = currentSensorSlot;
  }

  // PRIORITY 5: Periodic display refresh with event-driven optimization (FEATURE-SPEC-006)
  // Display shall redraw ONLY on:
  //   1. Page change (immediate)
  //   2. Sensor value change (detected by valuesChanged() or largeDisplayValueChanged())
  //   3. Timer event (INA219 seconds counter increment)
  //   4. Rate limited to max 5 Hz (200 ms minimum between updates) to prevent flickering
  if (currentPage != lastPage) {
    // Page changed: redraw immediately with full refresh
    // Safety: Reset large display mode when navigating to different page (FEATURE-SPEC-007)
    if (largeDisplayMode.active) {
      largeDisplayMode.active = false;
      largeDisplayMode.valueIndex = 0;
    }
    drawCurrentPage(true);
    lastPage = currentPage;
    updateCachedValues();
    nextDisplayMs = now + SENSOR_SLOT_INTERVAL_MS;  // Update timer for next refresh window
    nextLargeDisplayRefreshMs = now + 200;  // Rate limit: max 5 Hz (200 ms between updates)
  } else if ((largeDisplayValueChanged() || valuesChanged() || (int32_t)(now - nextDisplayMs) >= 0) && 
             (int32_t)(now - nextLargeDisplayRefreshMs) >= 0) {
    // Value changed OR timer event AND rate limit allows: partial refresh (data only, no header)
    // largeDisplayValueChanged() checks ONLY the displayed value in large mode
    // valuesChanged() checks all values for standard page display
    // nextLargeDisplayRefreshMs enforces max 5 Hz (200 ms between updates)
    drawCurrentPage(false);
    updateCachedValues();
    nextDisplayMs = now + SENSOR_SLOT_INTERVAL_MS;
    nextLargeDisplayRefreshMs = now + 200;  // Rate limit: max 5 Hz (200 ms between updates)
  }

  // PRIORITY 6: Update statistics (optional, configurable)
  updateInputStats();
  printGamepadStats();
  printEncoderStats();
  printIna219Stats();
}
