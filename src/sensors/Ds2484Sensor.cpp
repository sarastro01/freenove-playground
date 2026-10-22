/**
 * @file Ds2484Sensor.cpp
 * @brief Implementation of Ds2484Sensor class
 */

#include "Ds2484Sensor.h"

Ds2484Sensor::Ds2484Sensor() {
  // Initialize values to zero
  values.masterPresent = false;
  values.sensorCount = 0;
  values.lastScanMs = 0;
  values.lastConversionMs = 0;
  values.initialized = false;
  
  // Initialize all sensor slots
  for (uint8_t i = 0; i < 3; i++) {
    values.sensors[i].available = false;
    values.sensors[i].temperature = 0.0f;
    values.sensors[i].errorCount = 0;
  }
  
  // Initialize change detection
  lastDisplayValue = "";
}

void Ds2484Sensor::initialize() {
  // Attempt to detect DS2484 at I2C address 0x18
  available = device.begin(&Wire, DS2484_ADDR);
  values.masterPresent = available;
  
  if (available) {
    Serial.printf("[DS2484] Adapter detected at 0x18\n");
    // Do NOT perform bus scan here - deferred to update()
  } else {
    Serial.printf("[DS2484] Adapter NOT detected at 0x18\n");
  }
}

void Ds2484Sensor::update() {
  if (!available) return;

  // Mark as initialized on first update
  if (!values.initialized) {
    values.initialized = true;
  }

  const uint32_t nowMs = millis();

  // Bus scan at 60-second interval (or on first scan when sensorCount == 0)
  bool firstScan = values.sensorCount == 0;
  if (firstScan || (int32_t)(nowMs - values.lastScanMs) >= 60000) {
    performBusScan();
  }

  // Temperature polling at 3-second interval
  if (values.sensorCount > 0 && (int32_t)(nowMs - values.lastConversionMs) >= 3000) {
    readAllTemperatures();
  }
}

String Ds2484Sensor::getValue(uint8_t parameterIndex) const {
  if (!available) return "";

  // Map parameter index to cached value
  switch (parameterIndex) {
    case 0:  // Temperature sensor 0
      if (values.sensorCount > 0 && values.sensors[0].available) {
        return String(values.sensors[0].temperature, 2);
      }
      return "--";
    
    case 1:  // Temperature sensor 1
      if (values.sensorCount > 1 && values.sensors[1].available) {
        return String(values.sensors[1].temperature, 2);
      }
      return "--";
    
    case 2:  // Temperature sensor 2
      if (values.sensorCount > 2 && values.sensors[2].available) {
        return String(values.sensors[2].temperature, 2);
      }
      return "--";
    
    case 3:  // Sensor count
      return String(values.sensorCount);
    
    case 4:  // Bus scan status
      return values.sensorCount > 0 ? "OK" : "Scanning...";
    
    default:
      return "";
  }
}

String Ds2484Sensor::getLargeDisplayValue(uint8_t valueIndex,
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
    {"Temp Sensor 1", "°C", 0xF800},    // 0: Red (RGB565)
    {"Temp Sensor 2", "°C", 0x07E0},    // 1: Green (RGB565)
    {"Temp Sensor 3", "°C", 0x07FF},    // 2: Cyan (RGB565)
    {"Sensor Count", "", 0xFFE0},       // 3: Yellow (RGB565)
    {"Bus Status", "", 0xFC00},         // 4: Orange (RGB565)
  };

  const ParamInfo& param = params[valueIndex];
  *outLabel = param.label;
  *outUnit = param.unit;
  *outColor = param.color;

  return getValue(valueIndex);
}

bool Ds2484Sensor::hasValueChanged() const {
  if (!available) return false;

  // Check if sensor count or first temperature changed
  String currentValue = getValue(3) + "|" + getValue(0);
  if (currentValue != lastDisplayValue) {
    lastDisplayValue = currentValue;
    return true;
  }
  return false;
}

String Ds2484Sensor::getStatus() const {
  if (!available) {
    return "Adapter not detected at 0x18";
  }
  if (!values.initialized) {
    return "Not yet initialized";
  }
  if (values.sensorCount == 0) {
    return "No sensors found on 1-Wire bus";
  }
  return "OK: " + String(values.sensorCount) + " sensor(s)";
}

float Ds2484Sensor::getSensorTemperature(uint8_t sensorIndex) const {
  if (sensorIndex >= 3 || sensorIndex >= values.sensorCount) {
    return 0.0f;
  }
  return values.sensors[sensorIndex].available ? values.sensors[sensorIndex].temperature : 0.0f;
}

String Ds2484Sensor::getSensorRomId(uint8_t sensorIndex) const {
  if (sensorIndex >= 3 || sensorIndex >= values.sensorCount) {
    return "";
  }
  return formatRomId(values.sensors[sensorIndex].address);
}

void Ds2484Sensor::performBusScan() {
  Serial.println("[DS2484] Scanning 1-Wire bus...");

  // Reset sensor count and clear all sensors
  values.sensorCount = 0;
  for (uint8_t i = 0; i < 3; i++) {
    values.sensors[i].available = false;
    values.sensors[i].errorCount = 0;
  }

  // Scan for all devices on 1-Wire bus via DS2484
  uint8_t address[8];
  device.OneWireSearchReset();

  while (device.OneWireSearch(address) && values.sensorCount < 3) {
    // Verify CRC of address
    if (OneWire::crc8(address, 7) != address[7]) {
      continue;  // CRC mismatch, skip
    }

    Ds18b20Sensor &sensor = values.sensors[values.sensorCount];
    memcpy(sensor.address, address, 8);
    sensor.familyCode = address[0];
    sensor.available = true;
    sensor.errorCount = 0;
    sensor.temperature = 0.0f;
    sensor.lastReadMs = millis();

    Serial.printf("[DS2484] Found sensor %d: Family=%02X, Address=%s\n", 
      values.sensorCount, address[0], formatRomId(address).c_str());
    values.sensorCount++;
  }

  Serial.printf("[DS2484] Bus scan complete - found %d sensor(s)\n", values.sensorCount);
  values.lastScanMs = millis();
}

void Ds2484Sensor::readAllTemperatures() {
  if (values.sensorCount == 0) return;

  const uint32_t nowMs = millis();

  // Send Convert T command to all devices (0x44 via Skip ROM 0xCC)
  device.OneWireReset();
  device.OneWireWriteByte(0xCC);  // Skip ROM
  device.OneWireWriteByte(0x44);  // Convert T

  // Poll for conversion complete (750ms typical)
  // DS18B20 pulls bus low during conversion, releases when done
  uint32_t startMs = millis();
  uint8_t bit = 0;
  while (!(device.OneWireReadBit(&bit)) && (millis() - startMs) < 1000) {
    delay(10);
  }

  // Read temperatures from all sensors
  for (uint8_t i = 0; i < values.sensorCount && i < 3; i++) {
    Ds18b20Sensor &sensor = values.sensors[i];

    // Read scratchpad: ROM[0], then [CMD], [LSB], [MSB], ...
    device.OneWireReset();
    device.OneWireWriteByte(0x55);  // Match ROM
    for (uint8_t j = 0; j < 8; j++) {
      device.OneWireWriteByte(sensor.address[j]);
    }
    device.OneWireWriteByte(0xBE);  // Read Scratchpad

    // Read temperature bytes (LSB, MSB)
    uint8_t lsb = 0;
    uint8_t msb = 0;
    device.OneWireReadByte(&lsb);
    device.OneWireReadByte(&msb);

    // Convert to temperature: Temp = ((MSB << 8) | LSB) / 16
    int16_t rawTemp = ((msb << 8) | lsb);
    sensor.temperature = rawTemp / 16.0f;
    sensor.lastReadMs = nowMs;
    sensor.errorCount = 0;  // Clear errors on successful read
  }

  values.lastConversionMs = nowMs;
}

bool Ds2484Sensor::resetBus() {
  return device.OneWireReset();
}

String Ds2484Sensor::formatRomId(const uint8_t *address) const {
  String result = "";
  for (uint8_t i = 0; i < 8; i++) {
    if (i > 0) result += ":";
    if (address[i] < 0x10) result += "0";
    result += String(address[i], HEX);
  }
  return result;
}
