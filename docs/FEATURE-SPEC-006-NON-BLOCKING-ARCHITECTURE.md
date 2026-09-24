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
