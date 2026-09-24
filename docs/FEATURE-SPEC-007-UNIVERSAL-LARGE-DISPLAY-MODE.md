# FEATURE-SPEC-007: Universal Large Display Mode for All Sensor Pages

**Status:** ✅ **Implemented** (2026-09-25)  
**Created:** 2026-09-24  
**Priority:** Medium (UI Enhancement / User Experience)  
**Category:** User Interface / Display Rendering  
**Related Features:** FR-016, FR-017, FR-018, FR-019; **Depends on FEATURE-SPEC-008 (Central Parameter Definitions)**  
**Related Business Rules:** BR-SP-005 (existing, INA219-only) — now generalized to all pages

---

## Executive Summary

✅ **Fully Implemented:** Universal Large Display Mode now works on all 10 sensor pages (Pages 0–9), allowing users to focus on individual measurements in enlarged, centered format.

**Architecture:** FEATURE-SPEC-007 is built on **FEATURE-SPEC-008 (Central Parameter Definitions)**, which provides:
- Centralized parameter metadata (label, unit, color) for each sensor
- Unified value getter functions with consistent formatting
- Parameter counts per page for value cycling
- Single source of truth eliminates duplicate display code

**Key Achievement:** Both standard sensor page view and large display mode now delegate to identical central parameter definitions, guaranteeing consistency by design.

---

## Problem Analysis

### Current Limitation

**Issue:** Large Display Mode only works on Page 2 (INA219)
- Pages 0–1, 3–9 have no alternate viewing mode
- Users cannot zoom in on individual sensor values (e.g., just temperature from BME280)
- UI is limited to 7 data rows per page, may be too small for detailed viewing
- No way to focus on a single measurement during troubleshooting

### Use Case Rationale

**Scenario 1: Field Diagnostics**
- Technician uses device to troubleshoot sensor malfunction
- Wants to see only temperature (BME280) in large format
- Current: Must read small font value from multi-row display
- Desired: Press B to enter large display, see large temperature value

**Scenario 2: Real-Time Monitoring**
- Operator watches power consumption live on INA219 page
- Uses large display mode to focus on "Current" value
- Current: Works (existing INA219 mode)
- Desired: Same capability for other pages (e.g., CO₂ concentration from SCD30)

**Scenario 3: Presentation / Public Display**
- Device projected/displayed for audience
- Presenter wants to show individual measurement values in readable size
- Current: Font size limited, difficult to read from distance
- Desired: Switch to large display mode; value readable from 5 meters away

---

## Implementation Architecture (FEATURE-SPEC-008)

**Key Design Decision:** FEATURE-SPEC-007 is built entirely on FEATURE-SPEC-008 (Central Parameter Definitions). This dependency ensures consistency by design.

### Parameter Definition (Central Source of Truth)

Each sensor page defines its displayable parameters once:
```cpp
const ParameterTriple sensor_params[] = {
  {"Parameter Name 1", "unit1", TFT_COLOR1},
  {"Parameter Name 2", "unit2", TFT_COLOR2},
  // ... up to 8-9 parameters per sensor
};
const uint8_t SENSOR_PARAM_COUNT = N;
```

**Used by both:**
1. ✅ Standard sensor page renderer (`displaySensorPage()`)
2. ✅ Large display mode renderer (`renderLargeDisplay()`)

### Value Getter Functions

Each sensor implements a value getter:
```cpp
String sensor_getValue(uint8_t index) {
  if (index >= SENSOR_PARAM_COUNT || !available.sensor) return "";
  switch (index) {
    case 0: return String(sensorValues.field1, decimals);
    case 1: return String(sensorValues.field2, decimals);
    // ...
  }
}
```

**Used by both:**
1. ✅ `displaySensorPage()` loop (populate row values)
2. ✅ Large display helper (`sensor_getLargeDisplayData()`)

### Large Display Helper (Delegating Pattern)

Simplified 6-line helper that delegates to central definitions:
```cpp
String sensor_getLargeDisplayData(uint8_t idx, const char** label, const char** unit, uint16_t* color) {
  if (idx >= SENSOR_PARAM_COUNT) return "";
  *label = sensor_params[idx].label;
  *unit = sensor_params[idx].unit;
  *color = sensor_params[idx].color;
  return sensor_getValue(idx);  // Reuse value getter
}
```

**Before FEATURE-SPEC-008:** 30-line helper with duplicate label/unit/color arrays  
**After FEATURE-SPEC-008:** 6-line helper with pure delegation

### Unified Large Display Renderer

Single renderer dispatches to page-specific helpers:
```cpp
void renderLargeDisplay(uint8_t pageIndex, uint8_t valueIndex) {
  // Get page-specific data
  String value = "";
  const char* label = nullptr;
  const char* unit = nullptr;
  uint16_t color = TFT_WHITE;
  
  switch (pageIndex) {
    case 0: value = bme280_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    case 1: value = gamepad_getLargeDisplayData(valueIndex, &label, &unit, &color); break;
    // ... cases 2-9 ...
  }
  
  // Render identical layout for all pages
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(pageTitle, 240, 18, 4);      // Title (Font 4, Cyan)
  tft.drawString(label, 240, 70, 4);          // Label (Font 4, Cyan)
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, 240, 160, 8);         // Value (Font 8, color-coded)
  tft.drawString(unit, 240, 240, 4);          // Unit (Font 4, White)
  tft.drawString("B:next X:prev Y:exit", 240, 295, 2);  // Footer
}
```

**Benefits:**
- Single renderer serves all 10 pages
- No per-page rendering logic duplication
- Layout identical across all pages (consistency guaranteed)
- Easy to modify rendering style (change once, affects all pages)

---

## Detailed Analysis

### 1. Affected Requirements

#### Existing Requirements (Modified)

| Requirement | Current State | Change |
|-------------|---------------|--------|
| **FR-012** | INA219 large display only | **Extend** to all sensor pages |
| **FR-001** | 10 standard sensor pages | No change to standard mode |
| **FR-005** | Sensor caching architecture | No change; large display reads cache |
| **FR-015** | Display rate limiting (5 Hz) | **Apply** to large display mode |

#### New Requirements

| ID | Requirement | Details |
|----|-------------|---------|
| **FR-016** | Large Display Mode for All Pages | Each sensor page (0–9) supports large display with B button entry |
| **FR-017** | Sensor-Specific Value Selection | Each page defines which values display in large mode (not necessarily all) |
| **FR-018** | Large Display Navigation | B=forward, X=backward, Y=exit cycle through values per page |
| **FR-019** | Consistent UI Behavior | All pages follow same interaction pattern |

### 2. Affected Domain Entities

#### New/Modified Data Structures

```cpp
struct LargeDisplayConfig {
  bool enabled;                 // Page supports large display mode
  uint8_t valueCount;           // Number of displayable values (3-8)
  const char* pageTitle;        // e.g., "BME280 Large Display"
  // Value definitions per sensor (see sensor-specific sections below)
};

// Global large display state (ONE instance for entire app)
struct {
  bool active;                  // Large display mode currently active
  uint8_t currentPage;          // Page index when mode entered (0-9)
  uint8_t valueIndex;           // Current value being displayed (0-valueCount-1)
} largeDisplayMode;
```

#### Modified Structs

All existing sensor value structs (Bme280Values, Tsl2591Values, etc.) remain unchanged. Large display mode only reads from existing cache.

### 3. Affected Business Rules

#### Current Rules (Modified)

| Rule | Current | Change |
|------|---------|--------|
| **BR-SP-005** | Button B toggles large display on Page 2 | **Extend** to all pages |
| **BR-011** | Display rate limiting (200 ms min) | **Apply** to large display refresh |
| **BR-004** | Page rendering logic | **Add** large display rendering path |

#### New Rules

| Rule ID | Title | Details |
|---------|-------|---------|
| **BR-SP-006** | Large Display Mode Entry | Button B while on any sensor page activates large display at valueIndex=0 |
| **BR-SP-007** | Large Display Value Cycling | B=next, X=prev, Y=exit; wraps at boundaries (0 ↔ valueCount-1) |
| **BR-SP-008** | Page Navigation Disabled | Y/A buttons disabled in large display (Y=exit mode, A=inactive) |
| **BR-SP-009** | Mode Exit Behavior | Pressing Y exits mode, resets valueIndex=0, returns to standard page |
| **BR-SP-010** | Large Display Rate Limiting | Display updates every 200 ms minimum (5 Hz max), same as standard pages |

### 4. Affected Database Structures

**None** — This is an embedded firmware application with no persistent database.

### 5. Affected APIs

#### New Display Rendering Functions

**Generic Large Display Renderer (Reusable Across All Pages):**

```cpp
/// @brief Render large display mode for current sensor page
/// @param sensorPage  Current page index (0-9)
/// @param valueIndex  Current value to display (0 to pageValueCount-1)
/// Displays:
///   - Sensor title (header, Font 4, Cyan)
///   - Value label (Font 4, Cyan, centered)
///   - Numeric value (Font 8, color-coded, very large, centered)
///   - Unit (Font 2, White, centered)
///   - Footer instructions (Font 1, Dark Grey, centered)
void renderLargeDisplay(uint8_t sensorPage, uint8_t valueIndex);
```

**Sensor-Specific Helper Functions (One Set Per Page):**

```cpp
/// For each page that supports large display:

// Page 0 (BME280)
const char* bme280_getLargeDisplayLabel(uint8_t valueIndex);  // e.g., "Temperatur", "Luftdruck"
String bme280_getLargeDisplayValue(uint8_t valueIndex);       // e.g., "24.50"
const char* bme280_getLargeDisplayUnit(uint8_t valueIndex);   // e.g., "°C", "hPa"
uint16_t bme280_getLargeDisplayColor(uint8_t valueIndex);     // e.g., TFT_GREEN

// Page 1 (Gamepad)
const char* gamepad_getLargeDisplayLabel(uint8_t valueIndex);
String gamepad_getLargeDisplayValue(uint8_t valueIndex);
const char* gamepad_getLargeDisplayUnit(uint8_t valueIndex);
uint16_t gamepad_getLargeDisplayColor(uint8_t valueIndex);

// ... (similar for Pages 3-9; Page 2 already has these)
```

**Page Rendering Modifications (Each Sensor Page):**

```cpp
// In each page renderer (pageXxx()):
if (largeDisplayMode.active && largeDisplayMode.currentPage == pageIndex) {
  renderLargeDisplay(pageIndex, largeDisplayMode.valueIndex);
} else {
  // Standard page rendering (unchanged)
  pageXxxStandard();
}
```

**Button Handling Updates (In Main Loop):**

```cpp
// In gamepad button handler:
if (currentPage != lastPage) {
  largeDisplayMode.active = false;  // Auto-exit on page change
  largeDisplayMode.valueIndex = 0;
}

// Button B behavior (context-sensitive):
if (!largeDisplayMode.active && canEnterLargeDisplay(currentPage)) {
  largeDisplayMode.active = true;
  largeDisplayMode.currentPage = currentPage;
  largeDisplayMode.valueIndex = 0;
} else if (largeDisplayMode.active) {
  largeDisplayMode.valueIndex = (largeDisplayMode.valueIndex + 1) % getPageValueCount(currentPage);
}

// Button X behavior (context-sensitive):
if (largeDisplayMode.active) {
  largeDisplayMode.valueIndex = (largeDisplayMode.valueIndex - 1 + getPageValueCount(currentPage)) % getPageValueCount(currentPage);
}

// Button Y behavior (context-sensitive):
if (largeDisplayMode.active) {
  largeDisplayMode.active = false;
  largeDisplayMode.valueIndex = 0;
} else {
  // Standard page navigation
  currentPage = (currentPage + 10 - 1) % 10;
}
```

### 6. Affected UI Components

#### New/Modified Display Functions

| Component | Change |
|-----------|--------|
| Page 0 Header | **Add** large display conditional |
| Page 0 Renderer | **Add** large display rendering path |
| Page 1–9 | **Same** as Page 0 |
| Button Handler | **Extend** to handle mode switching |
| Footer | **Modify** to show mode-specific instructions |

#### Display Layout (Large Display Mode - Universal Template)

```
┌──────────────────────────────────────────────────┐
│                                                  │
│ [SENSOR TITLE - Top, Font 4, Cyan]               │  y ≈ 80
│ e.g., "BME280 Large Display"                     │
│                                                  │
│ [VALUE LABEL - Upper-mid, Font 4, Cyan]          │  y ≈ 140
│ e.g., "Temperatur"                               │
│                                                  │
│ [NUMERIC VALUE - Center, Font 8, Color-coded]    │  y ≈ 200
│ e.g., "24.50"                                    │
│ (Largest, most prominent element)                │
│                                                  │
│ [UNIT - Below value, Font 2, White]              │  y ≈ 280
│ e.g., "°C"                                       │
│                                                  │
│ [FOOTER INSTRUCTIONS - Bottom, Font 1, DarkGrey]│  y ≈ 350
│ "B:next X:prev Y:exit"                           │
│                                                  │
└──────────────────────────────────────────────────┘
```

**Color Scheme (Inherited from Standard Pages):**
- Title: TFT_CYAN (sensor page header style)
- Label: TFT_CYAN (derived/computed values)
- Value: Color-coded per sensor measurement (Green/Orange/Cyan/Red, never Magenta)
- Unit: TFT_WHITE
- Footer: TFT_DARKGREY

### 7. Security & Authorization

**No security changes.** Embedded device; no authentication/authorization required.

### 8. Tests Required (New)

#### Unit Tests

| Test | Purpose |
|------|---------|
| **UT-LDM-001** | Value index wrapping (forward/backward cycles) |
| **UT-LDM-002** | Page-specific value count validation |
| **UT-LDM-003** | Label/value/unit/color retrieval per sensor |
| **UT-LDM-004** | Mode activation/deactivation state machine |

#### Integration Tests

| Test | Purpose |
|------|---------|
| **IT-LDM-001** | Button B entry to large display on each page |
| **IT-LDM-002** | Button X/B cycling through all values per page |
| **IT-LDM-003** | Button Y exit, return to standard page |
| **IT-LDM-004** | Page navigation (Y/A) disabled in large mode |
| **IT-LDM-005** | Auto-exit on page change via Y/A |
| **IT-LDM-006** | Display rate limiting applied (200 ms min between updates) |
| **IT-LDM-007** | All 10 pages support entry (or partial subset) |

### 9. Documentation Files to Update

| File | Change |
|------|--------|
| **01-requirements.md** | FR-016, FR-017, FR-018, FR-019 |
| **06-business-rules.md** | BR-SP-006, BR-SP-007, BR-SP-008, BR-SP-009, BR-SP-010 |
| **05-api.md** | New large display rendering functions |
| **07-ui.md** | Layout specifications for all 10 pages in large display mode |
| **10-testing.md** | Test cases for large display mode across all pages |
| **14-reconstruction-plan.md** | Implementation sequence for each page |

---

## Per-Page Specifications

### Page 0: BME280 Large Display Mode

**Entry:** Button B on standard Page 0  
**Values:** 7 displayable values

| Index | Label | Value | Unit | Color | Notes |
|-------|-------|-------|------|-------|-------|
| 0 | Temperatur | 24.50 | °C | Green | Primary measurement |
| 1 | Druck | 1013.25 | hPa | Green | Atmospheric pressure |
| 2 | Feuchtigkeit | 45.30 | % | Orange | Relative humidity |
| 3 | Taupunkt | 14.20 | °C | Cyan | Derived value |
| 4 | Höhe | 123.45 | m | Cyan | Derived from pressure |
| 5 | Luftqualität | 0.95 | AQI | Orange | Computed index |
| 6 | Temperaturtrend | +0.5 | °C/min | Cyan | Rate of change |

**Display Strategy:**
- All 7 values from `bme280Values` struct
- Most frequently viewed: Temperature (index 0) — enter at index 0
- Color consistent with standard page

**Functions Required:**
- `bool canEnterLargeDisplay_Page0()` → checks `available.bme280`
- `bme280_getLargeDisplayLabel(index)` → German label
- `bme280_getLargeDisplayValue(index)` → formatted string
- `bme280_getLargeDisplayUnit(index)` → unit string (e.g., "°C", "%", "hPa")
- `bme280_getLargeDisplayColor(index)` → TFT color constant

---

### Page 1: Gamepad QT Large Display Mode

**Entry:** Button B on standard Page 1  
**Values:** 4 displayable values

| Index | Label | Value | Unit | Color | Notes |
|-------|-------|-------|------|-------|-------|
| 0 | Joystick X | -512 | ADC | Green | Horizontal axis |
| 1 | Joystick Y | +245 | ADC | Green | Vertical axis |
| 2 | Button State | "00001010" | Binary | Orange | Button bit flags |
| 3 | Drücke gesamt | 47 | Count | Cyan | Total presses |

**Display Strategy:**
- All 4 values from `gamepadValues` struct
- Most frequently viewed: Joystick X (index 0)
- Bit flags displayed as binary string for button state visualization

**Functions Required:**
- Similar to BME280 pattern

---

### Page 2: INA219 Large Display Mode

**Status:** Already implemented  
**Entry:** Button B on standard Page 2  
**Values:** 7 displayable values (unchanged from existing implementation)

**Note:** Existing helper functions (`getLargeDisplayLabel`, `getLargeDisplayValue`, `getLargeDisplayUnit`, `getLargeDisplayColor`) continue to work. No modifications needed for backward compatibility.

---

### Page 3: Rotary Encoder Large Display Mode

**Entry:** Button B on standard Page 3  
**Values:** 5 displayable values

| Index | Label | Value | Unit | Color | Notes |
|-------|-------|-------|------|-------|-------|
| 0 | Position | 1234 | Ticks | Green | Absolute position |
| 1 | Delta | +5 | Ticks | Orange | Since last read |
| 2 | Drücke | 23 | Count | Cyan | Press count |
| 3 | LED Farbe | "green" | Text | Cyan | Current color name |
| 4 | LED Helligkeit | 128 | /255 | Green | Brightness level |

**Display Strategy:**
- Most frequently viewed: Position (index 0)
- Encoder is live (20 ms refresh), so large display values update frequently
- LED state updates immediately on dial/button interaction

**Functions Required:**
- Similar to BME280 pattern

---

### Page 4: SCD30 Large Display Mode

**Entry:** Button B on standard Page 4  
**Values:** 3 displayable values

| Index | Label | Value | Unit | Color | Notes |
|-------|-------|-------|------|-------|-------|
| 0 | CO₂ Konzentration | 2100 | ppm | Green | Primary measurement |
| 1 | Temperatur | 22.50 | °C | Green | Ambient temperature |
| 2 | Feuchtigkeit | 55.30 | % | Orange | Relative humidity |

**Display Strategy:**
- CO₂ most important; enter at index 0
- All 3 values essential for air quality assessment

**Functions Required:**
- Similar pattern

---

### Page 5: SI1145 Large Display Mode

**Entry:** Button B on standard Page 5  
**Values:** 4 displayable values

| Index | Label | Value | Unit | Color | Notes |
|-------|-------|-------|------|-------|-------|
| 0 | Sichtbares Licht | 12345 | ADC | Green | Visible spectrum |
| 1 | Infrarot | 5432 | ADC | Green | IR spectrum |
| 2 | UV Index | 3.2 | Index | Cyan | Derived |
| 3 | UV Rohwert | 320 | Counts | Orange | Raw measurement |

**Display Strategy:**
- Visible light most relevant; enter at index 0

**Functions Required:**
- Similar pattern

---

### Page 6: TSL2591 Large Display Mode

**Entry:** Button B on standard Page 6  
**Values:** 4 displayable values

| Index | Label | Value | Unit | Color | Notes |
|-------|-------|-------|------|-------|-------|
| 0 | Vollspektrum | 45678 | ADC | Green | Full spectrum count |
| 1 | Infrarot | 12345 | ADC | Green | IR only count |
| 2 | Sichtbar | 33333 | ADC | Orange | Calculated (full-IR) |
| 3 | Luxwert | 254.30 | lux | Cyan | Illuminance |

**Display Strategy:**
- Lux value (index 3) most user-meaningful; but enter at Fullspectrum (index 0) for consistency

**Functions Required:**
- Similar pattern

---

### Page 7: VEML6070 Large Display Mode

**Entry:** Button B on standard Page 7  
**Values:** 3 displayable values

| Index | Label | Value | Unit | Color | Notes |
|-------|-------|-------|------|-------|-------|
| 0 | UV Rohwert | 1200 | ADC | Green | Raw ADC count |
| 1 | UV Stufe | "High" | Text | Orange | Classification |
| 2 | LSB/MSB | "0x40 / 0x04" | Hex | Cyan | Individual bytes |

**Display Strategy:**
- UV value most important; enter at index 0

**Functions Required:**
- Similar pattern

---

### Page 8: VEML6075 Large Display Mode

**Entry:** Button B on standard Page 8  
**Values:** 6 displayable values

| Index | Label | Value | Unit | Color | Notes |
|-------|-------|-------|------|-------|-------|
| 0 | UVA Rohwert | 450 | ADC | Green | Raw UVA count |
| 1 | UVB Rohwert | 320 | ADC | Green | Raw UVB count |
| 2 | UVA kompensiert | 385.20 | Calc | Cyan | Compensated UVA |
| 3 | UVB kompensiert | 268.50 | Calc | Cyan | Compensated UVB |
| 4 | UV Index | 4.15 | Index | Orange | Derived index |
| 5 | Kompensation | "2.22,1.33" | Factors | Orange | Compensation values |

**Display Strategy:**
- UV Index (index 4) most meaningful; but enter at Raw UVA for consistency

**Functions Required:**
- Similar pattern

---

### Page 9: DS2484 1-Wire Large Display Mode

**Entry:** Button B on standard Page 9  
**Values:** 3–24 displayable values (3 values per discovered 1-Wire sensor, up to 8 sensors)

| Index Range | Label | Value | Unit | Color | Notes |
|-------------|-------|-------|------|-------|-------|
| 0–2 | Sensor 1: ROM-ID, Temperature, Status | ... | ... | ... | Sensor 1 data |
| 3–5 | Sensor 2: ROM-ID, Temperature, Status | ... | ... | ... | Sensor 2 data |
| ... | ... | ... | ... | ... | ... |
| 21–23 | Sensor 8: ROM-ID, Temperature, Status | ... | ... | ... | Sensor 8 data (max) |

**Display Strategy:**
- Sensor count dynamic (0–8)
- Each sensor occupies 3 value slots:
  1. ROM-ID (8-byte hex address)
  2. Temperature (current reading)
  3. Status (connected/disconnected/error)
- Most frequently viewed: Temperatures (indices 1, 4, 7, ..., 22)

**Complexity Note:** This page requires dynamic value count and formatting based on discovered sensors.

**Functions Required:**
- Similar pattern, but with dynamic value count:
  ```cpp
  uint8_t ds2484_getValueCount()  // Returns 3*sensorCount
  const char* ds2484_getLargeDisplayLabel(uint8_t valueIndex);  // e.g., "Sensor 1: Temperatur"
  String ds2484_getLargeDisplayValue(uint8_t valueIndex);
  const char* ds2484_getLargeDisplayUnit(uint8_t valueIndex);   // e.g., "°C" or "Hex"
  uint16_t ds2484_getLargeDisplayColor(uint8_t valueIndex);
  ```

---

## Implementation Approach

### Strategy: Progressive Implementation

Recommended rollout sequence (not required to implement all at once):

1. **Phase 1: Core Infrastructure**
   - Implement generic `renderLargeDisplay()` function
   - Add global `largeDisplayMode` state struct
   - Modify page rendering and button handlers to support mode

2. **Phase 2: INA219 Verification**
   - Ensure existing Page 2 large display still works
   - Test backwards compatibility
   - Refactor INA219 to use generic functions

3. **Phase 3: High-Priority Pages**
   - Implement for Pages 0 (BME280) and 4 (SCD30)
   - These are most commonly used sensors

4. **Phase 4: Remaining Pages**
   - Pages 1, 3, 5, 6, 7, 8 (in any order)
   - Each page is independent

5. **Phase 5: DS2484 (Optional)**
   - Complex due to dynamic sensor count
   - Can be deferred if time-constrained

### Code Organization

**Suggested File Structure:**

```cpp
// src/main.cpp: Core changes
- Add largeDisplayMode struct
- Modify renderCurrentPage() to call renderLargeDisplay() conditional
- Modify button handlers for B/X/Y in large mode

// In page renderer sections (lines ~1500-2000):
// For each page XXX:
void pageXxxLargeDisplay(uint8_t valueIndex);  // New large display renderer
const char* xxxGetLargeDisplayLabel(uint8_t valueIndex);      // New helper
String xxxGetLargeDisplayValue(uint8_t valueIndex);           // New helper
const char* xxxGetLargeDisplayUnit(uint8_t valueIndex);       // New helper
uint16_t xxxGetLargeDisplayColor(uint8_t valueIndex);         // New helper

// Generic renderer function (reusable across all pages):
void renderLargeDisplay(uint8_t pageIndex, uint8_t valueIndex);
```

---

## Acceptance Criteria

### Functional Acceptance

- [ ] Button B on Page 0 enters large display mode
- [ ] Button B on Page 1 enters large display mode
- [ ] Button B on Pages 2–9 enter large display mode (or subset if partial implementation)
- [ ] B button cycles forward through all values per page
- [ ] X button cycles backward through all values per page
- [ ] Y button exits mode, returns to standard page
- [ ] A button is inactive (no navigation) in large display mode
- [ ] Page navigation (Y/A) disabled while in large display mode
- [ ] Switching pages (via earlier navigation) exits large display mode
- [ ] Display updates every 200 ms minimum (5 Hz rate limit applies)

### UI Acceptance

- [ ] Large display text readable from 5+ meters
- [ ] Color scheme consistent with standard pages (never Magenta)
- [ ] Footer instructions clear ("B:next X:prev Y:exit")
- [ ] No visual glitches or flicker on value cycling
- [ ] Transition between standard and large display is smooth

### Compatibility Acceptance

- [ ] Existing INA219 large display mode works unchanged
- [ ] All existing standard page displays work unchanged
- [ ] No performance degradation (loop frequency ≥ 50 Hz)
- [ ] Memory usage acceptable (typical embedded device limits)

---

## Known Constraints & Limitations

### Display Area Constraints

- Maximum font size: 8 (used for large value display)
- Minimum readable font: 1 (used for footer)
- Center alignment requires text width calculation
- Some values may overflow if not formatted carefully

### Sensor-Specific Constraints

| Page | Constraint |
|------|------------|
| **0 (BME280)** | All 7 values fit in 3-slot display paradigm |
| **1 (Gamepad)** | Button state as binary string may be awkward |
| **3 (Encoder)** | Live refresh (20 ms) may show flickering during mode |
| **4 (SCD30)** | Only 3 values; sparse cycling |
| **9 (DS2484)** | Dynamic value count; complex formatting |

### Performance Constraints

- Rate limiting (200 ms minimum) applies to large display
- No higher refresh rate even if requested
- Large font rendering (Font 8) may impact loop timing

---

## Open Questions

1. **Entry Point Clarity:**
   - Should ALL 10 pages support large display mode, or only a subset (e.g., sensor pages 0, 4-8 only, excluding input pages 1, 3)?
   - Current spec assumes all; should confirm with product owner.

2. **Value Selection Per Page:**
   - For pages with many values (e.g., BME280 with 7 values, VEML6075 with 6), are all values equally important?
   - Should some pages prioritize certain values (e.g., default to Temperature for BME280)?
   - Or should cycling always start at index 0 for consistency?

3. **Page 9 (DS2484) Complexity:**
   - Current implementation shows 3–24 values (3 per 1-Wire sensor).
   - Is this desired? Alternative: show only temperature for all sensors, skip ROM-ID display?
   - Or defer DS2484 support to Phase 2?

4. **Button B Ambiguity:**
   - If Page 0 already uses Button B for something (future feature), how to resolve conflict?
   - Current assumption: Button B only controls large display (no other function on sensor pages).
   - Confirm this is acceptable.

5. **Sensor Missing Behavior:**
   - If user on Page 0 (BME280) presses B, but sensor unavailable (error screen displayed)?
   - Current assumption: Button B press ignored (page locked to error view).
   - Confirm this is desired.

6. **Memory Budget for Helper Functions:**
   - Each page requires 4 helper functions (label, value, unit, color).
   - 10 pages × 4 functions = 40 functions total.
   - Is this acceptable code size? Alternative: combine into single function with switch statement?

7. **Standard Page Modification Scope:**
   - Should we refactor existing page renderers to use same helper functions?
   - Current approach: Page renderers read cache; helpers use same cache.
   - Or create unified rendering system for both modes?

8. **Documentation of "Most Important Value":**
   - For large display mode, which value should be shown first (index 0)?
   - Should documentation recommend a default entry point per page?
   - E.g., BME280 → Temperature, INA219 → Runtime, SCD30 → CO₂?

9. **Rate Limiting Interaction:**
   - Large display mode updates at 200 ms minimum (5 Hz).
   - If user rapidly presses B to cycle, will cycling feel sluggish?
   - Should cycling bypass rate limit and update immediately (only final value is rate-limited)?

10. **Gamepad Page (Page 1) Use Case:**
    - Is large display mode useful for Gamepad page (showing joystick values one at a time)?
    - Or should Gamepad be excluded from large display support?
    - Current spec includes it; confirm if this is desired.

---

## Recommendations for Implementation

1. **Start with Phase 1 & 2** (core infrastructure + INA219 verification)
   - Validate pattern before rolling out to 9 other pages

2. **Implement Phase 3 (BME280, SCD30)** next
   - These are most commonly used for field diagnostics

3. **Defer Page 9 (DS2484)** to Phase 2 (after core is stable)
   - Most complex due to dynamic sensor count

4. **Answer open questions before implementation**
   - Particularly: entry point scope, value count per page, button usage

5. **Consider unified rendering system** (future improvement)
   - Current approach requires per-page helper functions
   - Long-term: data-driven config table for values could reduce code duplication

---

## Success Metrics

- All large display modes activate correctly (B button entry works)
- No performance regression (loop frequency maintained ≥ 50 Hz)
- User can cycle through all values per page smoothly
- Display is readable from 5+ meters away
- Existing INA219 large display mode works unchanged (backward compatible)

---

## Related Documents

- [01-requirements.md](01-requirements.md) — High-level requirements (FR-012, new FR-016–019)
- [06-business-rules.md](06-business-rules.md) — Business rules (BR-SP-005, new BR-SP-006–010)
- [07-ui.md](07-ui.md) — UI specifications (all pages)
- [05-api.md](05-api.md) — Page rendering API
- [10-testing.md](10-testing.md) — Test specifications

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-09-24 | AI Assistant | Initial feature specification |
