#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>
#include <stdarg.h>

// ============================================================================
// VERSION & BUILD INFORMATION (FEATURE-SPEC-012)
// ============================================================================

#define VERSION "3.1.0"
#define BUILD_DATE "2026-10-22T17:20:00Z"

// ============================================================================
// TIMESTAMPED LOGGING FUNCTION (FEATURE-SPEC-012)
// ============================================================================

// logPrintf(): Prepends [HH:MM:SS.sss] timestamp to every Serial message
// Usage: logPrintf("[CATEGORY] message: %d", value);
// Output: [HH:MM:SS.sss] [CATEGORY] message: 123
void logPrintf(const char* format, ...);

// ============================================================================
// DIAGNOSTIC MONITORING FOR SPONTANEOUS REBOOTS
// ============================================================================

// Enable/disable diagnostics
#define ENABLE_HEAP_MONITORING 1
#define ENABLE_REBOOT_REASON 1
#define ENABLE_SENSOR_TIMING 1
#define ENABLE_I2C_MONITORING 1

// Monitoring intervals (milliseconds)
#define HEAP_MONITOR_INTERVAL 10000  // Every 10 seconds
#define I2C_MONITOR_INTERVAL 30000   // Every 30 seconds

// Thresholds for warnings
#define MIN_HEAP_WARNING 50000       // Warn if heap < 50KB
#define SLOW_SENSOR_THRESHOLD_MS 500 // Warn if sensor read > 500ms

// ============================================================================
// REBOOT REASON STRINGS
// ============================================================================

inline const char* getRebootReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_UNKNOWN:    return "UNKNOWN";
    case ESP_RST_POWERON:    return "POWER-ON";
    case ESP_RST_EXT:        return "EXTERNAL_RESET";
    case ESP_RST_SW:         return "SOFTWARE_RESET";
    case ESP_RST_PANIC:      return "PANIC";
    case ESP_RST_INT_WDT:    return "INTERNAL_WATCHDOG";
    case ESP_RST_TASK_WDT:   return "TASK_WATCHDOG";
    case ESP_RST_WDT:        return "OTHER_WATCHDOG";
    case ESP_RST_DEEPSLEEP:  return "DEEP_SLEEP_WAKEUP";
    case ESP_RST_BROWNOUT:   return "BROWNOUT";
    case ESP_RST_SDIO:       return "SDIO";
    default:                 return "UNDEFINED";
  }
}

// ============================================================================
// MONITORING FUNCTIONS
// ============================================================================

struct DiagnosticStats {
  uint32_t bootCount;
  uint32_t totalRuntimeMs;
  uint32_t minFreeHeap;
  uint32_t maxHeapUsage;
  uint32_t slowSensorReads;
  uint32_t i2cErrors;
  uint32_t lastHeapCheck;
  uint32_t lastI2cCheck;
};

extern DiagnosticStats diagStats;

inline void initDiagnostics() {
  #if ENABLE_REBOOT_REASON
  logPrintf("");
  logPrintf("[DIAG] ========== REBOOT DIAGNOSTICS ==========");
  logPrintf("[DIAG] Reboot Reason: %s", getRebootReason());
  logPrintf("[DIAG] Free Heap: %u bytes", ESP.getFreeHeap());
  logPrintf("[DIAG] Total Heap: %u bytes", ESP.getHeapSize());
  logPrintf("[DIAG] Min Free Heap (ever): %u bytes", ESP.getMinFreeHeap());
  logPrintf("[DIAG] ========================================");
  logPrintf("");
  #endif
  
  diagStats.bootCount++;
  diagStats.minFreeHeap = ESP.getFreeHeap();
  diagStats.maxHeapUsage = ESP.getHeapSize() - diagStats.minFreeHeap;
}

inline void monitorHeap(uint32_t now) {
  #if ENABLE_HEAP_MONITORING
  if (now - diagStats.lastHeapCheck >= HEAP_MONITOR_INTERVAL) {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeEver = ESP.getMinFreeHeap();
    
    // Update tracking
    if (freeHeap < diagStats.minFreeHeap) {
      diagStats.minFreeHeap = freeHeap;
    }
    
    // Warn if low
    if (freeHeap < MIN_HEAP_WARNING) {
      logPrintf("[WARN] HEAP LOW! Free: %u bytes (min ever: %u)", 
                    freeHeap, minFreeEver);
    } else {
      logPrintf("[DIAG] Heap OK: %u bytes free (min ever: %u)", 
                    freeHeap, minFreeEver);
    }
    
    diagStats.lastHeapCheck = now;
  }
  #endif
}

inline void trackSensorTiming(uint32_t startTime, const char* sensorName, uint32_t duration) {
  #if ENABLE_SENSOR_TIMING
  if (duration > SLOW_SENSOR_THRESHOLD_MS) {
    logPrintf("[WARN] SLOW SENSOR: %s took %lu ms!", sensorName, duration);
    diagStats.slowSensorReads++;
  }
  #endif
}

inline void trackI2cError(uint8_t addr, const char* device) {
  #if ENABLE_I2C_MONITORING
  logPrintf("[WARN] I2C ERROR: Device %s at 0x%02X failed!", device, addr);
  diagStats.i2cErrors++;
  #endif
}

inline void printDiagnosticReport() {
  logPrintf("");
  logPrintf("[DIAG] ========== DIAGNOSTIC REPORT ==========");
  logPrintf("[DIAG] Boot Count: %u", diagStats.bootCount);
  logPrintf("[DIAG] Min Free Heap: %u bytes", diagStats.minFreeHeap);
  logPrintf("[DIAG] Slow Sensor Reads: %u", diagStats.slowSensorReads);
  logPrintf("[DIAG] I2C Errors: %u", diagStats.i2cErrors);
  logPrintf("[DIAG] =========================================");
  logPrintf("");
}

#endif // DIAGNOSTICS_H
