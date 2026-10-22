/**
 * @file EncoderSensor.cpp
 * @brief EncoderSensor implementation
 * @author Refactoring Agent
 * @date 2026-10-22
 */

#include "EncoderSensor.h"
#include <Arduino.h>

// Color presets (RGB format: 0xRRGGBB)
const uint32_t EncoderSensor::COLOR_PRESETS[4] = {
  0x0060FF,  // Blue
  0x00FF20,  // Green
  0xFF3000,  // Red
  0xB400FF   // Purple
};

const char *EncoderSensor::COLOR_NAMES[4] = {
  "blue",
  "green",
  "red",
  "purple"
};

bool EncoderSensor::initialize() {
  // Attempt to initialize seesaw at I2C address 0x36
  if (!seesaw.begin(0x36)) {
    initialized = false;
    Serial.println("[Encoder] Initialization FAILED at 0x36");
    return false;
  }
  
  // Enable encoder module
  seesaw.enableEncoderInterrupt();
  
  // Get initial encoder position
  int32_t initialPos = seesaw.getEncoderPosition();
  
  // Initialize cached values
  values.position = initialPos;
  values.delta = 0;
  values.pressed = false;
  values.presses = 0;
  values.brightness = 64;
  values.colorIndex = 0;
  values.colorName = COLOR_NAMES[0];
  
  lastValues.position = initialPos;
  lastValues.pressed = false;
  lastValues.presses = 0;
  lastValues.brightness = 64;
  lastValues.colorIndex = 0;
  
  // Set initial LED color and brightness
  uint32_t color = COLOR_PRESETS[0];
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  seesaw.setPixelColor(0, seesaw.Color(r, g, b));
  seesaw.setBrightness(values.brightness);
  seesaw.show();
  
  initialized = true;
  Serial.println("[Encoder] Initialized successfully at 0x36");
  return true;
}

bool EncoderSensor::update() {
  if (!initialized) return false;
  
  // Track previous values
  lastValues.position = values.position;
  lastValues.pressed = values.pressed;
  lastValues.presses = values.presses;
  lastValues.brightness = values.brightness;
  lastValues.colorIndex = values.colorIndex;
  
  // Read encoder position
  int32_t newPosition = seesaw.getEncoderPosition();
  values.delta = newPosition - values.position;
  values.position = newPosition;
  
  // Read button state
  bool newPressed = seesaw.digitalRead(24) == 0;  // Button on pin 24, active low
  values.pressed = newPressed;
  
  // Detect button press edge (not hold)
  if (newPressed && !lastPressed) {
    values.presses++;
  }
  lastPressed = newPressed;
  
  return true;
}

String EncoderSensor::getValue(uint8_t index) const {
  switch (index) {
    case 0:  // Position
      return String(values.position);
    
    case 1:  // Delta
      return String(values.delta);
    
    case 2:  // Pressed
      return values.pressed ? "true" : "false";
    
    case 3:  // Presses
      return String(values.presses);
    
    case 4:  // Brightness
      return String(values.brightness);
    
    case 5:  // Color name
      return values.colorName;
    
    case 6:  // Color index
      return String(values.colorIndex);
    
    default:
      return "";
  }
}

LargeDisplayValue EncoderSensor::getLargeDisplayValue(uint8_t index) const {
  LargeDisplayValue result{};
  
  switch (index) {
    case 0:  // Position
      result.value = String(values.position);
      result.label = "Position";
      result.unit = "ticks";
      result.color = 0x00FFFF;  // Cyan
      break;
    
    case 1:  // Delta
      result.value = String(values.delta);
      result.label = "Delta";
      result.unit = "ticks";
      result.color = 0x00FF00;  // Green
      break;
    
    case 2:  // Pressed
      result.value = values.pressed ? "true" : "false";
      result.label = "Button";
      result.unit = "state";
      result.color = 0xFF0000;  // Red
      break;
    
    case 3:  // Presses
      result.value = String(values.presses);
      result.label = "Presses";
      result.unit = "count";
      result.color = 0xFFFF00;  // Yellow
      break;
    
    case 4:  // Brightness
      result.value = String(values.brightness);
      result.label = "Brightness";
      result.unit = "0-255";
      result.color = 0xFF00FF;  // Magenta
      break;
    
    case 5:  // Color name
      result.value = values.colorName;
      result.label = "LED Color";
      result.unit = "name";
      result.color = getColorForIndex(values.colorIndex);
      break;
    
    case 6:  // Color index
      result.value = String(values.colorIndex);
      result.label = "Color Index";
      result.unit = "0-3";
      result.color = 0x0000FF;  // Blue
      break;
    
    default:
      result.value = "";
      result.label = "Unknown";
      result.unit = "";
      result.color = 0x808080;  // Gray
      break;
  }
  
  return result;
}

bool EncoderSensor::hasValueChanged() const {
  // Check if any significant change occurred
  if (values.position != lastValues.position) return true;
  if (values.pressed != lastValues.pressed) return true;
  if (values.presses != lastValues.presses) return true;
  if (values.brightness != lastValues.brightness) return true;
  if (values.colorIndex != lastValues.colorIndex) return true;
  
  return false;
}

String EncoderSensor::getStatus() const {
  if (!initialized) return "ERROR: Not initialized";
  
  String status = "OK: ";
  status += "pos=" + String(values.position);
  status += ", delta=" + String(values.delta);
  status += ", pressed=" + String(values.pressed ? "true" : "false");
  status += ", presses=" + String(values.presses);
  status += ", brightness=" + String(values.brightness);
  status += ", color=" + String(values.colorName);
  
  return status;
}

bool EncoderSensor::setLedColor(uint8_t colorIndex) {
  if (colorIndex >= 4) return false;
  
  values.colorIndex = colorIndex;
  values.colorName = COLOR_NAMES[colorIndex];
  
  // Update LED immediately
  uint32_t color = COLOR_PRESETS[colorIndex];
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  seesaw.setPixelColor(0, seesaw.Color(r, g, b));
  seesaw.show();
  
  return true;
}

bool EncoderSensor::setLedBrightness(uint8_t brightness) {
  values.brightness = brightness;
  seesaw.setBrightness(brightness);
  seesaw.show();
  return true;
}

bool EncoderSensor::setLedColorAndBrightness(uint8_t colorIndex, uint8_t brightness) {
  if (colorIndex >= 4) return false;
  
  values.colorIndex = colorIndex;
  values.colorName = COLOR_NAMES[colorIndex];
  values.brightness = brightness;
  
  // Update LED immediately
  uint32_t color = COLOR_PRESETS[colorIndex];
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  seesaw.setPixelColor(0, seesaw.Color(r, g, b));
  seesaw.setBrightness(brightness);
  seesaw.show();
  
  return true;
}

uint32_t EncoderSensor::getColorForIndex(uint8_t colorIndex) {
  if (colorIndex < 4) {
    return COLOR_PRESETS[colorIndex];
  }
  return 0x808080;  // Gray for unknown
}

const char *EncoderSensor::getColorNameForIndex(uint8_t colorIndex) {
  if (colorIndex < 4) {
    return COLOR_NAMES[colorIndex];
  }
  return "unknown";
}
