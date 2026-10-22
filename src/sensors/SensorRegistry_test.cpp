/**
 * @file SensorRegistry_test.cpp
 * @brief Unit tests for SensorRegistry and BaseSensor interface
 * 
 * Compile and run with:
 *   g++ -std=c++14 SensorRegistry.cpp SensorRegistry_test.cpp -o test_registry
 *   ./test_registry
 * 
 * Note: This test version uses mock Serial/Arduino for testing.
 */

#include <iostream>
#include <cassert>
#include <cstring>
#include <memory>

// Mock Serial for testing
class MockSerial {
public:
  static void println(const char* msg) {
    std::cout << msg << std::endl;
  }
  
  void printf(const char* fmt, ...) {
    // Simplified - just print format string
    std::cout << fmt << std::endl;
  }
};

MockSerial Serial;

// Define Arduino types if not available
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
typedef signed char int8_t;

// Include headers (modified for testing)
// #include "BaseSensor.h"
// #include "SensorRegistry.h"

/**
 * Mock Sensor Implementation for Testing
 */
class MockSensor : public BaseSensor {
private:
  String sensorName;
  uint8_t paramCount;
  String testValue;
  
public:
  MockSensor(const char* name, uint8_t count)
    : sensorName(name), paramCount(count), testValue("test_value") {}
  
  void initialize() override {
    available = true;
  }
  
  void update() override {
    // No-op for mock
  }
  
  String getValue(uint8_t parameterIndex) const override {
    if (parameterIndex < paramCount) {
      return sensorName + String(parameterIndex);
    }
    return "N/A";
  }
  
  String getLargeDisplayValue(uint8_t valueIndex,
                              const char** outLabel,
                              const char** outUnit,
                              uint16_t* outColor) const override {
    static const char* label = "Test Value";
    static const char* unit = "unit";
    *outLabel = label;
    *outUnit = unit;
    *outColor = 0xFF00;
    return getValue(valueIndex);
  }
  
  uint8_t getParameterCount() const override {
    return paramCount;
  }
  
  const char* getSensorName() const override {
    return sensorName.c_str();
  }
  
  bool isAvailable() const override {
    return available;
  }
};

/**
 * Test Suite
 */
void test_registry_initialization() {
  std::cout << "\n=== Test: Registry Initialization ===" << std::endl;
  
  SensorRegistry registry;
  
  assert(registry.getTotalPages() == 10);
  assert(registry.getAvailableSensorCount() == 0);
  assert(registry.getSensor(0) == nullptr);
  
  std::cout << "✓ Registry initializes with 10 empty pages" << std::endl;
}

void test_register_sensor() {
  std::cout << "\n=== Test: Register Sensor ===" << std::endl;
  
  SensorRegistry registry;
  MockSensor sensor1("Test1", 5);
  MockSensor sensor2("Test2", 3);
  
  registry.registerSensor(0, &sensor1, "Sensor1", 0xFF0000);
  registry.registerSensor(2, &sensor2, "Sensor2", 0x00FF00);
  
  assert(registry.getSensor(0) == &sensor1);
  assert(registry.getSensor(1) == nullptr);
  assert(registry.getSensor(2) == &sensor2);
  
  std::cout << "✓ Sensors registered correctly" << std::endl;
}

void test_sensor_metadata() {
  std::cout << "\n=== Test: Sensor Metadata ===" << std::endl;
  
  SensorRegistry registry;
  MockSensor sensor("TestSensor", 7);
  
  registry.registerSensor(3, &sensor, "MySensor", 0xFF00FF);
  
  assert(std::string(registry.getPageName(3)) == "MySensor");
  assert(registry.getHeaderColor(3) == 0xFF00FF);
  
  std::cout << "✓ Sensor metadata accessible" << std::endl;
}

void test_initialize_all() {
  std::cout << "\n=== Test: Initialize All ===" << std::endl;
  
  SensorRegistry registry;
  MockSensor sensor1("Sensor1", 5);
  MockSensor sensor2("Sensor2", 3);
  
  registry.registerSensor(0, &sensor1, "S1", 0xFF0000);
  registry.registerSensor(5, &sensor2, "S2", 0x00FF00);
  
  // Sensors should not be available before initialization
  assert(!registry.isSensorAvailable(0));
  assert(!registry.isSensorAvailable(5));
  
  registry.initializeAll();
  
  // After initialization, should be available
  assert(registry.isSensorAvailable(0));
  assert(registry.isSensorAvailable(5));
  assert(registry.getAvailableSensorCount() == 2);
  
  std::cout << "✓ All sensors initialized" << std::endl;
}

void test_find_next_available() {
  std::cout << "\n=== Test: Find Next Available Page ===" << std::endl;
  
  SensorRegistry registry;
  MockSensor sensor1("S1", 5);
  MockSensor sensor2("S2", 3);
  MockSensor sensor3("S3", 4);
  
  registry.registerSensor(1, &sensor1, "S1", 0xFF0000);
  registry.registerSensor(3, &sensor2, "S2", 0x00FF00);
  registry.registerSensor(7, &sensor3, "S3", 0x0000FF);
  
  registry.initializeAll();
  
  // Find next forward from page 0
  uint8_t next = registry.findNextAvailablePage(0, 1);
  assert(next == 1);
  
  // Find next forward from page 5
  next = registry.findNextAvailablePage(5, 1);
  assert(next == 7);
  
  // Find next backward from page 8
  next = registry.findNextAvailablePage(8, -1);
  assert(next == 7);
  
  std::cout << "✓ Finding next available page works correctly" << std::endl;
}

void test_available_sensor_count() {
  std::cout << "\n=== Test: Available Sensor Count ===" << std::endl;
  
  SensorRegistry registry;
  MockSensor s1("S1", 5);
  MockSensor s2("S2", 3);
  MockSensor s3("S3", 4);
  
  registry.registerSensor(0, &s1, "S1", 0xFF0000);
  registry.registerSensor(3, &s2, "S2", 0x00FF00);
  registry.registerSensor(7, &s3, "S3", 0x0000FF);
  
  assert(registry.getAvailableSensorCount() == 0);  // Before init
  
  registry.initializeAll();
  
  assert(registry.getAvailableSensorCount() == 3);  // After init
  
  std::cout << "✓ Available sensor count tracked correctly" << std::endl;
}

void test_invalid_operations() {
  std::cout << "\n=== Test: Invalid Operations ===" << std::endl;
  
  SensorRegistry registry;
  
  // Invalid page index
  assert(registry.getSensor(99) == nullptr);
  assert(!registry.isSensorAvailable(99));
  assert(std::string(registry.getPageName(99)) == "???");
  
  // Null sensor registration (should be ignored)
  registry.registerSensor(0, nullptr, "Test", 0xFF0000);
  assert(registry.getSensor(0) == nullptr);
  
  std::cout << "✓ Invalid operations handled gracefully" << std::endl;
}

void test_update_all() {
  std::cout << "\n=== Test: Update All ===" << std::endl;
  
  SensorRegistry registry;
  MockSensor s1("S1", 5);
  MockSensor s2("S2", 3);
  
  registry.registerSensor(0, &s1, "S1", 0xFF0000);
  registry.registerSensor(1, &s2, "S2", 0x00FF00);
  
  registry.initializeAll();
  
  // Should not crash when updating
  registry.updateAll();
  
  // Should work multiple times
  registry.updateAll();
  registry.updateAll();
  
  std::cout << "✓ Update all works correctly" << std::endl;
}

/**
 * Main Test Runner
 */
int main() {
  std::cout << "======================================" << std::endl;
  std::cout << "  Sensor Registry Test Suite" << std::endl;
  std::cout << "======================================" << std::endl;
  
  try {
    test_registry_initialization();
    test_register_sensor();
    test_sensor_metadata();
    test_initialize_all();
    test_find_next_available();
    test_available_sensor_count();
    test_invalid_operations();
    test_update_all();
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "  ✓ All tests passed!" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
    return 1;
  }
}
