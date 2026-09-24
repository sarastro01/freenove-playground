# FEATURE-SPEC-005: Implement Caching Architecture for BME280, TSL2591, SI1145 Sensors

**Date:** 2026-09-24  
**Status:** DRAFT  
**Priority:** HIGH  

## Summary

Refactor BME280, TSL2591, and SI1145 sensor reading to use the same caching architecture as other sensors (VEML6075, VEML6070, SCD30, DS2484) instead of reading directly from sensors on every display update.

## Problem Statement

**Current Issue:**
- BME280, TSL2591, and SI1145 sensors are initialized in `setup()` but lack:
  - Dedicated sensor value structures to cache readings
  - Read functions in `readSensors()` method
  - Consistent data flow pattern with other sensors
- Page display functions (`pageBme280()`, `pageTsl2591()`, `pageSi1145()`) directly read from sensors via I2C on every draw cycle (every 500ms)
- This causes:
  - Blocking I2C operations during display refresh
  - Potential conflicts with high-frequency sensor reads (INA219 @ 50Hz, Encoder @ 50Hz, Gamepad @ 40Hz)
  - Inefficient use of I2C bus
  - Inconsistent data patterns across the codebase

**Desired State:**
- All sensors follow the same caching pattern:
  1. `readSensors()` (500ms interval) → reads and caches to struct
  2. Page functions → display cached data (no I2C operations)
  3. Consistent, predictable I2C scheduling

## Affected Components

### 1. Existing Requirements Affected
- **REQ-PERF-001**: Sensor reading performance and I2C scheduling
- **REQ-ARCH-001**: Consistent sensor data flow architecture
- **REQ-UI-001**: Display refresh without blocking operations

### 2. Domain Entities Affected

**NEW Structures to Create:**
```cpp
struct Bme280Values {
  float temperature;      // °C
  float humidity;         // %
  float pressure;         // hPa
  float altitude;         // m (calculated from pressure)
  float dewPoint;         // °C (calculated)
};

struct Tsl2591Values {
  uint32_t fullSpectrum;  // Full spectrum reading
  uint32_t infrared;      // Infrared reading
  uint32_t visible;       // Visible light (calculated: full - IR)
  float lux;              // Calculated lux value
  uint16_t gain;          // Current sensor gain setting
  uint16_t integrationTime; // Integration time (ms)
};

struct Si1145Values {
  uint16_t visible;       // Visible light
  uint16_t infrared;      // Infrared
  uint16_t uv;            // UV index × 100 (raw)
  float uvIndex;          // UV index (calculated: uv / 100.0)
};
```

**MODIFIED Structures:**
- None required

**Existing Structures (to be used with new pattern):**
- `SensorStatus` - already has `.bme280`, `.tsl2591`, `.si1145` flags

### 3. Business Rules Affected

**New Rules:**
- BME280/TSL2591/SI1145 values shall be cached in their respective structs
- Cache update frequency: 500ms (consistent with `SENSOR_REFRESH_MS`)
- Page display functions shall **NOT** perform I2C reads (read-only access to cached values)
- If sensor unavailable: cached values remain at last-known state (no stale-data handling needed yet)

**Maintained Rules:**
- Sensor availability detection in `detectSensors()` unchanged
- Display refresh rate remains 500ms
- No changes to other sensor read rates (INA219 @ 50Hz, Encoder @ 50Hz, Gamepad @ 40Hz)

### 4. Data Changes

**New Global Variables:**
```cpp
Bme280Values bme280Values{};    // Add to global scope
Tsl2591Values tsl2591Values{};  // Add to global scope
Si1145Values si1145Values{};    // Add to global scope
```

**Data Flow Changes:**
| Sensor | Before | After |
|--------|--------|-------|
| BME280 | Page fn reads I2C every 500ms draw | `readSensors()` reads I2C, Page fn reads struct |
| TSL2591 | Page fn reads I2C every 500ms draw | `readSensors()` reads I2C, Page fn reads struct |
| SI1145 | Page fn reads I2C every 500ms draw | `readSensors()` reads I2C, Page fn reads struct |

### 5. API/Function Changes

**New Functions to Create:**

```cpp
void readBme280(Bme280Values &values)
{
  if (!available.bme280) return;
  
  uint32_t startTime = micros();  // For timing measurement
  
  values.temperature = bme280.readTemperature();
  values.humidity = bme280.readHumidity();
  values.pressure = bme280.readPressure() / 100.0f;
  values.altitude = bme280.readAltitude(1013.25f);
  values.dewPoint = values.temperature - ((100.0f - values.humidity) / 5.0f);
  
  uint32_t endTime = micros();
  Serial.printf("BME280 read time: %lu µs\n", endTime - startTime);
}

void readTsl2591(Tsl2591Values &values)
{
  if (!available.tsl2591) return;
  
  uint32_t startTime = micros();
  
  values.fullSpectrum = tsl2591.getFullSpectrum(SENSOR_ADDR);
  values.infrared = tsl2591.getIR(SENSOR_ADDR);
  values.visible = values.fullSpectrum - values.infrared;
  values.lux = tsl2591.calculateLux(values.fullSpectrum, values.infrared);
  values.gain = tsl2591.getGain();
  values.integrationTime = tsl2591.getIntegrationTime();
  
  uint32_t endTime = micros();
  Serial.printf("TSL2591 read time: %lu µs\n", endTime - startTime);
}

void readSi1145(Si1145Values &values)
{
  if (!available.si1145) return;
  
  uint32_t startTime = micros();
  
  values.visible = si1145.readVisible();
  values.infrared = si1145.readIR();
  values.uv = si1145.readUV();
  values.uvIndex = values.uv / 100.0f;
  
  uint32_t endTime = micros();
  Serial.printf("SI1145 read time: %lu µs\n", endTime - startTime);
}
```

**Modified Functions:**

```cpp
void readSensors()
{
  if (available.veml6075) readVeml6075(veml6075Values);
  if (available.veml6070) readVeml6070(veml6070Values);
  if (available.scd30 && scd30Ready()) readScd30(scd30Values);
  if (available.ds2484) {
    performDs2484Scan();
    readDs2484Temperatures();
  }
  // NEW: Add these three
  if (available.bme280) readBme280(bme280Values);
  if (available.tsl2591) readTsl2591(tsl2591Values);
  if (available.si1145) readSi1145(si1145Values);
}
```

**Page Functions to Modify:**
- `pageBme280()` - replace I2C reads with struct access
- `pageTsl2591()` - replace I2C reads with struct access
- `pageSi1145()` - replace I2C reads with struct access

### 6. UI Components Affected

**No UI Changes Required**
- Display content and layout remain identical
- Only internal data source changes (I2C read → cached struct)
- Users see no difference

### 7. Security/Authorization
- N/A (Embedded system, no auth/security implications)

### 8. Testing Strategy

**Note:** No formal test framework exists in this embedded project. Manual testing applies.

**Test Cases:**
1. **TC-001: BME280 Cache Update**
   - Navigate to BME280 page (Page 0)
   - Verify temperature/humidity/pressure display updates every ~500ms
   - Verify no I2C blocking during display refresh
   - Check serial output: `BME280 read time: XXX µs`

2. **TC-002: TSL2591 Cache Update**
   - Navigate to TSL2591 page (Page 6)
   - Verify lux/IR/visible values update every ~500ms
   - Verify responsive display refresh
   - Check serial output: `TSL2591 read time: XXX µs`

3. **TC-003: SI1145 Cache Update**
   - Navigate to SI1145 page (Page 5)
   - Verify visible/IR/UV values update every ~500ms
   - Verify responsive gamepad/encoder input (no I2C blocking)
   - Check serial output: `SI1145 read time: XXX µs`

4. **TC-004: Concurrent Sensor Operations**
   - Navigate between pages rapidly
   - Simultaneously manipulate encoder/gamepad
   - Verify no crashes or I2C lockups
   - Monitor serial timing output for anomalies

5. **TC-005: Sensor Unavailable**
   - Disconnect one sensor (e.g., BME280)
   - Restart board
   - Verify page shows "Sensor nicht vorhanden"
   - Verify no I2C errors in serial output

### 9. Documentation Changes

**Files to Update:**
- `sensor-pages-dashboard/README.md` - Update sensor data flow diagram/documentation
- `sensor-pages-dashboard/AUDIT-DS2484-IMPLEMENTATION.md` - May reference architecture pattern

**New Documentation:**
- Add comment block in `main.cpp` above new functions explaining caching pattern

## Acceptance Criteria

- [ ] `Bme280Values`, `Tsl2591Values`, `Si1145Values` structs created
- [ ] Global instances created: `bme280Values`, `tsl2591Values`, `si1145Values`
- [ ] Three new read functions implemented with timing measurements
- [ ] `readSensors()` calls all three new read functions (when sensor available)
- [ ] `pageBme280()` displays data from `bme280Values` struct (no I2C reads)
- [ ] `pageTsl2591()` displays data from `tsl2591Values` struct (no I2C reads)
- [ ] `pageSi1145()` displays data from `si1145Values` struct (no I2C reads)
- [ ] Code compiles without errors or warnings
- [ ] Display refresh rate remains consistent (~500ms)
- [ ] Serial timing output confirms all three sensors logged with µs precision
- [ ] Gamepad/Encoder responsiveness unchanged (no new I2C blocking)
- [ ] Manual testing passes all test cases TC-001 through TC-005
- [ ] No increase in Flash/RAM usage (< 1% impact expected)

## Open Questions

1. **Lux Calculation for TSL2591**: The current `pageTsl2591()` may not call `calculateLux()` correctly. Should we verify the API in the Adafruit library?

2. **SI1145 Gain/Integration Time**: Should we expose TSL2591's gain and integration time settings in the UI, or only read them for logging?

3. **Error Handling**: If an I2C read fails in `readBme280()` etc., should we:
   - Keep last-known values (current approach)?
   - Mark sensor as unavailable?
   - Log error to serial?

4. **Sensor Read Order**: Should we read BME280/TSL2591/SI1145 in a specific order to balance I2C bus load? Currently suggested order is arbitrary.

5. **Initialization State**: Should cached structs be initialized to meaningful default values (e.g., 0.0f, ERROR_CODE) instead of garbage values?

## Implementation Checklist

- [ ] Create issue for code review
- [ ] Implement struct definitions
- [ ] Implement three read functions
- [ ] Update `readSensors()` function
- [ ] Refactor `pageBme280()` to use cached struct
- [ ] Refactor `pageTsl2591()` to use cached struct
- [ ] Refactor `pageSi1145()` to use cached struct
- [ ] Compile and verify no errors
- [ ] Flash to device
- [ ] Execute manual test cases
- [ ] Document any discovered issues
- [ ] Update README/documentation if needed
- [ ] Close implementation issue

## Related Issues/References

- **Issue**: "BME280, TSL2591, SI1145 not cached consistently with other sensors"
- **Previous Review**: AUDIT-DS2484-IMPLEMENTATION.md (similar caching pattern)
- **Architecture Pattern**: Consistent with VEML6075, VEML6070, SCD30 pattern

---

**Author:** GitHub Copilot  
**Last Updated:** 2026-09-24
