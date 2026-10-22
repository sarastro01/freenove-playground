/**
 * @file Bme280Sensor.cpp
 * @brief Implementation of Bme280Sensor class
 */

#include "Bme280Sensor.h"

Bme280Sensor::Bme280Sensor() {
  // Initialize values to zero
  values.temperature = 0.0f;
  values.humidity = 0.0f;
  values.pressure = 0.0f;
  values.altitude = 0.0f;
  values.dewPoint = 0.0f;
  values.initialized = false;
  
  // Initialize change detection
  lastDisplayValue = "";
}

void Bme280Sensor::initialize() {
  // Attempt to detect device at I2C address 0x77 (primary)
  available = device.begin(BME280_ADDR, &Wire);
  
  if (available) {
    Serial.printf("[BME280] Sensor detected at 0x77\n");
  } else {
    Serial.printf("[BME280] Sensor NOT detected at 0x77\n");
  }
}

void Bme280Sensor::update() {
  if (!available) return;

  // Mark as initialized on first read
  if (!values.initialized) {
    values.initialized = true;
  }

  // Read sensor values
  float temperature = device.readTemperature();
  float humidity = device.readHumidity();
  float pressure = device.readPressure() / 100.0f;  // Pa → hPa
  float altitude = device.readAltitude(1013.25f);   // m, using 1013.25 hPa reference

  // Plausibility check: Verify values in valid ranges
  // Valid ranges: T[-40,85]°C, RH[0,100]%, P[300,1100]hPa
  if (temperature < -40 || temperature > 85 ||
      humidity < 0 || humidity > 100 ||
      pressure < 300 || pressure > 1100) {
    // Do NOT update cache; last known values persist
    return;
  }

  // Calculate dew point
  float dewPoint = calculateDewPoint(temperature, humidity);

  // Update cache (only on successful read)
  values.temperature = temperature;
  values.humidity = humidity;
  values.pressure = pressure;
  values.altitude = altitude;
  values.dewPoint = dewPoint;
}

String Bme280Sensor::getValue(uint8_t parameterIndex) const {
  if (!available) return "";

  // Map parameter index to cached value
  switch (parameterIndex) {
    case 0: return String(values.temperature, 2);  // Temperature (°C)
    case 1: return String(values.humidity, 1);     // Humidity (%RH)
    case 2: return String(values.pressure, 1);     // Pressure (hPa)
    case 3: return String(values.altitude, 1);     // Altitude (m)
    case 4: return String(values.dewPoint, 2);     // Dew Point (°C)
    default: return "";
  }
}

String Bme280Sensor::getLargeDisplayValue(uint8_t valueIndex,
                                         const char** outLabel,
                                         const char** outUnit,
                                         uint16_t* outColor) const {
  if (!available) {
    *outLabel = "Error";
    *outUnit = "";
    *outColor = 0xF800;  // Red (RGB565)
    return "N/A";
  }

  if (valueIndex >= getParameterCount()) {
    *outLabel = "Invalid";
    *outUnit = "";
    *outColor = 0xF800;  // Red (RGB565)
    return "";
  }

  // Define parameter metadata (label, unit, color)
  struct ParamInfo {
    const char* label;
    const char* unit;
    uint16_t color;
  };

  static const ParamInfo params[] = {
    {"Temperature", "°C", 0xF800},            // 0: Red (RGB565) (hot)
    {"Humidity", "%RH", 0x0810},              // 1: Blue (RGB565) (water)
    {"Pressure", "hPa", 0x07E0},              // 2: Green (RGB565) (air)
    {"Altitude", "m", 0xFC00},                // 3: Orange (RGB565) (elevation)
    {"Dew Point", "°C", 0x07FF},              // 4: Cyan (RGB565) (condensation)
  };

  const ParamInfo& param = params[valueIndex];
  *outLabel = param.label;
  *outUnit = param.unit;
  *outColor = param.color;

  return getValue(valueIndex);
}

bool Bme280Sensor::hasValueChanged() const {
  if (!available) return false;

  // Check if temperature (parameter 0) has changed
  String currentValue = getValue(0);
  if (currentValue != lastDisplayValue) {
    lastDisplayValue = currentValue;
    return true;
  }
  return false;
}

String Bme280Sensor::getStatus() const {
  if (!available) {
    return "Device not detected at 0x77";
  }
  if (!values.initialized) {
    return "Not yet initialized (no reads)";
  }
  return "OK";
}

float Bme280Sensor::calculateDewPoint(float temperature, float humidity) const {
  // Alduchov & Eskridge formula
  // Valid for: 0-50°C, 1-100% RH, accuracy ±0.35°C
  const float a = 17.62f;
  const float b = 243.12f;
  float alpha = ((a * temperature) / (b + temperature)) + log(humidity / 100.0f);
  float dewPoint = (b * alpha) / (a - alpha);
  return dewPoint;
}
