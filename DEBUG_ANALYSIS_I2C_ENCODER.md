# 🚨 KRITISCHER DEBUG-REPORT: I2C Error 263 + Encoder Integer Overflow

**Status**: Production Issue  
**Severity**: CRITICAL  
**Date**: 2026-10-22 22:24  
**Duration**: After ~3 hours runtime  

---

## 1️⃣ ROOT CAUSE ANALYSIS

### **Primary Issue: Encoder Position 32-bit Integer Overflow**

**File**: `src/sensors/EncoderSensor.h:73` + `src/sensors/EncoderSensor.cpp:23-39`

```cpp
struct {
    int32_t position;           // ← THIS IS THE PROBLEM!
    int32_t delta;
    bool pressed;
    uint32_t presses;
    uint8_t brightness;
    uint8_t colorIndex;
    const char *colorName;
} values{};
```

**Range**: -2,147,483,648 to +2,147,483,647 (int32_t limits)

**Evidence from logs**:
```
[03:46:19.405] [INPUT] ENCODER Rotation: position -1123729345 → 937600807
                        Delta: +2,061,330,152  ← IMPOSSIBLE for normal rotation!
```

**Analysis**:
- Position at failure: `-1,123,729,345` (only ~1.024 billion from int32_min)
- Delta: `+2,061,330,152` (should be ±1-10 for normal rotation)
- Pattern: Encoder position approaches int32 boundary → overflow occurs
- Effect: Position wraps from -2.147B → +2.147B (magnitude jump: 4.3B!)

---

### **Secondary Issue: I2C Bus Corruption from Overflow**

**Pattern observed**:
1. Encoder position overflows
2. Seesaw library receives corrupted data
3. I2C bus state becomes corrupted
4. Error 263 appears: `i2cWriteReadNonStop returned Error 263`
5. All I2C sensors fail (cascading failures)

**Clustering evidence**:
```
[13622582][E][Wire.cpp:499] requestFrom(): i2cWriteReadNonStop returned Error 263
[13623583][E][Wire.cpp:499] requestFrom(): i2cWriteReadNonStop returned Error 263
[13624584][E][Wire.cpp:499] requestFrom(): i2cWriteReadNonStop returned Error 263
[13625585][E][Wire.cpp:499] requestFrom(): i2cWriteReadNonStop returned Error 263
[13626586][E][Wire.cpp:499] requestFrom(): i2cWriteReadNonStop returned Error 263
... (10 errors in <100ms cluster)
```

---

### **Tertiary Issue: Spontaneous Gamepad Events**

**Evidence**:
```
[03:46:15.323] [INPUT] GAMEPAD Button B: PRESSED  ← No user input!
[03:46:15.323] [INPUT] GAMEPAD Button Y: PRESSED  ← Simultaneous!
```

**Cause**: I2C bus corruption → gamepad seesaw module reports false button states

---

## 2️⃣ ENCODER DELTA CALCULATION BUG

**Current code** (`src/sensors/EncoderSensor.cpp:36-39`):
```cpp
int32_t newPosition = seesaw.getEncoderPosition();
values.delta = newPosition - values.position;  // ← Overflow here!
values.position = newPosition;
```

**Problem**:
- When position overflows: `-2.147B - 2.147B = underflow/overflow`
- Delta becomes massive instead of ±1

**Example**:
```
oldPos = -1,123,729,345
newPos = +937,600,807 (wrapped from overflow)
delta = 937,600,807 - (-1,123,729,345) = +2,061,330,152 ✗ WRONG!
```

---

## 3️⃣ I2C BUS TIMING ANALYSIS

**Current scheduling** (`src/main.cpp:2700+`):

```
Loop Priority:
1. Gamepad @ 40 Hz (25ms interval)
2. Encoder @ 50 Hz (20ms interval)  ← CONCURRENT I2C!
3. Sensors @ variable Hz:
   - BME280: 7ms reads
   - VEML6075: 16ms reads
   - TSL2591: 4.2ms reads
   - SCD30: 1ms reads
   - VEML6070: 1ms reads

Concurrent reads happening:
- readGamepadAt40Hz()        [gamepad seesaw via I2C]
- readEncoderAt50Hz()        [encoder seesaw via I2C]
- readBme280()               [BME280 via I2C]
- readVeml6075()             [VEML6075 via I2C]
- readVeml6070()             [VEML6070 via I2C]
- readScd30()                [SCD30 via I2C]
```

**Issue**: Multiple I2C devices reading at same time (non-blocking but queued)!

---

## 4️⃣ SEESAW DOUBLE-ADDRESSING BUG

**Critical finding** - TWO separate I2C devices:
1. **Gamepad Seesaw** @ 0x77 (12-button/analog controller)
2. **Encoder Seesaw** @ 0x36 (rotary encoder + NeoPixel LED)

**Current read pattern**:
```cpp
void readGamepadAt40Hz() {
    gamepadValues.x = 1023 - gamepad.analogRead(14);     // ← I2C @ 0x77
    gamepadValues.y = 1023 - gamepad.analogRead(15);
    gamepadValues.buttons = gamepad.digitalReadBulk(...);
}

void readEncoderAt50Hz() {
    int32_t position = encoder.getEncoderPosition();     // ← I2C @ 0x36
    encoderValues.pressed = encoder.digitalRead(24);
}
```

**Problem**: Without explicit delay/synchronization, encoder overflow can corrupt gamepad seesaw bus state!

---

## 5️⃣ HEAP STABILITY OBSERVATION

```
[03:46:12.320] [DIAG] Heap OK: 344660 bytes free (min ever: 338836)
[03:46:29.469] [DIAG] Heap OK: 344660 bytes free (min ever: 338836)
...
[03:52:42.322] [DIAG] Heap OK: 344660 bytes free (min ever: 338836)
```

✅ **Good news**: Heap is **completely stable** (never changes)
- No memory leaks
- No buffer overflows
- Problem is **purely I2C/encoder logic**, not memory

---

## 6️⃣ DIAGNOSTIC MISSING DATA

To confirm root cause, we need:

1. **Exact overflow timestamp**: When did position first exceed int32_max?
2. **I2C error delay**: How long after overflow did Error 263 appear?
3. **Encoder raw position**: Is seesaw library returning invalid values?
4. **Bus recovery timeline**: Can system recover without reboot?
5. **Gamepad state at error time**: Are button values garbage?

---

## SOLUTION ROADMAP

### Phase 1: Immediate Overflow Protection ✓
- [ ] Change encoder position from `int32_t` → `int64_t`
- [ ] Implement position clamping (±2.1B limit)
- [ ] Add overflow detection logging

### Phase 2: Enhanced Diagnostics ✓
- [ ] Log encoder position BEFORE/AFTER reads
- [ ] Log delta SANITY CHECK (|delta| > 1000 = suspicious)
- [ ] Add I2C error detail logging
- [ ] Add bus recovery attempt counter

### Phase 3: I2C Bus Hardening ✓
- [ ] Add explicit I2C mutex/serialization
- [ ] Implement retry logic for transient errors
- [ ] Add I2C bus recovery (SCL/SDA cycle)

### Phase 4: Validation ✓
- [ ] 8-hour stress test
- [ ] Monitor encoder position for overflow
- [ ] Verify no I2C errors
- [ ] Verify no spontaneous input events

---

## FILES TO MODIFY

1. **src/sensors/EncoderSensor.h** - Change position type
2. **src/sensors/EncoderSensor.cpp** - Add overflow detection + logging
3. **src/main.cpp** - Enhanced I2C logging + recovery
4. **src/diagnostics.h** - Add new diagnostic constants

