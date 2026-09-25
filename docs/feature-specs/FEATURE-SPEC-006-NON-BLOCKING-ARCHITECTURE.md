# FEATURE-SPEC-006: Non-Blocking Sensor Architecture with Rotated Reads

**Status:** ✓ IMPLEMENTED  
**Created:** 2026-09-24  
**Updated:** 2026-09-25 (10-slot, 2000ms design)  
**Priority:** High (Performance/Responsiveness)  
**Category:** Performance Optimization + Architecture Refactoring

---

## Executive Summary

The system implements a **10-slot rotating sensor architecture** that decouples slow sensor reads from fast input handling:

**FEATURE-SPEC-006** provides:
1. **Phase-Based Sensor Rotation** — Async sensors start early, sync sensors read while conversions proceed, async results checked late
2. **10-Slot Schedule** — 10 × 200ms = 2000ms cycle (improved from previous 1400ms)
3. **Guaranteed Input Latency** — Gamepad/Encoder read within **20 ms guarantee** (independent of sensor timing)
4. **Asynchronous Measurements** — TSL2591 (200ms) and DS18B20 (750ms) don't block main loop
5. **Decoupled Display Updates** — Display updates only when sensor values change (event-driven)

**Result:** Responsive UI, consistent 2000ms sensor freshness, 50-100 Hz loop frequency, <100ms input latency.

---

## Problem Analysis

### Previous Behavior (BEFORE FEATURE-SPEC-006)

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

### Issues Addressed

1. **Blocked Loop:** 250 ms blocking in readSensors() delayed all other operations
2. **Sluggish Input:** Gamepad button press took ~250 ms to see display update
3. **Synchronous Bottleneck:** Fast sensors (INA219, gamepad) tied to slow sensor timing
4. **TSL2591 Blocking:** 200 ms integration time blocked main loop

### Current Behavior (AFTER FEATURE-SPEC-006)

```
Main Loop (NON-BLOCKING, ~10-30 ms per iteration):

Slot 0 (0ms):   Start TSL2591 async measurement
Slot 1 (200ms): Start DS18B20 async conversion
Slot 2 (400ms): Read BME280 (2ms)
Slot 3 (600ms): Read SI1145 (1ms)
Slot 4 (800ms): Read VEML6075 (1ms)
Slot 5 (1000ms): Read VEML6070 (1ms)
Slot 6 (1200ms): Read SCD30 (5ms)
Slot 7 (1400ms): DS2484 bus scan (10ms, every 60s)
Slot 8 (1600ms): Check TSL2591 result (TSL2591 started 1600ms ago)
Slot 9 (1800ms): Check DS18B20 result (DS18B20 started 1600ms ago)
[Cycle repeats at 2000ms]

Loop frequency: 50-100 Hz (responsive!)
Gamepad latency: <100 ms total
Sensor update rate: 2000ms per sensor
```

---

## Detailed Specification

### Functional Requirements

#### FR-1: 10-Slot Rotating Sensor Reading Schedule

The sensor dispatcher cycles through 10 slots, each 200ms apart, totaling 2000ms per cycle:

**Phase 1: START ASYNC (Slots 0-1, 0-200ms)**

| Slot | Time | Operation | Function | Duration |
|------|------|-----------|----------|----------|
| 0 | 0 ms | Start TSL2591 measurement | `startTsl2591Measurement()` | ~0 ms |
| 1 | 200 ms | Start DS18B20 conversion | `startDs18b20Conversion()` | ~0 ms |

**Phase 2: READ SYNC (Slots 2-7, 400-1400ms)**

| Slot | Time | Sensor | Function | Duration |
|------|------|--------|----------|----------|
| 2 | 400 ms | BME280 | `readBme280()` | ~2 ms |
| 3 | 600 ms | SI1145 | `readSi1145()` | ~1 ms |
| 4 | 800 ms | VEML6075 | `readVeml6075()` | ~1 ms |
| 5 | 1000 ms | VEML6070 | `readVeml6070()` | ~1 ms |
| 6 | 1200 ms | SCD30 | `readScd30()` | ~5-10 ms |
| 7 | 1400 ms | DS2484 Scan | `performDs2484Scan()` (60s interval) | ~10 ms |

**Phase 3: CHECK ASYNC (Slots 8-9, 1600-1800ms)**

| Slot | Time | Operation | Function | Timing Guarantee |
|------|------|-----------|----------|------------------|
| 8 | 1600 ms | Check TSL2591 result | `checkTsl2591Async()` | Started at 0ms, needs 200ms → 1400ms margin ✓ |
| 9 | 1800 ms | Check DS18B20 result | `checkDs18b20Async()` | Started at 200ms, needs 750ms → 850ms margin ✓ |

**Cycle Time:** 10 slots × 200 ms = **2000 ms per complete rotation**

**Benefit:** Each sensor read blocks loop for only 1-10 ms; async sensors don't block at all.

#### FR-2: Asynchronous TSL2591 Measurement (Two-Phase)

**Phase 1 (Slot 0, 0 ms):** Start measurement
- Call `startTsl2591Measurement()`
- Trigger ADC conversion on lux sensor
- Set `tsl2591State = MEASURING`
- Return immediately (non-blocking)

**Phase 2 (Slot 8, 1600 ms):** Read result
- Call `checkTsl2591Async()`
- If `millis() - tsl2591MeasureStartMs >= 200ms`:
  - Read converted values
  - Update `tsl2591Values` cache
  - Set `tsl2591State = IDLE`
- Else: return early (not ready yet)

**State Machine:**
```
IDLE → (Slot 0) → MEASURING → (wait 200+ ms) → (Slot 8) → IDLE
```

**Timing Guarantee:** 1600ms between start and check; 1400ms margin after integration completes.

#### FR-3: Asynchronous DS18B20 Conversion (Two-Phase, NEW)

**Phase 1 (Slot 1, 200 ms):** Start conversion
- Call `startDs18b20Conversion()` for all discovered sensors
- Send 1-Wire Convert T command (0x44)
- Set `ds18b20State = CONVERTING`
- Record `ds18b20ConvertStartMs`
- Return immediately (non-blocking)

**Phase 2 (Slot 9, 1800 ms):** Read result
- Call `checkDs18b20Async()`
- If `millis() - ds18b20ConvertStartMs >= 750ms`:
  - Read temperature scratchpad from each sensor
  - Update `ds2484Values.sensors[i].temperature`
  - Set `ds18b20State = IDLE`
- Else: return early (not ready yet)

**State Machine:**
```
IDLE → (Slot 1) → CONVERTING → (wait 750+ ms) → (Slot 9) → IDLE
```

**Timing Guarantee:** 1600ms between start and check; 850ms margin after conversion completes.

**Error Handling:** Sensors with 3 consecutive failures marked unavailable (three-strike lockout).

#### FR-4: Guaranteed Input Response Latency (≤ 100 ms)

Fast input processing independent of slow sensor timing:

- **Gamepad:** Every 25 ms (40 Hz), `readGamepadAt40Hz()`
- **Encoder:** Every 20 ms (50 Hz, CONTINUOUSLY — not page-conditional), `readEncoderAt50Hz()`
- **INA219:** Every 20 µs (50 kHz via `micros()`), `readIna219At50Hz()`

**Result:** User input processed within 20-100 ms regardless of sensor slot.

#### FR-5: Decoupled Display from Sensor Polling

- Display updates only when sensor values change (event-driven)
- Page navigation happens immediately (gamepad to display: <100 ms)
- No redraw when values unchanged (saves CPU/power)

#### FR-6: Slot Calculation Algorithm

Slots are determined by elapsed time using modulo arithmetic (not state counting):

```cpp
uint32_t currentSensorSlot = (millis() / SENSOR_SLOT_INTERVAL_MS) % SLOT_COUNT;
// With SENSOR_SLOT_INTERVAL_MS = 200 and SLOT_COUNT = 10:
// t=0-199ms: slot 0
// t=200-399ms: slot 1
// ...
// t=1800-1999ms: slot 9
// t=2000+ms: cycle repeats (slot 0 again)
```

**Benefits:**
- Startup-safe (no need to initialize slot counter)
- Automatic synchronization with wallclock
- Robust across sleep/wake cycles

---

## Implementation Details

### Global State Variables

```cpp
// Slot dispatcher configuration
constexpr uint32_t SENSOR_SLOT_INTERVAL_MS = 200;    // Immutable: slot duration
constexpr uint32_t SLOT_COUNT = 10;                  // Immutable: slots per cycle
constexpr uint32_t INPUT_STATS_INTERVAL_MS = 1000;   // Immutable: stats reporting period

// Sensor-specific timing (adjustable)
static uint32_t TSL2591_INTEGRATION_TIME_MS = 200;   // Can adjust for different integration
static uint32_t DS18B20_CONVERSION_TIME_MS = 750;    // Can adjust for different precision

// Async state tracking
enum class Tsl2591State { IDLE, MEASURING };
static Tsl2591State tsl2591State = Tsl2591State::IDLE;
static uint32_t tsl2591MeasureStartMs = 0;

enum class Ds18b20State { IDLE, CONVERTING };
static Ds18b20State ds18b20State = Ds18b20State::IDLE;
static uint32_t ds18b20ConvertStartMs = 0;

// Feature toggles
#define ENABLE_STATS_REPORTING 1  // 1=enabled (default), 0=disabled
```

### New Functions Required

1. **`void rotatedReadSensor(uint32_t slot)`**
   - Dispatcher function for all 10 slots
   - Routes to appropriate sensor handler based on slot
   - Called once per 200ms by main loop

2. **`void startTsl2591Measurement()`**
   - Trigger ADC, set state to MEASURING
   - Non-blocking (returns immediately)

3. **`void checkTsl2591Async()`**
   - Wait for integration to complete
   - Non-blocking if not ready yet
   - Updates `tsl2591Values` on completion

4. **`void startDs18b20Conversion()`**
   - Send 1-Wire Convert T command to all sensors
   - Set state to CONVERTING
   - Non-blocking

5. **`void checkDs18b20Async()`**
   - Read temperature scratchpad if conversion complete
   - Handle three-strike lockout on errors
   - Non-blocking if not ready yet

---

## Performance Targets

| Metric | Before | After | Benefit |
|--------|--------|-------|---------|
| **Loop iteration time** | 250 ms | 10-30 ms | 8-25× faster |
| **Input response latency** | 200+ ms | <100 ms | 2× faster |
| **Sensor update frequency** | 500 ms | 2000 ms | Consistent/predictable |
| **Gamepad responsiveness** | Sluggish | Smooth | 20-50 Hz loop |
| **TSL2591 blocking** | 200 ms | 0 ms | No blocking |
| **DS18B20 blocking** | 750 ms | 0 ms | No blocking |

**Note:** Sensor freshness increased from 500-1000ms to 2000ms to accommodate DS18B20 750ms conversion + 850ms margin. This is acceptable because sensors are still polled continuously without blocking input.

---

## Constraints & Business Rules

1. **Slot Timing:** Exact 200ms interval (immutable, determined by architecture)
2. **Cycle Time:** 10 slots = 2000ms (immutable)
3. **Async Margins:** Must guarantee conversion complete before checking
   - TSL2591: ≥200ms margin (1400ms provided)
   - DS18B20: ≥750ms margin (850ms provided)
4. **No Blocking:** All slot handlers must complete within 100ms
5. **Input Priority:** Gamepad/Encoder/INA219 independent of slot timing

---

## Testing & Validation

### Key Measurements

1. Slot transition timing (should be every 200ms ±10ms)
2. Async sensor completion guarantee (1600ms > 750ms)
3. Loop iteration frequency (should be 50-100 Hz)
4. Input response latency (gamepad press to display update < 100ms)
5. Sensor value freshness (each sensor reads once per cycle)

### Acceptance Criteria

✓ All 10 slots execute in 2000ms cycle  
✓ TSL2591 integration completes before slot 8 check  
✓ DS18B20 conversion completes before slot 9 check  
✓ No sensor read blocks loop >100ms  
✓ Gamepad/Encoder latency <100ms  
✓ Loop runs at 50+ Hz  
✓ Sensor values update every 2000ms  

---

## Version History

| Date | Status | Changes |
|------|--------|---------|
| 2026-09-24 | Specification | Initial 5-7 slot design |
| 2026-09-25 | Implemented | Updated to 10-slot, 2000ms design with phases; added DS18B20 async; corrected performance targets |

---

**Version:** 2.0 (Final Implementation)  
**Status:** ✓ IMPLEMENTED AND TESTED


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
├─ Encoder read: ~1 ms (every 20 ms, CONTINUOUSLY — not page-conditional)
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

## Detailed Specification

### Functional Requirements

#### FR-1: Rotated Sensor Reading Schedule

Each of 5 slow sensors is read independently every 200 ms in a rotating pattern:

| Slot | Time | Sensor | Duration |
|------|------|--------|----------|
| 0 | 0 ms | BME280 | ~2 ms |
| 1 | 200 ms | TSL2591 START | ~0 ms (async) |
| 2 | 400 ms | SI1145 | ~0.7 ms |
| 3 | 600 ms | VEML6075 | ~? ms |
| 4 | 800 ms | VEML6070 / TSL2591 READ | ~? ms / ~1 ms |

**Full cycle:** 5 slots × 200 ms = 1000 ms (1 second per complete rotation)

**Benefit:** Each sensor read only blocks loop for ~1-2 ms (max ~100 ms with margin).

#### FR-2: Async TSL2591 Measurement (Two-Stage)

**Phase 1 (Slot 1, 200 ms):** Start measurement
- Trigger ADC conversion
- Return immediately (non-blocking)

**Phase 2 (Slot 4, 800 ms):** Read result
- Wait 200+ ms after trigger
- Read converted values
- Update cache

**State Machine:**
```
IDLE → (slot 1: getFullLuminosity() call) → MEASURING → (wait 200 ms) → READY → (read values) → IDLE
```

#### FR-3: Guaranteed Input Response Latency (≤ 20 ms)

- **Gamepad:** Every 25 ms (40 Hz)
- **Encoder:** Every 20 ms (50 Hz, CONTINUOUSLY — not page-conditional)
- **INA219:** Every 20 µs (50 Hz via micros())
- **Result:** Any loop iteration must complete within 30 ms max

#### FR-4: Decoupled Display from Gamepad

- Gamepad button changes `currentPage` variable only
- Display rendering happens in separate timing gate
- Display update latency: < 100 ms total

#### FR-5: Detailed Performance Metrics

**Per-Sensor Timing (all reads):**
```
BME280 read time: 2157 µs
TSL2591 read time: 895 µs
SI1145 read time: 687 µs
```

**Aggregated Stats (every 1 second):**
```
Gamepad Stats (1s): Count=40, MinTime=4800, MaxTime=5234, SumTime=201456
Encoder Stats (1s): Count=50, MinTime=987, MaxTime=1156, SumTime=50123
INA219 Stats (1s): Count=50, MinTime=1100, MaxTime=1289, SumTime=61234
```

---

## Implementation Requirements

### New Global State Variables

```cpp
static uint8_t currentSensorSlot = 0;                    // [0-4]
static uint32_t nextSlotMs = 0;

enum Tsl2591State { TSL_IDLE, TSL_MEASURING, TSL_READY };
static Tsl2591State tsl2591_state = TSL_IDLE;
static uint32_t tsl2591_measurement_start_ms = 0;

struct InputStats {
  uint32_t count, minTimeUs, maxTimeUs, sumTimeUs;
};
static InputStats gamepadStats, encoderStats, ina219Stats;
static uint32_t nextStatsReportMs = 0;
```

### New Functions Required

1. **`void startTsl2591Measurement()`**
   - Trigger ADC, set state to MEASURING
   - Non-blocking

2. **`void readTsl2591Async()`**
   - Wait 200+ ms, then read result
   - Non-blocking if not ready

3. **`void rotatedReadSensor()`**
   - Dispatch to correct sensor based on slot
   - Advance to next slot
   - Log per-sensor timing

4. **`void updateInputStats(uint32_t durationUs, InputStats &stats)`**
   - Update min/max/sum/count

5. **`void reportInputStats()`**
   - Print aggregated stats every 1 second
   - Reset counters

### Modified Functions

1. **`void loop()`**
   - Replace `readSensors()` with `rotatedReadSensor()`
   - Change sensor gate from 500 ms to 200 ms
   - Add stats reporting gate
   - Ensure input reads stay in priority order

2. **`void readGamepadAt40Hz()`, `readEncoderAt50Hz()`, `readIna219At50Hz()`**
   - Add `updateInputStats()` calls to track timing

### Removed Functions

- ~~`void readSensors()`~~ — Replaced with rotated approach

---

## Performance Targets

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Loop iteration time | 250 ms | 10-30 ms | 8-25× faster |
| Input response latency | 200+ ms | < 100 ms | 2× faster |
| Sensor freshness | 500 ms | 1000 ms | 2× slower (acceptable) |
| Display responsiveness | Sluggish | Smooth | Significant |

---

## Constraints & Business Rules

1. **Sensor Update Freshness:** Each sensor ≥ 1 per second (vs 500 ms current)
2. **Loop Iteration Budget:** ≤ 30 ms (excluding display render)
3. **Sensor Slot Budget:** ≤ 100 ms per slot
4. **Input Guarantee:** Gamepad/Encoder/INA219 within their scheduled intervals
5. **No Blocking Delays:** All read operations must be interruptible

---

## Testing & Validation

### Key Measurements

1. Loop iteration time (use `micros()`)
2. Input response latency (timestamp button → display change)
3. Per-sensor read times (log and verify within budget)
4. TSL2591 async accuracy (compare values)
5. Sensor freshness (no reads missed)

### Acceptance Criteria

✓ Loop iteration ≤ 30 ms  
✓ Input response < 100 ms  
✓ All sensors read ≥ 1/second  
✓ TSL2591 async works correctly  
✓ Serial logs show per-sensor and aggregated timing  
✓ No regressions in sensor values or display

---

## Open Questions

1. **SCD30 Handling:** Include in rotation or keep separate 500 ms?
2. **VEML6070/VEML6075 Read Times:** Need actual measurements
3. **DS18B20 1-Wire:** Implement async like TSL2591 (750 ms conversion)?
4. **Display Refresh Rate:** Keep 500 ms or increase to 200 ms?
5. **Stats Verbosity:** Always on or configurable?
6. **TSL2591 Integration Time:** Hard-code 200 ms or make configurable?

---

## References

- FEATURE-SPEC-005: Sensor Caching Architecture (predecessor)
- 02-architecture.md: Event Loop
- 06-business-rules.md: Business Rules
- TSL2591 Datasheet: Integration times and measurement cycles

---

**Version:** 1.0  
**Status:** Specification Ready for Review
