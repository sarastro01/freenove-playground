/**
 * @file Veml6075Sensor.h
 * @brief SparkFun VEML6075 UVA/UVB sensor implementation
 * 
 * Wraps the SparkFun VEML6075 library in a BaseSensor-compatible class.
 * Measures and tracks:
 * - UVA raw (16-bit) - ultraviolet A radiation intensity
 * - UVB raw (16-bit) - ultraviolet B radiation intensity
 * - UVA compensated (float) - UVA with compensation
 * - UVB compensated (float) - UVB with compensation
 * - UV Index (float) - standard UV exposure index
 * 
 * I2C Address: 0x10 (default, fixed)
 * Refresh Rate: ~10 Hz (100 ms interval)
 * 
 * Architecture:
 * - Inherits from BaseSensor
 * - Encapsulates VEML6075 device instance
 * - Caches all values (not global)
 * - Manages lifecycle (init, update, status)
 * 
 * Integration with main.cpp:
 *   1. Create instance: Veml6075Sensor veml6075Sensor;
 *   2. Register in setup: registry.registerSensor(6, &veml6075Sensor, "VEML6075", 0x00FF00);
 *   3. Initialize: registry.initializeAll();
 *   4. Update: registry.updateAll(); (in loop)
 *   5. Access: sensor->getValue(index) or sensor->getLargeDisplayValue(...)
 */

#ifndef VEML6075_SENSOR_H
#define VEML6075_SENSOR_H

#include "BaseSensor.h"
#include <SparkFun_VEML6075_Arduino_Library.h>

// ============================================================================
// Veml6075Values Struct - Forward Declaration for getValues() Accessor
// ============================================================================
struct Veml6075Values {
  uint16_t uva;            ///< UVA raw (16-bit)
  uint16_t uvb;            ///< UVB raw (16-bit)
  uint16_t comp1;          ///< Compensation 1
  uint16_t comp2;          ///< Compensation 2
  float uvaComp;           ///< UVA compensated
  float uvbComp;           ///< UVB compensated
  float index;             ///< UV Index
};

/**
 * @class Veml6075Sensor
 * @brief VEML6075 UVA/UVB sensor implementation
 * 
 * Provides 5 parameters:
 *   0: UVA Raw (16-bit intensity)
 *   1: UVB Raw (16-bit intensity)
 *   2: UVA Compensated (float)
 *   3: UVB Compensated (float)
 *   4: UV Index (calculated)
 * 
 * The VEML6075 sensor measures both UVA (315-400nm) and UVB (280-315nm)
 * radiation separately using photodiode arrays. It also provides internal
 * compensation for visible light interference.
 */
class Veml6075Sensor : public BaseSensor {
private:
  /// I2C device instance
  VEML6075 device;
  
  /// Cached sensor values (replaces global Veml6075Values struct)
  struct {
    uint16_t uvaRaw;         ///< Raw UVA intensity
    uint16_t uvbRaw;         ///< Raw UVB intensity
    uint16_t comp1;          ///< Compensation 1 (comp diode)
    uint16_t comp2;          ///< Compensation 2 (comp diode)
    float uvaComp;           ///< Compensated UVA
    float uvbComp;           ///< Compensated UVB
    float index;             ///< UV Index (calculated)
    bool initialized;        ///< Whether sensor has been read once
  } values;
  
  /// Last value for change detection (used by display layer)
  mutable String lastDisplayValue;

public:
  /**
   * @brief Constructor
   * 
   * Creates sensor instance with default I2C address (0x10).
   * Device is not accessed until initialize() is called.
   */
  Veml6075Sensor();
  
  /**
   * @brief Destructor
   * 
   * No special cleanup needed; VEML6075 has no resources to release.
   */
  ~Veml6075Sensor() = default;

  // === BaseSensor Interface Implementation ===

  /**
   * @brief Initialize sensor and detect device
   * 
   * Calls device.begin() to detect VEML6075 at I2C address 0x10.
   * Sets available flag based on detection result.
   * Initializes cached values to zero.
   * 
   * Called once during setup() via registry.initializeAll().
   */
  void initialize() override;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Reads all light spectrum values from device:
   * - UVA raw intensity (16-bit)
   * - UVB raw intensity (16-bit)
   * - Compensation diode 1 raw
   * - Compensation diode 2 raw
   * - Compensated UVA (float)
   * - Compensated UVB (float)
   * - UV Index (calculated)
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
   * @return Formatted value string (e.g., "512" or "3.45")
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
   * @return 5 (UVA Raw, UVB Raw, UVA Comp, UVB Comp, UV Index)
   */
  uint8_t getParameterCount() const override { return 5; }

  /**
   * @brief Get sensor name for display and logging
   * 
   * @return "VEML6075"
   */
  const char* getSensorName() const override { return "VEML6075"; }

  /**
   * @brief Check if sensor is available
   * 
   * Returns whether device was detected during initialization.
   * 
   * @return true if VEML6075 initialized successfully, false otherwise
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

  // === VEML6075-Specific Methods ===

  /**
   * @brief Get raw UVA intensity
   * 
   * @return UVA raw (0-65535)
   */
  uint16_t getUVARaw() const { return values.uvaRaw; }

  /**
   * @brief Get raw UVB intensity
   * 
   * @return UVB raw (0-65535)
   */
  uint16_t getUVBRaw() const { return values.uvbRaw; }

  /**
   * @brief Get compensated UVA
   * 
   * @return UVA compensated value
   */
  float getUVACompensated() const { return values.uvaComp; }

  /**
   * @brief Get compensated UVB
   * 
   * @return UVB compensated value
   */
  float getUVBCompensated() const { return values.uvbComp; }

  /**
   * @brief Get UV Index
   * 
   * @return UV Index (typically 0-15+)
   */
  float getUVIndex() const { return values.index; }

  /**
   * @brief Get complete sensor values for display/caching (PHASE 3 refactoring)
   * 
   * @return Const reference to Veml6075Values struct
   */
  const struct Veml6075Values& getValues() const;
};

#endif  // VEML6075_SENSOR_H
