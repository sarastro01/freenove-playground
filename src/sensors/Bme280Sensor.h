/**
 * @file Bme280Sensor.h
 * @brief Adafruit BME280 environmental sensor implementation
 * 
 * Wraps the Adafruit_BME280 library in a BaseSensor-compatible class.
 * Measures and tracks:
 * - Temperature (°C) - ambient temperature from thermometer
 * - Humidity (%RH) - relative humidity from capacitive sensor
 * - Pressure (hPa) - atmospheric pressure from barometer
 * - Altitude (m) - calculated altitude from pressure
 * - Dew Point (°C) - calculated from temperature and humidity
 * 
 * I2C Address: 0x77 (default, 0x76 alternative)
 * Refresh Rate: ~10 Hz (100 ms interval)
 * 
 * Architecture:
 * - Inherits from BaseSensor
 * - Encapsulates Adafruit_BME280 device instance
 * - Caches all values (not global)
 * - Manages lifecycle (init, update, status)
 * - Includes dew point calculation with Alduchov & Eskridge formula
 * 
 * Integration with main.cpp:
 *   1. Create instance: Bme280Sensor bme280Sensor;
 *   2. Register in setup: registry.registerSensor(3, &bme280Sensor, "BME280", 0x0080FF);
 *   3. Initialize: registry.initializeAll();
 *   4. Update: registry.updateAll(); (in loop)
 *   5. Access: sensor->getValue(index) or sensor->getLargeDisplayValue(...)
 */

#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#include "BaseSensor.h"
#include <Adafruit_BME280.h>
#include <math.h>

// BME280 I2C address constants
#define BME280_ADDR 0x77  // Primary address
#define BME280_ADDR_ALT 0x76  // Alternative address

// ============================================================================
// Bme280Values Struct - Forward Declaration for getValues() Accessor
// ============================================================================
// This struct is also defined in main.cpp for backward compatibility
// Both definitions must remain in sync
//
struct Bme280Values {
  float temperature;      ///< °C - ambient temperature
  float humidity;         ///< %RH - relative humidity
  float pressure;         ///< hPa - atmospheric pressure
  float altitude;         ///< m - altitude above sea level
  float dewPoint;         ///< °C - dew point temperature
};

/**
 * @class Bme280Sensor
 * @brief BME280 environmental sensor implementation
 * 
 * Provides 5 parameters:
 *   0: Temperature (°C)
 *   1: Humidity (%RH)
 *   2: Pressure (hPa)
 *   3: Altitude (m)
 *   4: Dew Point (°C)
 * 
 * The BME280 integrates three sensors in one package:
 * - Thermometer: -40°C to +85°C
 * - Humidity sensor: 0% to 100% RH
 * - Barometer: 300 hPa to 1100 hPa
 * 
 * All three are read simultaneously to ensure consistency.
 * Altitude and dew point are calculated from the raw readings.
 */
class Bme280Sensor : public BaseSensor {
private:
  /// I2C device instance
  Adafruit_BME280 device;
  
  /// Cached sensor values (replaces global Bme280Values struct)
  struct {
    float temperature;      ///< °C - ambient temperature
    float humidity;         ///< %RH - relative humidity
    float pressure;         ///< hPa - atmospheric pressure
    float altitude;         ///< m - altitude above sea level
    float dewPoint;         ///< °C - dew point temperature
    bool initialized;       ///< Whether sensor has been read once
  } values;
  
  /// Last value for change detection (used by display layer)
  mutable String lastDisplayValue;

public:
  /**
   * @brief Constructor
   * 
   * Creates sensor instance with default I2C address (0x77).
   * Device is not accessed until initialize() is called.
   */
  Bme280Sensor();
  
  /**
   * @brief Destructor
   * 
   * No special cleanup needed; Adafruit_BME280 has no resources to release.
   */
  ~Bme280Sensor() = default;

  // === BaseSensor Interface Implementation ===

  /**
   * @brief Initialize sensor and detect device
   * 
   * Calls device.begin() to detect BME280 at I2C address 0x77.
   * Sets available flag based on detection result.
   * Initializes cached values to zero.
   * 
   * Called once during setup() via registry.initializeAll().
   */
  void initialize() override;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Reads all three sensor values from device:
   * - Temperature (°C) from thermometer
   * - Humidity (%RH) from humidity sensor
   * - Pressure (Pa → hPa) from barometer
   * - Altitude (m) calculated from pressure
   * 
   * Calculates:
   * - Dew Point using Alduchov & Eskridge formula
   *   Valid for: 0-50°C, 1-100% RH, accuracy ±0.35°C
   * 
   * Validates readings:
   * - Temperature: -40°C to +85°C
   * - Humidity: 0% to 100%
   * - Pressure: 300 hPa to 1100 hPa
   * 
   * If any value is out of range, cache is NOT updated (last values persist).
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
   * @param parameterIndex 0-4 (see class description for mapping)
   * @return Formatted value string (e.g., "23.45" or "65.2")
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
   * 
   * Example output:
   *   value: "23.45"
   *   label: "Temperature"
   *   unit: "°C"
   *   color: 0xFF0000 (red)
   */
  String getLargeDisplayValue(uint8_t valueIndex,
                              const char** outLabel,
                              const char** outUnit,
                              uint16_t* outColor) const override;

  /**
   * @brief Get number of parameters this sensor provides
   * 
   * @return 5 (Temperature, Humidity, Pressure, Altitude, Dew Point)
   */
  uint8_t getParameterCount() const override { return 5; }

  /**
   * @brief Get sensor name for display and logging
   * 
   * @return "BME280"
   */
  const char* getSensorName() const override { return "BME280"; }

  /**
   * @brief Check if sensor is available
   * 
   * Returns whether device was detected during initialization.
   * 
   * @return true if BME280 initialized successfully, false otherwise
   */
  bool isAvailable() const override { return available; }

  /**
   * @brief Detect if primary value has changed since last read
   * 
   * Checks if temperature (primary parameter) has changed.
   * Used by display layer to optimize redraw timing.
   * 
   * @return true if temperature changed, false otherwise
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get sensor status for debugging
   * 
   * @return Status string (e.g., "OK" or error details)
   */
  String getStatus() const override;

  // === BME280-Specific Methods ===

  /**
   * @brief Get current temperature
   * 
   * @return Temperature in °C (-40 to +85)
   */
  float getTemperature() const { return values.temperature; }

  /**
   * @brief Get current humidity
   * 
   * @return Relative humidity in %RH (0 to 100)
   */
  float getHumidity() const { return values.humidity; }

  /**
   * @brief Get current pressure
   * 
   * @return Atmospheric pressure in hPa (300 to 1100)
   */
  float getPressure() const { return values.pressure; }

  /**
   * @brief Get current altitude
   * 
   * Calculated from pressure using standard atmosphere model.
   * Reference pressure: 1013.25 hPa (sea level standard).
   * 
   * @return Altitude in meters
   */
  float getAltitude() const { return values.altitude; }

  /**
   * @brief Get current dew point
   * 
   * Calculated using Alduchov & Eskridge formula.
   * Represents the temperature at which water vapor condenses.
   * 
   * @return Dew point in °C
   */
  float getDewPoint() const { return values.dewPoint; }

  /**
   * @brief Get complete sensor values for display/caching (PHASE 3 refactoring)
   * 
   * Returns all current sensor values packed into Bme280Values struct.
   * Used by main.cpp display layer instead of direct global variable access.
   * This enables better encapsulation without moving the struct definition.
   * 
   * @return Const reference to Bme280Values struct with current sensor state
   * 
   * @note The returned struct is reconstructed from internal values on each call.
   *       For performance-critical code, consider caching the result.
   * 
   * @see Bme280Values (defined in main.cpp)
   */
  const struct Bme280Values& getValues() const;

protected:
  /**
   * @brief Calculate dew point using Alduchov & Eskridge formula
   * 
   * Formula: Dp = (b × α) / (a - α)
   * where α = (a × T) / (b + T) + ln(RH/100)
   * 
   * Parameters: a = 17.62, b = 243.12
   * Valid for: 0-50°C, 1-100% RH
   * Accuracy: ±0.35°C
   * 
   * @param temperature Temperature in °C
   * @param humidity Relative humidity in %RH (0-100)
   * @return Dew point in °C
   */
  float calculateDewPoint(float temperature, float humidity) const;
};

#endif  // BME280_SENSOR_H
