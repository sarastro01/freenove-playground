/**
 * @file Scd30Sensor.cpp
 * @brief Implementation of Scd30Sensor class
 */

#include "Scd30Sensor.h"

Scd30Sensor::Scd30Sensor() {
  // Initialize values to zero
  values.co2 = 0.0f;
  values.temperature = 0.0f;
  values.humidity = 0.0f;
  values.ascEnabled = false;
  values.ascTempOffset = 0.0f;
  values.initialized = false;
  values.lastAscQuery = 0;
  values.pendingAscWrite = false;
  
  // Initialize change detection
  lastDisplayValue = "";
}

void Scd30Sensor::initialize() {
  // Attempt to detect device at I2C address 0x61
  available = device.begin();
  
  if (available) {
    // Start continuous measurement
    device.beginMeasuring();
    Serial.printf("[SCD30] Sensor detected at 0x61, continuous measurement started\n");
  } else {
    Serial.printf("[SCD30] Sensor NOT detected at 0x61\n");
  }
}

void Scd30Sensor::update() {
  if (!available) return;

  // Check if new data is available (sensor has 2 second minimum interval)
  if (!device.dataAvailable()) {
    return;
  }

  // Mark as initialized on first read
  if (!values.initialized) {
    values.initialized = true;
  }

  // Read sensor values using SparkFun SCD30 API
  // Always read values to prevent display flicker (values are cached internally)
  values.co2 = device.getCO2();
  values.temperature = device.getTemperature();
  values.humidity = device.getHumidity();

  // Query ASC status every 3 seconds (to avoid I2C flooding)
  uint32_t nowMs = millis();
  if (nowMs - values.lastAscQuery >= 3000) {
    values.ascEnabled = device.getAutoSelfCalibration();
    values.lastAscQuery = nowMs;
    values.pendingAscWrite = false;
  }
}

String Scd30Sensor::getValue(uint8_t parameterIndex) const {
  if (!available) return "";

  // Map parameter index to cached value
  switch (parameterIndex) {
    case 0: return String(values.co2, 1);           // CO2 (ppm)
    case 1: return String(values.temperature, 2);   // Temperature (°C)
    case 2: return String(values.humidity, 1);      // Humidity (%RH)
    case 3: return values.ascEnabled ? "ON" : "OFF"; // ASC Status
    case 4: return String(values.ascTempOffset, 2); // ASC Temp Offset
    default: return "";
  }
}

String Scd30Sensor::getLargeDisplayValue(uint8_t valueIndex,
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
    {"CO2", "ppm", 0x07E0},             // 0: Green (RGB565)
    {"Temperature", "°C", 0xF800},      // 1: Red (RGB565)
    {"Humidity", "%RH", 0x0810},        // 2: Blue (RGB565)
    {"ASC Status", "", 0xFFE0},         // 3: Yellow (RGB565)
    {"ASC Offset", "°C", 0xFC00},       // 4: Orange (RGB565)
  };

  const ParamInfo& param = params[valueIndex];
  *outLabel = param.label;
  *outUnit = param.unit;
  *outColor = param.color;

  return getValue(valueIndex);
}

bool Scd30Sensor::hasValueChanged() const {
  if (!available) return false;

  // Check if CO2 (parameter 0) has changed
  String currentValue = getValue(0);
  if (currentValue != lastDisplayValue) {
    lastDisplayValue = currentValue;
    return true;
  }
  return false;
}

String Scd30Sensor::getStatus() const {
  if (!available) {
    return "Device not detected at 0x61";
  }
  if (!values.initialized) {
    return "Not yet initialized (no reads)";
  }
  if (values.pendingAscWrite) {
    return "ASC toggle in progress...";
  }
  return "OK";
}

bool Scd30Sensor::setASCEnabled(bool enable) {
  if (!available) return false;
  
  device.setAutoSelfCalibration(enable);
  values.pendingAscWrite = true;
  return true;
}
