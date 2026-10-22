/**
 * @file Ina219Sensor.cpp
 * @brief Implementation of Ina219Sensor class
 */

#include "Ina219Sensor.h"

Ina219Sensor::Ina219Sensor() : device(0x40) {
  // Initialize values to zero
  values.shunt = 0;
  values.bus = 0;
  values.load = 0;
  values.current = 0;
  values.power = 0;
  values.chargeMah = 0;
  values.initialized = false;
  values.lastSampleUs = 0;
  values.startTimeUs = 0;
  
  // Initialize stats
  stats.readCount = 0;
  stats.minTime = 0;
  stats.maxTime = 0;
  stats.totalTime = 0;
  
  // Initialize change detection
  lastDisplayValue = "";
}

void Ina219Sensor::initialize() {
  // Attempt to detect device at I2C address 0x40
  available = device.begin(&Wire);
  
  if (available) {
    // Configure for 16V, 400mA range (same as original main.cpp)
    device.setCalibration_16V_400mA();
    Serial.printf("[INA219] Sensor detected at 0x40, configured for 16V/400mA\n");
  } else {
    Serial.printf("[INA219] Sensor NOT detected at 0x40\n");
  }
}

void Ina219Sensor::update() {
  if (!available) return;

  // Get current timestamp
  const uint32_t nowUs = micros();

  // Initialize start time on first read
  if (!values.initialized) {
    values.startTimeUs = nowUs;
    values.lastSampleUs = nowUs;
    values.initialized = true;
  } else {
    // Calculate accumulated charge based on time elapsed
    const uint32_t elapsedUs = nowUs - values.lastSampleUs;
    if (elapsedUs < 200000) {  // Sanity check: skip if gap > 200ms
      // Integrate current over time: mAh = mA × hours
      // = mA × (us / 3600000000 us/hour)
      values.chargeMah += values.current * (float)elapsedUs / 3600000000.0f;
    }
  }

  // Record sample time for next iteration
  values.lastSampleUs = nowUs;

  // Read from device with optional timing
  uint32_t readStartUs = micros();
  
  // Read all ADC values from device
  // Note: Negative sign matches original main.cpp implementation
  values.shunt = -device.getShuntVoltage_mV();
  values.bus = device.getBusVoltage_V();
  values.current = -device.getCurrent_mA();
  values.power = device.getPower_mW();
  
  // Calculate load voltage (bus - shunt, in volts)
  values.load = values.bus - values.shunt / 1000.0f;

  uint32_t readEndUs = micros();
  uint32_t readDuration = readEndUs - readStartUs;

  // Track statistics (optional, compile-time controlled)
  #ifdef ENABLE_STATS_REPORTING
  stats.readCount++;
  stats.totalTime += readDuration;
  if (stats.readCount == 1) {
    stats.minTime = readDuration;
    stats.maxTime = readDuration;
  } else {
    if (readDuration < stats.minTime) stats.minTime = readDuration;
    if (readDuration > stats.maxTime) stats.maxTime = readDuration;
  }
  #endif
}

String Ina219Sensor::getValue(uint8_t parameterIndex) const {
  if (!available) return "";

  // Map parameter index to cached value
  // Note: Order differs from original main.cpp for display consistency
  switch (parameterIndex) {
    case 0: return formatRuntime(getRuntimeSeconds());  // Runtime (HH:MM:SS)
    case 1: return String(values.load, 3);              // Load voltage (V)
    case 2: return String(values.bus, 3);               // Bus voltage (V)
    case 3: return String(values.current, 1);           // Current (mA)
    case 4: return String(values.chargeMah, 3);         // Charge (mAh)
    case 5: return String(values.shunt, 2);             // Shunt voltage (mV)
    case 6: return String((uint16_t)values.power);      // Power (mW)
    default: return "";
  }
}

String Ina219Sensor::getLargeDisplayValue(uint8_t valueIndex,
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
    {"Runtime", "HH:MM:SS", 0xFFE0},   // 0: Yellow (RGB565)
    {"Load Voltage", "V", 0x07E0},     // 1: Green (RGB565)
    {"Bus Voltage", "V", 0x07FF},      // 2: Cyan (RGB565)
    {"Current", "mA", 0xF81F},         // 3: Magenta (RGB565)
    {"Charge", "mAh", 0xFC00},         // 4: Orange (RGB565)
    {"Shunt Voltage", "mV", 0xF810},   // 5: Pink (RGB565)
    {"Power", "mW", 0xF800},           // 6: Red (RGB565)
  };

  const ParamInfo& param = params[valueIndex];
  *outLabel = param.label;
  *outUnit = param.unit;
  *outColor = param.color;

  return getValue(valueIndex);
}

bool Ina219Sensor::hasValueChanged() const {
  if (!available) return false;

  // Check if runtime (parameter 0) has changed
  // This happens approximately once per second
  String currentValue = getValue(0);
  if (currentValue != lastDisplayValue) {
    lastDisplayValue = currentValue;
    return true;
  }
  return false;
}

String Ina219Sensor::getStatus() const {
  if (!available) {
    return "Device not detected at 0x40";
  }
  if (!values.initialized) {
    return "Not yet initialized (no reads)";
  }
  return "OK";
}

String Ina219Sensor::formatRuntime(uint32_t seconds) const {
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours, minutes, secs);
  return String(buffer);
}

// ============================================================================
// PHASE 3a Accessor: getValues() for main.cpp display/caching integration
// ============================================================================
// Returns a copy of the internal values struct in main.cpp-compatible format
// This enables gradual refactoring without moving struct definitions

const struct Ina219Values& Ina219Sensor::getValues() const {
  static Ina219Values result;
  
  // Populate result struct with current internal values
  result.shunt = values.shunt;
  result.bus = values.bus;
  result.load = values.load;
  result.current = values.current;
  result.power = values.power;
  result.chargeMah = values.chargeMah;
  result.initialized = values.initialized;
  result.lastSampleUs = values.lastSampleUs;
  result.startTimeUs = values.startTimeUs;
  
  return result;
}
