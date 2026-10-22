/**
 * @file SensorRegistry.h
 * @brief Central registry for managing all sensor instances
 * 
 * This class maintains a list of all sensors, maps them to pages,
 * and provides a unified interface for initialization and updates.
 * 
 * Architecture:
 *   - Each page (0-9) can have one sensor
 *   - Page metadata: sensor pointer, name, header color
 *   - Centralized initialization and update
 *   - Query methods for display layer
 * 
 * Usage in main.cpp:
 *   SensorRegistry registry;
 *   registry.registerSensor(0, &bme280Sensor, "BME280", 0xFF0000);
 *   registry.initializeAll();
 *   
 *   loop() {
 *     registry.updateAll();
 *     if (registry.isSensorAvailable(currentPage)) {
 *       BaseSensor* sensor = registry.getSensor(currentPage);
 *       String value = sensor->getValue(0);
 *     }
 *   }
 */

#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include "BaseSensor.h"
#include <array>

/**
 * @class SensorRegistry
 * @brief Central management of all sensor instances
 * 
 * Responsibilities:
 * - Register sensors with page index and metadata
 * - Initialize all sensors once during setup
 * - Update all sensors periodically
 * - Query sensor availability
 * - Provide page metadata (name, color)
 * 
 * Design decision:
 *   - Fixed array of 10 pages (see docs/07-ui.md for page mapping)
 *   - Each page has one sensor (1:1 mapping)
 *   - nullptr entries represent unavailable pages
 */
class SensorRegistry {
public:
  /// Maximum number of pages in the display
  static constexpr uint8_t MAX_PAGES = 10;

private:
  /// Metadata for one page-sensor mapping
  struct PageMapping {
    BaseSensor* sensor;        ///< Pointer to sensor instance (nullptr if unused)
    const char* name;          ///< Display name (e.g., "INA219")
    uint16_t headerColor;      ///< RGB565 color for page header
  };

  /// Array of page mappings
  std::array<PageMapping, MAX_PAGES> pages;

public:
  /// Constructor: Initialize all pages to empty
  SensorRegistry();

  /**
   * @brief Register a sensor for a specific page
   * 
   * Must be called before initializeAll().
   * Can be called multiple times if sensor needs re-registration.
   * 
   * @param pageIndex Page number (0-9)
   * @param sensor Pointer to sensor instance
   * @param name Display name (e.g., "INA219")
   * @param headerColor RGB565 header color
   * 
   * @note Sensor pointer must remain valid throughout application lifetime
   * @note Name should be a static string (PROGMEM or static data)
   */
  void registerSensor(uint8_t pageIndex,
                      BaseSensor* sensor,
                      const char* name,
                      uint16_t headerColor);

  /**
   * @brief Get sensor instance for a page
   * 
   * @param pageIndex Page number (0-9)
   * @return Pointer to sensor, or nullptr if page has no sensor
   */
  BaseSensor* getSensor(uint8_t pageIndex) const;

  /**
   * @brief Get display name for a page
   * 
   * @param pageIndex Page number (0-9)
   * @return Display name (e.g., "INA219"), or "???" if page invalid
   */
  const char* getPageName(uint8_t pageIndex) const;

  /**
   * @brief Get header color for a page
   * 
   * @param pageIndex Page number (0-9)
   * @return RGB565 color value
   */
  uint16_t getHeaderColor(uint8_t pageIndex) const;

  /**
   * @brief Check if sensor on page is available
   * 
   * Queries sensor->isAvailable() if page has sensor.
   * 
   * @param pageIndex Page number (0-9)
   * @return true if sensor initialized and responding
   */
  bool isSensorAvailable(uint8_t pageIndex) const;

  /**
   * @brief Count available sensors
   * 
   * Useful for display layer to determine if multiple pages exist.
   * 
   * @return Number of available sensors across all pages
   */
  uint8_t getAvailableSensorCount() const;

  /**
   * @brief Initialize all registered sensors
   * 
   * Should be called once in setup().
   * Iterates through all pages and calls sensor->initialize().
   * 
   * @note This can take several hundred milliseconds
   */
  void initializeAll();

  /**
   * @brief Update all registered sensors
   * 
   * Should be called periodically from main loop (typically every 10-500ms).
   * Only updates available sensors (skips unavailable ones).
   * 
   * @note This should complete in < 50ms under normal conditions
   */
  void updateAll();

  /**
   * @brief Find next page with available sensor
   * 
   * Useful for skipping unavailable pages in display navigation.
   * 
   * @param startPage Starting page for search
   * @param direction +1 for forward, -1 for backward
   * @return Page index of available sensor, or startPage if none found
   */
  uint8_t findNextAvailablePage(uint8_t startPage, int8_t direction) const;

  /**
   * @brief Debugging: Print registry status to Serial
   * 
   * Format:
   *   Page 0: INA219 [AVAIL] 0xFFFF
   *   Page 1: BME280 [N/A]   0xFF00
   *   ...
   */
  void printStatus() const;

  /**
   * @brief Get total number of registered pages
   * 
   * @return MAX_PAGES (10)
   */
  uint8_t getTotalPages() const { return MAX_PAGES; }
};

#endif  // SENSOR_REGISTRY_H
