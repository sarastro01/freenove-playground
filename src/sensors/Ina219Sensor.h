/**
 * @file Ina219Sensor.h
 * @brief Adafruit INA219 High-Side DC Current Sensor implementation
 * 
 * Wraps the Adafruit_INA219 library in a BaseSensor-compatible class.
 * Measures and tracks:
 * - Shunt voltage (mV) - voltage across sense resistor
 * - Bus voltage (V) - supply voltage
 * - Load voltage (V) - bus - shunt (approximate)
 * - Current (mA) - calculated from shunt voltage
 * - Power (mW) - bus voltage × current
 * - Charge (mAh) - accumulated charge since boot
 * - Runtime (HH:MM:SS) - elapsed time since sensor boot
 * 
 * I2C Address: 0x40 (default, configurable)
 * Refresh Rate: ~50 Hz (20 ms interval)
 * 
 * Architecture:
 * - Inherits from BaseSensor
 * - Encapsulates Adafruit_INA219 device instance
 * - Caches all values (not global)
 * - Manages lifecycle (init, update, status)
 * 
 * Integration with main.cpp:
 *   1. Create instance: Ina219Sensor ina219Sensor;
 *   2. Register in setup: registry.registerSensor(2, &ina219Sensor, "INA219", 0x00FF00);
 *   3. Initialize: registry.initializeAll();
 *   4. Update: registry.updateAll(); (in loop)
 *   5. Access: sensor->getValue(index) or sensor->getLargeDisplayValue(...)
 */

#ifndef INA219_SENSOR_H
#define INA219_SENSOR_H

#include "BaseSensor.h"
#include <Adafruit_INA219.h>

/**
 * @class Ina219Sensor
 * @brief INA219 current/power monitor sensor implementation
 * 
 * Provides 7 parameters:
 *   0: Runtime (HH:MM:SS)
 *   1: Load Voltage (V)
 *   2: Bus Voltage (V)
 *   3: Current (mA)
 *   4: Charge (mAh)
 *   5: Shunt Voltage (mV)
 *   6: Power (mW)
 * 
 * Note: Parameter indices differ from original main.cpp due to
 * reordering for display consistency. Mapping:
 *   original[0] shunt → getValue(5)
 *   original[1] load  → getValue(1)
 *   original[2] bus   → getValue(2)
 *   original[3] current → getValue(3)
 *   original[4] charge → getValue(4)
 *   original[5] power  → getValue(6)
 *   original[6] runtime → getValue(0)
 */
class Ina219Sensor : public BaseSensor {
private:
  /// I2C device instance
  Adafruit_INA219 device;
  
  /// Cached sensor values (replaces global Ina219Values struct)
  struct {
    float shunt;          ///< mV - voltage across shunt
    float bus;            ///< V - bus supply voltage
    float load;           ///< V - approximate load voltage (bus - shunt/1000)
    float current;        ///< mA - current through shunt
    float power;          ///< mW - power consumption
    float chargeMah;      ///< mAh - accumulated charge
    bool initialized;     ///< Whether sensor has been read once
    uint32_t lastSampleUs;    ///< Timestamp of last sample (us)
    uint32_t startTimeUs;     ///< Sensor initialization time (us)
  } values;
  
  /// Last value for change detection (used by display layer)
  String lastDisplayValue;
  
  /// Statistics for performance monitoring (optional, compile-time enabled)
  struct {
    uint32_t readCount;
    uint32_t minTime;
    uint32_t maxTime;
    uint32_t totalTime;
  } stats;

public:
  /**
   * @brief Constructor
   * 
   * Creates sensor instance with default I2C address (0x40).
   * Device is not accessed until initialize() is called.
   */
  Ina219Sensor();
  
  /**
   * @brief Destructor
   * 
   * No special cleanup needed; Adafruit_INA219 has no resources to release.
   */
  ~Ina219Sensor() = default;

  // === BaseSensor Interface Implementation ===

  /**
   * @brief Initialize sensor and detect device
   * 
   * Calls device.begin() to detect INA219 at I2C address 0x40.
   * Sets available flag based on detection result.
   * Initializes cached values to zero.
   * 
   * Called once during setup() via registry.initializeAll().
   */
  void initialize() override;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Reads all 4 ADC values from device:
   * - Shunt voltage (mV)
   * - Bus voltage (V)
   * - Current (mA)
   * - Power (mW)
   * 
   * Calculates:
   * - Load voltage (V) = Bus - Shunt/1000
   * - Accumulated charge (mAh) based on current and time
   * 
   * Called periodically from main loop (~50 Hz, 20 ms intervals).
   * Includes optional performance statistics tracking.
   */
  void update() override;

  /**
   * @brief Get single parameter as formatted string
   * 
   * Maps parameter index to cached value and returns as string.
   * Used by display layer for rendering sensor values.
   * 
   * @param parameterIndex 0-6 (see class description for mapping)
   * @return Formatted value string (e.g., "12.34" or "00:15:30")
   *         Empty string if index out of range
   */
  String getValue(uint8_t parameterIndex) const override;

  /**
   * @brief Get detailed display information for large display mode
   * 
   * Returns parameter value with label, unit, and color.
   * Used by large display mode for detailed sensor display.
   * 
   * @param valueIndex Which parameter to display (0-6)
   * @param outLabel Pointer to label string (set by this method)
   * @param outUnit Pointer to unit string (set by this method)
   * @param outColor Pointer to RGB565 color (set by this method)
   * @return Formatted value string
   * 
   * Example output:
   *   value: "12.34"
   *   label: "Shunt Voltage"
   *   unit: "mV"
   *   color: 0xFF8000 (orange)
   */
  String getLargeDisplayValue(uint8_t valueIndex,
                              const char** outLabel,
                              const char** outUnit,
                              uint16_t* outColor) const override;

  /**
   * @brief Get number of parameters this sensor provides
   * 
   * @return 7 (Runtime, Load, Bus, Current, Charge, Shunt, Power)
   */
  uint8_t getParameterCount() const override { return 7; }

  /**
   * @brief Get sensor name for display and logging
   * 
   * @return "INA219"
   */
  const char* getSensorName() const override { return "INA219"; }

  /**
   * @brief Check if sensor is available
   * 
   * Returns whether device was detected during initialization.
   * 
   * @return true if INA219 initialized successfully, false otherwise
   */
  bool isAvailable() const override { return available; }

  /**
   * @brief Detect if primary value has changed since last read
   * 
   * Checks if runtime (primary parameter) has changed.
   * Used by display layer to optimize redraw timing.
   * 
   * @return true if runtime changed (every second), false otherwise
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get sensor status for debugging
   * 
   * @return Status string (e.g., "OK" or error details)
   */
  String getStatus() const override;

  // === INA219-Specific Methods ===

  /**
   * @brief Get raw shunt voltage
   * 
   * @return Shunt voltage in mV
   */
  float getShuntVoltage() const { return values.shunt; }

  /**
   * @brief Get raw bus voltage
   * 
   * @return Bus voltage in V
   */
  float getBusVoltage() const { return values.bus; }

  /**
   * @brief Get measured current
   * 
   * @return Current in mA (negative indicates power draw)
   */
  float getCurrent() const { return values.current; }

  /**
   * @brief Get power consumption
   * 
   * @return Power in mW
   */
  float getPower() const { return values.power; }

  /**
   * @brief Get accumulated charge
   * 
   * @return Charge in mAh since sensor boot
   */
  float getCharge() const { return values.chargeMah; }

  /**
   * @brief Get runtime in seconds
   * 
   * @return Seconds elapsed since sensor initialization
   */
  uint32_t getRuntimeSeconds() const {
    if (!values.initialized) return 0;
    const uint32_t runtimeUs = micros() - values.startTimeUs;
    return runtimeUs / 1000000;
  }

  /**
   * @brief Get performance statistics (if compiled with ENABLE_STATS_REPORTING)
   * 
   * @param outReadCount Pointer to read count (set by this method)
   * @param outMinTime Pointer to minimum read time in us
   * @param outMaxTime Pointer to maximum read time in us
   * @param outAvgTime Pointer to average read time in us
   */
  void getStats(uint32_t* outReadCount, uint32_t* outMinTime,
                uint32_t* outMaxTime, uint32_t* outAvgTime) const {
    *outReadCount = stats.readCount;
    *outMinTime = stats.minTime;
    *outMaxTime = stats.maxTime;
    *outAvgTime = (stats.readCount > 0) ? (stats.totalTime / stats.readCount) : 0;
  }

protected:
  /**
   * @brief Format runtime as HH:MM:SS string
   * 
   * @param seconds Elapsed seconds
   * @return Formatted time string (e.g., "01:23:45")
   */
  String formatRuntime(uint32_t seconds) const;
};

#endif  // INA219_SENSOR_H
