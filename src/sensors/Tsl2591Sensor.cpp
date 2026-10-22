/**
 * @file Tsl2591Sensor.cpp
 * @brief Implementation of Tsl2591Sensor class
 */

#include "Tsl2591Sensor.h"

Tsl2591Sensor::Tsl2591Sensor() : device(2591) {
  // Initialize values to zero
  values.fullSpectrum = 0;
  values.infrared = 0;
  values.visible = 0;
  values.lux = 0.0f;
  values.gain = 0;
  values.integrationTime = 0;
  values.initialized = false;
  
  // Initialize change detection
  lastDisplayValue = "";
}

void Tsl2591Sensor::initialize() {
  // Attempt to detect device at I2C address 0x29
  available = device.begin();
  
  if (available) {
    // Configure default gain and integration time (same as original main.cpp)
    device.setGain(TSL2591_GAIN_MED);
    device.setTiming(TSL2591_INTEGRATIONTIME_200MS);
    Serial.printf("[TSL2591] Sensor detected at 0x29, configured for MED gain, 200ms integration\n");
  } else {
    Serial.printf("[TSL2591] Sensor NOT detected at 0x29\n");
  }
}

void Tsl2591Sensor::update() {
  if (!available) return;

  // Mark as initialized on first read
  if (!values.initialized) {
    values.initialized = true;
  }

  // Read sensor values using Adafruit_TSL2591 API
  uint32_t fullLum = device.getFullLuminosity();
  uint32_t fullSpectrum = fullLum & 0xFFFF;        // Lower 16 bits (CHAN0)
  uint32_t infrared = fullLum >> 16;               // Upper 16 bits (CHAN1)
  uint32_t visible = (fullSpectrum > infrared) ? (fullSpectrum - infrared) : 0;
  float lux = device.calculateLux(fullSpectrum, infrared);
  uint16_t gain = device.getGain();
  uint16_t integrationTime = device.getTiming();

  // Error check: If both channels read as zero, likely I2C error
  if (fullSpectrum == 0 && infrared == 0) {
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
}

String Tsl2591Sensor::getValue(uint8_t parameterIndex) const {
  if (!available) return "";

  // Map parameter index to cached value
  switch (parameterIndex) {
    case 0: return String(values.fullSpectrum);        // Full spectrum raw
    case 1: return String(values.infrared);            // Infrared raw
    case 2: return String(values.visible);             // Visible (calculated)
    case 3: return String(values.lux, 1);              // Lux (1 decimal)
    case 4: return String(values.gain);                // Gain setting
    case 5: return String(values.integrationTime);     // Integration time (ms)
    default: return "";
  }
}

String Tsl2591Sensor::getLargeDisplayValue(uint8_t valueIndex,
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
    {"Full Spectrum", "raw", 0xFC00},         // 0: Orange (RGB565)
    {"Infrared", "raw", 0xF800},              // 1: Red (RGB565)
    {"Visible Light", "raw", 0xFFE0},         // 2: Yellow (RGB565)
    {"Illuminance", "lux", 0x07E0},           // 3: Green (RGB565)
    {"Gain", "", 0x07FF},                     // 4: Cyan (RGB565)
    {"Integration Time", "ms", 0xF81F},       // 5: Magenta (RGB565)
  };

  const ParamInfo& param = params[valueIndex];
  *outLabel = param.label;
  *outUnit = param.unit;
  *outColor = param.color;

  return getValue(valueIndex);
}

bool Tsl2591Sensor::hasValueChanged() const {
  if (!available) return false;

  // Check if Lux (parameter 3) has changed
  String currentValue = getValue(3);
  if (currentValue != lastDisplayValue) {
    lastDisplayValue = currentValue;
    return true;
  }
  return false;
}

String Tsl2591Sensor::getStatus() const {
  if (!available) {
    return "Device not detected at 0x29";
  }
  if (!values.initialized) {
    return "Not yet initialized (no reads)";
  }
  return "OK";
}

// ============================================================================
// PHASE 3 Refactoring: getValues() Accessor
// ============================================================================
// Provides const reference to Tsl2591Values struct for main.cpp display layer.
// The struct is populated from internal cached values on each call.
// This is a backward-compatibility bridge that enables encapsulation
// without moving the struct definition to the header.
//
// Implementation detail: Uses static local struct to avoid heap allocation.
// Thread-safe for single-threaded Arduino context.
// 
const struct Tsl2591Values& Tsl2591Sensor::getValues() const {
  static struct Tsl2591Values valuesBuffer;
  
  // Populate buffer from internal state
  valuesBuffer.fullSpectrum = values.fullSpectrum;
  valuesBuffer.infrared = values.infrared;
  valuesBuffer.visible = values.visible;
  valuesBuffer.lux = values.lux;
  valuesBuffer.gain = values.gain;
  valuesBuffer.integrationTime = values.integrationTime;
  
  return valuesBuffer;
}
