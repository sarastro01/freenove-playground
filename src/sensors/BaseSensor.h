/**
 * @file BaseSensor.h
 * @brief Abstract base class for all sensor implementations
 * 
 * This file defines the interface that all sensor classes must implement.
 * It provides a consistent API for initializing, updating, and reading sensor data.
 * 
 * Usage:
 *   class MyCustomSensor : public BaseSensor {
 *     // implement pure virtual methods
 *   };
 * 
 * Architecture:
 *   - BaseSensor: Abstract interface (pure virtual methods)
 *   - Concrete Sensors: Ina219Sensor, Bme280Sensor, etc.
 *   - SensorRegistry: Manages all sensor instances
 */

#ifndef BASE_SENSOR_H
#define BASE_SENSOR_H

#include <Arduino.h>

/**
 * @class BaseSensor
 * @brief Abstract base class for all sensor implementations
 * 
 * Defines the interface that all sensor classes must implement.
 * Each sensor provides:
 * - Lifecycle methods (initialize, update)
 * - Data access (getValue, getLargeDisplayValue)
 * - Status queries (isAvailable, getParameterCount)
 * - Metadata (getSensorName)
 */
class BaseSensor {
public:
  virtual ~BaseSensor() = default;

  /**
   * @brief Initialize the sensor and I2C device
   * 
   * Must be called once during setup(). Sets available = true/false
   * based on device detection. Should handle I2C address conflicts.
   * 
   * Implementations:
   *   - Detect device at I2C address
   *   - Configure sensor parameters
   *   - Set available flag
   *   - Initialize cached value structures
   */
  virtual void initialize() = 0;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Called periodically (typically every 10-500ms) from main loop.
   * Should read from hardware and cache results for display.
   * Must handle I2C errors gracefully.
   * 
   * Implementations:
   *   - Read raw sensor values
   *   - Apply calibration/conversion
   *   - Cache formatted values
   *   - Update change flags if needed
   */
  virtual void update() = 0;

  /**
   * @brief Get a single parameter value as string
   * 
   * Called by display layer to render a specific parameter.
   * Index 0 = primary parameter (usually first value).
   * 
   * @param parameterIndex Index of the parameter (0 to getParameterCount()-1)
   * @return String formatted value (e.g., "1.23" or "N/A")
   * 
   * Example for INA219:
   *   0: Shunt voltage
   *   1: Bus voltage
   *   2: Load voltage
   *   3: Current
   *   4: Power
   *   5: Charge
   *   6: Runtime
   */
  virtual String getValue(uint8_t parameterIndex) const = 0;

  /**
   * @brief Get detailed display information for large display mode
   * 
   * Returns formatted value with label, unit, and color for large display.
   * Called by UI layer for rendering sensor details.
   * 
   * @param valueIndex Which value to display
   * @param outLabel Pointer to label string (set by implementation)
   * @param outUnit Pointer to unit string (set by implementation)
   * @param outColor Pointer to color value (set by implementation)
   * @return String formatted value
   * 
   * Implementation note:
   *   You can use static strings from PROGMEM or heap:
   *   - outLabel = "Shunt Voltage"
   *   - outUnit = "mV"
   *   - outColor = ILI9341_CYAN
   */
  virtual String getLargeDisplayValue(uint8_t valueIndex,
                                      const char** outLabel,
                                      const char** outUnit,
                                      uint16_t* outColor) const = 0;

  /**
   * @brief Get the number of parameters this sensor provides
   * 
   * Used by display layer to validate parameter indices.
   * 
   * @return Number of parameters (typically 5-10)
   * 
   * Examples:
   *   - INA219: 7 (shunt, bus, load, current, power, charge, runtime)
   *   - BME280: 4 (temperature, pressure, humidity, altitude)
   *   - SCD30: 6 (CO2, temp, humidity, ASC status, + ASC temp offset)
   */
  virtual uint8_t getParameterCount() const = 0;

  /**
   * @brief Get the sensor's name for display and logging
   * 
   * @return Short sensor name (e.g., "INA219", "BME280")
   */
  virtual const char* getSensorName() const = 0;

  /**
   * @brief Check if sensor is available and responding
   * 
   * Returns false if initialization failed or device is not detected.
   * Display layer should hide unavailable sensors from page navigation.
   * 
   * @return true if sensor initialized successfully and is responding
   */
  virtual bool isAvailable() const = 0;

  /**
   * @brief Optional: Detect if primary value has changed since last read
   * 
   * Used by large display mode to redraw only when data changes.
   * Base implementation returns false (always redraw).
   * Override to optimize display updates.
   * 
   * @return true if primary value changed, false otherwise
   */
  virtual bool hasValueChanged() const {
    return false;  // Default: always redraw
  }

  /**
   * @brief Optional: Get sensor status as human-readable string
   * 
   * Used for debugging. Base implementation returns empty string.
   * Override to provide initialization status, error details, etc.
   * 
   * @return Status string (e.g., "OK", "I2C Error at 0x40", "No device detected")
   */
  virtual String getStatus() const {
    return available ? "OK" : "Not Available";
  }

protected:
  /// Derived classes should set this in initialize()
  bool available = false;
};

#endif  // BASE_SENSOR_H
