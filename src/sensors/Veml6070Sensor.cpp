/**
 * @file Veml6070Sensor.cpp
 * @brief Implementation of Veml6070Sensor class
 */

#include "Veml6070Sensor.h"

Veml6070Sensor::Veml6070Sensor() : device(nullptr) {
  // Initialize values to zero
  values.raw = 0;
  values.lsb = 0;
  values.msb = 0;
  values.uvLevel = 0;
  values.initialized = false;
  
  // Initialize change detection
  lastDisplayValue = "";
}

void Veml6070Sensor::initialize() {
  // Check if device pointer was set via setDevice()
  if (!device) {
    Serial.printf("[VEML6070] ERROR: Device pointer not set! Call setDevice() from main.cpp\n");
    available = false;
    return;
  }
  
  // Device was already initialized in main.cpp, just mark as available
  available = true;
  Serial.printf("[VEML6070] Sensor initialized (via main.cpp)\n");
}

void Veml6070Sensor::update() {
  if (!available || !device) return;

  // Mark as initialized on first read
  if (!values.initialized) {
    values.initialized = true;
  }

  // Read sensor values using Adafruit VEML6070 API
  uint16_t raw = device->readUV();
  values.raw = raw;
  values.lsb = raw & 0xFF;
  values.msb = (raw >> 8) & 0xFF;
  
  // Calculate UV level classification
  values.uvLevel = calculateUVLevel(raw);
}

String Veml6070Sensor::getValue(uint8_t parameterIndex) const {
  if (!available) return "";

  // Map parameter index to cached value
  switch (parameterIndex) {
    case 0: return String(values.raw);              // Raw UV value
    case 1: return String(values.uvLevel);          // UV Level (0-10)
    default: return "";
  }
}

String Veml6070Sensor::getLargeDisplayValue(uint8_t valueIndex,
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
    {"UV Raw", "raw", 0xFFE0},         // 0: Yellow (RGB565)
    {"UV Level", "0-10", 0xF800},      // 1: Red (RGB565)
  };

  const ParamInfo& param = params[valueIndex];
  *outLabel = param.label;
  *outUnit = param.unit;
  *outColor = param.color;

  return getValue(valueIndex);
}

bool Veml6070Sensor::hasValueChanged() const {
  if (!available) return false;

  // Check if UV Level (parameter 1) has changed
  String currentValue = getValue(1);
  if (currentValue != lastDisplayValue) {
    lastDisplayValue = currentValue;
    return true;
  }
  return false;
}

String Veml6070Sensor::getStatus() const {
  if (!available) {
    return "Device not detected at 0x38/0x39";
  }
  if (!values.initialized) {
    return "Not yet initialized (no reads)";
  }
  return "OK";
}

uint8_t Veml6070Sensor::calculateUVLevel(uint16_t rawValue) const {
  // Convert raw UV value to 0-10 scale
  // Maps 0-65535 to 0-10: level = (raw * 10) / 65535
  uint32_t level = ((uint32_t)rawValue * 10) / 65535;
  if (level > 10) level = 10;
  return (uint8_t)level;
}

const struct Veml6070Values& Veml6070Sensor::getValues() const {
  static struct Veml6070Values valuesBuffer;
  valuesBuffer.raw = values.raw;
  valuesBuffer.lsb = values.lsb;
  valuesBuffer.msb = values.msb;
  return valuesBuffer;
}
