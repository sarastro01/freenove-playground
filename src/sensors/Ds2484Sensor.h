/**
 * @file Ds2484Sensor.h
 * @brief Adafruit DS2484 I2C-to-1-Wire adapter + DS18B20 temperature sensors
 * 
 * Wraps the Adafruit_DS248x library in a BaseSensor-compatible class.
 * Manages multi-sensor 1-Wire bus with DS2484 I2C adapter.
 * Measures and tracks:
 * - Up to 3 DS18B20 temperature sensors on 1-Wire bus
 * - Sensor availability/online status
 * - Temperature readings with error handling
 * - Bus scan status and sensor count
 * 
 * I2C Address: 0x18 (default, fixed)
 * Scan Rate: 60-second interval
 * Read Rate: 3-second temperature conversion interval
 * 
 * Architecture:
 * - Inherits from BaseSensor
 * - Encapsulates Adafruit_DS248x I2C master instance
 * - Manages array of Ds18b20Sensor structs (1-3 sensors)
 * - Caches all values (not global)
 * - Includes periodic bus scan and temperature polling
 * 
 * Notes:
 * - DS18B20 sensors require unique ROM-IDs on the bus
 * - Temperature conversion takes ~750ms per sensor
 * - Bus scan is expensive (I2C overhead), limited to 60s interval
 * 
 * Integration with main.cpp:
 *   1. Create instance: Ds2484Sensor ds2484Sensor;
 *   2. Register in setup: registry.registerSensor(9, &ds2484Sensor, "DS2484", 0xFF00FF);
 *   3. Initialize: registry.initializeAll();
 *   4. Update: registry.updateAll(); (in loop)
 *   5. Access: sensor->getValue(index) or sensor->getLargeDisplayValue(...)
 */

#ifndef DS2484_SENSOR_H
#define DS2484_SENSOR_H

#include "BaseSensor.h"
#include <Adafruit_DS248x.h>
#include <OneWire.h>

// DS2484 I2C address
#define DS2484_ADDR 0x18

/**
 * @struct Ds18b20Sensor
 * @brief Single DS18B20 temperature sensor on 1-Wire bus
 */
struct Ds18b20Sensor {
  uint8_t address[8];      ///< 8-byte ROM-ID
  uint8_t familyCode;      ///< First byte of ROM-ID (0x28 for DS18B20)
  float temperature;       ///< Latest temperature reading (°C)
  uint32_t lastReadMs;     ///< Timestamp of last read
  bool available;          ///< Online/offline status
  uint8_t errorCount;      ///< Error counter (0-3, then offline)
};

/**
 * @class Ds2484Sensor
 * @brief DS2484 I2C-to-1-Wire adapter + temperature sensor array implementation
 * 
 * Provides variable parameters based on discovered sensors:
 *   0-2: Temperature of sensor 0-2 (if discovered)
 *   3: Sensor count (0-3)
 *   4: Bus scan status
 * 
 * Note: Temperature parameters are only valid if sensorCount > index.
 * Display layer should check parameter availability before rendering.
 * 
 * The DS2484 adapter communicates with up to 3 DS18B20 temperature sensors
 * on a single 1-Wire bus. Each sensor is identified by its 8-byte ROM-ID.
 */
class Ds2484Sensor : public BaseSensor {
private:
  /// I2C device instance (DS2484 adapter)
  Adafruit_DS248x device;
  
  /// Cached sensor values (replaces global Ds2484Values struct)
  struct {
    bool masterPresent;           ///< DS2484 adapter detected via I2C probe
    uint8_t sensorCount;          ///< Number of discovered sensors (0-3)
    Ds18b20Sensor sensors[3];     ///< Array of up to 3 sensors
    uint32_t lastScanMs;          ///< Timestamp of last bus scan (60s interval)
    uint32_t lastConversionMs;    ///< Timestamp of last temperature conversion (3s interval)
    bool initialized;             ///< Whether sensor has been read once
  } values;
  
  /// Last value for change detection (used by display layer)
  mutable String lastDisplayValue;

public:
  /**
   * @brief Constructor
   * 
   * Creates sensor instance with default I2C address (0x18).
   * Device is not accessed until initialize() is called.
   */
  Ds2484Sensor();
  
  /**
   * @brief Destructor
   * 
   * No special cleanup needed; Adafruit_DS248x has no resources to release.
   */
  ~Ds2484Sensor() = default;

  // === BaseSensor Interface Implementation ===

  /**
   * @brief Initialize sensor and detect device
   * 
   * Calls device.begin() to detect DS2484 at I2C address 0x18.
   * Sets available flag based on detection result.
   * Initializes cached values to zero.
   * Does NOT perform bus scan (deferred to update()).
   * 
   * Called once during setup() via registry.initializeAll().
   */
  void initialize() override;

  /**
   * @brief Read sensor data and update cached values
   * 
   * Performs two operations:
   * 1. Bus scan at 60-second interval
   *    - Discovers all DS18B20 sensors on 1-Wire bus
   *    - Stores ROM-IDs and initializes sensor structs
   *    - Limited to 3 sensors maximum
   * 
   * 2. Temperature polling at 3-second interval
   *    - Sends Convert T command to all sensors
   *    - Reads cached temperature values
   *    - Updates error counters for offline sensors
   * 
   * Called periodically from main loop (every 10-500ms).
   * Internally throttled to 60s (scan) and 3s (reads).
   */
  void update() override;

  /**
   * @brief Get single parameter as formatted string
   * 
   * Maps parameter index to cached value and returns as string.
   * Used by display layer for rendering sensor values.
   * 
   * @param parameterIndex 0-4 (see class description for mapping)
   * @return Formatted value string (e.g., "23.45" or "3")
   *         Empty string if index out of range or sensor not found
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
   * Base is 5 (Temp0, Temp1, Temp2, Count, Status)
   * but discovered sensors determine actual usage.
   * 
   * @return 5 (fixed for compatibility)
   */
  uint8_t getParameterCount() const override { return 5; }

  /**
   * @brief Get sensor name for display and logging
   * 
   * @return "DS2484"
   */
  const char* getSensorName() const override { return "DS2484"; }

  /**
   * @brief Check if sensor is available
   * 
   * Returns whether DS2484 adapter was detected.
   * Note: Returns true even if no 1-Wire sensors discovered.
   * 
   * @return true if DS2484 initialized successfully, false otherwise
   */
  bool isAvailable() const override { return available; }

  /**
   * @brief Detect if primary value has changed since last read
   * 
   * Checks if sensor count or first sensor temperature changed.
   * Used by display layer to optimize redraw timing.
   * 
   * @return true if values changed, false otherwise
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get sensor status for debugging
   * 
   * @return Status string (e.g., "OK: 2 sensors" or error details)
   */
  String getStatus() const override;

  // === DS2484-Specific Methods ===

  /**
   * @brief Get number of discovered sensors
   * 
   * @return Sensor count (0-3)
   */
  uint8_t getSensorCount() const { return values.sensorCount; }

  /**
   * @brief Get temperature from specific sensor
   * 
   * @param sensorIndex Index of sensor (0-2)
   * @return Temperature in °C, or 0.0f if sensor not available
   */
  float getSensorTemperature(uint8_t sensorIndex) const;

  /**
   * @brief Get sensor ROM-ID as hex string
   * 
   * @param sensorIndex Index of sensor (0-2)
   * @return ROM-ID formatted as "XX:XX:XX:XX:XX:XX:XX:XX"
   */
  String getSensorRomId(uint8_t sensorIndex) const;

protected:
  /**
   * @brief Perform 1-Wire bus scan for DS18B20 sensors
   * 
   * Called at 60-second interval by update().
   * Discovers all devices on bus and stores ROM-IDs.
   * Verifies ROM-ID CRC before adding sensor.
   */
  void performBusScan();

  /**
   * @brief Read temperatures from all discovered sensors
   * 
   * Called at 3-second interval by update().
   * Sends Convert T to all sensors, reads cached values.
   * Updates error counters for offline sensors.
   */
  void readAllTemperatures();

  /**
   * @brief Reset 1-Wire bus via DS2484
   * 
   * @return true if bus reset successful
   */
  bool resetBus();

  /**
   * @brief Format ROM-ID as hex string
   * 
   * @param address Pointer to 8-byte ROM-ID array
   * @return Formatted string "XX:XX:XX:XX:XX:XX:XX:XX"
   */
  String formatRomId(const uint8_t *address) const;
};

#endif  // DS2484_SENSOR_H
