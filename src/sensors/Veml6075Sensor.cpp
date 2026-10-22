/**
 * @file Veml6075Sensor.cpp
 * @brief Implementation of Veml6075Sensor class
 */

#include "Veml6075Sensor.h"

Veml6075Sensor::Veml6075Sensor() {
  // Initialize values to zero
  values.uvaRaw = 0;
  values.uvbRaw = 0;
  values.comp1 = 0;
  values.comp2 = 0;
  values.uvaComp = 0.0f;
  values.uvbComp = 0.0f;
  values.index = 0.0f;
  values.initialized = false;
  
  // Initialize change detection
  lastDisplayValue = "";
}

void Veml6075Sensor::initialize() {
  // Attempt to detect device at I2C address 0x10
  available = device.begin();
  
  if (available) {
    Serial.printf("[VEML6075] Sensor detected at 0x10\n");
  } else {
    Serial.printf("[VEML6075] Sensor NOT detected at 0x10\n");
  }
}

void Veml6075Sensor::update() {
  if (!available) return;

  // Mark as initialized on first read
  if (!values.initialized) {
    values.initialized = true;
  }

  // Read sensor values using SparkFun VEML6075 API
  values.uvaRaw = device.rawUva();
  values.uvbRaw = device.rawUvb();
  values.comp1 = device.uvComp1();
  values.comp2 = device.uvComp2();
  values.uvaComp = device.uva();
  values.uvbComp = device.uvb();
  values.index = device.index();
}

String Veml6075Sensor::getValue(uint8_t parameterIndex) const {
  if (!available) return "";

  // Map parameter index to cached value
  switch (parameterIndex) {
    case 0: return String(values.uvaRaw);           // UVA Raw
    case 1: return String(values.uvbRaw);           // UVB Raw
    case 2: return String(values.uvaComp, 2);       // UVA Compensated
    case 3: return String(values.uvbComp, 2);       // UVB Compensated
    case 4: return String(values.index, 2);         // UV Index
    default: return "";
  }
}

String Veml6075Sensor::getLargeDisplayValue(uint8_t valueIndex,
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
    {"UVA Raw", "raw", 0xF81F},         // 0: Magenta (RGB565)
    {"UVB Raw", "raw", 0x07FF},         // 1: Cyan (RGB565)
    {"UVA Comp", "", 0xFC00},           // 2: Orange (RGB565)
    {"UVB Comp", "", 0xFFE0},           // 3: Yellow (RGB565)
    {"UV Index", "", 0xF800},           // 4: Red (RGB565)
  };

  const ParamInfo& param = params[valueIndex];
  *outLabel = param.label;
  *outUnit = param.unit;
  *outColor = param.color;

  return getValue(valueIndex);
}

bool Veml6075Sensor::hasValueChanged() const {
  if (!available) return false;

  // Check if UV Index (parameter 4) has changed
  String currentValue = getValue(4);
  if (currentValue != lastDisplayValue) {
    lastDisplayValue = currentValue;
    return true;
  }
  return false;
}

String Veml6075Sensor::getStatus() const {
  if (!available) {
    return "Device not detected at 0x10";
  }
  if (!values.initialized) {
    return "Not yet initialized (no reads)";
  }
  return "OK";
}

const struct Veml6075Values& Veml6075Sensor::getValues() const {
  static struct Veml6075Values valuesBuffer;
  valuesBuffer.uva = values.uvaRaw;        // Map internal uvaRaw to uva in buffer
  valuesBuffer.uvb = values.uvbRaw;        // Map internal uvbRaw to uvb in buffer
  valuesBuffer.comp1 = values.comp1;
  valuesBuffer.comp2 = values.comp2;
  valuesBuffer.uvaComp = values.uvaComp;
  valuesBuffer.uvbComp = values.uvbComp;
  valuesBuffer.index = values.index;
  return valuesBuffer;
}
