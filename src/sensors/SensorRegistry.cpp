/**
 * @file SensorRegistry.cpp
 * @brief Implementation of SensorRegistry
 */

#include "SensorRegistry.h"

SensorRegistry::SensorRegistry() {
  // Initialize all pages to empty state
  for (auto& page : pages) {
    page.sensor = nullptr;
    page.name = nullptr;
    page.headerColor = 0xFFFF;  // White default
  }
}

void SensorRegistry::registerSensor(uint8_t pageIndex,
                                    BaseSensor* sensor,
                                    const char* name,
                                    uint16_t headerColor) {
  // Validate parameters
  if (pageIndex >= MAX_PAGES) {
    Serial.printf("ERROR: Invalid page index %d (max %d)\n", pageIndex, MAX_PAGES - 1);
    return;
  }

  if (!sensor) {
    Serial.printf("ERROR: Null sensor pointer for page %d\n", pageIndex);
    return;
  }

  if (!name) {
    name = "???";
  }

  // Register sensor
  pages[pageIndex].sensor = sensor;
  pages[pageIndex].name = name;
  pages[pageIndex].headerColor = headerColor;

  Serial.printf("Registered: Page %d -> %s (color: 0x%04X)\n", 
                pageIndex, name, headerColor);
}

BaseSensor* SensorRegistry::getSensor(uint8_t pageIndex) const {
  if (pageIndex >= MAX_PAGES) {
    return nullptr;
  }
  return pages[pageIndex].sensor;
}

const char* SensorRegistry::getPageName(uint8_t pageIndex) const {
  if (pageIndex >= MAX_PAGES) {
    return "???";
  }

  const char* name = pages[pageIndex].name;
  return name ? name : "---";
}

uint16_t SensorRegistry::getHeaderColor(uint8_t pageIndex) const {
  if (pageIndex >= MAX_PAGES) {
    return 0xFFFF;
  }
  return pages[pageIndex].headerColor;
}

bool SensorRegistry::isSensorAvailable(uint8_t pageIndex) const {
  if (pageIndex >= MAX_PAGES) {
    return false;
  }

  BaseSensor* sensor = pages[pageIndex].sensor;
  if (!sensor) {
    return false;
  }

  return sensor->isAvailable();
}

uint8_t SensorRegistry::getAvailableSensorCount() const {
  uint8_t count = 0;
  for (const auto& page : pages) {
    if (page.sensor && page.sensor->isAvailable()) {
      count++;
    }
  }
  return count;
}

void SensorRegistry::initializeAll() {
  Serial.println("\n=== Initializing Sensors ===");
  
  uint8_t initCount = 0;
  uint8_t availCount = 0;

  for (uint8_t i = 0; i < MAX_PAGES; i++) {
    BaseSensor* sensor = pages[i].sensor;
    if (!sensor) {
      continue;
    }

    initCount++;
    Serial.printf("  [%d] Initializing %s...", i, pages[i].name);
    
    sensor->initialize();
    
    if (sensor->isAvailable()) {
      availCount++;
      Serial.printf(" OK\n");
    } else {
      Serial.printf(" NOT FOUND\n");
    }
  }

  Serial.printf("=== Initialization Complete: %d/%d available ===\n\n", 
                availCount, initCount);
}

void SensorRegistry::updateAll() {
  for (const auto& page : pages) {
    if (page.sensor && page.sensor->isAvailable()) {
      page.sensor->update();
    }
  }
}

uint8_t SensorRegistry::findNextAvailablePage(uint8_t startPage, 
                                              int8_t direction) const {
  if (direction == 0) {
    direction = 1;  // Default forward
  }

  uint8_t currentPage = startPage;
  uint8_t attempts = 0;

  while (attempts < MAX_PAGES) {
    if (isSensorAvailable(currentPage)) {
      return currentPage;
    }

    // Move to next page (circular)
    if (direction > 0) {
      currentPage = (currentPage + 1) % MAX_PAGES;
    } else {
      currentPage = (currentPage > 0) ? (currentPage - 1) : (MAX_PAGES - 1);
    }

    attempts++;
  }

  // No available sensor found, return start page
  return startPage;
}

void SensorRegistry::printStatus() const {
  Serial.println("\n=== Sensor Registry Status ===");
  
  for (uint8_t i = 0; i < MAX_PAGES; i++) {
    const auto& page = pages[i];

    if (!page.sensor) {
      Serial.printf("  [%d] ---\n", i);
      continue;
    }

    const char* name = page.name ? page.name : "???";
    const char* status = page.sensor->isAvailable() ? "AVAIL" : "N/A  ";
    const char* statusDetail = page.sensor->getStatus().c_str();

    Serial.printf("  [%d] %-10s [%s] 0x%04X  %s\n",
                  i, name, status, page.headerColor, statusDetail);
  }

  Serial.printf("=== Total: %d/%d sensors available ===\n\n", 
                getAvailableSensorCount(), MAX_PAGES);
}
