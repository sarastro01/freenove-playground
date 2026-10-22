/**
 * @file Veml6070Sensor.h
 * @brief Adafruit VEML6070 UV sensor implementation
 * 
 * Wraps the Adafruit VEML6070 library in a BaseSensor-compatible class.
 * Measures and tracks:
 * - UV Raw (16-bit) - raw ultraviolet light intensity
 * - UV Level (calculated) - UV exposure level classification
 * 
 * I2C Address: 0x38/0x39 (fixed dual-byte protocol)
 * Refresh Rate: ~10 Hz (100 ms interval)
 * 
 * Architecture:
 * - Inherits from BaseSensor
 * - Encapsulates Adafruit_VEML6070 device instance
 * - Caches all values (not global)
 * - Manages lifecycle (init, update, status)
 * 
 * Integration with main.cpp:
 *   1. Create instance: Veml6070Sensor veml6070Sensor;
 *   2. Register in setup: registry.registerSensor(7, &veml6070Sensor, "VEML6070", 0xFF8000);
 *   3. Initialize: registry.initializeAll();
 *   4. Update: registry.updateAll(); (in loop)
 *   5. Access: sensor->getValue(index) or sensor->getLargeDisplayValue(...)
 */

#ifndef VEML6070_SENSOR_H
#define VEML6070_SENSOR_H

#include "BaseSensor.h"
#include <Adafruit_VEML6070.h>

/**
 * @class Veml6070Sensor
 * @brief VEML6070 UV sensor implementation
 * 
 * Provides 2 parameters:
 *   0: UV Raw (16-bit)
 *   1: UV Level (classification: 0=Low to 10=Extreme)
 * 
 * The VEML6070 sensor measures ultraviolet light intensity (280-400nm)
 * at a fixed integration time. The raw value is typically converted to
 * a UV index or exposure level for human interpretation.
 */
class Veml6070Sensor : public BaseSensor {
private:
  /// I2C device instance
  Adafruit_VEML6070 device;
  
  /// Cached sensor values (replaces global Veml6070Values struct)
  struct {
    uint16_t raw;            ///< Raw UV value (0-65535)
    uint16_t lsb;            ///< LSB of raw value
    uint16_t msb;            ///< MSB of raw value
    uint8_t uvLevel;         ///< UV level classification (0-10)
    bool initialized;        ///< Whether sensor has been read once
  } values;
  
  /// Last value for change detection (used by display layer)
  mutable String lastDisplayValue;

public:
  /**
   * @brief Constructor
   * 
   * Creates sensor instance with default I2C addresses (0x38/0x39).
   * Device is not accessed until initialize() is called.
   */
  Veml6070Sensor();
  
  /**
   * @brief Destructor
   * 
   * No special cleanup needed; Adafruit_VEML6070 has no resources to release.
   */
  ~Veml6070Sensor() = default;

  // === BaseSensor Interface Implementation ===

  /**
   * @brief Initialize sensor and detect device
   * 
   * Calls device.begin() to detect VEML6070 at I2C addresses 0x38/0x39.
   * Sets available flag based on detection result.
   * Initializes cached values to zero.
   * 
   * Called once during setup() via registry.initializeAll().
   */
  void initialize() override;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Reads UV intensity value from device:
   * - Raw 16-bit value via getFullLuminosity() equivalent
   * - Splits into LSB and MSB components
   * - Calculates UV level classification
   * 
   * Called periodically from main loop (~10 Hz, 100 ms intervals).
   */
  void update() override;

  /**
   * @brief Get single parameter as formatted string
   * 
   * Maps parameter index to cached value and returns as string.
   * Used by display layer for rendering sensor values.
   * 
   * @param parameterIndex 0-1 (see class description for mapping)
   * @return Formatted value string (e.g., "512" or "3")
   *         Empty string if index out of range
   */
  String getValue(uint8_t parameterIndex) const override;

  /**
   * @brief Get detailed display information for large display mode
   * 
   * Returns parameter value with label, unit, and color.
   * Used by large display mode for detailed sensor display.
   * 
   * @param valueIndex Which parameter to display (0-1)
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
   * @return 2 (UV Raw, UV Level)
   */
  uint8_t getParameterCount() const override { return 2; }

  /**
   * @brief Get sensor name for display and logging
   * 
   * @return "VEML6070"
   */
  const char* getSensorName() const override { return "VEML6070"; }

  /**
   * @brief Check if sensor is available
   * 
   * Returns whether device was detected during initialization.
   * 
   * @return true if VEML6070 initialized successfully, false otherwise
   */
  bool isAvailable() const override { return available; }

  /**
   * @brief Detect if primary value has changed since last read
   * 
   * Checks if UV Level (primary parameter) has changed.
   * Used by display layer to optimize redraw timing.
   * 
   * @return true if UV Level changed, false otherwise
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get sensor status for debugging
   * 
   * @return Status string (e.g., "OK" or error details)
   */
  String getStatus() const override;

  // === VEML6070-Specific Methods ===

  /**
   * @brief Get raw UV value
   * 
   * @return Raw UV intensity (0-65535)
   */
  uint16_t getRawUV() const { return values.raw; }

  /**
   * @brief Get UV level classification
   * 
   * @return UV level (0=Low to 10=Extreme)
   */
  uint8_t getUVLevel() const { return values.uvLevel; }

protected:
  /**
   * @brief Calculate UV level from raw value
   * 
   * Converts raw UV intensity to a 0-10 classification level.
   * Formula: level = raw / 6553.5 (maps 0-65535 to 0-10)
   * 
   * @param rawValue Raw UV intensity
   * @return UV level (0-10)
   */
  uint8_t calculateUVLevel(uint16_t rawValue) const;
};

#endif  // VEML6070_SENSOR_H
