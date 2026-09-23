#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_INA219.h>
#include <Adafruit_TSL2591.h>
#include <Adafruit_SI1145.h>
#include <Adafruit_BME280.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>
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

#define VEML6075_REG_CONF 0x00
#define VEML6075_REG_UVA 0x07
#define VEML6075_REG_UVB 0x09
#define VEML6075_REG_COMP1 0x0A
#define VEML6075_REG_COMP2 0x0B
#define VEML6075_REG_ID 0x0C
#define SCD30_CMD_START 0x0010
#define SCD30_CMD_READY 0x0202
#define SCD30_CMD_READ 0x0300

constexpr uint8_t PAGE_COUNT = 9;
constexpr uint32_t SENSOR_REFRESH_MS = 500;
constexpr uint32_t INPUT_REFRESH_MS = 20;
constexpr uint32_t BUTTON_MASK = (1UL << GAMEPAD_BUTTON_X) | (1UL << GAMEPAD_BUTTON_Y) |
                                  (1UL << GAMEPAD_BUTTON_A) | (1UL << GAMEPAD_BUTTON_B) |
                                  (1UL << GAMEPAD_BUTTON_SELECT) | (1UL << GAMEPAD_BUTTON_START);

TFT_eSPI tft = TFT_eSPI(320, 480);
Adafruit_INA219 ina219(0x40);
Adafruit_TSL2591 tsl2591(2591);
Adafruit_SI1145 si1145;
Adafruit_BME280 bme280;
Adafruit_seesaw gamepad;
Adafruit_seesaw encoder;
seesaw_NeoPixel encoderLed(1, ENCODER_NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800, &Wire);

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
};
SensorStatus available{};

struct Ina219Values { float shunt; float bus; float load; float current; float power; float chargeMah; bool initialized; uint32_t lastSampleUs; uint32_t startTimeUs; };
struct Veml6075Values { uint16_t uva; uint16_t uvb; uint16_t comp1; uint16_t comp2; float uvaComp; float uvbComp; float index; };
struct Veml6070Values { uint16_t raw; uint8_t lsb; uint8_t msb; };
struct Scd30Values { float co2; float temp; float humidity; };
struct EncoderValues { int32_t position; int32_t delta; bool pressed; uint32_t presses; uint8_t brightness; const char *color; };
struct GamepadValues { int x; int y; uint32_t buttons; uint8_t eventCount; uint8_t lastEvent; bool hasEvent; };

Ina219Values ina219Values{};
Veml6075Values veml6075Values{};
Veml6070Values veml6070Values{};
Scd30Values scd30Values{};
EncoderValues encoderValues{};
GamepadValues gamepadValues{};
uint32_t fwVersion = 0;
uint32_t optionsMask = 0;
float seesawTemperature = 0.0f;
uint8_t currentPage = 2;  // Start with INA219 page (Page 2)
uint8_t lastPage = 255;
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
}

void readSensors()
{
  if (available.veml6075) readVeml6075(veml6075Values);
  if (available.veml6070) readVeml6070(veml6070Values);
  if (available.scd30 && scd30Ready()) readScd30(scd30Values);
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
  ina219Values.shunt = -ina219.getShuntVoltage_mV();
  ina219Values.bus = ina219.getBusVoltage_V();
  ina219Values.current = -ina219.getCurrent_mA();
  ina219Values.power = ina219.getPower_mW();
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
  row(152, "Ladung:", String(ina219Values.chargeMah, 3) + " mAh", TFT_MAGENTA);
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
  row(176, "UV index:", String(veml6075Values.index, 2), TFT_MAGENTA);
  footer("Y/A: Seite wechseln | VEML6075");
}

void pageTsl2591()
{
  if (!available.tsl2591) return unavailable("TSL2591", "0x29", "I2C address:");
  uint32_t luminosity = tsl2591.getFullLuminosity();
  uint16_t ir = luminosity >> 16;
  uint16_t full = luminosity & 0xFFFF;
  int32_t visible = max(0L, (long)full - (long)ir);
  float lux = tsl2591.calculateLux(full, ir);
  row(56, "Full spectrum:", String(full), TFT_GREEN);
  row(80, "Infrared:", String(ir), TFT_GREEN);
  row(104, "Visible:", String((long)visible), TFT_ORANGE);
  row(128, "Lux:", String(lux, 2), TFT_CYAN);
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
  uint32_t visible = si1145.readVisible();
  uint32_t infrared = si1145.readIR();
  uint16_t uv = si1145.readUV();
  row(56, "Visible:", String((unsigned long)visible), TFT_GREEN);
  row(80, "Infrared:", String((unsigned long)infrared), TFT_GREEN);
  row(104, "UV raw:", String(uv), TFT_ORANGE);
  row(128, "UV index:", String(uv / 100.0f, 2), TFT_CYAN);
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
  float temp = bme280.readTemperature();
  float humidity = bme280.readHumidity();
  float pressure = bme280.readPressure() / 100.0f;
  float altitude = bme280.readAltitude(1013.25f);
  float dewPoint = temp - ((100.0f - humidity) / 5.0f);
  row(56, "Temperature:", String(temp, 2) + " C", TFT_GREEN);
  row(80, "Humidity:", String(humidity, 2) + " %", TFT_GREEN);
  row(104, "Pressure:", String(pressure, 2) + " hPa", TFT_ORANGE);
  row(128, "Altitude:", String(altitude, 1) + " m", TFT_CYAN);
  row(152, "Dew point:", String(dewPoint, 2) + " C", TFT_CYAN);
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
  row(224, "Button mask:", String("0x") + String((unsigned long)gamepadValues.buttons, HEX), TFT_MAGENTA);
  row(248, "Last event:", gamepadEventText(), TFT_MAGENTA);
  footer("Y/A: Seite wechseln | Gamepad QT");
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
      "VEML6075 (0x10)"
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
    const bool yPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_Y));
    const bool aPressed = !(gamepadValues.buttons & (1UL << GAMEPAD_BUTTON_A));
    const bool lastY = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_Y));
    const bool lastA = !(buttonsBefore & (1UL << GAMEPAD_BUTTON_A));
    if (yPressed && !lastY) currentPage = (currentPage + PAGE_COUNT - 1) % PAGE_COUNT;
    if (aPressed && !lastA) currentPage = (currentPage + 1) % PAGE_COUNT;
    nextGamepadMs = now + INPUT_REFRESH_MS;
  }

  // PRIORITY 2: IMMEDIATE draw if page changed (no delay!)
  if (currentPage != lastPage) {
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
