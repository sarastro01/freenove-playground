/**
 * @file Si1145Sensor.cpp
 * @brief Implementation of Si1145Sensor class
 */

#include "Si1145Sensor.h"

Si1145Sensor::Si1145Sensor() {
  // Initialize values to zero
  values.visible = 0;
  values.infrared = 0;
  values.uv = 0;
  values.uvIndex = 0.0f;
  values.initialized = false;
  
  // Initialize change detection
  lastDisplayValue = "";
}

void Si1145Sensor::initialize() {
  // Attempt to detect device at I2C address 0x60
  available = device.begin();
  
  if (available) {
    Serial.printf("[SI1145] Sensor detected at 0x60\n");
  } else {
    Serial.printf("[SI1145] Sensor NOT detected at 0x60\n");
  }
}

void Si1145Sensor::update() {
  if (!available) return;

  // Mark as initialized on first read
  if (!values.initialized) {
    values.initialized = true;
  }

  // Read sensor values using Adafruit_SI1145 API
  uint16_t visible = device.readVisible();
  uint16_t infrared = device.readIR();
  uint16_t uv = device.readUV();  // Raw UV from register 0x2C

  // Error check: If all three values are 0, likely I2C error
  if (visible == 0 && infrared == 0 && uv == 0) {
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
}

String Si1145Sensor::getValue(uint8_t parameterIndex) const {
  if (!available) return "";

  // Map parameter index to cached value
  switch (parameterIndex) {
    case 0: return String(values.visible);         // Visible light (raw)
    case 1: return String(values.infrared);        // Infrared (raw)
    case 2: return String(values.uv);              // UV raw value
    case 3: return String(values.uvIndex, 2);      // UV Index (float, 2 decimals)
    default: return "";
  }
}

String Si1145Sensor::getLargeDisplayValue(uint8_t valueIndex,
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
    {"Visible Light", "raw", 0xFFE0},      // 0: Yellow (RGB565)
    {"Infrared Light", "raw", 0xF800},     // 1: Red (RGB565)
    {"UV Raw", "", 0xF81F},                // 2: Magenta (RGB565)
    {"UV Index", "", 0x07FF},              // 3: Cyan (RGB565)
  };

  const ParamInfo& param = params[valueIndex];
  *outLabel = param.label;
  *outUnit = param.unit;
  *outColor = param.color;

  return getValue(valueIndex);
}

bool Si1145Sensor::hasValueChanged() const {
  if (!available) return false;

  // Check if UV Index (parameter 3) has changed
  String currentValue = getValue(3);
  if (currentValue != lastDisplayValue) {
    lastDisplayValue = currentValue;
    return true;
  }
  return false;
}

String Si1145Sensor::getStatus() const {
  if (!available) {
    return "Device not detected at 0x60";
  }
  if (!values.initialized) {
    return "Not yet initialized (no reads)";
  }
  return "OK";
}
