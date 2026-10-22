/**
 * @file GamepadSensor.cpp
 * @brief GamepadSensor implementation
 * @author Refactoring Agent
 * @date 2026-10-22
 */

#include "GamepadSensor.h"
#include <Arduino.h>

bool GamepadSensor::initialize() {
  // Attempt to initialize gamepad at I2C address 0x77
  if (gamepad.begin(0x77)) {
    initialized = true;
    memset(&values, 0, sizeof(values));
    memset(&lastValues, 0, sizeof(lastValues));
    Serial.println("[Gamepad] Initialized successfully at 0x77");
    return true;
  }
  
  initialized = false;
  Serial.println("[Gamepad] Initialization FAILED at 0x77");
  return false;
}

bool GamepadSensor::update() {
  if (!initialized) return false;
  
  // Read gamepad state
  gamepad.read();
  
  // Track previous values for change detection
  lastValues.x = values.x;
  lastValues.y = values.y;
  lastValues.buttons = values.buttons;
  lastValues.eventCount = values.eventCount;
  
  // Get current values from gamepad
  values.x = gamepad.analogRead(GAMEPAD_X);
  values.y = gamepad.analogRead(GAMEPAD_Y);
  
  // Read buttons as bitmask
  uint32_t newButtons = 0;
  if (gamepad.buttonA()) newButtons |= 0x001;
  if (gamepad.buttonB()) newButtons |= 0x002;
  if (gamepad.buttonX()) newButtons |= 0x004;
  if (gamepad.buttonY()) newButtons |= 0x008;
  if (gamepad.buttonL1()) newButtons |= 0x010;
  if (gamepad.buttonL2()) newButtons |= 0x020;
  if (gamepad.buttonR1()) newButtons |= 0x040;
  if (gamepad.buttonR2()) newButtons |= 0x080;
  if (gamepad.buttonL3()) newButtons |= 0x100;
  if (gamepad.buttonR3()) newButtons |= 0x200;
  if (gamepad.buttonStart()) newButtons |= 0x400;
  if (gamepad.buttonSelect()) newButtons |= 0x800;
  
  // Detect button changes
  uint32_t buttonChanges = lastValues.buttons ^ newButtons;
  values.buttons = newButtons;
  
  // Update event detection
  values.hasEvent = false;
  values.lastEvent = 0;  // 0 = no event
  
  if (buttonChanges != 0) {
    values.eventCount++;
    values.hasEvent = true;
    values.lastEvent = 1;  // 1 = button event
  }
  
  // Detect analog stick changes (deadzone: 50)
  int deltaX = abs(values.x - lastValues.x);
  int deltaY = abs(values.y - lastValues.y);
  if (deltaX > 50 || deltaY > 50) {
    if (values.lastEvent == 0) {  // Only if no button event in this update
      values.eventCount++;
      values.hasEvent = true;
      values.lastEvent = 2;  // 2 = analog event
    }
  }
  
  return true;
}

String GamepadSensor::getValue(uint8_t index) const {
  switch (index) {
    case 0:  // X position
      return String(values.x);
    
    case 1:  // Y position
      return String(values.y);
    
    case 2:  // Buttons as hex
      return formatButtonMask(values.buttons);
    
    case 3:  // Event count
      return String(values.eventCount);
    
    case 4:  // Last event type
      return String(values.lastEvent);
    
    case 5:  // Has event
      return values.hasEvent ? "true" : "false";
    
    case 6:  // Raw buttons (same as param 2 for debugging)
      return formatButtonMask(values.buttons);
    
    case 7:  // Event status text
      if (values.lastEvent == 0) return "none";
      if (values.lastEvent == 1) return "button";
      if (values.lastEvent == 2) return "stick";
      return "unknown";
    
    default:
      return "";
  }
}

LargeDisplayValue GamepadSensor::getLargeDisplayValue(uint8_t index) const {
  LargeDisplayValue result{};
  
  switch (index) {
    case 0:  // X Position
      result.value = String(values.x);
      result.label = "X Position";
      result.unit = "pos";
      result.color = 0x00FFFF;  // Cyan
      break;
    
    case 1:  // Y Position
      result.value = String(values.y);
      result.label = "Y Position";
      result.unit = "pos";
      result.color = 0x00FF00;  // Green
      break;
    
    case 2:  // Buttons
      result.value = formatButtonMask(values.buttons);
      result.label = "Button State";
      result.unit = "bits";
      result.color = 0xFF0000;  // Red
      break;
    
    case 3:  // Event Count
      result.value = String(values.eventCount);
      result.label = "Events";
      result.unit = "count";
      result.color = 0xFFFF00;  // Yellow
      break;
    
    case 4:  // Last Event
      result.value = String(values.lastEvent);
      result.label = "Last Event";
      result.unit = "type";
      result.color = 0xFF00FF;  // Magenta
      break;
    
    case 5:  // Has Event
      result.value = values.hasEvent ? "true" : "false";
      result.label = "New Event";
      result.unit = "status";
      result.color = 0x0000FF;  // Blue
      break;
    
    case 6:  // Raw Buttons (debugging)
      result.value = formatButtonMask(values.buttons);
      result.label = "Raw Buttons";
      result.unit = "hex";
      result.color = 0xFFFFFF;  // White
      break;
    
    case 7:  // Event Status Text
      {
        const char *status;
        if (values.lastEvent == 0) status = "none";
        else if (values.lastEvent == 1) status = "button";
        else if (values.lastEvent == 2) status = "stick";
        else status = "unknown";
        
        result.value = status;
        result.label = "Event Status";
        result.unit = "text";
        result.color = 0x00FFFF;  // Cyan
      }
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

bool GamepadSensor::hasValueChanged() const {
  // Check if any significant change occurred
  if (values.x != lastValues.x) return true;
  if (values.y != lastValues.y) return true;
  if (values.buttons != lastValues.buttons) return true;
  if (values.eventCount != lastValues.eventCount) return true;
  if (values.hasEvent) return true;
  
  return false;
}

String GamepadSensor::getStatus() const {
  if (!initialized) return "ERROR: Not initialized";
  
  String status = "OK: ";
  status += "x=" + String(values.x);
  status += ", y=" + String(values.y);
  status += ", btns=" + formatButtonMask(values.buttons);
  status += ", events=" + String(values.eventCount);
  status += ", event=" + String(values.lastEvent);
  
  return status;
}

void GamepadSensor::clearEvents() {
  values.hasEvent = false;
  // Note: We don't reset eventCount - it's cumulative
}

const char *GamepadSensor::getButtonName(uint32_t button) {
  switch (button) {
    case 0x001: return "A";
    case 0x002: return "B";
    case 0x004: return "X";
    case 0x008: return "Y";
    case 0x010: return "L1";
    case 0x020: return "L2";
    case 0x040: return "R1";
    case 0x080: return "R2";
    case 0x100: return "L3";
    case 0x200: return "R3";
    case 0x400: return "Start";
    case 0x800: return "Select";
    default: return "Unknown";
  }
}

String GamepadSensor::formatButtonMask(uint32_t buttons) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "0x%04lX", buttons & 0xFFFF);
  return String(buffer);
}
