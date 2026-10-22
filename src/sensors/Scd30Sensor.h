/**
 * @file Scd30Sensor.h
 * @brief SparkFun SCD30 CO2 + ASC sensor implementation
 * 
 * Wraps the SparkFun SCD30 library in a BaseSensor-compatible class.
 * Measures and tracks:
 * - CO2 (ppm) - carbon dioxide concentration
 * - Temperature (°C) - ambient temperature
 * - Humidity (%RH) - relative humidity
 * - ASC Status (boolean) - Auto Self Calibration enabled/disabled
 * - ASC Temp Offset (°C) - temperature offset for ASC adjustment
 * 
 * I2C Address: 0x61 (default, fixed)
 * Refresh Rate: ~2 Hz (500 ms interval due to sensor limitations)
 * 
 * Architecture:
 * - Inherits from BaseSensor
 * - Encapsulates SCD30 device instance from SparkFun library
 * - Caches all values (not global)
 * - Manages lifecycle (init, update, status)
 * - Includes ASC state machine for toggles
 * 
 * Notes:
 * - The SCD30 has a mandatory 2-second measurement interval
 * - Use dataAvailable() before read to avoid stale values
 * - ASC (Automatic Self Calibration) requires stable CO2-free periods
 * 
 * Integration with main.cpp:
 *   1. Create instance: Scd30Sensor scd30Sensor;
 *   2. Register in setup: registry.registerSensor(8, &scd30Sensor, "SCD30", 0x00FF00);
 *   3. Initialize: registry.initializeAll();
 *   4. Update: registry.updateAll(); (in loop)
 *   5. Access: sensor->getValue(index) or sensor->getLargeDisplayValue(...)
 */

#ifndef SCD30_SENSOR_H
#define SCD30_SENSOR_H

#include "BaseSensor.h"
#include <SparkFun_SCD30_Arduino_Library.h>

/**
 * @class Scd30Sensor
 * @brief SCD30 CO2 and climate sensor implementation
 * 
 * Provides 5 parameters:
 *   0: CO2 (ppm) - carbon dioxide concentration
 *   1: Temperature (°C) - ambient temperature
 *   2: Humidity (%RH) - relative humidity
 *   3: ASC Status (ON/OFF) - Auto Self Calibration mode
 *   4: ASC Temp Offset (°C) - temperature adjustment for ASC
 * 
 * The SCD30 integrates an NDIR (Non-Dispersive Infrared) sensor for
 * CO2 measurement along with a humidity and temperature sensor.
 * It features Automatic Self Calibration (ASC) which continuously
 * adjusts zero points based on assumed outdoor air exposure.
 */
class Scd30Sensor : public BaseSensor {
private:
  /// I2C device instance
  SCD30 device;
  
  /// Cached sensor values (replaces global Scd30Values struct)
  struct {
    float co2;               ///< CO2 concentration in ppm
    float temperature;       ///< Temperature in °C
    float humidity;          ///< Relative humidity in %RH
    bool ascEnabled;         ///< ASC (Auto Self Calibration) status
    float ascTempOffset;     ///< Temperature offset for ASC in °C
    bool initialized;        ///< Whether sensor has been read once
    uint32_t lastAscQuery;   ///< Timestamp of last ASC status query (ms)
    bool pendingAscWrite;    ///< Whether an ASC toggle is in-flight
  } values;
  
  /// Last value for change detection (used by display layer)
  mutable String lastDisplayValue;

public:
  /**
   * @brief Constructor
   * 
   * Creates sensor instance with default I2C address (0x61).
   * Device is not accessed until initialize() is called.
   */
  Scd30Sensor();
  
  /**
   * @brief Destructor
   * 
   * No special cleanup needed; SCD30 has no resources to release.
   */
  ~Scd30Sensor() = default;

  // === BaseSensor Interface Implementation ===

  /**
   * @brief Initialize sensor and detect device
   * 
   * Calls device.begin() to detect SCD30 at I2C address 0x61.
   * Sets available flag based on detection result.
   * Starts continuous measurement mode.
   * Initializes cached values to zero.
   * 
   * Called once during setup() via registry.initializeAll().
   */
  void initialize() override;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Reads all sensor values from device (when available):
   * - CO2 concentration (ppm)
   * - Temperature (°C)
   * - Humidity (%RH)
   * - ASC enabled status
   * - ASC temperature offset
   * 
   * Uses dataAvailable() to check for new measurements.
   * Queries ASC status every 3 seconds to avoid I2C flooding.
   * 
   * Called periodically from main loop (~2 Hz, 500 ms intervals).
   */
  void update() override;

  /**
   * @brief Get single parameter as formatted string
   * 
   * Maps parameter index to cached value and returns as string.
   * Used by display layer for rendering sensor values.
   * 
   * @param parameterIndex 0-4 (see class description for mapping)
   * @return Formatted value string (e.g., "412.5" or "ON")
   *         Empty string if index out of range
   */
  String getValue(uint8_t parameterIndex) const override;

  /**
   * @brief Get detailed display information for large display mode
   * 
   * Returns parameter value with label, unit, and color.
   * Used by large display mode for detailed sensor display.
   * 
   * @param valueIndex Which parameter to display (0-4)
   * @param outLabel Pointer to label string (set by this method)
   * @param outUnit Pointer to unit string (set by this method)
   * @param outColor Pointer to RGB565 color (set by this method)
   * @return Formatted value string
   */
  String getLargeDisplayValue(uint8_t valueIndex,
                              const char** outLabel,
                              const char** outUnit,
                              uint16_t* outColor) const override;

  /**
   * @brief Get number of parameters this sensor provides
   * 
   * @return 5 (CO2, Temperature, Humidity, ASC Status, ASC Temp Offset)
   */
  uint8_t getParameterCount() const override { return 5; }

  /**
   * @brief Get sensor name for display and logging
   * 
   * @return "SCD30"
   */
  const char* getSensorName() const override { return "SCD30"; }

  /**
   * @brief Check if sensor is available
   * 
   * Returns whether device was detected during initialization.
   * 
   * @return true if SCD30 initialized successfully, false otherwise
   */
  bool isAvailable() const override { return available; }

  /**
   * @brief Detect if primary value has changed since last read
   * 
   * Checks if CO2 (primary parameter) has changed.
   * Used by display layer to optimize redraw timing.
   * 
   * @return true if CO2 changed, false otherwise
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get sensor status for debugging
   * 
   * @return Status string (e.g., "OK" or error details)
   */
  String getStatus() const override;

  // === SCD30-Specific Methods ===

  /**
   * @brief Get CO2 concentration
   * 
   * @return CO2 in ppm (typically 400-5000 ppm)
   */
  float getCO2() const { return values.co2; }

  /**
   * @brief Get temperature
   * 
   * @return Temperature in °C
   */
  float getTemperature() const { return values.temperature; }

  /**
   * @brief Get humidity
   * 
   * @return Relative humidity in %RH (0-100)
   */
  float getHumidity() const { return values.humidity; }

  /**
   * @brief Get ASC (Auto Self Calibration) status
   * 
   * @return true if ASC is enabled, false otherwise
   */
  bool getASCEnabled() const { return values.ascEnabled; }

  /**
   * @brief Toggle ASC (Auto Self Calibration) mode
   * 
   * @param enable true to enable ASC, false to disable
   * @return true if command was queued successfully
   */
  bool setASCEnabled(bool enable);

  /**
   * @brief Get ASC temperature offset
   * 
   * @return Temperature offset in °C
   */
  float getASCTempOffset() const { return values.ascTempOffset; }
};

#endif  // SCD30_SENSOR_H
