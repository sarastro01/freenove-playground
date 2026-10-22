/**
 * @file Tsl2591Sensor.h
 * @brief Adafruit TSL2591 digital light sensor implementation
 * 
 * Wraps the Adafruit_TSL2591 library in a BaseSensor-compatible class.
 * Measures and tracks:
 * - Full Spectrum (raw 16-bit) - combined visible + infrared
 * - Infrared (raw 16-bit) - infrared spectrum only
 * - Visible (calculated) - full spectrum minus infrared
 * - Lux (calculated) - illuminance in lux from full spectrum and IR
 * - Gain (current setting) - sensor gain configuration
 * - Integration Time (current setting) - exposure time in milliseconds
 * 
 * I2C Address: 0x29 (default, fixed)
 * Refresh Rate: ~5 Hz (200 ms interval, configurable)
 * 
 * Architecture:
 * - Inherits from BaseSensor
 * - Encapsulates Adafruit_TSL2591 device instance
 * - Caches all values (not global)
 * - Manages lifecycle (init, update, status)
 * - Includes async measurement state machine (from main.cpp)
 * 
 * Integration with main.cpp:
 *   1. Create instance: Tsl2591Sensor tsl2591Sensor;
 *   2. Register in setup: registry.registerSensor(4, &tsl2591Sensor, "TSL2591", 0x00FF00);
 *   3. Initialize: registry.initializeAll();
 *   4. Update: registry.updateAll(); (in loop)
 *   5. Access: sensor->getValue(index) or sensor->getLargeDisplayValue(...)
 */

#ifndef TSL2591_SENSOR_H
#define TSL2591_SENSOR_H

#include "BaseSensor.h"
#include <Adafruit_TSL2591.h>

/**
 * @class Tsl2591Sensor
 * @brief TSL2591 digital light sensor implementation
 * 
 * Provides 6 parameters:
 *   0: Full Spectrum (raw 16-bit CHAN0)
 *   1: Infrared (raw 16-bit CHAN1)
 *   2: Visible (calculated, full - infrared)
 *   3: Lux (calculated illuminance)
 *   4: Gain (current sensor gain setting)
 *   5: Integration Time (milliseconds)
 * 
 * The TSL2591 uses two photodiode channels (visible and infrared)
 * to calculate both raw light levels and calibrated lux values.
 * Integration time can be set from 100ms to 600ms for different
 * sensitivity/speed trade-offs.
 */
class Tsl2591Sensor : public BaseSensor {
private:
  /// I2C device instance
  Adafruit_TSL2591 device;
  
  /// Cached sensor values (replaces global Tsl2591Values struct)
  struct {
    uint32_t fullSpectrum;       ///< Full spectrum raw (16-bit CHAN0)
    uint32_t infrared;           ///< Infrared raw (16-bit CHAN1)
    uint32_t visible;            ///< Visible light (calculated: full - IR)
    float lux;                   ///< Illuminance in lux
    uint16_t gain;               ///< Sensor gain setting
    uint16_t integrationTime;    ///< Integration time in ms
    bool initialized;            ///< Whether sensor has been read once
  } values;
  
  /// Last value for change detection (used by display layer)
  mutable String lastDisplayValue;

public:
  /**
   * @brief Constructor
   * 
   * Creates sensor instance with default I2C address (0x29).
   * Uses Adafruit_TSL2591(2591) constructor convention.
   * Device is not accessed until initialize() is called.
   */
  Tsl2591Sensor();
  
  /**
   * @brief Destructor
   * 
   * No special cleanup needed; Adafruit_TSL2591 has no resources to release.
   */
  ~Tsl2591Sensor() = default;

  // === BaseSensor Interface Implementation ===

  /**
   * @brief Initialize sensor and detect device
   * 
   * Calls device.begin() to detect TSL2591 at I2C address 0x29.
   * Sets available flag based on detection result.
   * Configures default gain and integration time.
   * Initializes cached values to zero.
   * 
   * Called once during setup() via registry.initializeAll().
   * 
   * Default Configuration:
   *   - Gain: TSL2591_GAIN_MED (high sensitivity, medium dynamic range)
   *   - Integration Time: TSL2591_INTEGRATIONTIME_200MS (200ms, balanced)
   */
  void initialize() override;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Reads both light spectrum channels from device:
   * - Full spectrum raw (16-bit CHAN0)
   * - Infrared raw (16-bit CHAN1)
   * 
   * Calculates:
   * - Visible (raw) = Full Spectrum - Infrared
   * - Lux (illuminance) using Adafruit lux calculation
   * - Retrieves current gain and integration time settings
   * 
   * Error checks: If both channels read as zero, likely I2C error.
   * Does NOT update cache in case of error (last values persist).
   * 
   * Called periodically from main loop (~5 Hz, 200 ms intervals).
   */
  void update() override;

  /**
   * @brief Get single parameter as formatted string
   * 
   * Maps parameter index to cached value and returns as string.
   * Used by display layer for rendering sensor values.
   * 
   * @param parameterIndex 0-5 (see class description for mapping)
   * @return Formatted value string (e.g., "1024" or "125.45")
   *         Empty string if index out of range
   */
  String getValue(uint8_t parameterIndex) const override;

  /**
   * @brief Get detailed display information for large display mode
   * 
   * Returns parameter value with label, unit, and color.
   * Used by large display mode for detailed sensor display.
   * 
   * @param valueIndex Which parameter to display (0-5)
   * @param outLabel Pointer to label string (set by this method)
   * @param outUnit Pointer to unit string (set by this method)
   * @param outColor Pointer to RGB565 color (set by this method)
   * @return Formatted value string
   * 
   * Example output:
   *   value: "512"
   *   label: "Full Spectrum"
   *   unit: "raw"
   *   color: 0xFF8000 (orange)
   */
  String getLargeDisplayValue(uint8_t valueIndex,
                              const char** outLabel,
                              const char** outUnit,
                              uint16_t* outColor) const override;

  /**
   * @brief Get number of parameters this sensor provides
   * 
   * @return 6 (Full Spectrum, IR, Visible, Lux, Gain, Integration Time)
   */
  uint8_t getParameterCount() const override { return 6; }

  /**
   * @brief Get sensor name for display and logging
   * 
   * @return "TSL2591"
   */
  const char* getSensorName() const override { return "TSL2591"; }

  /**
   * @brief Check if sensor is available
   * 
   * Returns whether device was detected during initialization.
   * 
   * @return true if TSL2591 initialized successfully, false otherwise
   */
  bool isAvailable() const override { return available; }

  /**
   * @brief Detect if primary value has changed since last read
   * 
   * Checks if Lux (primary parameter) has changed.
   * Used by display layer to optimize redraw timing.
   * 
   * @return true if Lux changed, false otherwise
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get sensor status for debugging
   * 
   * @return Status string (e.g., "OK" or error details)
   */
  String getStatus() const override;

  // === TSL2591-Specific Methods ===

  /**
   * @brief Get raw full spectrum value
   * 
   * @return Full spectrum raw (0-65535)
   */
  uint32_t getFullSpectrum() const { return values.fullSpectrum; }

  /**
   * @brief Get raw infrared value
   * 
   * @return Infrared raw (0-65535)
   */
  uint32_t getInfrared() const { return values.infrared; }

  /**
   * @brief Get calculated visible light
   * 
   * @return Visible = Full Spectrum - Infrared (0-65535)
   */
  uint32_t getVisible() const { return values.visible; }

  /**
   * @brief Get illuminance in lux
   * 
   * @return Lux value calculated by Adafruit library
   */
  float getLux() const { return values.lux; }

  /**
   * @brief Get current gain setting
   * 
   * @return Gain value (TSL2591_GAIN_LOW or TSL2591_GAIN_MED or TSL2591_GAIN_HIGH)
   */
  uint16_t getGain() const { return values.gain; }

  /**
   * @brief Get current integration time
   * 
   * @return Integration time in milliseconds
   */
  uint16_t getIntegrationTime() const { return values.integrationTime; }
};

#endif  // TSL2591_SENSOR_H
