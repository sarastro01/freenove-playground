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
#include <math.h>
#include <string.h>

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

constexpr uint8_t PAGE_COUNT = 10;  // Updated: Added DS2484 page
constexpr uint32_t SENSOR_REFRESH_MS = 500;
constexpr uint32_t INPUT_REFRESH_MS = 10;    // 100 Hz for responsive buttons
constexpr uint32_t BUTTON_MASK = (1UL << GAMEPAD_BUTTON_X) | (1UL << GAMEPAD_BUTTON_Y) |
                                  (1UL << GAMEPAD_BUTTON_A) | (1UL << GAMEPAD_BUTTON_B) |
                                  (1UL << GAMEPAD_BUTTON_SELECT) | (1UL << GAMEPAD_BUTTON_START);

// Display Configuration: FNK0114N 3.5" TFT with ILI9341 controller
// Physical Resolution: 320×480 pixels
// Display Mode: LANDSCAPE (rotation=3 via User_Setup.h)
// Effective Coordinate System: x=0-480 (horizontal), y=0-320 (vertical)
// Header: y=0-38, Content Area: y=40-320
TFT_eSPI tft = TFT_eSPI(320, 480);
Adafruit_INA219 ina219(0x40);
Adafruit_TSL2591 tsl2591(2591);
Adafruit_SI1145 si1145;
Adafruit_BME280 bme280;
Adafruit_seesaw gamepad;
Adafruit_seesaw encoder;
seesaw_NeoPixel encoderLed(1, ENCODER_NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800, &Wire);

// DS2484 I2C-to-1-Wire adapter via Adafruit library
Adafruit_DS248x ds2484Master;

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
  bool ds2484;      // DS2484 I2C-to-1-Wire adapter detected
};
SensorStatus available{};

struct Ina219Values { float shunt; float bus; float load; float current; float power; float chargeMah; bool initialized; uint32_t lastSampleUs; uint32_t startTimeUs; };
struct Veml6075Values { uint16_t uva; uint16_t uvb; uint16_t comp1; uint16_t comp2; float uvaComp; float uvbComp; float index; };
struct Veml6070Values { uint16_t raw; uint8_t lsb; uint8_t msb; };
struct Scd30Values { float co2; float temp; float humidity; };
struct EncoderValues { int32_t position; int32_t delta; bool pressed; uint32_t presses; uint8_t brightness; const char *color; };
struct GamepadValues { int x; int y; uint32_t buttons; uint8_t eventCount; uint8_t lastEvent; bool hasEvent; };

// 1-Wire temperature sensor struct (single sensor on DS2484)
struct Ds18b20Sensor {
  uint8_t address[8];      // 8-byte ROM-ID
  uint8_t familyCode;      // First byte of ROM-ID (0x28, 0x22, 0x10, etc.)
  float temperature;       // Latest temperature reading (°C)
  uint32_t lastReadMs;     // Timestamp of last read
  bool available;          // Online/offline status
  uint8_t errorCount;      // Error counter (0-3, then offline)
};

// DS2484 I2C-to-1-Wire adapter struct
struct Ds2484Values {
  bool masterPresent;           // DS2484 adapter detected via I2C probe
  uint8_t sensorCount;          // Number of discovered sensors (0-3)
  Ds18b20Sensor sensors[3];     // Array of up to 3 sensors
  uint32_t lastScanMs;          // Timestamp of last bus scan (60s interval)
  uint32_t lastConversionMs;    // Timestamp of last temperature conversion (3s interval)
};

// BME280 climate sensor cache struct (FEATURE-SPEC-005)
struct Bme280Values {
  float temperature;      // °C
  float humidity;         // %RH
  float pressure;         // hPa
  float altitude;         // m
  float dewPoint;         // °C
};

// TSL2591 light sensor cache struct (FEATURE-SPEC-005)
struct Tsl2591Values {
  uint32_t fullSpectrum;       // Full spectrum raw (16-bit CHAN0)
  uint32_t infrared;           // Infrared raw (16-bit CHAN1)
  uint32_t visible;            // Visible light (calculated)
  float lux;                   // Illuminance in lux
  uint16_t gain;               // Sensor gain setting
  uint16_t integrationTime;    // Integration time in ms
};

// SI1145 multi-spectrum sensor cache struct (FEATURE-SPEC-005)
struct Si1145Values {
  uint16_t visible;       // Visible light raw (16-bit)
  uint16_t infrared;      // Infrared raw (16-bit)
  uint16_t uv;            // UV raw (register 0x2C)
  float uvIndex;          // UV index (calculated: uv / 100.0f)
};

Ina219Values ina219Values{};
Veml6075Values veml6075Values{};
Veml6070Values veml6070Values{};
Scd30Values scd30Values{};
Bme280Values bme280Values{};       // BME280 cache
Tsl2591Values tsl2591Values{};     // TSL2591 cache
Si1145Values si1145Values{};       // SI1145 cache
EncoderValues encoderValues{};
GamepadValues gamepadValues{};
Ds2484Values ds2484Values{};  // DS2484 1-Wire adapter and sensors
uint32_t fwVersion = 0;
uint32_t optionsMask = 0;
float seesawTemperature = 0.0f;
uint8_t currentPage = 2;  // Start with INA219 page (Page 2)
uint8_t lastPage = 255;
bool largeDisplayMode = false;        // INA219 large display mode active
uint8_t largeDisplayIndex = 0;        // Current value index in large display (0-6)
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

// Forward declarations
String formatRuntime(uint32_t elapsedUs);

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

void row(int y, const char *label, const String &value, uint16_t color = TFT_WHITE)
{
  writeText(6, y, label, TFT_WHITE);
  tft.setTextPadding(320);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawRightString(value, 474, y, 2);
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
  footer("Keine Daten verfuegbar");
}

// ==================== LARGE DISPLAY MODE HELPER FUNCTIONS (INA219 Page 2) ====================

const char* getLargeDisplayLabel(uint8_t valueIndex)
{
  static const char* labels[] = {
    "Laufzeit",          // 0 - Runtime
    "Batterie",          // 1 - Bus voltage
    "Versorgung",        // 2 - Shunt voltage
    "Strom",             // 3 - Current
    "Ladung",            // 4 - Charge
    "Shunt-Spannung",    // 5 - Load voltage
    "Power"              // 6 - Power
  };
  return valueIndex < 7 ? labels[valueIndex] : "???";
}

String getLargeDisplayValue(uint8_t valueIndex)
{
  // Must match standard pageIna219() formatting exactly per BR-033
  switch (valueIndex) {
    case 0:  // Runtime (Laufzeit) - use formatRuntime() for HH:MM:SS format
      {
        const uint32_t runtimeUs = micros() - ina219Values.startTimeUs;
        return formatRuntime(runtimeUs);
      }
    case 1:  // Battery/Load voltage (Batterie) - MUST use ina219Values.load!
      return String(ina219Values.load, 3);  // 3 decimals
    case 2:  // Supply/Bus voltage (Versorgung) - MUST use ina219Values.bus!
      return String(ina219Values.bus, 3);  // 3 decimals
    case 3:  // Current (Strom) - 1 decimal to match standard page!
      return String(ina219Values.current, 1);  // 1 decimal (not 3!)
    case 4:  // Charge (Ladung)
      return String(ina219Values.chargeMah, 3);  // 3 decimals
    case 5:  // Load voltage / Shunt voltage (Shunt-Spannung) - MUST use ina219Values.shunt, 2 decimals!
      return String(ina219Values.shunt, 2);  // 2 decimals (not 3!)
    case 6:  // Power (Power) - 0 decimals (cast to uint16_t) to match standard page!
      return String((uint16_t)ina219Values.power);  // 0 decimals (not 3!)
    default:
      return "???";
  }
}

const char* getLargeDisplayUnit(uint8_t valueIndex)
{
  static const char* units[] = {
    "",       // 0 - Runtime (no unit, already formatted as HH:MM:SS)
    "V",      // 1 - Batterie (load voltage)
    "V",      // 2 - Versorgung (bus voltage)
    "mA",     // 3 - Strom (current)
    "mAh",    // 4 - Ladung (charge)
    "mV",     // 5 - Shunt-Spannung (shunt voltage)
    "mW"      // 6 - Power
  };
  return valueIndex < 7 ? units[valueIndex] : "";
}

uint16_t getLargeDisplayColor(uint8_t valueIndex)
{
  switch (valueIndex) {
    case 0:  // Laufzeit - Cyan (computed value)
      return TFT_CYAN;
    case 1:  // Batterie - Green (primary)
      return TFT_GREEN;
    case 2:  // Versorgung - Green (primary)
      return TFT_GREEN;
    case 3:  // Strom - Orange (secondary)
      return TFT_ORANGE;
    case 4:  // Ladung - Cyan (computed value)
      return TFT_CYAN;
    case 5:  // Shunt-Spannung - Green (primary)
      return TFT_GREEN;
    case 6:  // Power - Orange (secondary)
      return TFT_ORANGE;
    default:
      return TFT_WHITE;  // Fallback error color
  }
}

void pageLargeDisplayIna219(uint8_t valueIndex)
{
  // Main renderer for large display mode
  // Display Resolution: 480px HORIZONTAL (x) × 320px VERTICAL (y) in landscape mode
  // Optimized rendering: fillScreen once, then draw all elements in order (top to bottom)
  // This reduces flicker compared to incremental drawing
  
  // Get display components first (before any drawing)
  const char* label = getLargeDisplayLabel(valueIndex);
  String value = getLargeDisplayValue(valueIndex);
  const char* unit = getLargeDisplayUnit(valueIndex);
  uint16_t color = getLargeDisplayColor(valueIndex);
  
  // Single screen clear
  tft.fillScreen(TFT_BLACK);
  
  // Set middle-center alignment for all text (x is center at 240 for 480px width)
  tft.setTextDatum(MC_DATUM);
  
  // Draw header section (Font 4 title + separator line)
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("INA219 Large Display", 240, 18, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawFastHLine(6, 48, 468, TFT_DARKGREY);
  
  // Draw label (Font 4, centered at x=240, y=70) - below header with breathing room
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(label, 240, 70, 4);
  
  // Draw value (Font 8, centered at x=240, y=160, with color per value type)
  // This is the focal point: large and centered in the display
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, 240, 160, 8);
  
  // Draw unit (Font 4, centered at x=240, y=240 - SAME SIZE AS LABEL, good spacing)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(unit, 240, 240, 4);
  
  // Draw footer with navigation instructions (Font 2, centered at x=240, y=295)
  // Must stay within 320px vertical boundary!
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("B:next X:prev Y:exit", 240, 295, 2);
  
  // Reset text datum to default
  tft.setTextDatum(TL_DATUM);
}

// ==================== END LARGE DISPLAY MODE HELPERS ====================

bool readReg16(uint8_t address, uint8_t reg, uint16_t &value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)address, 2) != 2) return false;
  uint8_t lsb = Wire.read();
  uint8_t msb = Wire.read();
  value = ((uint16_t)msb << 8) | lsb;
  return true;
}

bool writeReg16(uint8_t address, uint8_t reg, uint16_t value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value & 0xFF);
  Wire.write(value >> 8);
  return Wire.endTransmission() == 0;
}

uint8_t scd30Crc(const uint8_t *data, size_t length)
{
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

bool scd30Command(uint16_t command)
{
  Wire.beginTransmission(SCD30_ADDR);
  Wire.write(command >> 8);
  Wire.write(command & 0xFF);
  return Wire.endTransmission() == 0;
}

bool scd30Ready()
{
  if (!scd30Command(SCD30_CMD_READY) || Wire.requestFrom(SCD30_ADDR, 3) != 3) return false;
  uint8_t data[3];
  data[0] = (uint8_t)Wire.read();
  data[1] = (uint8_t)Wire.read();
  data[2] = (uint8_t)Wire.read();
  return scd30Crc(data, 2) == data[2] && (((uint16_t)data[0] << 8) | data[1]) == 1;
}

float scd30Float(const uint8_t *data)
{
  uint8_t bytes[4] = {data[3], data[2], data[1], data[0]};
  float value;
  memcpy(&value, bytes, sizeof(value));
  return value;
}

bool readScd30(Scd30Values &values)
{
  if (!scd30Command(SCD30_CMD_READ) || Wire.requestFrom(SCD30_ADDR, 18) != 18) return false;
  uint8_t raw[18];
  for (uint8_t i = 0; i < 18; i++) raw[i] = Wire.read();
  for (uint8_t i = 0; i < 18; i += 3) if (scd30Crc(&raw[i], 2) != raw[i + 2]) return false;
  values.co2 = scd30Float(&raw[0]);
  values.temp = scd30Float(&raw[6]);
  values.humidity = scd30Float(&raw[12]);
  return true;
}

bool readVeml6075(Veml6075Values &values)
{
  if (!readReg16(VEML6075_ADDR, VEML6075_REG_UVA, values.uva) || !readReg16(VEML6075_ADDR, VEML6075_REG_UVB, values.uvb) ||
      !readReg16(VEML6075_ADDR, VEML6075_REG_COMP1, values.comp1) || !readReg16(VEML6075_ADDR, VEML6075_REG_COMP2, values.comp2)) return false;
  values.uvaComp = max(0.0f, (float)values.uva - 2.22f * values.comp1 - 1.33f * values.comp2);
  values.uvbComp = max(0.0f, (float)values.uvb - 2.95f * values.comp1 - 1.74f * values.comp2);
  values.index = ((values.uvaComp * 0.001461f) + (values.uvbComp * 0.002591f)) * 0.5f;
  return true;
}

bool readVeml6070(Veml6070Values &values)
{
  if (Wire.requestFrom(VEML6070_ADDR_LSB, 1) != 1) return false;
  values.lsb = Wire.read();
  if (Wire.requestFrom(VEML6070_ADDR_MSB, 1) != 1) return false;
  values.msb = Wire.read();
  values.raw = ((uint16_t)values.msb << 8) | values.lsb;
  return true;
}

// New sensor read functions (FEATURE-SPEC-005: Unified Sensor Caching Architecture)

void readBme280(Bme280Values &values)
{
  if (!available.bme280) return;
  
  uint32_t startTime = micros();
  
  // Read sensor values
  float temperature = bme280.readTemperature();
  float humidity = bme280.readHumidity();
  float pressure = bme280.readPressure() / 100.0f;  // Pa → hPa
  float altitude = bme280.readAltitude(1013.25f);   // m, using 1013.25 hPa reference
  
  // Plausibility check: Verify values in valid ranges
  // Valid ranges: T[-40,85]°C, RH[0,100]%, P[300,1100]hPa
  if (temperature < -40 || temperature > 85 ||
      humidity < 0 || humidity > 100 ||
      pressure < 300 || pressure > 1100) {
    Serial.printf("ERROR: BME280 read returned invalid values: T=%.1f, RH=%.1f, P=%.1f\n",
                  temperature, humidity, pressure);
    // Do NOT update cache; last known values persist
    return;
  }
  
  // Calculate dew point using Alduchov & Eskridge formula (±0.35°C accuracy)
  // Valid for 0-50°C, 1-100%RH
  const float a = 17.62f;
  const float b = 243.12f;
  float alpha = ((a * temperature) / (b + temperature)) + log(humidity / 100.0f);
  float dewPoint = (b * alpha) / (a - alpha);
  
  // Update cache (only on successful read)
  values.temperature = temperature;
  values.humidity = humidity;
  values.pressure = pressure;
  values.altitude = altitude;
  values.dewPoint = dewPoint;
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("BME280 read time: %lu µs\n", duration);
}

void readTsl2591(Tsl2591Values &values)
{
  if (!available.tsl2591) return;
  
  uint32_t startTime = micros();
  
  // API: getFullLuminosity() returns uint32_t with [IR(16-31) | FullSpec(0-15)]
  // Lower 16 bits: CHAN0 (fullSpectrum = visible + infrared)
  // Upper 16 bits: CHAN1 (infrared only)
  uint32_t fullLum = tsl2591.getFullLuminosity();
  uint32_t fullSpectrum = fullLum & 0xFFFF;        // Lower 16 bits (CHAN0)
  uint32_t infrared = fullLum >> 16;               // Upper 16 bits (CHAN1)
  uint32_t visible = (fullSpectrum > infrared) ? (fullSpectrum - infrared) : 0;
  float lux = tsl2591.calculateLux(fullSpectrum, infrared);
  uint16_t gain = tsl2591.getGain();
  uint16_t integrationTime = tsl2591.getTiming();
  
  // Error check: If both channels read as zero, likely I2C error
  if (fullSpectrum == 0 && infrared == 0) {
    Serial.println("ERROR: TSL2591 read returned zero values");
    // Do NOT update cache; last known values persist
    return;
  }
  
  // Update cache (only on successful read)
  values.fullSpectrum = fullSpectrum;
  values.infrared = infrared;
  values.visible = visible;
  values.lux = lux;
  values.gain = gain;
  values.integrationTime = integrationTime;
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("TSL2591 read time: %lu µs\n", duration);
}

void readSi1145(Si1145Values &values)
{
  if (!available.si1145) return;
  
  uint32_t startTime = micros();
  
  // Read sensor values using Adafruit_SI1145 API
  uint16_t visible = si1145.readVisible();
  uint16_t infrared = si1145.readIR();
  uint16_t uv = si1145.readUV();  // Raw UV from register 0x2C
  
  // Error check: If all three values are 0, likely I2C error
  if (visible == 0 && infrared == 0 && uv == 0) {
    Serial.println("ERROR: SI1145 read returned all zeros");
    // Do NOT update cache; last known values persist
    return;
  }
  
  // Calculate UV index (standard Adafruit formula: raw / 100.0)
  float uvIndex = uv / 100.0f;
  
  // Update cache (only on successful read)
  values.visible = visible;
  values.infrared = infrared;
  values.uv = uv;
  values.uvIndex = uvIndex;
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("SI1145 read time: %lu µs\n", duration);
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
    Serial.println("DS2484: Initialized at 0x18");
  } else {
    Serial.println("DS2484: Initialization FAILED");
  }
  return success;
}

// DS2484 1-Wire operations via Adafruit library

bool ds2484ResetBus()
{
  // DS2484 reset bus via Adafruit library
  return ds2484Master.reset();
}

void performDs2484Scan()
{
  if (!available.ds2484) return;
  
  const uint32_t nowMs = millis();
  
  // Bus scan at 60-second interval (or on first scan when sensorCount == 0)
  bool firstScan = ds2484Values.sensorCount == 0;
  if (!firstScan && (int32_t)(nowMs - ds2484Values.lastScanMs) < 60000) return;
  
  Serial.println("DS2484: Scanning 1-Wire bus...");
  
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
    
    Serial.printf("DS2484: Found sensor %d: %s\n", ds2484Values.sensorCount, formatRomId(address).c_str());
    ds2484Values.sensorCount++;
  }
  
  Serial.printf("DS2484: Bus scan complete - found %d sensors\n", ds2484Values.sensorCount);
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
    
    Serial.printf("DS2484: Sensor %d temp = %.2f°C\n", i, sensor.temperature);
  }
  
  ds2484Values.lastConversionMs = nowMs;
}

void updateEncoderLed()
{
  if (!available.encoder) return;
  encoderLed.setBrightness(encoderBrightness);
  encoderLed.setPixelColor(0, encoderColors[encoderColorIndex]);
  encoderLed.show();
}

void readEncoderAt50Hz()
{
  if (!available.encoder) return;

  uint32_t startTime = micros();
  int32_t position = encoder.getEncoderPosition();
  encoderValues.delta = position - encoderLastPosition;
  encoderLastPosition = position;
  encoderValues.position = position;
  encoderValues.pressed = encoder.digitalRead(ENCODER_BUTTON_PIN) == LOW;
  if (encoderValues.pressed && !encoderLastPressed) {
    encoderPressCount++;
    encoderColorIndex = (encoderColorIndex + 1) % 4;
    updateEncoderLed();
  }
  if (encoderValues.delta != 0) {
    const int32_t nextBrightness = (int32_t)encoderBrightness + encoderValues.delta * 8;
    encoderBrightness = (uint8_t)constrain(nextBrightness, 0, 255);
    updateEncoderLed();
  }
  if (encoderValues.pressed && !encoderPressedSince) encoderPressedSince = millis();
  if (!encoderValues.pressed) encoderPressedSince = 0;
  encoderLastPressed = encoderValues.pressed;
  encoderValues.presses = encoderPressCount;
  encoderValues.brightness = encoderBrightness;
  encoderValues.color = encoderColorNames[encoderColorIndex];
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("Encoder read time: %lu µs\n", duration);
}

void readSensors()
{
  if (available.bme280) readBme280(bme280Values);       // New: BME280 caching (FEATURE-SPEC-005)
  if (available.tsl2591) readTsl2591(tsl2591Values);    // New: TSL2591 caching (FEATURE-SPEC-005)
  if (available.si1145) readSi1145(si1145Values);       // New: SI1145 caching (FEATURE-SPEC-005)
  if (available.veml6075) readVeml6075(veml6075Values);
  if (available.veml6070) readVeml6070(veml6070Values);
  if (available.scd30 && scd30Ready()) readScd30(scd30Values);
  if (available.ds2484) {
    performDs2484Scan();      // 60-second bus scan
    readDs2484Temperatures(); // 3-second temperature polling
  }
}

void readGamepadAt40Hz()
{
  if (!available.gamepad) return;

  gamepadValues.x = 1023 - gamepad.analogRead(14);
  gamepadValues.y = 1023 - gamepad.analogRead(15);
  gamepadValues.buttons = gamepad.digitalReadBulk(BUTTON_MASK);
  gamepadValues.hasEvent = false;
  
  // Skip getKeypadCount() as it causes I2C blocking with other sensors
  // Button states are read directly via digitalReadBulk() above
}


void readIna219At50Hz()
{
  if (!available.ina219) return;

  const uint32_t nowUs = micros();
  if (ina219Values.initialized) {
    const uint32_t elapsedUs = nowUs - ina219Values.lastSampleUs;
    if (elapsedUs < 200000) {
      ina219Values.chargeMah += ina219Values.current * (float)elapsedUs / 3600000000.0f;
    }
  } else {
    // Store start time when first initialized
    ina219Values.startTimeUs = nowUs;
    ina219Values.initialized = true;
  }

  ina219Values.lastSampleUs = nowUs;
  uint32_t readStartUs = micros();
  ina219Values.shunt = -ina219.getShuntVoltage_mV();
  ina219Values.bus = ina219.getBusVoltage_V();
  ina219Values.current = -ina219.getCurrent_mA();
  ina219Values.power = ina219.getPower_mW();
  uint32_t readEndUs = micros();
  uint32_t readDuration = readEndUs - readStartUs;
  ina219Values.load = ina219Values.bus - ina219Values.shunt / 1000.0f;
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
  
  // Check if in large display mode
  if (largeDisplayMode) {
    pageLargeDisplayIna219(largeDisplayIndex);
    return;
  }
  
  // Standard page layout (existing code)
  // Calculate runtime since measurement started
  const uint32_t runtimeUs = micros() - ina219Values.startTimeUs;
  String runtimeStr = formatRuntime(runtimeUs);
  
  // Determine charging status based on current polarity
  const char *statusLabel = ina219Values.current > 0 ? "Charging" : "Discharging";
  
  // Display all values
  row(56, "Laufzeit:", runtimeStr, TFT_CYAN);
  row(80, "Batterie:", String(ina219Values.load, 3) + " V", TFT_GREEN);
  row(104, "Versorgung:", String(ina219Values.bus, 3) + " V", TFT_GREEN);
  row(128, "Strom:", String(ina219Values.current, 1) + " mA [" + statusLabel + "]", TFT_ORANGE);
  row(152, "Ladung:", String(ina219Values.chargeMah, 3) + " mAh", TFT_CYAN);
  row(176, "Shunt-Spannung:", String(ina219Values.shunt, 2) + " mV", TFT_GREEN);
  row(200, "Power:", String((uint16_t)ina219Values.power) + " mW", TFT_ORANGE);
  
  footer("Y/A: Seite wechseln | INA219");
}

void pageVeml6075()
{
  if (!available.veml6075) return unavailable("VEML6075", "0x10", "I2C address:");
  row(56, "UVA raw:", String(veml6075Values.uva), TFT_GREEN);
  row(80, "UVB raw:", String(veml6075Values.uvb), TFT_GREEN);
  row(104, "UVCOMP1/2:", String(veml6075Values.comp1) + " / " + String(veml6075Values.comp2), TFT_ORANGE);
  row(128, "UVA compensated:", String(veml6075Values.uvaComp, 1), TFT_CYAN);
  row(152, "UVB compensated:", String(veml6075Values.uvbComp, 1), TFT_CYAN);
  row(176, "UV index:", String(veml6075Values.index, 2), TFT_CYAN);
  footer("Y/A: Seite wechseln | VEML6075");
}

void pageTsl2591()
{
  if (!available.tsl2591) return unavailable("TSL2591", "0x29", "I2C address:");
  // Read from cache (no direct I2C), updated every 500ms in readSensors() (FEATURE-SPEC-005)
  row(56, "Full spectrum:", String(tsl2591Values.fullSpectrum), TFT_GREEN);
  row(80, "Infrared:", String(tsl2591Values.infrared), TFT_GREEN);
  row(104, "Visible:", String(tsl2591Values.visible), TFT_ORANGE);
  row(128, "Lux:", String(tsl2591Values.lux, 2), TFT_CYAN);
  footer("Y/A: Seite wechseln | TSL2591");
}

void pageEncoder()
{
  if (!available.encoder) return unavailable("Rotary Encoder", "0x36", "I2C address:");
  row(56, "Position:", String((long)encoderValues.position), TFT_GREEN);
  row(80, "Delta:", String((long)encoderValues.delta), TFT_GREEN);
  row(104, "Button:", encoderValues.pressed ? "PRESSED" : "released", TFT_ORANGE);
  row(128, "Presses:", String((unsigned long)encoderValues.presses), TFT_ORANGE);
  row(152, "LED color:", encoderValues.color, TFT_CYAN);
  row(176, "LED brightness:", String((int)encoderValues.brightness) + " / 255", TFT_CYAN);
  row(200, "Seesaw temp:", String(seesawTemperature, 1) + " C", TFT_ORANGE);
  footer("Y/A: Seite wechseln | Encoder");
}

void pageVeml6070()
{
  if (!available.veml6070) return unavailable("VEML6070", "0x38/0x39", "I2C address:");
  const char *level = veml6070Values.raw < 200 ? "Very low" : veml6070Values.raw < 500 ? "Low" : veml6070Values.raw < 1000 ? "Moderate" : veml6070Values.raw < 2000 ? "High" : "Very high";
  row(56, "UVA raw:", String(veml6070Values.raw), TFT_GREEN);
  row(80, "LSB / MSB:", String("0x") + String(veml6070Values.lsb, HEX) + " / 0x" + String(veml6070Values.msb, HEX), TFT_ORANGE);
  row(104, "UV level:", level, TFT_CYAN);
  footer("Y/A: Seite wechseln | VEML6070");
}

void pageSi1145()
{
  if (!available.si1145) return unavailable("SI1145", "0x60", "I2C address:");
  // Read from cache (no direct I2C), updated every 500ms in readSensors() (FEATURE-SPEC-005)
  row(56, "Visible:", String(si1145Values.visible), TFT_GREEN);
  row(80, "Infrared:", String(si1145Values.infrared), TFT_GREEN);
  row(104, "UV raw:", String(si1145Values.uv), TFT_ORANGE);
  row(128, "UV index:", String(si1145Values.uvIndex, 2), TFT_CYAN);
  footer("Y/A: Seite wechseln | SI1145");
}

void pageScd30()
{
  if (!available.scd30) return unavailable("SCD30", "0x61", "I2C address:");
  row(56, "CO2 ppm:", String(scd30Values.co2, 0), TFT_GREEN);
  row(80, "Temperature:", String(scd30Values.temp, 2) + " C", TFT_GREEN);
  row(104, "Humidity:", String(scd30Values.humidity, 2) + " %", TFT_ORANGE);
  footer("Y/A: Seite wechseln | SCD30");
}

void pageBme280()
{
  if (!available.bme280) return unavailable("BME280", "0x77", "I2C address:");
  // Read from cache (no direct I2C), updated every 500ms in readSensors() (FEATURE-SPEC-005)
  row(56, "Temperature:", String(bme280Values.temperature, 2) + " C", TFT_GREEN);
  row(80, "Humidity:", String(bme280Values.humidity, 2) + " %", TFT_GREEN);
  row(104, "Pressure:", String(bme280Values.pressure, 2) + " hPa", TFT_ORANGE);
  row(128, "Altitude:", String(bme280Values.altitude, 1) + " m", TFT_CYAN);
  row(152, "Dew point:", String(bme280Values.dewPoint, 2) + " C", TFT_CYAN);
  footer("Y/A: Seite wechseln | BME280");
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
  row(56, "Joystick X:", String(gamepadValues.x), TFT_GREEN);
  row(80, "Joystick Y:", String(gamepadValues.y), TFT_GREEN);
  row(104, "Button X:", gamepadButtonState(GAMEPAD_BUTTON_X), TFT_ORANGE);
  row(128, "Button Y:", gamepadButtonState(GAMEPAD_BUTTON_Y), TFT_ORANGE);
  row(152, "Button A:", gamepadButtonState(GAMEPAD_BUTTON_A), TFT_ORANGE);
  row(176, "Button B:", gamepadButtonState(GAMEPAD_BUTTON_B), TFT_ORANGE);
  row(200, "Select / Start:", gamepadButtonState(GAMEPAD_BUTTON_SELECT) + " / " + gamepadButtonState(GAMEPAD_BUTTON_START), TFT_ORANGE);
  row(224, "Button mask:", String("0x") + String((unsigned long)gamepadValues.buttons, HEX), TFT_CYAN);
  row(248, "Last event:", gamepadEventText(), TFT_CYAN);
  footer("Y/A: Seite wechseln | Gamepad QT");
}

void pageDs2484()
{
  if (!available.ds2484) return unavailable("DS2484 1-Wire", "0x18", "I2C address:");
  
  // Status line: adapter present and sensor count
  const char *statusMsg = ds2484Values.sensorCount == 0 ? "OK - No Sensors Found" :
                          ds2484Values.sensorCount == 1 ? "OK - 1 Sensor" :
                          ds2484Values.sensorCount == 2 ? "OK - 2 Sensors" :
                          "OK - 3 Sensors";
  row(56, "Status:", statusMsg, TFT_GREEN);
  
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
  footer("Y/A: Seite wechseln | DS2484");
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
    drawHeader(titles[currentPage]);
  }
  switch (currentPage) {
    case 0: pageBme280(); break;
    case 1: pageGamepad(); break;
    case 2: pageIna219(); break;
    case 3: pageEncoder(); break;
    case 4: pageScd30(); break;
    case 5: pageSi1145(); break;
    case 6: pageTsl2591(); break;
    case 7: pageVeml6070(); break;
    case 8: pageVeml6075(); break;
    case 9: pageDs2484(); break;
  }
}

void detectSensors()
{
  uint16_t veml6075DeviceId = 0;
  available.ina219 = ina219.begin(&Wire);
  available.veml6075 = readReg16(VEML6075_ADDR, VEML6075_REG_ID, veml6075DeviceId);
  if (available.veml6075) writeReg16(VEML6075_ADDR, VEML6075_REG_CONF, 0x0000);
  available.tsl2591 = tsl2591.begin();
  if (available.tsl2591) {
    tsl2591.setGain(TSL2591_GAIN_MED);
    tsl2591.setTiming(TSL2591_INTEGRATIONTIME_200MS);
  }
  available.encoder = encoder.begin(ENCODER_ADDR);
  if (available.encoder) {
    encoder.pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
    encoderLastPosition = encoder.getEncoderPosition();
    encoderLed.begin(ENCODER_ADDR);
    encoderLed.setBrightness(encoderBrightness);
    updateEncoderLed();
  }
  Wire.beginTransmission(VEML6070_ADDR_LSB);
  Wire.write(0x02);
  available.veml6070 = Wire.endTransmission() == 0 && Wire.requestFrom(VEML6070_ADDR_MSB, 1) == 1;
  available.si1145 = si1145.begin();
  available.scd30 = scd30Command(SCD30_CMD_START);
  available.bme280 = bme280.begin(BME280_ADDR, &Wire);
  available.gamepad = gamepad.begin(GAMEPAD_ADDR);
  if (available.gamepad) gamepad.pinModeBulk(BUTTON_MASK, INPUT_PULLUP);
  if (available.gamepad) gamepad.setGPIOInterrupts(BUTTON_MASK, true);
  
  // Probe DS2484 I2C-to-1-Wire adapter at 0x18
  Serial.println("Probing DS2484 at 0x18...");
  available.ds2484 = ds2484Master.begin(&Wire, DS2484_ADDR);
  if (available.ds2484) {
    Serial.println("DS2484: FOUND");
    initDs2484();  // Initialize DS2484
    ds2484Values.masterPresent = true;
    ds2484Values.lastScanMs = 0;  // Trigger immediate bus scan
    ds2484Values.lastConversionMs = 0;
  } else {
    Serial.println("DS2484: NOT FOUND");
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
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  tft.init();
  tft.setRotation(3);
  drawHeader("Sensor Dashboard");
  detectSensors();
  initializeSensorCaches();  // Initialize sensor caches with NaN (FEATURE-SPEC-005)
  if (!available.gamepad) Serial.println("WARNING: Gamepad QT not found at 0x50");
  readSensors();
  drawCurrentPage(true);
}

void loop()
{
  static uint32_t nextGamepadMs = 0;
  static uint32_t nextEncoderMs = 0;
  static uint32_t nextSensorMs = 0;
  static uint32_t nextDisplayMs = 0;
  static uint32_t nextInaSampleUs = 0;
  uint32_t now = millis();

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
    
    // Large Display Mode Logic (INA219 Page 2)
    if (currentPage == 2) {  // INA219 page
      // Button B: Enter mode or cycle forward
      if (bPressed && !lastB) {
        if (!largeDisplayMode) {
          // Enter large display mode
          largeDisplayMode = true;
          largeDisplayIndex = 0;
        } else {
          // Cycle forward in large display mode
          largeDisplayIndex = (largeDisplayIndex + 1) % 7;
        }
      }
      
      // Button X: Cycle backward (only in large display mode)
      if (xPressed && !lastX && largeDisplayMode) {
        largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7;
      }
      
      // Button Y: Exit mode or navigate to previous page
      if (yPressed && !lastY) {
        if (largeDisplayMode) {
          // Exit large display mode with immediate clean rendering
          largeDisplayMode = false;
          largeDisplayIndex = 0;
          // Clear screen and render standard page for clean transition
          drawHeader("INA219 (0x40)");  // fillScreen(TFT_BLACK) + header
          pageIna219();  // Render standard page data
        } else {
          // Normal page navigation
          currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;
        }
      }
      
      // Button A: Inactive in large display mode, or navigate to next page
      if (aPressed && !lastA) {
        if (!largeDisplayMode) {
          // Normal page navigation
          currentPage = (currentPage + 1) % PAGE_COUNT;
        }
        // else: Button A is inactive in large display mode (do nothing)
      }
    } else {
      // Normal page navigation for all other pages
      if (yPressed && !lastY) currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;
      if (aPressed && !lastA) currentPage = (currentPage + 1) % PAGE_COUNT;
    }
    
    nextGamepadMs = now + INPUT_REFRESH_MS;
  }

  // PRIORITY 2: IMMEDIATE draw if page changed (no delay!)
  if (currentPage != lastPage) {
    // Safety: Reset large display mode when leaving Page 2
    if (lastPage == 2 && currentPage != 2) {
      largeDisplayMode = false;
      largeDisplayIndex = 0;
    }
    drawCurrentPage(true);
    lastPage = currentPage;
    nextDisplayMs = now + SENSOR_REFRESH_MS;
  }

  // PRIORITY 3: Read encoder input (if on encoder page)
  if (available.encoder && currentPage == 3 && (int32_t)(now - nextEncoderMs) >= 0) {
    readEncoderAt50Hz();
    nextEncoderMs = now + INPUT_REFRESH_MS;
  }

  // PRIORITY 4: Read INA219 at independent schedule
  const uint32_t nowUs = micros();
  if (available.ina219 && (int32_t)(nowUs - nextInaSampleUs) >= 0) {
    readIna219At50Hz();
    nextInaSampleUs = nowUs + 20000;
  }

  // PRIORITY 5: Read other I2C sensors
  if ((int32_t)(now - nextSensorMs) >= 0) {
    readSensors();
    nextSensorMs = now + SENSOR_REFRESH_MS;
  }

  // PRIORITY 6: Periodic display refresh (only if page hasn't changed)
  if ((int32_t)(now - nextDisplayMs) >= 0 && currentPage == lastPage) {
    drawCurrentPage(false);
    nextDisplayMs = now + SENSOR_REFRESH_MS;
  }
}
