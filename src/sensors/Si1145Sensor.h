/**
 * @file Si1145Sensor.h
 * @brief Adafruit SI1145 multi-spectrum light sensor implementation
 * 
 * Wraps the Adafruit_SI1145 library in a BaseSensor-compatible class.
 * Measures and tracks:
 * - Visible light (raw 16-bit) - visible spectrum intensity
 * - Infrared light (raw 16-bit) - infrared spectrum intensity
 * - UV light (raw value) - ultraviolet radiation
 * - UV Index (calculated) - standard UV exposure index
 * 
 * I2C Address: 0x60 (default, fixed)
 * Refresh Rate: ~20 Hz (50 ms interval)
 * 
 * Architecture:
 * - Inherits from BaseSensor
 * - Encapsulates Adafruit_SI1145 device instance
 * - Caches all values (not global)
 * - Manages lifecycle (init, update, status)
 * 
 * Integration with main.cpp:
 *   1. Create instance: Si1145Sensor si1145Sensor;
 *   2. Register in setup: registry.registerSensor(5, &si1145Sensor, "SI1145", 0xFF00FF);
 *   3. Initialize: registry.initializeAll();
 *   4. Update: registry.updateAll(); (in loop)
 *   5. Access: sensor->getValue(index) or sensor->getLargeDisplayValue(...)
 */

#ifndef SI1145_SENSOR_H
#define SI1145_SENSOR_H

#include "BaseSensor.h"
#include <Adafruit_SI1145.h>

/**
 * @class Si1145Sensor
 * @brief SI1145 multi-spectrum light sensor implementation
 * 
 * Provides 4 parameters:
 *   0: Visible Light (raw 16-bit)
 *   1: Infrared Light (raw 16-bit)
 *   2: UV Raw Value
 *   3: UV Index (calculated from raw / 100.0)
 * 
 * The SI1145 sensor measures light across three spectra simultaneously:
 * visible (400-1000nm), infrared (900-1700nm), and ultraviolet (280-400nm).
 * It uses photodiode arrays and an I2C interface for communication.
 */
class Si1145Sensor : public BaseSensor {
private:
  /// I2C device instance
  Adafruit_SI1145 device;
  
  /// Cached sensor values (replaces global Si1145Values struct)
  struct {
    uint16_t visible;        ///< Visible light raw (16-bit)
    uint16_t infrared;       ///< Infrared raw (16-bit)
    uint16_t uv;             ///< UV raw value (from register 0x2C)
    float uvIndex;           ///< UV index (calculated: uv / 100.0f)
    bool initialized;        ///< Whether sensor has been read once
  } values;
  
  /// Last value for change detection (used by display layer)
  mutable String lastDisplayValue;

public:
  /**
   * @brief Constructor
   * 
   * Creates sensor instance with default I2C address (0x60).
   * Device is not accessed until initialize() is called.
   */
  Si1145Sensor();
  
  /**
   * @brief Destructor
   * 
   * No special cleanup needed; Adafruit_SI1145 has no resources to release.
   */
  ~Si1145Sensor() = default;

  // === BaseSensor Interface Implementation ===

  /**
   * @brief Initialize sensor and detect device
   * 
   * Calls device.begin() to detect SI1145 at I2C address 0x60.
   * Sets available flag based on detection result.
   * Initializes cached values to zero.
   * 
   * Called once during setup() via registry.initializeAll().
   */
  void initialize() override;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Reads all 3 light spectrum values from device:
   * - Visible light raw (16-bit)
   * - Infrared raw (16-bit)
   * - UV raw value
   * 
   * Calculates:
   * - UV Index (float) from raw / 100.0f
   * 
   * Error checks: If all three values are zero, likely I2C error.
   * Does NOT update cache in case of error (last values persist).
   * 
   * Called periodically from main loop (~20 Hz, 50 ms intervals).
   */
  void update() override;

  /**
   * @brief Get single parameter as formatted string
   * 
   * Maps parameter index to cached value and returns as string.
   * Used by display layer for rendering sensor values.
   * 
   * @param parameterIndex 0-3 (see class description for mapping)
   * @return Formatted value string (e.g., "1024" or "2.34")
   *         Empty string if index out of range
   */
  String getValue(uint8_t parameterIndex) const override;

  /**
   * @brief Get detailed display information for large display mode
   * 
   * Returns parameter value with label, unit, and color.
   * Used by large display mode for detailed sensor display.
   * 
   * @param valueIndex Which parameter to display (0-3)
   * @param outLabel Pointer to label string (set by this method)
   * @param outUnit Pointer to unit string (set by this method)
   * @param outColor Pointer to RGB565 color (set by this method)
   * @return Formatted value string
   * 
   * Example output:
   *   value: "512"
   *   label: "Visible Light"
   *   unit: "raw"
   *   color: 0xFFFF00 (yellow)
   */
  String getLargeDisplayValue(uint8_t valueIndex,
                              const char** outLabel,
                              const char** outUnit,
                              uint16_t* outColor) const override;

  /**
   * @brief Get number of parameters this sensor provides
   * 
   * @return 4 (Visible, Infrared, UV Raw, UV Index)
   */
  uint8_t getParameterCount() const override { return 4; }

  /**
   * @brief Get sensor name for display and logging
   * 
   * @return "SI1145"
   */
  const char* getSensorName() const override { return "SI1145"; }

  /**
   * @brief Check if sensor is available
   * 
   * Returns whether device was detected during initialization.
   * 
   * @return true if SI1145 initialized successfully, false otherwise
   */
  bool isAvailable() const override { return available; }

  /**
   * @brief Detect if primary value has changed since last read
   * 
   * Checks if UV Index (primary parameter) has changed.
   * Used by display layer to optimize redraw timing.
   * 
   * @return true if UV Index changed, false otherwise
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get sensor status for debugging
   * 
   * @return Status string (e.g., "OK" or error details)
   */
  String getStatus() const override;

  // === SI1145-Specific Methods ===

  /**
   * @brief Get raw visible light value
   * 
   * @return Visible light intensity (0-65535)
   */
  uint16_t getVisible() const { return values.visible; }

  /**
   * @brief Get raw infrared value
   * 
   * @return Infrared intensity (0-65535)
   */
  uint16_t getInfrared() const { return values.infrared; }

  /**
   * @brief Get raw UV value
   * 
   * @return UV raw value (0-65535)
   */
  uint16_t getUVRaw() const { return values.uv; }

  /**
   * @brief Get calculated UV index
   * 
   * Standard formula: UV Index = raw / 100.0
   * 
   * @return UV Index as float (0.0 to ~15.0+)
   */
  float getUVIndex() const { return values.uvIndex; }
};

#endif  // SI1145_SENSOR_H
