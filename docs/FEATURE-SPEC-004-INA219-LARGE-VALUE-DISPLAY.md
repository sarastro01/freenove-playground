# Feature Specification: INA219 Large Value Display Mode

**Feature Name:** Large Value Display Mode (INA219)  
**Status:** ✅ Fully Specified — Ready for Implementation  
**Target Component:** Page 2 (INA219 Power Monitor)  
**Affected Gamepad Buttons:** B (forward cycle), X (reverse cycle), Y (exit mode)  
**Complexity:** Medium  
**Priority:** [To be determined]  

---

## 1. Executive Summary

This feature adds a dedicated **large value display mode** to the INA219 power monitor page (Page 2). Users can cycle through individual sensor measurements in an enlarged, easy-to-read format optimized for quick glancing at a single metric at a time.

**Primary Use Case:** Check a single metric (e.g., current draw) without navigating away from the page, while values update in real-time.

---

## 2. Functional Behavior

### 2.1 Entry Point

**Trigger:** User presses Gamepad Button B while on INA219 page (Page 2)

**Pre-conditions:**
- Currently viewing Page 2 (INA219 Power Monitor)
- INA219 sensor is available (`available.ina219 == true`)
- Not already in large display mode

**Action:** 
- Transition to large display mode
- Display the first value in the cycle: **"Laufzeit" (Runtime)**
- Render label and value in large format (details below)
- Mode indicator appears on screen (visual confirmation)

**Post-conditions:**
- Large display mode is active
- Display shows first value from cycle
- Button B bound to "next value" (forward cycle)
- Button X bound to "previous value" (reverse cycle)
- Button Y bound to "exit mode"

---

### 2.2 Forward Cycling (Button B)

**Trigger:** Gamepad Button B pressed while in large display mode

**Cycle Order (INA219 Page 2):**
```
1. Laufzeit (Runtime)        → HH:MM:SS
2. Batterie (Load Voltage)   → x.xxx V
3. Versorgung (Bus Voltage)  → x.xxx V
4. Strom (Current)           → x.x mA [status]
5. Ladung (Charge)           → x.xxx mAh
6. Shunt-Spannung            → x.xx mV
7. Power                     → xxxx mW
8. Back to Laufzeit          (wrap-around)
```

**Behavior per Press:**
- Increment internal value index: `largeDisplayIndex = (largeDisplayIndex + 1) % 7`
- Clear screen
- Re-render large display with new value
- Update continues in real-time

**Edge Case:** If user mashes Button B repeatedly, only the most recent press within a single frame matters (debouncing handled by existing gamepad handler @ 40 Hz).

---

### 2.3 Reverse Cycling (Button X)

**Trigger:** Gamepad Button X pressed while in large display mode

**Behavior:**
- Decrement internal value index: `largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7`
- Clear screen
- Re-render large display with new value
- Cycles in reverse order (Laufzeit → Power → Shunt-Spannung → ... → Laufzeit)

**Symmetry:** Button X is the exact inverse of Button B; pressing X then B (or vice versa) returns to the same value.

---

### 2.4 Exit Mode (Button Y)

**Trigger:** Gamepad Button Y pressed while in large display mode

**Action:**
- Restore normal page navigation mode
- Clear large display mode state: `largeDisplayMode = false`, `largeDisplayIndex = 0`
- Re-render standard INA219 page (all 7 values visible)
- Button Y restored to "previous page" navigation
- Buttons B and X restored to unbound (or to their normal behavior if they have any)

**Post-conditions:**
- Large display mode is inactive
- Standard page layout displayed
- Normal navigation controls active

**Edge Case:** If user rapidly presses Y multiple times, the page remains in standard view (idempotent).

---

### 2.5 Real-Time Value Updates

**Trigger:** Every sensor read cycle (all 20 µs or 500 ms, depending on sensor timing)

**Behavior (while in large display mode):**
- Large displayed value is **not cached**; it reads fresh from the global `ina219Values` struct on each render cycle
- Label and value re-rendered with latest data
- No flicker (handled by display refresh rate)

**Example:**
- User enters large display mode on "Strom"
- Current value = 50 mA
- After 1 second, sensor reads 75 mA
- Large display automatically shows 75 mA without user action

---

## 3. UI Specifications

### 3.1 Large Display Layout

**Screen Layout in Large Display Mode:**

```
┌──────────────────────────────────────────────┐
│                                              │
│  INA219 Large Display (Header)               │  y=18, Font 4, Cyan
│  ─────────────────────────────────────────   │  y=48 (separator line)
│                                              │
│  Batterie (Label)                            │  y=70, Font 4, Cyan
│                                              │
│                                              │
│  12.456 (Value - Focal Point)                │  y=160, Font 8, Color-coded
│                                              │
│                                              │
│  V (Unit)                                    │  y=240, Font 4, White
│                                              │
│  B:next X:prev Y:exit (Footer)               │  y=295, Font 2, DarkGrey
│                                              │
└──────────────────────────────────────────────┘

Display Resolution: 480×320 px (landscape orientation)
All text centered horizontally at x=240 (midpoint)
```

---

### 3.2 Typography

| Element | Font | Size | Color | Position (pixels) |
|---------|------|------|-------|-------------------|
| Header | TFT Font 4 | Medium | TFT_CYAN | x=240 (centered), y=18 |
| Separator | — | — | TFT_DARKGREY | Horizontal line at y=48 |
| Label | TFT Font 4 | Medium | TFT_CYAN | x=240 (centered), y=70 |
| Value | TFT Font 8 | Large | Value-dependent color | x=240 (centered), y=160 |
| Unit | TFT Font 4 | Medium | TFT_WHITE | x=240 (centered), y=240 |
| Footer | TFT Font 2 | Small | TFT_DARKGREY | x=240 (centered), y=295 |

**Design Rationale:**
- Font 8 maximizes value readability (focal point of display)
- All text centered (MC_DATUM alignment) for balanced visual composition
- Y coordinates optimized for 320 px vertical height
- Footer positioned near screen bottom for easy reference

### 3.3 Color Scheme for Values

Each value retains its standard color from the normal page display:

| Value | Color | Rationale |
|-------|-------|-----------|
| Laufzeit | TFT_CYAN | Computed/derived value |
| Batterie (Load) | TFT_GREEN | Primary reading |
| Versorgung (Bus) | TFT_GREEN | Primary reading |
| Strom (Current) | TFT_ORANGE | Warning/caution indicator |
| Ladung (Charge) | TFT_CYAN | Computed/accumulated value |
| Shunt-Spannung | TFT_GREEN | Primary reading |
| Power | TFT_ORANGE | Secondary measurement |

---

### 3.4 Real-Time Refresh Rate

**Large Display Refresh:** 
- Inherit from sensor read frequency (currently 20 µs for INA219)
- **No separate polling required**; use existing `ina219Values` struct
- Large display re-renders on every main loop iteration (or every display refresh cycle, whichever is slower)

---

## 4. State Management

### 4.1 New Global State Variables

```cpp
// In main.cpp (global scope)
bool largeDisplayMode = false;       // Is large display mode active?
uint8_t largeDisplayIndex = 0;       // Current value index (0–6, 7 values total)
uint32_t largeDisplayLastUpdateMs = 0; // Timestamp of last large display update (for refresh timing)
```

### 4.2 State Transitions

```
Normal Page View
      ↓
   [Button B pressed on Page 2]
      ↓
Large Display Mode (Index 0: Laufzeit)
      ↓
   [Button B] → Index 1 (Batterie)
   [Button X] → Index 6 (Power)
   [Button Y] → Exit → Normal Page View
      ↓
Normal Page View
```

---

## 5. Data Model

### 5.1 Affected Data Structure

**Existing:**
```cpp
struct Ina219Values {
  float shunt;          // mV
  float bus;            // V
  float load;           // V
  float current;        // mA
  float power;          // mW
  float chargeMah;      // mAh
  bool initialized;
  uint32_t lastSampleUs;
  uint32_t startTimeUs;
};
```

**No changes to this struct.** Large display mode reads from existing fields.

### 5.2 Value Index Mapping

**Actual Implementation Mapping (per getLargeDisplayValue() function):**

```
Index | Label               | Value Source              | Decimals | Format       | Unit
------|---------------------|---------------------------|----------|--------------|------
  0   | Laufzeit            | Runtime (HH:MM:SS)        | N/A      | formatRuntime() | —
  1   | Batterie            | load (V)                  | 3        | x.xxx        | V
  2   | Versorgung          | bus (V)                   | 3        | x.xxx        | V
  3   | Strom               | current (mA)              | 1        | x.x          | mA
  4   | Ladung              | chargeMah (mAh)           | 3        | x.xxx        | mAh
  5   | Shunt-Spannung      | shunt (mV)                | 2        | x.xx         | mV
  6   | Power               | power (mW)                | 0        | xxxx         | mW
```

**Source:** Implementation in src/main.cpp, function `getLargeDisplayValue(uint8_t valueIndex)`

**Formatting Rules:**
- Index 0: Special case; runtime formatted as HH:MM:SS (hours:minutes:seconds) using `formatRuntime()`
- Indices 1–6: Formatted using `sprintf()` with appropriate decimal places
- All values read from live `ina219Values` struct (no caching)

---

## 6. API Changes

### 6.1 New Functions Required

```cpp
/**
 * Render large value display for INA219
 * 
 * Clears screen, displays label, value, unit, mode indicator, and instructions.
 * Reads current value from global ina219Values struct.
 * 
 * @param valueIndex (0-6) - Which value to display
 * 
 * @returns void
 */
void pageLargeDisplayIna219(uint8_t valueIndex);

/**
 * Get label string for a specific INA219 value index
 * 
 * @param valueIndex (0-6)
 * @returns const char* - Label string (e.g., "Laufzeit")
 */
const char* getLargeDisplayLabel(uint8_t valueIndex);

/**
 * Get unit string for a specific INA219 value index
 * 
 * @param valueIndex (0-6)
 * @returns const char* - Unit string (e.g., "V", "mA", "mAh", or empty string)
 */
const char* getLargeDisplayUnit(uint8_t valueIndex);

/**
 * Get formatted value string for a specific INA219 value index
 * Applies same formatting as standard page display
 * 
 * @param valueIndex (0-6)
 * @returns String - Formatted value (e.g., "25.500" or "12:34:56")
 */
String getLargeDisplayValue(uint8_t valueIndex);

/**
 * Get color for a specific INA219 value index
 * 
 * @param valueIndex (0-6)
 * @returns uint16_t - TFT_eSPI color constant
 */
uint16_t getLargeDisplayColor(uint8_t valueIndex);
```

### 6.2 Modified Functions

**`pageIna219()`** - Existing function:
- Add condition at start: `if (largeDisplayMode) { pageLargeDisplayIna219(largeDisplayIndex); return; }`
- Rest of function unchanged

**`loop()`** - Main loop:
- Add gamepad button logic for large display mode:
  ```cpp
  if (currentPage == 2 && largeDisplayMode) {  // Page 2 is INA219
    // Only B, X, Y are active; Y/A reserved for page navigation
    if (buttonBPressed && !lastB) {
      largeDisplayIndex = (largeDisplayIndex + 1) % 7;
    }
    if (buttonXPressed && !lastX) {
      largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7;
    }
    if (buttonYPressed && !lastY) {
      largeDisplayMode = false;  // Exit mode
    }
  } else if (currentPage == 2 && buttonBPressed && !lastB) {
    // Enter large display mode
    largeDisplayMode = true;
    largeDisplayIndex = 0;
  }
  ```

---

## 7. Business Rules

### BR-NEW-001: Large Display Mode Entry

**Description:**  
Pressing Gamepad Button B while on INA219 page (Page 2) enters large display mode. This is a mode toggle; subsequent B presses cycle values, not re-entry.

**Trigger:**  
Button B edge (transition from released to pressed) detected while `currentPage == 2` and `largeDisplayMode == false`.

**Conditions:**
- Currently viewing Page 2 (INA219 Power Monitor)
- INA219 sensor is available
- Not already in large display mode

**Behavior:**
- Set `largeDisplayMode = true`
- Set `largeDisplayIndex = 0` (start with Laufzeit)
- Next render cycle calls `pageLargeDisplayIna219()` instead of normal `pageIna219()`
- Button B behavior changes to "cycle forward"

**Failure Behavior:**
- If INA219 not available (`available.ina219 == false`), button press has no effect (guard condition)
- Large display mode remains inactive

---

### BR-NEW-002: Large Display Value Cycling (Forward)

**Description:**  
While in large display mode on INA219, pressing Gamepad Button B cycles to the next value in the sequence (with wrap-around).

**Trigger:**  
Button B edge (transition from released to pressed) detected while `currentPage == 2` and `largeDisplayMode == true`.

**Conditions:**
- Currently in large display mode
- Button B newly pressed (edge detection)

**Behavior:**
- Increment `largeDisplayIndex`: `largeDisplayIndex = (largeDisplayIndex + 1) % 7`
- On next render cycle, display new value

**Wrap-around:**  
Value 6 (Power) → Value 0 (Laufzeit)

---

### BR-NEW-003: Large Display Value Cycling (Reverse)

**Description:**  
While in large display mode on INA219, pressing Gamepad Button X cycles to the previous value in the sequence (with wrap-around).

**Trigger:**  
Button X edge (transition from released to pressed) detected while `currentPage == 2` and `largeDisplayMode == true`.

**Conditions:**
- Currently in large display mode
- Button X newly pressed (edge detection)

**Behavior:**
- Decrement `largeDisplayIndex`: `largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7`
- On next render cycle, display new value

**Wrap-around:**  
Value 0 (Laufzeit) → Value 6 (Power)

---

### BR-NEW-004: Large Display Mode Exit

**Description:**  
Pressing Gamepad Button Y while in large display mode on INA219 exits the mode and returns to standard page view. The transition is immediate and atomic, clearing the display before rendering new content.

**Trigger:**  
Button Y edge (transition from released to pressed) detected while `currentPage == 2` and `largeDisplayMode == true`.

**Conditions:**
- Currently in large display mode
- Button Y newly pressed (edge detection)

**Behavior:**
- Set `largeDisplayMode = false`
- Set `largeDisplayIndex = 0` (reset for next entry)
- **Immediately render standard INA219 page** via `drawHeader()` (clears screen atomically with `fillScreen(TFT_BLACK)`) followed by `pageIna219()`
- Button Y restored to "previous page" navigation (with button A for "next page")
- No visual artifacts; clean transition

**Rationale for Immediate Rendering:**  
The immediate rendering ensures an atomic screen clear (via `fillScreen()` in `drawHeader()`) before new content is drawn. This prevents leftover text or visual glitches from the large display being visible during the transition.

**Idempotency:**  
Pressing Y multiple times while already in standard view has no effect.

---

### BR-NEW-005: Large Display Real-Time Updates

**Description:**  
While in large display mode, the displayed value is always current (not cached). As sensor readings update, the large display automatically reflects new values on the next render cycle.

**Trigger:**  
Sensor read cycle completes; `ina219Values` struct updated with latest data.

**Behavior:**
- Large display re-renders value using current `ina219Values` data
- No user action required
- Refresh rate inherited from page rendering cycle (same as standard view)

**No Caching:**  
Large display does not buffer or timestamp values; it reads directly from the global struct.

---

### BR-NEW-006: Large Display Value Formatting Consistency

**Description:**  
Values displayed in large format use identical formatting and precision as the standard page display. This ensures consistency and user predictability.

**Mapping:**
| Value | Format | Decimals |
|-------|--------|----------|
| Laufzeit | HH:MM:SS | N/A |
| Batterie | V | 3 |
| Versorgung | V | 3 |
| Strom | mA [status] | 1 |
| Ladung | mAh | 3 |
| Shunt-Spannung | mV | 2 |
| Power | mW | 0 |

---

## 8. Validation Rules

### V-001: Large Display Index Range

**Rule:** `largeDisplayIndex` must always be in range [0, 6].

**Validation:**
- After increment: `largeDisplayIndex = (largeDisplayIndex + 1) % 7`
- After decrement: `largeDisplayIndex = (largeDisplayIndex - 1 + 7) % 7`
- On mode entry: `largeDisplayIndex = 0`

**Enforcement:** Modulo arithmetic ensures boundary; no explicit check needed.

---

### V-002: Large Display Mode Only on INA219

**Rule:** Large display mode is only active on Page 2 (INA219). On any other page, `largeDisplayMode` should be false.

**Validation:**
- On page change via button Y/A, set `largeDisplayMode = false`
- On page render, check: `if (currentPage != 2) largeDisplayMode = false`

---

### V-003: Button State Consistency

**Rule:** Button press detection uses edge detection (transition from released to pressed). Button state must not cause duplicate action on the same physical press.

**Validation:**
- Compare `buttonsBefore` vs current button state
- Record `lastB`, `lastX`, `lastY` boolean flags
- Action only triggered when `pressed && !last`

---

## 9. Authorization & Security

**None applicable.** This is an embedded firmware feature with no user authentication, permissions, or security context. All users have equal access to all pages and modes.

---

## 10. Error Handling

### 10.1 Sensor Unavailable

**Scenario:** User enters large display mode on INA219, then INA219 sensor becomes unavailable (unplugged, failed I2C read).

**Behavior:**
- `available.ina219` remains false (no re-detection at runtime)
- Large display mode remains active but displays stale cached values
- No error message displayed (current system design doesn't show runtime errors)

**Mitigation:** User should exit large display mode (press Y) and restart to detect sensor status change.

---

### 10.2 Font Rendering Failure

**Scenario:** Large font size (e.g., Font 8) is not available or causes render corruption.

**Behavior:**
- TFT_eSPI falls back to smaller font (implementation-dependent)
- Large display is readable but not optimally large

**Mitigation:** Test font sizes at build/flash time; document which fonts are safe.

---

## 11. Tests

### 11.1 Unit Tests

**Test Suite:** `test_large_display_ina219.cpp` (new file)

**Test Cases:**

```cpp
// Test: Enter large display mode
TEST_CASE("Large Display: Enter mode on INA219 button B press") {
  currentPage = 2;
  largeDisplayMode = false;
  largeDisplayIndex = 0;
  
  simulateButtonPress(GAMEPAD_BUTTON_B);
  
  ASSERT_TRUE(largeDisplayMode);
  ASSERT_EQ(largeDisplayIndex, 0);
}

// Test: Forward cycling
TEST_CASE("Large Display: Forward cycle through all 7 values") {
  currentPage = 2;
  largeDisplayMode = true;
  largeDisplayIndex = 0;
  
  for (int i = 0; i < 7; i++) {
    ASSERT_EQ(largeDisplayIndex, i % 7);
    simulateButtonPress(GAMEPAD_BUTTON_B);
    ASSERT_EQ(largeDisplayIndex, (i + 1) % 7);
  }
}

// Test: Reverse cycling
TEST_CASE("Large Display: Reverse cycle from 0 to 6") {
  currentPage = 2;
  largeDisplayMode = true;
  largeDisplayIndex = 0;
  
  simulateButtonPress(GAMEPAD_BUTTON_X);
  ASSERT_EQ(largeDisplayIndex, 6);
  
  simulateButtonPress(GAMEPAD_BUTTON_X);
  ASSERT_EQ(largeDisplayIndex, 5);
}

// Test: Exit mode
TEST_CASE("Large Display: Exit mode on button Y press") {
  currentPage = 2;
  largeDisplayMode = true;
  largeDisplayIndex = 3;
  
  simulateButtonPress(GAMEPAD_BUTTON_Y);
  
  ASSERT_FALSE(largeDisplayMode);
  ASSERT_EQ(largeDisplayIndex, 0);  // Reset on exit
}

// Test: Not active on other pages
TEST_CASE("Large Display: Button B has no effect on page 0") {
  currentPage = 0;
  largeDisplayMode = false;
  
  simulateButtonPress(GAMEPAD_BUTTON_B);
  
  ASSERT_FALSE(largeDisplayMode);  // No mode entry
}

// Test: Value formatting
TEST_CASE("Large Display: Value formatting matches standard display") {
  String largeValue = getLargeDisplayValue(0);  // Laufzeit
  // Compare with formatRuntime() output
  ASSERT_EQUAL(largeValue, formatRuntime(micros() - ina219Values.startTimeUs));
}

// Test: Real-time update
TEST_CASE("Large Display: Value updates when sensor data changes") {
  currentPage = 2;
  largeDisplayMode = true;
  largeDisplayIndex = 1;  // Batterie
  
  float initialValue = ina219Values.load;
  ina219Values.load = 99.999f;  // Simulate sensor update
  
  String displayedValue = getLargeDisplayValue(1);
  ASSERT_CONTAINS(displayedValue, "99.999");
}

// Test: Mode only on Page 2
TEST_CASE("Large Display: Changing page exits large display mode") {
  currentPage = 2;
  largeDisplayMode = true;
  
  simulatePageNavigation(NEXT_PAGE);
  currentPage = 3;
  
  ASSERT_FALSE(largeDisplayMode);
}
```

### 11.2 Integration Tests

**Test Suite:** `test_ina219_navigation.cpp` (existing; add cases)

```cpp
// Test: Full workflow
TEST_CASE("Large Display: Full workflow (enter, cycle, exit)") {
  // Start at INA219 page
  currentPage = 2;
  ASSERT_FALSE(largeDisplayMode);
  
  // Enter large display
  simulateButtonPress(GAMEPAD_BUTTON_B);
  ASSERT_TRUE(largeDisplayMode);
  ASSERT_EQ(largeDisplayIndex, 0);
  
  // Cycle forward 3 times
  for (int i = 0; i < 3; i++) {
    simulateButtonPress(GAMEPAD_BUTTON_B);
  }
  ASSERT_EQ(largeDisplayIndex, 3);  // Strom
  
  // Cycle back 1 time
  simulateButtonPress(GAMEPAD_BUTTON_X);
  ASSERT_EQ(largeDisplayIndex, 2);  // Versorgung
  
  // Exit
  simulateButtonPress(GAMEPAD_BUTTON_Y);
  ASSERT_FALSE(largeDisplayMode);
}

// Test: Button masking
TEST_CASE("Large Display: Page navigation buttons work when exiting") {
  currentPage = 2;
  largeDisplayMode = true;
  
  // While in large display, Y is "exit", not "previous page"
  simulateButtonPress(GAMEPAD_BUTTON_Y);
  ASSERT_FALSE(largeDisplayMode);
  ASSERT_EQ(currentPage, 2);  // Still on page 2
  
  // Now Y should navigate (next frame)
  simulateButtonPress(GAMEPAD_BUTTON_Y);
  ASSERT_EQ(currentPage, 1);  // Previous page
}
```

### 11.3 UI/Manual Tests

**Test Cases:**

- [ ] **T-001:** Display each of 7 values in large format; verify legibility and color correctness
- [ ] **T-002:** Cycle through all values with B; verify wrap-around from 6 to 0
- [ ] **T-003:** Cycle reverse with X; verify wrap-around from 0 to 6
- [ ] **T-004:** While displaying a value, observe sensor data update live (e.g., current draw changes)
- [ ] **T-005:** Press Y to exit; verify standard page layout restored
- [ ] **T-006:** Try entering large display on another page (e.g., page 0); verify no effect
- [ ] **T-007:** Enter large display, then navigate to another page with Y/A; verify mode cleared
- [ ] **T-008:** Mash button B repeatedly; verify no crashes or undefined state
- [ ] **T-009:** Check that charge (mAh) displays cumulative value without reset
- [ ] **T-010:** Verify footer text or mode indicator visible while in large display mode

---

## 12. Documentation Updates Required

### 12.1 User Interface (docs/07-ui.md)

**Changes:**
- [ ] Add new section: **"Page 2 Alternate Views: Large Display Mode"**
  - Describe entry point (Button B)
  - List all 7 values in display order
  - Document navigation (X, B, Y)
  - Show example screenshot/layout
  
**Example Addition:**
```markdown
#### Page 2 Alternate View: Large Display Mode

**Entry:** Press Gamepad Button B while on Page 2 (INA219)

**Display:** Single value enlarged (font sizes TBD), updated in real-time

**Navigation:**
- Button B: Next value (forward cycle, wraps)
- Button X: Previous value (reverse cycle, wraps)
- Button Y: Exit large display, return to normal view

**Value Sequence:**
1. Laufzeit (HH:MM:SS)
2. Batterie (x.xxx V)
3. Versorgung (x.xxx V)
4. Strom (x.x mA [status])
5. Ladung (x.xxx mAh)
6. Shunt-Spannung (x.xx mV)
7. Power (xxxx mW)

[Back to normal view]
```

---

### 12.2 Business Rules (docs/06-business-rules.md)

**Changes:**
- [ ] Add section: **"Large Display Mode Rules"**
  - BR-NEW-001 through BR-NEW-006
  - Describe state transitions
  - Document button behavior remapping

---

### 12.3 Requirements (docs/01-requirements.md)

**Changes:**
- [ ] Add functional requirement for large display mode (if not present)
  - Entry via Button B on Page 2
  - Real-time updates
  - Cycling navigation

---

### 12.4 Data Model (docs/04-data-model.md)

**Changes:**
- [ ] Add section for large display state
  ```
  Global Large Display State:
  - largeDisplayMode (bool): Is mode active?
  - largeDisplayIndex (uint8_t): Current value index (0-6)
  ```

---

### 12.5 Testing (docs/10-testing.md)

**Changes:**
- [ ] Add test cases for large display mode
- [ ] Update UI/manual test checklist

---

### 12.6 Reconstruction Plan (docs/14-reconstruction-plan.md)

**Changes:**
- [ ] Add implementation phase/group for large display feature
  - Function stubs: `pageLargeDisplayIna219()`, `getLargeDisplayLabel()`, etc.
  - State variables: `largeDisplayMode`, `largeDisplayIndex`
  - Button handling logic in `loop()`

---

## 13. Implementation Strategy (Outline)

*Note: This section is guidance; implementation is out of scope for this specification.*

### Phase 1: State & Helpers
1. Define global state variables (`largeDisplayMode`, `largeDisplayIndex`)
2. Implement helper functions:
   - `getLargeDisplayLabel(index)`
   - `getLargeDisplayValue(index)`
   - `getLargeDisplayUnit(index)`
   - `getLargeDisplayColor(index)`

### Phase 2: Rendering
1. Implement `pageLargeDisplayIna219(index)` function
2. Add conditional in `pageIna219()`: if in large mode, call large display function
3. Test font sizes and layout on target hardware

### Phase 3: Button Handling
1. Update `loop()` to detect large display mode entry (Button B on Page 2)
2. Add button handling for forward/reverse cycling and exit
3. Ensure button state doesn't leak to page navigation

### Phase 4: Testing & Validation
1. Run unit tests (mock/sim environment)
2. Manual UI testing on actual hardware
3. Verify state cleanup on page changes

---

## 14. Open Questions & Decisions

### ✓ DECIDED

1. **Font Size for Large Display:** ✓ **Font 8** — OK, can adjust later if needed.

2. **Large Display on Other Pages:** ✓ **INA219-only (Page 2)** — Start with INA219; extend to other pages later if desired.

3. **Label + Value vs. Value-Only:** ✓ **Label + Value** — Display both label (e.g., "Laufzeit") and value (e.g., "12:34:56").

### ✓ ALL DECIDED

4. **Mode Indicator Placement:** ✓ **Footer text is sufficient** — Display context-specific footer ("B:next X:prev Y:exit") to indicate mode and available actions. No additional mode indicator required; footer provides necessary visual feedback.

5. **Real-Time Refresh vs. Page Render Cycle:** ✓ **Inherit page's native 500ms cycle** — Large display mode does not force higher refresh rate. Inherits INA219 page's native 500ms sensor read cycle. Rationale: Display updates are visual feedback for sensor values; 500ms is adequate for user perception without CPU overhead.

6. **Exit Behavior on Page Navigation:** ✓ **Page navigation COMPLETELY DISABLED during large display mode** — Button Y (previous page) and Button A (next page) are functionally disabled. Button Y is reinterpreted as "exit large display mode"; normal page navigation only resumes after exiting mode. Rationale: Prevents accidental page changes while cycling values; context-sensitive button behavior. Enforced by conditional: `if (!largeDisplayMode) { /* page navigation */ }`

7. **Button Priority:** ✓ **Button A is completely inactive during large display mode** — Pressing Button A has no effect; button is ignored and reserved for future use. No mode exit, no page change, no side effects.

8. **Gamepad Button Conflicts:** ✓ **Only Page 2 (INA219) activates large display mode** — Button B on any other page has no effect. Page 1 (Gamepad) button presses are independent; physical button states display on Page 1, but do not trigger large display mode. Feature is intentionally scoped to Page 2 to avoid conflicts and confusion.

9. **Charge (mAh) Accumulation:** ✓ **Display cumulative charge value as-is** — Large display shows `ina219Values.charge` (cumulative mAh, resets only on reboot), identical to standard page display. No distinction between "session charge" and "total charge"; uses existing data model without modification.

10. **Performance Impact:** ✓ **No additional profiling required** — Large display rendering uses identical TFT_eSPI calls as standard page rendering. Render time is constrained by display refresh (500ms), not CPU sampling. Font 8 rendering is handled by existing TFT library optimizations. Acceptance criterion: render completes within 500ms page cycle (no blocking). Measured in performance testing phase if needed.

11. **Accessibility:** ✓ **Font 8 with optional compile-time configuration** — Font 8 is the primary font size (determined adequate for general use). For users requiring larger text, font size can be compile-time configurable via `#define` in User_Setup.h or platformio.ini build flags. No run-time font size switching; configuration is static per build.

12. **Future Extensibility:** ✓ **Helper functions designed as generic templates** — All 5 helper functions (`getLargeDisplayLabel()`, `getLargeDisplayValue()`, `getLargeDisplayUnit()`, `getLargeDisplayColor()`, `pageLargeDisplayIna219()`) are implemented with generic structure supporting future extension to other sensor pages (Page 1, 3–9). No INA219-specific hardcoding; parameterized by `valueIndex`. Future pages can reuse same helper pattern with different labels/units/colors.

---

## 14a. Resolution Summary

**All 12 open questions have been resolved and integrated into the official documentation:**

| # | Question | Decision | Evidence |
|---|----------|----------|----------|
| 1 | Font size | Font 8 | docs/07-ui.md, Section 3.2 |
| 2 | Scope (pages) | INA219-only (Page 2), extensible | docs/04-data-model.md, Section 2.9 |
| 3 | Layout | Label + Value + Unit | docs/05-api.md, Section 0 |
| 4 | Mode indicator | Footer text sufficient | docs/07-ui.md, Page 2 Alternate View |
| 5 | Refresh rate | Inherit 500ms page cycle | docs/04-data-model.md, Section 2.9 |
| 6 | Page nav during mode | DISABLED (Y/A reinterpreted) | docs/06-business-rules.md, BR-031 |
| 7 | Button A behavior | Completely inactive | docs/06-business-rules.md, BR-032 |
| 8 | Button conflicts | Page 2 only, intentional | docs/06-business-rules.md, BR-028 |
| 9 | Charge display | Cumulative mAh as-is | docs/04-data-model.md (value index 4) |
| 10 | Performance | No additional profiling needed | docs/14-reconstruction-plan.md, Group 4 |
| 11 | Accessibility | Font 8, compile-time configurable | docs/07-ui.md, Typography section |
| 12 | Extensibility | Generic helper functions | docs/05-api.md, Section 0 |

**Integration Status:** ✅ All decisions documented in official project documentation  
**Documentation Files Updated:** 6 (04-data-model, 05-api, 06-business-rules, 07-ui, 10-testing, 14-reconstruction-plan)  
**Cross-References:** Established between FR-012 → BR-028–033 → Section 2.9 → Helper Functions → Test Cases  

---

## 15. Implementation Readiness

**Status:** ✅ **Fully Specified — Ready for Implementation**  
**Decisions Made:** ✅ **All 12 questions resolved**  
**Blocked On:** None — implementation can proceed immediately  
**Next Phase:** Code implementation phase (see Section 13)

---

## 16. Acceptance Criteria

### Functional Acceptance

- [x] Feature specified (this document)
- [x] Key decisions documented (Font 8, INA219-only, Label+Value)
- [ ] Implementation complete
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Code compiles without errors/warnings
- [ ] Code size increase acceptable (Flash < 400 KB target)
- [ ] RAM usage acceptable (< 50 KB)

### User Acceptance (Manual Testing)

- [ ] Button B enters large display on Page 2
- [ ] Each of 7 values displays legibly in large format
- [ ] Button B cycles forward through all 7 values
- [ ] Button X cycles backward through all 7 values
- [ ] Button Y exits large display and returns to normal view
- [ ] Values update live while in large display mode
- [ ] Pressing Y/A to change pages auto-exits large display mode
- [ ] Mode works when INA219 is available; has no effect if unavailable
- [ ] No crashes, freezes, or undefined behavior under normal use

### Documentation Acceptance

- [x] UI specification updated (docs/07-ui.md) ✅
- [x] Business rules documented (docs/06-business-rules.md) ✅
- [x] Reconstruction plan includes implementation steps ✅
- [x] All open questions resolved ✅ (All 12 questions answered and documented)

---

## Appendix A: Value Display Details

### Laufzeit (Runtime)

**Label:** "Laufzeit"  
**Source:** `formatRuntime(micros() - ina219Values.startTimeUs)`  
**Format:** HH:MM:SS (e.g., "12:34:56")  
**Color:** TFT_CYAN  
**Unit:** (none, time format)  

**Formula:**
```cpp
uint32_t runtimeUs = micros() - ina219Values.startTimeUs;
uint32_t seconds = runtimeUs / 1000000;
uint32_t minutes = seconds / 60;
uint32_t hours = minutes / 60;
String runtimeStr = String(hours) + ":" + 
                    String(minutes % 60) + ":" + 
                    String(seconds % 60);
```

---

### Batterie (Load Voltage)

**Label:** "Batterie"  
**Source:** `ina219Values.load`  
**Format:** x.xxx V (3 decimals)  
**Color:** TFT_GREEN  
**Unit:** V  

**Formula:** `load = bus - shunt / 1000`

---

### Versorgung (Bus Voltage)

**Label:** "Versorgung"  
**Source:** `ina219Values.bus`  
**Format:** x.xxx V (3 decimals)  
**Color:** TFT_GREEN  
**Unit:** V  

**Source:** `getBusVoltage_V()` from INA219 sensor

---

### Strom (Current)

**Label:** "Strom"  
**Source:** `ina219Values.current` + charge/discharge status  
**Format:** x.x mA [status] (1 decimal, status = "Charging" or "Discharging")  
**Color:** TFT_ORANGE  
**Unit:** mA  

**Status Logic:**
```cpp
const char *statusLabel = ina219Values.current > 0 ? "Charging" : "Discharging";
```

---

### Ladung (Charge)

**Label:** "Ladung"  
**Source:** `ina219Values.chargeMah`  
**Format:** x.xxx mAh (3 decimals)  
**Color:** TFT_CYAN  
**Unit:** mAh  

**Accumulation:** Cumulative, resets only on reboot

---

### Shunt-Spannung (Shunt Voltage)

**Label:** "Shunt-Spannung"  
**Source:** `ina219Values.shunt`  
**Format:** x.xx mV (2 decimals)  
**Color:** TFT_GREEN  
**Unit:** mV  

**Source:** `getShuntVoltage_mV()` from INA219 sensor (negated)

---

### Power

**Label:** "Power"  
**Source:** `ina219Values.power`  
**Format:** xxxx mW (0 decimals, integer)  
**Color:** TFT_ORANGE  
**Unit:** mW  

**Source:** `getPower_mW()` from INA219 sensor

---

## Appendix B: Gamepad Button Mapping

### Current Button Mapping (Global)

| Button | Page 0 | Page 1 | **Page 2 (INA219)** | Page 3+ |
|--------|--------|--------|------------|---------|
| Y | Prev Page | Prev Page | **Exit Large Display** (if in mode) / Prev Page (if not) | Prev Page |
| A | Next Page | Next Page | **Next Page** | Next Page |
| B | (unused) | (unused) | **Enter/Cycle Large Display** | (unused) |
| X | (unused) | (unused) | **Cycle Large Display Reverse** (if in mode) | (unused) |

**Note:** This spec only affects Page 2 (INA219). Other pages remain unchanged.

---

## Appendix C: Screen Resolution Reference

**Display:** ILI9341 3.5" TFT  
**Physical Resolution:** 320×480 pixels  
**Orientation (sensor-pages-dashboard):** Landscape (Rotation 3)  
**Effective Display Coordinates:** 480×320 px (width × height)

**Coordinate System (Landscape, Rotation 3):**
- X axis: 0–480 (left to right)
- Y axis: 0–320 (top to bottom)
- Center: (240, 160)

**Large Display Layout (Landscape):**
- Header: y=18 (top margin)
- Separator: y=48
- Label: y=70 (large font, good readability)
- Value: y=160 (center of screen vertically; focal point)
- Unit: y=240
- Footer: y=295 (near bottom, navigation hint)
- All elements: x=240 (horizontal center)

---

**Document End**
