# FEATURE-SPEC-008: CENTRAL PARAMETER TRIPLE DEFINITIONS

## Implemented: September 25, 2026

### Overview
Refactored parameter management to eliminate redundancy and guarantee consistency by design.

**Before**: Parameters were defined twice (once in Large Display Helper, once in Sensor Page)
**After**: Parameters defined ONCE, used by BOTH display modes

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│         CENTRAL PARAMETER DEFINITIONS (SPEC-008)            │
├─────────────────────────────────────────────────────────────┤
│ For each sensor: const ParameterTriple sensor_params[]      │
│ Format: {Label, Unit, Color} × N parameters                 │
└──────────────────┬──────────────────┬──────────────────────┘
                   │                  │
     ┌─────────────┘                  └──────────────┐
     ▼                                               ▼
┌──────────────────────┐                 ┌──────────────────────┐
│  VALUE GETTER        │                 │  LARGE DISPLAY       │
│  sensor_getValue()   │                 │  HELPER              │
│  Delivers values for │                 │  sensor_getData()    │
│  each index          │                 │  Returns Label+      │
│                      │                 │  Value+Unit+Color    │
└──────────────────────┘                 └──────────────────────┘
     │                                               │
     └──────────────┬──────────────────┬────────────┘
                    │                  │
     ┌──────────────┘                  └──────────────┐
     ▼                                               ▼
┌─────────────────────────┐              ┌─────────────────────┐
│ SENSOR PAGE RENDERER    │              │ LARGE DISPLAY MODE  │
│ pageSensor()            │              │ renderLargeDisplay()│
│ Uses displaySensorPage()│              │ Calls helper        │
│ RESULT: All params      │              │ RESULT: Focal       │
│ displayed with central  │              │ display of single   │
│ definitions             │              │ parameter           │
└─────────────────────────┘              └─────────────────────┘
```

### Implementation Details

#### 1. Central Definition Structure

```cpp
struct ParameterTriple {
  const char* label;    // "Temperature", "Pressure", etc.
  const char* unit;     // "C", "hPa", "%", etc.
  uint16_t color;       // TFT_GREEN, TFT_ORANGE, etc.
};

// Example for BME280 (7 parameters)
const ParameterTriple bme280_params[] = {
  {"Temperature", "C", TFT_GREEN},
  {"Pressure", "hPa", TFT_GREEN},
  // ... 5 more ...
};
const uint8_t BME280_PARAM_COUNT = 7;
```

#### 2. Central Value Getter Functions

```cpp
String bme280_getValue(uint8_t index) {
  if (index >= BME280_PARAM_COUNT || !available.bme280) return "";
  switch (index) {
    case 0: return String(bme280Values.temperature, 2);
    case 1: return String(bme280Values.pressure, 2);
    // ... returns value as string, no label/unit/color ...
    default: return "";
  }
}
```

Each sensor has identical pattern:
- Input: parameter index (0 to PARAM_COUNT-1)
- Output: value as formatted String
- Availability check built-in

#### 3. Simplified Large Display Helpers

```cpp
/// Before: 30 lines with duplicate data
String bme280_getLargeDisplayData(uint8_t valueIndex, ...) {
  static const char* labels[] = { "Temp", "Pressure", ... };
  static const char* units[] = { "C", "hPa", ... };
  static const uint16_t colors[] = { TFT_GREEN, ... };
  if (!available.bme280 || valueIndex >= 7) return "";
  *outLabel = labels[valueIndex];
  *outUnit = units[valueIndex];
  *outColor = colors[valueIndex];
  // 20-line switch to get value ...
}

/// After: 6 lines - pure delegation to central definition
String bme280_getLargeDisplayData(uint8_t valueIndex, ...) {
  if (valueIndex >= BME280_PARAM_COUNT) return "";
  *outLabel = bme280_params[valueIndex].label;
  *outUnit = bme280_params[valueIndex].unit;
  *outColor = bme280_params[valueIndex].color;
  return bme280_getValue(valueIndex);
}
```

Reduction: **30 lines → 6 lines per sensor**

#### 4. Generic Sensor Page Renderer

```cpp
/// Replaces all hardcoded rowTriple() calls
void displaySensorPage(
  const ParameterTriple* params,
  uint8_t paramCount,
  String (*getValueFunc)(uint8_t)
) {
  int displayY = 56;
  for (uint8_t i = 0; i < paramCount && displayY < 280; i++) {
    String value = getValueFunc(i);
    String label = String(params[i].label) + ":";
    rowTriple(displayY, label.c_str(), value, params[i].unit, params[i].color);
    displayY += 24;
  }
  tft.setTextPadding(0);
}
```

#### 5. Simplified Sensor Page Implementation

```cpp
/// Before: 10 rowTriple() calls
void pageBme280() {
  if (!available.bme280) return unavailable(...);
  if (largeDisplayMode.active && ...) return renderLargeDisplay(...);
  
  rowTriple(56, "Temperature:", String(bme280Values.temperature, 2), "C", TFT_GREEN);
  rowTriple(80, "Pressure:", String(bme280Values.pressure, 2), "hPa", TFT_GREEN);
  // ... 8 more rows ...
  footer("Y/A: Change page | BME280");
}

/// After: Single call to generic renderer
void pageBme280() {
  if (!available.bme280) return unavailable(...);
  if (largeDisplayMode.active && ...) return renderLargeDisplay(...);
  
  displaySensorPage(bme280_params, BME280_PARAM_COUNT, bme280_getValue);
  footer("Y/A: Change page | BME280");
}
```

Reduction: **15 lines → 6 lines per sensor page**

### Benefits

#### ✅ Consistency by Design
- Parameters defined ONCE
- Both display modes use same definition
- **Impossible to have inconsistencies** (before: discovered ~20 mismatches)

#### ✅ Reduced Code Size
- Flash: **381,697 bytes → 376,045 bytes** (Reduction: 5,652 bytes / 1.5%)
- Eliminated ~300+ lines of duplicate declarations
- Each sensor saves ~50 lines of redundant code

#### ✅ Maintainability
- To change parameter: Edit ONE place
- Add new sensor: Create arrays, add getValue(), done
- No risk of forgetting to update second copy

#### ✅ Cleaner Code
- Central definitions = single source of truth
- Large Display Helpers: Pure delegation (6 lines each)
- Sensor Pages: Single generic renderer call
- Value logic centralized in Value Getters

### All 10 Sensors Refactored

| Sensor | Params | Definition | Getter | Large Helper | Page | Status |
|--------|--------|------------|--------|--------------|------|--------|
| BME280 | 7 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| Gamepad | 8 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| INA219 | 7 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| Encoder | 7 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| SCD30 | 3 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| SI1145 | 4 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| TSL2591 | 4 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| VEML6070 | 3 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| VEML6075 | 6 | ✅ | ✅ | ✅ 6L | ✅ 1L | ✅ |
| DS2484 | Dynamic | N/A | N/A | ✅ Kept special | ✅ Kept special | ✅ |

### Code Statistics

**Before Refactoring:**
- Parameter definitions: 270+ lines (9 sensors × 30 lines each)
- Value switches: 180+ lines (9 sensors × 20 lines each)
- Sensor pages: 150+ lines (9 sensors × 15 lines each)
- **Total redundancy: ~600 lines**

**After Refactoring:**
- Central definitions: 95 lines (9 sensors × ~10 lines each)
- Value getters: 100 lines (9 sensors × ~11 lines each)
- Large display helpers: 54 lines (9 sensors × 6 lines each)
- Sensor pages: 54 lines (9 sensors × 6 lines each)
- Generic renderer: 15 lines
- **Total: ~320 lines (47% reduction)**

### Compilation Results

```
Flash: 376,045 bytes (28.7%) - DECREASED from 381,697 (29.1%)
RAM:   23,072 bytes (7.0%) - Unchanged
Savings: 5,652 bytes
```

### Key Design Decisions

1. **ParameterTriple struct** instead of separate arrays
   - Keeps related data together
   - Type-safe
   - Easier to understand

2. **Separate Value Getter functions** instead of embedding in Large Display Helpers
   - Cleaner separation of concerns
   - Reusable by both display modes
   - Easier to test

3. **Generic displaySensorPage()** function
   - Works for all 9 sensors identically
   - Eliminates 9× duplicate page rendering code
   - Makes adding new sensors trivial

4. **DS2484 kept special**
   - Dynamic number of parameters (1-9)
   - Different data structure (array of sensors)
   - Requires custom rendering logic
   - Not forced into generic pattern

### Testing Checklist

- [x] All 10 sensor pages render parameters correctly
- [x] Large Display Mode shows correct values
- [x] Parameter order matches between modes
- [x] Units display correctly
- [x] Colors display correctly  
- [x] Display scrolls all parameters properly
- [x] Button B cycles correctly
- [x] Button Y exits without artifacts
- [x] Firmware compiled smaller (not larger)
- [x] All sensors functional and responsive

### Conclusion

**FEATURE-SPEC-008 successfully achieves consistency through design rather than discipline.**

No more manual synchronization between helpers and pages. Parameters are the single source of truth. Both display modes delegate to the same central definitions and value getters.

Result: **Impossible to have parameter inconsistencies by the design itself.**
