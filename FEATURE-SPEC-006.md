# FEATURE-SPEC-006: Non-Blocking Sensor Architecture with Rotated Reads

**Status:** Specification (Not Implemented)  
**Created:** 2026-09-24  
**Priority:** High (Performance/Responsiveness)  
**Category:** Performance Optimization + Architecture Refactoring

---

## Executive Summary

The current system suffers from a **250 ms blocking delay** in the main loop during sensor reads (particularly TSL2591), making the user interface feel sluggish. User input (gamepad, encoder, INA219) is delayed waiting for slow sensors to complete.

**FEATURE-SPEC-006** decouples slow sensor reads from fast input handling by:
1. **Rotating sensor reads** — Each of 5 slow sensors read sequentially every 200 ms (not 500 ms)
2. **Keeping fast input on dedicated schedule** — Gamepad/Encoder/INA219 read within **20 ms guarantee**
3. **Async TSL2591 reads** — Two-stage measurement (start → wait → read) to eliminate blocking
4. **Decoupled display updates** — Display updates separately from gamepad input
5. **Performance metrics** — Per-sensor timing + aggregated stats (per second)

**Result:** Responsive UI, background sensor polling, no frame drops.

---

## Problem Analysis

### Current Behavior (BEFORE)

```
Main Loop (BLOCKING):
├─ Gamepad read: 5 ms
├─ Display update: 2 ms
├─ Encoder read: 1 ms (conditional)
├─ INA219 read: 1.2 ms
└─ readSensors(): 250 ms (BLOCKING!)
   ├─ BME280: 2.2 ms
   ├─ TSL2591: 200 ms ← Main bottleneck!
   ├─ SI1145: 0.7 ms
   ├─ VEML6075: ? ms
   ├─ VEML6070: ? ms
   └─ SCD30: ? ms

Total per loop: ~260 ms
Loop frequency: ~3.8 Hz (very slow!)
```

### Issues

1. **Blocked Loop:** readSensors() blocks for 250 ms, delaying all other operations
2. **Sluggish Input Response:** User presses gamepad button, waits ~250 ms to see display update
3. **Sensor Update Imbalance:** Fast sensors (INA219, gamepad) don't need 500 ms refresh rate
4. **TSL2591 Blocking:** Integration time of 200 ms is unavoidable, but should be asynchronous

### Target Behavior (AFTER)

```
Main Loop (NON-BLOCKING, ~10-20 ms per iteration):
├─ Check timing gates:
├─ Gamepad read: ~5 ms (every 10 ms)
├─ Encoder read: ~1 ms (every 10 ms, page 3 only)
├─ INA219 read: ~1.2 ms (every 20 µs via micros())
├─ Rotated sensor read: ~50 ms max (every 200 ms, ONE sensor per call)
│  ├─ Cycle 0 (200 ms): BME280 → 2.2 ms
│  ├─ Cycle 1 (400 ms): TSL2591 START → 0 ms (async trigger)
│  ├─ Cycle 2 (600 ms): SI1145 → 0.7 ms
│  ├─ Cycle 3 (800 ms): VEML6075 → ? ms
│  ├─ Cycle 4 (1000 ms): TSL2591 READ → ~1 ms (async complete)
│  └─ Cycle 5 (1200 ms): Loop repeats
└─ Display update: ~2 ms (only if no sensor read in this cycle)

Loop frequency: ~50-100 Hz (responsive!)
Gamepad latency: < 20 ms (smooth, button press within 2 frames)
```

---

## Functional Requirements

### FR-1: Rotated Sensor Reading Schedule

**Requirement:** Split `readSensors()` into five independent sensor read operations, called sequentially in a rotating pattern every 200 ms instead of all together every 500 ms.

**Details:**

- **Rotation Cycle:** 7 iterations × 200 ms = 1400 ms (1.4 seconds per full cycle)
- **Sensor Sequence (Round-Robin):**
  1. **Slot 0 (0 ms):** BME280 read (~2.2 ms)
  2. **Slot 1 (200 ms):** TSL2591 measurement START (async, ~0 ms)
  3. **Slot 2 (400 ms):** SI1145 read (~0.7 ms)
  4. **Slot 3 (600 ms):** VEML6075 read (~? ms)
  5. **Slot 4 (800 ms):** VEML6070 read (~? ms)
  6. **Slot 5 (1000 ms):** SCD30 read (~? ms with CRC validation)
  7. **Slot 6 (1200 ms):** DS18B20 measurement START (async 1-Wire, ~0 ms) OR TSL2591 READ (async)

- **Async Completion Handlers:** 
  - TSL2591 async read: Check readiness and read in Slot 7 or next available
  - DS18B20 async read: Check readiness and read conversion result when available (750 ms later)
- **Backward Compatibility:** All 5 sensors read at least once per second (within acceptable range for slow sensors)
- **Page Visibility:** When user navigates to sensor page, cached values are already available (≤ 1 second old)

**Validation Rules:**
- Each sensor must complete its read within allocated time window
- No sensor read shall block the main loop for > 100 ms
- If a sensor read exceeds its time budget, log WARNING to Serial

**Implementation Pattern:**

```cpp
// Global state
enum SensorSlot { SLOT_BME280 = 0, SLOT_TSL2591_START = 1, SLOT_SI1145 = 2, 
                  SLOT_VEML6075 = 3, SLOT_VEML6070 = 4 };
static uint8_t currentSensorSlot = 0;
static uint32_t nextSlotMs = 0;

// In loop():
if ((int32_t)(now - nextSlotMs) >= 0) {
  uint32_t readStart = micros();
  
  switch (currentSensorSlot) {
    case SLOT_BME280:
      if (available.bme280) readBme280(bme280Values);
      break;
    case SLOT_TSL2591_START:
      if (available.tsl2591) startTsl2591Measurement();  // NEW
      break;
    case SLOT_SI1145:
      if (available.si1145) readSi1145(si1145Values);
      break;
    // ... etc
  }
  
  uint32_t readDuration = micros() - readStart;
  logSensorTiming(currentSensorSlot, readDuration);  // NEW
  
  currentSensorSlot = (currentSensorSlot + 1) % 5;
  nextSlotMs = now + 200;  // Changed from 500 to 200
}
```

---

### FR-2: Async TSL2591 Measurement (Two-Stage)

**Requirement:** Split TSL2591 I2C reads into two phases to eliminate blocking during integration time.

**Phase 1 (Measurement Start):**
- Call `getFullLuminosity()` to trigger ADC conversion
- Store timestamp of measurement initiation
- Return immediately (non-blocking)

**Phase 2 (Read Result):**
- Wait 200+ ms after trigger
- Call `getFullLuminosity()` again to retrieve converted values
- Update `tsl2591Values` cache
- Process bitwise extraction and lux calculation

**State Machine:**

```cpp
enum Tsl2591State { TSL_IDLE, TSL_MEASURING, TSL_READY };
static Tsl2591State tsl2591_state = TSL_IDLE;
static uint32_t tsl2591_measurement_start_ms = 0;
static const uint32_t TSL2591_INTEGRATION_TIME_MS = 200;

// In SLOT_1 (200 ms):
void startTsl2591Measurement() {
  if (tsl2591_state == TSL_IDLE) {
    tsl2591.getFullLuminosity();  // Trigger, returns immediately
    tsl2591_measurement_start_ms = millis();
    tsl2591_state = TSL_MEASURING;
    Serial.println("TSL2591: Measurement started");
  }
}

// In SLOT_4 (800 ms) - OR whenever ready:
void readTsl2591Async() {
  if (tsl2591_state != TSL_MEASURING) return;
  
  uint32_t elapsed = millis() - tsl2591_measurement_start_ms;
  if (elapsed < TSL2591_INTEGRATION_TIME_MS) return;  // Not ready yet
  
  uint32_t readStart = micros();
  uint32_t fullLum = tsl2591.getFullLuminosity();  // Read result
  
  uint32_t fullSpectrum = fullLum & 0xFFFF;
  uint32_t infrared = fullLum >> 16;
  float lux = tsl2591.calculateLux(fullSpectrum, infrared);
  
  tsl2591Values.fullSpectrum = fullSpectrum;
  tsl2591Values.infrared = infrared;
  tsl2591Values.visible = (fullSpectrum > infrared) ? (fullSpectrum - infrared) : 0;
  tsl2591Values.lux = lux;
  
  uint32_t duration = micros() - readStart;
  Serial.printf("TSL2591 read time: %lu µs\n", duration);
  
  tsl2591_state = TSL_IDLE;
}
```

**Edge Cases:**
- If measurement not completed within 300 ms (instead of 200 ms), retry in next cycle
- If `getFullLuminosity()` returns all zeros during read, log ERROR (I2C failure)
- Page display continues showing last-known values if read fails

---

### FR-3: Guaranteed Input Response Latency (≤ 20 ms)

**Requirement:** Ensure gamepad, encoder, and INA219 reads complete within 20 ms, independent of sensor reads.

**Details:**

- **Gamepad Read:** 40 Hz (every 25 ms, ~5 ms actual read time)
- **Encoder Read:** 50 Hz (every 20 ms on page 3, ~1 ms actual read time)
- **INA219 Read:** 50 Hz (every 20 µs via micros(), ~1.2 ms actual read time)
- **Main Loop Iteration:** Must complete within ~20 ms to maintain responsiveness

**Constraint:** Sensor reads in main loop must NOT exceed 100 ms to prevent blocking these input operations.

**Validation:**
- Measure loop iteration time; if > 30 ms, log WARNING
- If gamepad/encoder/INA219 reads miss their deadline (> 25/20/20 ms), log MISSED_DEADLINE

**Loop Priority Order (UNCHANGED):**
1. **Gamepad read** (every 25 ms)
2. **Immediate page draw** (if page changed)
3. **Encoder read** (every 20 ms, page 3 only)
4. **INA219 read** (every 20 µs via micros())
5. **Rotated sensor read** (every 200 ms, ~50 ms max per call)
6. **Display refresh** (every 500 ms, only if no other draw)

---

### FR-4: Smart Display Updates (Value Changes Only + Timer Updates)

**Requirement:** Display updates only when sensor values change OR on timer-based events (INA219 timer increments every second). Decouples gamepad input from display rendering.

**Current Behavior:**
```
loop():
  readGamepad()  // Updates currentPage immediately
  if (page changed) drawCurrentPage()  // Immediate render
  
  // Every 500 ms:
  drawCurrentPage(false)  // Data-only refresh regardless of changes
```

**New Behavior:**
```
loop():
  readGamepad()  // Updates currentPage, NO drawing
  
  // Detect value changes:
  if (page changed) drawCurrentPage(true)  // Full redraw on page change
  else if (sensor values changed) drawCurrentPage(false)  // Data-only redraw
  else if (timer event) drawCurrentPage(false)  // INA219 timer event
  // Otherwise: no redraw (saves power, reduces flicker)
```

**Details:**
- Gamepad button changes `currentPage` variable only
- Sensor cache values compared to previous values
- INA219 timer increments every 1 second (display must update for timer changes)
- `drawCurrentPage()` only called when values actually changed
- Result: Display updates smoothly without unnecessary redraws

**Benefit:** Reduces CPU/GPU load, eliminates flicker, responsive input.

**Display Update Timing:**
- Immediate (< 20 ms) if page changed
- On value change (≤ 200 ms, next sensor slot completion)
- On timer event (every 1 second for INA219 page)

---

### FR-5: Detailed Performance Metrics and Configurable Aggregation

**Requirement:** Log per-sensor timing for all reads, with configurable aggregated statistics per second for high-frequency inputs.

**Configuration:**
```cpp
#define ENABLE_STATS_REPORTING 1  // 1 = on, 0 = off (compile-time)
```

**When ENABLED:** Aggregated stats printed every 1 second  
**When DISABLED:** Only per-sensor timings logged, no aggregation (minimal overhead)

#### A. Per-Sensor Timing Logs (All Sensors, Always On)

**Format:** `[SensorName] read time: [duration] µs`

**Examples:**
```
BME280 read time: 2157 µs
TSL2591 measurement started: 0 µs
TSL2591 read time: 895 µs
SI1145 read time: 687 µs
VEML6075 read time: 1234 µs
VEML6070 read time: 456 µs
SCD30 read time: 3456 µs
```

**When:** Immediately after each sensor read completes.

#### B. Aggregated Frequency Stats (Gamepad, Encoder, INA219)

**Aggregation Window:** 1 second  
**Format:** `[InputName] Stats (1s): Count=[N], MinTime=[µs], MaxTime=[µs], SumTime=[µs]`

**Example Output (every second):**
```
Gamepad Stats (1s): Count=40, MinTime=4800, MaxTime=5234, SumTime=201456
Encoder Stats (1s): Count=50, MinTime=987, MaxTime=1156, SumTime=50123
INA219 Stats (1s): Count=50, MinTime=1100, MaxTime=1289, SumTime=61234
```

**Details:**
- Count = Number of reads in the 1-second window
- MinTime = Fastest read in window
- MaxTime = Slowest read in window
- SumTime = Total time spent reading in window
- Resets every second (next stat printed 1000 ms later)

**Implementation:**
```cpp
struct InputStats {
  uint32_t count;
  uint32_t minTimeUs;
  uint32_t maxTimeUs;
  uint32_t sumTimeUs;
  uint32_t lastReportMs;
};

static InputStats gamepadStats, encoderStats, ina219Stats;

// In readGamepad():
uint32_t start = micros();
// ... read logic ...
uint32_t elapsed = micros() - start;
gamepadStats.count++;
gamepadStats.minTimeUs = min(gamepadStats.minTimeUs, elapsed);
gamepadStats.maxTimeUs = max(gamepadStats.maxTimeUs, elapsed);
gamepadStats.sumTimeUs += elapsed;

// In loop(), every second:
if ((int32_t)(now - nextStatsMs) >= 0) {
  Serial.printf("Gamepad Stats (1s): Count=%u, MinTime=%lu, MaxTime=%lu, SumTime=%lu\n",
    gamepadStats.count, gamepadStats.minTimeUs, gamepadStats.maxTimeUs, gamepadStats.sumTimeUs);
  // Reset and repeat for encoder, ina219
  gamepadStats = {0, UINT32_MAX, 0, 0, now};
  nextStatsMs = now + 1000;
}
```

---

## Domain Entities & Data Model Changes

### New State Variables

**Global Scope (add to file):**

```cpp
// Rotated sensor reading state
static uint8_t currentSensorSlot = 0;                  // [0, 1, 2, 3, 4]
static uint32_t nextSlotMs = 0;                        // Next slot deadline

// TSL2591 async measurement state
enum Tsl2591State { TSL_IDLE = 0, TSL_MEASURING = 1, TSL_READY = 2 };
static Tsl2591State tsl2591_state = TSL_IDLE;
static uint32_t tsl2591_measurement_start_ms = 0;

// Input timing aggregation (per second)
struct InputStats {
  uint32_t count;
  uint32_t minTimeUs;
  uint32_t maxTimeUs;
  uint32_t sumTimeUs;
};

static InputStats gamepadStats = {0, UINT32_MAX, 0, 0};
static InputStats encoderStats = {0, UINT32_MAX, 0, 0};
static InputStats ina219Stats = {0, UINT32_MAX, 0, 0};
static uint32_t nextStatsReportMs = 0;
```

### Struct Changes

**No changes to existing sensor value structs** (Bme280Values, Tsl2591Values, etc. remain unchanged).

### Constants

**Modify (or add as #define):**
```cpp
// CHANGED from 500 to 200
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;  // Each slot interval

// NEW
constexpr uint32_t TSL2591_INTEGRATION_TIME_MS = 200;  // Measurement duration
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;     // Aggregation window
```

---

## Business Rules & Constraints

### BR-1: Rotated Read Scheduling

**Rule:** No two sensors shall be read in the same 200 ms slot.

**Enforcement:** `currentSensorSlot` advances sequentially; only one sensor per call.

### BR-2: Sensor Update Freshness

**Rule:** Each slow sensor (BME280, SI1145, VEML6075, VEML6070) shall be read at least once per 1000 ms.

**Rationale:** Acceptable freshness for climate/light sensors on a dashboard.

**Validation:** Log WARNING if any sensor misses deadline (> 1000 ms since last read).

### BR-3: TSL2591 Async Measurement

**Rule:** Measurement must be started at least 200 ms before next read attempt.

**Implementation:** State machine enforces 200+ ms wait before reading.

### BR-4: Input Response Guarantee

**Rule:** Gamepad, Encoder, INA219 reads must complete within their scheduled intervals (25/20/20 ms respectively).

**Validation:** Log MISSED_DEADLINE if actual read time > budget.

### BR-5: Display Update Latency

**Rule:** When user presses gamepad button, display update must occur within 100 ms.

**Implementation:**
- Gamepad sets `currentPage` immediately (< 20 ms)
- Display draws within next 100 ms (due to sensor slot taking ≤ 100 ms)

### BR-6: Loop Iteration Budget

**Rule:** Main loop iteration (excluding display draw) shall complete within 30 ms.

**Measurement:** Capture loop iteration time; alert if exceeded.

---

## API Changes

### New Functions

#### 1. `void startTsl2591Measurement()`

**Purpose:** Start async TSL2591 measurement (non-blocking).

**Signature:**
```cpp
void startTsl2591Measurement();
```

**Behavior:**
- Check if `tsl2591_state == TSL_IDLE`
- Call `tsl2591.getFullLuminosity()` (triggers ADC)
- Set `tsl2591_state = TSL_MEASURING`
- Record start timestamp
- Return immediately

**Serial Output:**
```
TSL2591: Measurement started
```

---

#### 2. `void readTsl2591Async()`

**Purpose:** Read TSL2591 result after measurement completes (non-blocking if not ready).

**Signature:**
```cpp
void readTsl2591Async();
```

**Behavior:**
- If `tsl2591_state != TSL_MEASURING`, return immediately
- If elapsed time < 200 ms, return immediately (not ready)
- If ready, read `getFullLuminosity()`, extract channels, compute lux
- Update `tsl2591Values` cache
- Set `tsl2591_state = TSL_IDLE`
- Log timing: `TSL2591 read time: XXX µs`

**Error Cases:**
- If `getFullLuminosity()` returns 0x00000000, log ERROR (I2C failure)
- On error, skip cache update (last-known values persist)

---

#### 3. `void rotatedReadSensor()`

**Purpose:** Read one sensor per invocation (called every 200 ms from main loop).

**Signature:**
```cpp
void rotatedReadSensor();
```

**Behavior:**
- Dispatch to correct sensor read function based on `currentSensorSlot`
- Call appropriate `readBme280()`, `startTsl2591Measurement()`, etc.
- Measure and log per-sensor timing
- Advance `currentSensorSlot` to next
- Record deadline for next slot

**Pseudo-code:**
```cpp
void rotatedReadSensor() {
  uint32_t start = micros();
  
  switch (currentSensorSlot) {
    case SLOT_BME280:
      if (available.bme280) readBme280(bme280Values);
      break;
    case SLOT_TSL2591_START:
      if (available.tsl2591) startTsl2591Measurement();
      break;
    case SLOT_SI1145:
      if (available.si1145) readSi1145(si1145Values);
      break;
    case SLOT_VEML6075:
      if (available.veml6075) readVeml6075(veml6075Values);
      break;
    case SLOT_VEML6070:
      if (available.veml6070) readVeml6070(veml6070Values);
      // Possibly also check if TSL2591 async read is ready
      if (tsl2591_state == TSL_MEASURING) {
        uint32_t elapsed = millis() - tsl2591_measurement_start_ms;
        if (elapsed >= TSL2591_INTEGRATION_TIME_MS) {
          readTsl2591Async();
        }
      }
      break;
  }
  
  uint32_t duration = micros() - start;
  const char *slotNames[] = {"BME280", "TSL2591-START", "SI1145", "VEML6075", "VEML6070"};
  Serial.printf("%s read time: %lu µs\n", slotNames[currentSensorSlot], duration);
  
  currentSensorSlot = (currentSensorSlot + 1) % 5;
}
```

---

#### 4. `void updateInputStats(uint32_t duration, InputStats &stats)`

**Purpose:** Update per-input timing statistics.

**Signature:**
```cpp
void updateInputStats(uint32_t durationUs, InputStats &stats);
```

**Behavior:**
- Increment `stats.count`
- Update `stats.minTimeUs` if `durationUs < minTimeUs`
- Update `stats.maxTimeUs` if `durationUs > maxTimeUs`
- Add to `stats.sumTimeUs`

---

#### 5. `void reportInputStats()`

**Purpose:** Report aggregated input statistics to Serial (called once per second).

**Signature:**
```cpp
void reportInputStats();
```

**Behavior:**
- Print line for each input type:
  ```
  Gamepad Stats (1s): Count=40, MinTime=4800, MaxTime=5234, SumTime=201456
  Encoder Stats (1s): Count=50, MinTime=987, MaxTime=1156, SumTime=50123
  INA219 Stats (1s): Count=50, MinTime=1100, MaxTime=1289, SumTime=61234
  ```
- Reset all stats to initial state
- Schedule next report 1000 ms later

---

### Modified Functions

#### 1. `void loop()`

**Changes:**
- Replace `readSensors()` call with `rotatedReadSensor()`
- Change sensor read timing gate from 500 ms to 200 ms interval
- Ensure input reads (gamepad/encoder/INA219) complete before sensor read in priority order
- Add stats reporting timer

**Pseudo-code:**
```cpp
void loop() {
  static uint32_t nextSensorSlotMs = 0;
  static uint32_t nextStatsMs = 0;
  uint32_t now = millis();
  
  // ... existing gamepad/encoder/ina219 reads (unchanged) ...
  
  // Rotated sensor read (CHANGED: 200 ms instead of 500 ms)
  if ((int32_t)(now - nextSensorSlotMs) >= 0) {
    rotatedReadSensor();  // NEW function
    nextSensorSlotMs = now + SENSOR_SLOT_INTERVAL_MS;  // 200 ms
  }
  
  // Statistics reporting (NEW)
  if ((int32_t)(now - nextStatsMs) >= 0) {
    reportInputStats();
    nextStatsMs = now + INPUT_STATS_INTERVAL_MS;  // 1000 ms
  }
}
```

#### 2. `void readGamepadAt40Hz()`

**Changes:**
- Measure read duration using `micros()`
- Call `updateInputStats()` to track min/max/sum

**Pseudo-code:**
```cpp
void readGamepadAt40Hz() {
  uint32_t start = micros();
  
  // ... existing gamepad read logic ...
  
  uint32_t duration = micros() - start;
  updateInputStats(duration, gamepadStats);
}
```

#### 3. `void readEncoderAt50Hz()`

**Changes:**
- Measure read duration using `micros()`
- Call `updateInputStats()` to track min/max/sum

---

#### 4. `void readIna219At50Hz()`

**Changes:**
- Measure read duration using `micros()`
- Call `updateInputStats()` to track min/max/sum
- Remove existing `Serial.printf("INA219 read time: ...")` (already committed as cleanup)

---

#### 5. `void readBme280()` / `readTsl2591()` / `readSi1145()` / etc.

**Changes:**
- No changes to read logic, but keep timing output:
  ```cpp
  Serial.printf("[SensorName] read time: %lu µs\n", duration);
  ```

---

### Removed Functions

- ~~`void readSensors()`~~ **REMOVED** — Replaced with `rotatedReadSensor()` + `startTsl2591Measurement()` + `readTsl2591Async()`

---

## UI/Display Changes

### Page Rendering

**Current Behavior:**
- Page renders immediately when button is pressed (in gamepad handler)

**New Behavior:**
- Gamepad button sets `currentPage` variable
- Page renders on next available display update gate (≤ 100 ms later)
- User sees minimal latency (< 100 ms)

**Implementation:**
```cpp
// In loop():
if (currentPage != lastPage) {
  drawCurrentPage(true);  // Full page with header
  lastPage = currentPage;
  nextDisplayMs = now + SENSOR_REFRESH_MS;
}

// Later in loop(), if no sensor read happened:
if ((int32_t)(now - nextDisplayMs) >= 0 && currentPage == lastPage) {
  drawCurrentPage(false);  // Data-only refresh
  nextDisplayMs = now + SENSOR_REFRESH_MS;
}
```

**No visual changes** — Page layout, colors, formatting remain identical.

---

## Configuration & Constants

### New Configuration Constants

```cpp
// Timing (constexpr)
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;      // Rotated read interval (changed from 500)
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;      // Stats reporting interval (1 second)

// Sensor-specific timing (configurable)
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;      // Measurement duration (configurable at runtime or compile-time)
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;       // 1-Wire conversion time (12-bit, can be faster for lower precision)

// Feature toggles (#define, compile-time)
#define ENABLE_STATS_REPORTING 1   // 1 = aggregated stats every 1s, 0 = off (default: 1)
```

---

## Testing & Validation

### Unit Tests (to be added)

1. **Test: Sensor Slot Rotation**
   - Verify `currentSensorSlot` cycles 0→1→2→3→4→0
   - Verify each slot fires exactly once per 1000 ms
   - Verify no two slots fire in same cycle

2. **Test: TSL2591 State Machine**
   - Verify state transitions: IDLE → MEASURING → IDLE
   - Verify measurement start triggers without blocking
   - Verify async read completes after 200 ms delay
   - Verify early read returns without error

3. **Test: Input Stats Aggregation**
   - Simulate 40 gamepad reads with varying timings
   - Verify min/max/sum/count calculated correctly
   - Verify stats reset after report

4. **Test: Loop Iteration Time**
   - Measure loop execution time
   - Alert if > 30 ms
   - Verify sensor read doesn't exceed 100 ms budget

### Integration Tests

1. **Test: Responsive Input**
   - Press gamepad button
   - Measure time until page changes on display
   - Verify < 100 ms latency

2. **Test: Concurrent Operations**
   - Gamepad reads running at 40 Hz
   - Encoder reads running at 50 Hz
   - INA219 reads running at 50 Hz
   - Sensor rotation every 200 ms
   - Verify no reads are missed
   - Verify no blocking delays

3. **Test: TSL2591 Accuracy**
   - Compare async read values against current (synchronous) implementation
   - Verify identical values retrieved

### Performance Benchmarks

1. **Loop Iteration Time**
   - Baseline (current): ~250-260 ms (due to readSensors blocking)
   - Target (optimized): 10-30 ms max
   - Measurement: Use `micros()` to track loop start/end

2. **Input Response Latency**
   - Baseline (current): 200+ ms (blocked by sensor read)
   - Target (optimized): < 100 ms
   - Measurement: Timestamp button press, measure display update time

3. **Sensor Read Times** (no change expected)
   - BME280: ~2 ms
   - TSL2591 start: ~0 ms
   - TSL2591 read: ~1 ms (after 200 ms delay)
   - SI1145: ~0.7 ms
   - VEML6075: ~? ms (measure)
   - VEML6070: ~? ms (measure)

---

## Documentation Updates Required

### Files to Create/Modify

1. **[docs/FEATURE-SPEC-006-NON-BLOCKING-ARCHITECTURE.md]** (NEW)
   - Full feature specification (this document, moved to docs/)
   - Detailed architecture diagrams
   - State machine diagrams (TSL2591)
   - Timing diagrams (slot rotation)

2. **[docs/02-architecture.md]** (MODIFY)
   - Update "Event Loop" section with new non-blocking pattern
   - Add "Sensor Slot Rotation" subsection
   - Add timing diagram showing new loop structure
   - Add constraints on loop iteration time (≤ 30 ms)

3. **[docs/06-business-rules.md]** (MODIFY)
   - Add BR-SC-005: "Rotated Sensor Reading Schedule"
   - Add BR-SC-006: "Async TSL2591 Measurement"
   - Add BR-SC-007: "Input Response Guarantee (≤ 20 ms)"
   - Add BR-SC-008: "Display Update Decoupling"
   - Add BR-SC-009: "Performance Metrics Aggregation"

4. **[docs/07-ui.md]** (MODIFY)
   - Update display latency expectations
   - Note: "Page updates now deferred < 100 ms instead of immediate"
   - No visual/layout changes

5. **[docs/09-configuration.md]** (MODIFY)
   - Add table entry for `SENSOR_SLOT_INTERVAL_MS = 200`
   - Add table entry for `TSL2591_INTEGRATION_TIME_MS = 200`
   - Add table entry for `INPUT_STATS_INTERVAL_MS = 1000`
   - Add timing budget table:
     | Component | Budget | Typical | Notes |
     |-----------|--------|---------|-------|
     | Gamepad | 25 ms | 5 ms | 40 Hz |
     | Encoder | 20 ms | 1 ms | 50 Hz, page 3 only |
     | INA219 | 20 µs | 1.2 ms | 50 Hz (micros) |
     | Sensor Slot | 100 ms | ≤ 50 ms | Rotated, one per 200 ms |
     | Loop Iteration | 30 ms | 10-20 ms | Max w/o display |

6. **[docs/10-testing.md]** (MODIFY)
   - Add test cases for rotated sensor reads
   - Add test cases for TSL2591 async measurement
   - Add acceptance criteria for loop iteration time
   - Add benchmarks for input response latency

7. **[docs/14-reconstruction-plan.md]** (MODIFY)
   - Add Phase 6 (NEW): "Non-Blocking Sensor Architecture (FEATURE-SPEC-006)"
     - Step 1: Implement rotated sensor slot logic
     - Step 2: Split TSL2591 into async start/read functions
     - Step 3: Add input stats aggregation
     - Step 4: Update main loop scheduling
     - Step 5: Add performance metrics logging
     - Step 6: Verify timing budgets met

---

## Security & Authorization

**No changes.** This feature does not add user authentication, data encryption, or access control. All security considerations remain identical to FEATURE-SPEC-005.

---

## Error Handling & Edge Cases

### Error Case 1: Sensor Read Exceeds Time Budget

**Condition:** Rotated sensor read takes > 100 ms

**Action:**
```cpp
uint32_t maxBudget = 100000;  // 100 ms in µs
if (duration > maxBudget) {
  Serial.printf("WARNING: %s read exceeded budget! Duration=%lu µs (budget=%lu µs)\n",
    slotNames[currentSensorSlot], duration, maxBudget);
}
```

### Error Case 2: TSL2591 Measurement Not Ready

**Condition:** 200+ ms have not passed since measurement start, but `readTsl2591Async()` is called

**Action:**
```cpp
if (tsl2591_state != TSL_MEASURING) return;  // Do nothing
if (elapsed < TSL2591_INTEGRATION_TIME_MS) return;  // Not ready yet
```

### Error Case 3: TSL2591 I2C Failure During Read

**Condition:** `getFullLuminosity()` returns 0x00000000

**Action:**
```cpp
if (fullSpectrum == 0 && infrared == 0) {
  Serial.println("ERROR: TSL2591 read returned zero values (I2C failure?)");
  tsl2591_state = TSL_IDLE;  // Reset state
  return;  // Skip cache update, last-known values persist
}
```

### Error Case 4: Loop Iteration Exceeds 30 ms

**Condition:** Main loop iteration takes > 30 ms

**Action:**
```cpp
uint32_t loopStart = micros();
// ... loop logic ...
uint32_t loopDuration = micros() - loopStart;
if (loopDuration > 30000) {  // 30 ms in µs
  Serial.printf("WARNING: Loop iteration took %lu µs (budget=30000 µs)\n", loopDuration);
}
```

### Error Case 5: Input Stat Reset Edge Case

**Condition:** Stats reporting called, but no reads occurred in the 1000 ms window

**Action:**
```cpp
if (gamepadStats.count == 0) {
  Serial.println("Gamepad Stats (1s): Count=0 (no reads in window)");
} else {
  // Normal reporting
}
```

---

## Backward Compatibility

### Data Compatibility
- All existing sensor value structs remain unchanged
- Cache format identical
- Page display format identical

### API Compatibility
- ~~`readSensors()` **REMOVED**~~ → Replaced with `rotatedReadSensor()`
- Any code calling `readSensors()` will fail to compile (intentional breaking change)
- Migration path: Remove calls to `readSensors()` from custom code; use loop-based rotation instead

### Behavior Compatibility
- Display output remains visually identical
- Sensor values identical (no calculation changes)
- Responsive input may feel "faster" (this is an improvement, not a regression)

---

## Performance Impact

### Memory Usage
- **New global variables:** ~100 bytes (state machine vars + stats structs)
- **Negligible impact:** ESP32 has 327 KB RAM; current usage ~22 KB (7%)

### Flash Usage
- **New functions:** ~1-2 KB (rotatedReadSensor, TSL2591 async helpers, stats)
- **Expected total:** 373 KB → 375 KB (29% usage, still well within limit)

### Runtime Performance
- **Loop iteration time:** 250 ms → 10-30 ms (8-25× faster!)
- **Input response latency:** 200+ ms → < 100 ms (2× faster!)
- **Sensor update freshness:** 500 ms → 1000 ms (acceptable trade-off)
- **Power consumption:** Unchanged (same total I2C activity, just distributed)

---

## Rollback & Mitigation

### If Performance Goals Not Met
1. Reduce slot interval from 200 ms to 150 ms (faster rotation)
2. Increase max budget from 100 ms to 150 ms (tolerate slower sensors)
3. Revert to synchronous TSL2591 reads (sacrifice responsiveness)
4. Add dynamic priority (read high-frequency sensors more often)

### Rollback Procedure
1. Revert to commit before FEATURE-SPEC-006 implementation
2. Restore `readSensors()` function
3. Revert loop() timing gates to 500 ms
4. Rebuild and upload

---

## Acceptance Criteria

✓ **Must-Have:**
1. [ ] Loop iteration time ≤ 30 ms (measured with `micros()`)
2. [ ] Input response latency ≤ 100 ms (button press → display update)
3. [ ] All sensor reads complete within allocated 100 ms budget
4. [ ] TSL2591 async measurement starts and reads correctly
5. [ ] Serial output includes per-sensor timing for all reads
6. [ ] Serial output includes aggregated stats every 1 second
7. [ ] No sensors missed in rotation (each read ≥ 1 per second)
8. [ ] Display remains responsive (no visible lag during sensor reads)
9. [ ] Backward compatibility: existing pages unchanged

✓ **Should-Have:**
1. [ ] Loop iteration time < 20 ms (ideal responsiveness)
2. [ ] Gamepad latency < 50 ms (very smooth)
3. [ ] Stats output clearly formatted and readable
4. [ ] WARNING logged if any timing budget exceeded

✓ **Could-Have:**
1. [ ] Configurable slot intervals (#define)
2. [ ] Dashboard showing live loop performance metrics
3. [ ] Adaptive slot timing based on actual sensor read times

---

## Open Questions

1. **SCD30 Sensor Handling:**
   - Current: 500 ms interval (FEATURE-SPEC-005)
   - Question: Should SCD30 be included in rotated schedule, or remain separate?
   - Impact: CRC validation adds latency; may need independent handling
   - Decision needed: Keep separate (500 ms) or rotate (1000 ms)?

2. **VEML6070 & VEML6075 Read Times:**
   - Unknown: Actual read duration for VEML6070 and VEML6075
   - Impact: Affects slot allocation and time budget
   - Action: Measure actual read times and verify fit within 100 ms budget

3. **DS18B20 (1-Wire) Temperature Sensors:**
   - Current: Every 3 seconds (FEATURE-SPEC-005)
   - Question: Should async reads also apply to 1-Wire temperature conversion?
   - Rationale: 750 ms conversion time is longer than TSL2591
   - Decision needed: Implement async 1-Wire polling like TSL2591?

4. **Display Refresh Rate:**
   - Current spec: 500 ms data refresh
   - Question: Should display refresh rate be adjusted to compensate for 1-second sensor cycle?
   - Impact: May need faster refresh (every 200 ms) to show updated values sooner
   - Decision needed: Keep 500 ms or increase to 200 ms?

5. **Stats Output Verbosity:**
   - Current spec: Aggregated stats every 1 second
   - Question: Should this be configurable (e.g., #define ENABLE_STATS)?
   - Impact: Reduces Serial output noise in production
   - Decision needed: Always on, conditional compile, or runtime flag?

6. **Sensor Slot Order Optimization:**
   - Current spec: Fixed order (BME280 → TSL2591 → SI1145 → VEML6075 → VEML6070)
   - Question: Should slot order be optimized by read time (fastest first)?
   - Impact: May allow more aggressive sensor polling
   - Decision needed: Keep fixed order or dynamically reorder?

7. **INA219 Coulomb Counter Impact:**
   - Current: Already reads at 50 Hz (independent of SENSOR_REFRESH_MS)
   - Question: Does Coulomb counter accuracy suffer with higher loop frequency?
   - Impact: May need to verify power measurement correctness
   - Action: Hardware test after implementation

8. **TSL2591 Measurement Start Timing:**
   - Current spec: Start in Slot 1 (200 ms), read in Slot 4 (800 ms)
   - Question: Is 200 ms integration time sufficient, or should it be configurable?
   - Impact: Changing to 100 ms would speed up reads but reduce accuracy
   - Decision needed: Hard-code 200 ms or make configurable?

---

## References

- **FEATURE-SPEC-005:** Sensor Caching Architecture (predecessor)
- **02-architecture.md:** Event Loop structure
- **06-business-rules.md:** Business rules for sensor timing
- **TSL2591 Datasheet:** Measurement cycles and integration time
- **Hardware:** ESP32, Freenove FNK0114N display

---

## Revision History

| Version | Date | Author | Change |
|---------|------|--------|--------|
| 1.0 | 2026-09-24 | Specification | Initial feature specification (not implemented) |

