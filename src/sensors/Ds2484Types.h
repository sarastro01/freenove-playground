/**
 * @file Ds2484Types.h
 * @brief Shared type definitions for DS2484 sensor
 */

#ifndef DS2484_TYPES_H
#define DS2484_TYPES_H

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
 * @struct Ds2484Values
 * @brief DS2484 adapter and temperature sensor array cache
 */
struct Ds2484Values {
  bool masterPresent;           ///< DS2484 adapter detected via I2C probe
  uint8_t sensorCount;          ///< Number of discovered sensors (0-3)
  Ds18b20Sensor sensors[3];     ///< Array of up to 3 sensors
  uint32_t lastScanMs;          ///< Timestamp of last bus scan (60s interval)
  uint32_t lastConversionMs;    ///< Timestamp of last temperature conversion (3s interval)
};

#endif // DS2484_TYPES_H
