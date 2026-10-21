# FEATURE-SPEC-011: Rotary Encoder Navigation

**Status:** Ready for Implementation  
**Created:** 2026-10-22  
**Version:** 1.1 (Decisions Confirmed)  
**Feature ID:** FEATURE-SPEC-011  
**Related Specs:** FEATURE-SPEC-007 (Large Display Mode), FEATURE-SPEC-010 (Sensor Libraries)

---

## Executive Summary

This feature enables users to navigate the sensor dashboard display and control Large Display Mode using the rotary encoder (already installed and integrated via Adafruit seesaw library). The encoder rotation will replace/supplement gamepad buttons Y and A for page navigation, and encoder button press will replace/supplement gamepad button B for mode toggling. Within Large Display Mode, encoder rotation will scroll parameter values (replacing gamepad buttons B and X).

---

## 1. Functional Behavior

### 1.1 Standard Display Mode (Normal Page Navigation)

**Current Behavior (Gamepad):**
- Gamepad Button Y: Navigate to previous page (circular: page 9 → 0)
- Gamepad Button A: Navigate to next page (circular: page 0 → 9)
- Gamepad buttons Y/A have debounce tracking via `lastY`/`lastA` flags

**Proposed Encoder Behavior (Standard Mode):**
- **Encoder Rotation Delta:** Each rotation step (delta >= threshold) = ±1 page
  - Rotation clockwise → next page (A equivalent): `page++`
  - Rotation counter-clockwise → previous page (Y equivalent): `page--`
  - Circular wrapping: page 9 → 0, page 0 → 9
  - **Delta threshold:** Minimum 1 position unit per poll cycle = ±1 page step
  
- **Encoder Button Press (Down):** Enter Large Display Mode (B button equivalent)
  - If not in Large Display Mode: transition to Large Display Mode for current page
  - Display updates to show first parameter value for current page
  - Encoder LED feedback changes to indicate mode transition

- **Gamepad Button Status:** Gamepad buttons Y/A remain active (full redundancy model)

### 1.2 Large Display Mode (Parameter Value Scrolling)

**Current Behavior (Gamepad):**
- Gamepad Button B: Cycle forward through parameter values (wraps to first value)
- Gamepad Button X: Cycle backward through parameter values (wraps to last value)
- Gamepad Button Y: Exit Large Display Mode, return to standard mode

**Proposed Encoder Behavior (Large Display Mode):**
- **Encoder Rotation Delta:** Each rotation step (delta >= threshold) scrolls parameter values
  - Rotation clockwise → next parameter value (B equivalent): `valueIndex++`
  - Rotation counter-clockwise → previous parameter value (X equivalent): `valueIndex--`
  - Circular wrapping: last value → first value, first value → last value
  - **Delta threshold:** Minimum 1 position unit per poll cycle = ±1 value step
  
- **Encoder Button Press (Down):** Exit Large Display Mode (Y button equivalent)
  - Transition back to standard display mode
  - Return to page view with sensor summary (not Large Display parameter view)
  - Encoder LED feedback reverts to standard mode indication

- **Gamepad Button Status:** Gamepad buttons B/X remain active (full redundancy model)

### 1.3 Encoder Button Debounce Strategy

**Requirement FR-001 Constraint:** Input response <100 ms

**Debounce Approach:**
- Track encoder button state with `lastEncoderPressed` flag (similar to gamepad `lastY`, `lastA`, `lastB`, `lastX`)
- Detect transition from NOT pressed → pressed (rising edge)
- **Single press = one mode transition** (no repeat on hold)
- Held button (multiple cycles) = ignored after initial press transition
- Release and re-press required for next transition

**Implementation Location:**
- Function: `handleButtons()` in src/main.cpp (lines 2556-2563)
- Data: Add `bool lastEncoderPressed = false` to global state
- Logic: Only trigger transition on state change (0 → 1 in pressed detection)

---

## 2. Validation Rules

### 2.1 Encoder Position Bounds

| Rule | Description | Enforcement |
|------|-------------|------------|
| VR-ENC-001 | Encoder position must map to valid page index [0, 9] | Modulo arithmetic: `page = encoderValues.position % 10` |
| VR-ENC-002 | Encoder position wraps circular | Pages cycle: 9→0→1...8→9→0 |
| VR-ENC-003 | Value index bounds within Large Display Mode | Modulo by page's parameter count: `valueIdx = encoderValues.position % paramCount[currentPage]` |

### 2.2 Button Press Debounce

| Rule | Description | Enforcement |
|------|-------------|------------|
| VR-ENC-BTN-001 | Encoder button press detected only on state change | Rising edge detection via `lastEncoderPressed` |
| VR-ENC-BTN-002 | No mode transition occurs if already in target mode | Guard condition before state change |
| VR-ENC-BTN-003 | Press event cleared after processing | Reset debounce flag after transition |

### 2.3 Mode Transition Validity

| Rule | Description | Enforcement |
|------|-------------|------------|
| VR-MODE-001 | Can only enter Large Display Mode from standard mode | Condition: `!largeDisplayMode.active` before transition |
| VR-MODE-002 | Can only exit Large Display Mode when already active | Condition: `largeDisplayMode.active` before transition |
| VR-MODE-003 | Mode transition completes within single update cycle | Non-blocking state change |

### 2.4 LED Feedback Validation

| Rule | Description | Enforcement |
|------|-------------|------------|
| VR-LED-001 | Encoder LED updates reflect current page in standard mode | LED color tied to page index (defined in §5.3) |
| VR-LED-002 | Encoder LED updates reflect current value index in Large Display Mode | LED brightness tied to value position |
| VR-LED-003 | LED state change synchronized with display update | updateEncoderLed() called after each encoder input |

---

## 3. Authorization & Security

**Analysis:** No authorization changes required.

- Encoder navigation is local input only (no network component)
- No sensitive data exposed through encoder
- Same authorization model as gamepad buttons (no auth layer for dashboard input)
- All users with physical access to device can use encoder (same as gamepad)

**Security Constraints:**
- Encoder input must not bypass any sensor data validation (inherited from FEATURE-SPEC-010)
- Large Display Mode entry via encoder must respect same display permissions as gamepad (none currently defined, open for future)

---

## 4. Error Cases

### 4.1 Hardware Errors

| Error Case | Detection | Handling | User Feedback |
|-----------|-----------|----------|----------------|
| EC-001: Encoder I2C communication failure | pollEncoder() returns error status | Log error; ignore input this cycle; retry next cycle | LED dimmed or off (existing behavior) |
| EC-002: Encoder button stuck pressed | Multiple consecutive presses in rapid succession | Debounce filter may mask stuck button | No transition occurs (debounce blocks rapid toggles) |
| EC-003: Encoder position corrupted | Position value > 1000 ticks | Modulo arithmetic normalizes to [0, 9] | Silent correction (wrapping handles bounds) |

### 4.2 State Errors

| Error Case | Detection | Handling | User Feedback |
|-----------|-----------|----------|----------------|
| EC-004: Large Display Mode inconsistent state | valueIndex >= parameter count | Modulo arithmetic bounds valueIndex | No display corruption (bounds enforced) |
| EC-005: Page index out of range | currentPage > 9 after navigation | Modulo arithmetic wraps to [0, 9] | Circular navigation masks error |

### 4.3 Input Conflict Errors

| Error Case | Detection | Handling | User Feedback |
|-----------|-----------|----------|----------------|
| EC-006: Simultaneous gamepad + encoder input | Both inputs active same cycle | Both processed independently (no conflict) | Redundancy allows either input to navigate |
| EC-007: Encoder button press during display update | Press detected mid-frame | Queued for next cycle (non-blocking) | No display flicker (async handling) |

---

## 5. Data Changes

### 5.1 New Global State Variables

**File:** `src/main.cpp`

| Variable | Type | Purpose | Initial Value | Scope | Notes |
|----------|------|---------|---|---|---|
| `encoderLastPressed` | `bool` | Debounce flag for encoder button | `false` | global | Tracks previous press state for rising-edge detection in `handleButtons()` (line 189); enables single press = single transition behavior |
| `encoderLastPosition` | `int32_t` | Previous position for delta calculation | `0` | global | Updated in `readEncoderAt50Hz()` (line 1300); used to compute encoder rotation delta every polling cycle |
| `lastEncoderNavigationPosition` | `int32_t` | Position tracking for navigation delta | `0` | static local in `handleButtons()` (line 2681) | Prevents double-processing of encoder rotation steps; separate from polling-layer tracking; reset only when page/value navigation occurs |

**Naming Convention Notes:**
- **`encoderLastXxx`** = global state managed by encoder input handler
  - Updated by polling functions (`readEncoderAt50Hz()`)
  - Used by navigation handler (`handleButtons()`)
  - Examples: `encoderLastPressed` (debounce), `encoderLastPosition` (delta source)
  
- **`lastEncoderXxx`** = legacy naming convention for consistency with gamepad pattern
  - Matches gamepad button naming: `lastY`, `lastA`, `lastB`, `lastX` (all boolean debounce flags)
  - Encoder naming follows same pattern: `encoderLastPressed` aligns with `lastY`, `lastA`, etc.
  - Improves code readability by pattern recognition
  
- **Static local `lastEncoderNavigationPosition`** (not global)
  - Scope: Only visible within `handleButtons()` function
  - Lifetime: Persists across function calls (static), reset only when navigation occurs
  - Purpose: Cycle-by-cycle tracking to prevent reprocessing of same encoder position
  - Pattern: Similar to gamepad's use of local static state for edge detection

### 5.2 Modified Structures

**EncoderValues Struct (no changes needed):**
- Already tracks: `position`, `delta`, `pressed`, `presses`, `color`, `brightness`
- Existing fields sufficient for feature

**largeDisplayMode Struct (no changes needed):**
- Already tracks: `active`, `currentPage`, `valueIndex`, `needsFullRedraw`, `lastDisplayedValue`
- Existing fields sufficient for value scrolling

### 5.3 No Database/Persistent Storage Changes

- Encoder navigation is transient (session state only)
- No calibration, history, or preferences to persist
- Feature requires no config files or settings storage

---

## 6. API Changes

**Analysis:** No public API changes.

- Feature is purely UI input handling (internal to main.cpp)
- Sensor data APIs unchanged (FEATURE-SPEC-010 defines sensor APIs)
- Display rendering APIs unchanged (FEATURE-SPEC-007 defines Large Display Mode APIs)

**Internal Function Changes:**

| Function | Change | Reason | Location | Execution Context |
|----------|--------|--------|----------|---|
| **Encoder Navigation Handler** (in `handleButtons()`) | New block for encoder rotation and button debounce | Processes encoder position delta (page navigation in standard mode, value scrolling in Large Display mode) and encoder button press (mode toggling) with rising-edge debounce; ensures navigation state is current before display rendering | **PRIORITY 1** (lines 2672-2756) in main loop | Executes FIRST in loop before display rendering; guarantees encoder input is processed and state updated before any display refresh; critical for state consistency |
| `updateEncoderLed()` | Enhanced to support both standard mode page colors and Large Display mode brightness scaling | Standard mode: LED color reflects current page (10-color map, full brightness). Large Display mode: LED brightness reflects current value index (50-100% range, page color preserved). | Lines 1256-1291; called every INPUT_REFRESH_MS (10 ms) at lines 2437 and 2756 | Runs at 100 Hz (every 10 ms); synchronized with encoder polling; provides immediate visual feedback within same loop cycle |
| `readEncoderAt50Hz()` | Existing function; unchanged | Polls encoder position/delta/button state; feeds data to navigation handler | Lines 1299-1339; called every INPUT_REFRESH_MS (10 ms) | Runs at 100 Hz; queries seesaw co-processor for current encoder state; state stored in `encoderValues` struct for navigation handler to process |

---

## 7. UI Component Changes

### 7.1 Display Rendering

**Standard Mode:**
- Display layout unchanged (FEATURE-SPEC-007)
- Current page indicator unchanged (top-right corner shows page number)
- Encoder rotation causes page change → automatic re-render via displayCurrentPage()

**Large Display Mode:**
- Parameter display layout unchanged
- Encoder rotation causes value index change → refresh via displayParameterValue()
- Exit to standard mode uses existing Y-button exit logic (already implemented)

### 7.2 Encoder LED Feedback

**Standard Mode (Normal Page Navigation):**
```
Page 0: Red
Page 1: Orange
Page 2: Yellow
Page 3: Green
Page 4: Cyan
Page 5: Blue
Page 6: Purple
Page 7: White
Page 8: Pink
Page 9: Light Blue
```
- LED color represents current page
- Brightness: fixed at standard level (not tied to position)

**Large Display Mode (Parameter Scrolling):**
```
Value 0:   Brightness 50%
Value 1:   Brightness 60%
Value 2:   Brightness 70%
...
Value N:   Brightness 100%
```
- LED color remains same (page color)
- Brightness increases with parameter index (provides scrolling feedback)

### 7.3 Display Update Timing

- Encoder input processed in main loop (pollEncoder() called every cycle)
- Display update triggered by page change or mode transition (non-blocking)
- Respects FR-001 constraint: input → display <100 ms

---

## 8. Security & Authorization Rules

**No new security rules required.**

- Encoder input is local hardware only (no remote access)
- Same threat model as gamepad buttons (physical access required)
- Existing sensor data validation applies (FEATURE-SPEC-010)
- No privilege escalation vector (user/admin distinction not applicable)

**Constraint from existing rules:**
- BR-DISPLAY-FLICKER-001: Mode transitions must not cause flicker
  - Encoder press triggers same transition as gamepad button B/Y
  - Existing flicker prevention logic applies (double-buffering, needsFullRedraw flag)

---

## 9. Tests to Add/Modify

### 9.1 New Unit Tests

| Test ID | Name | Purpose | Acceptance Criteria |
|---------|------|---------|------------------|
| TC-ENC-NAV-001 | Encoder position maps to page index | Validate modulo arithmetic | Position 0→10→20 maps to pages 0→0→0 |
| TC-ENC-NAV-002 | Encoder rotation increments page | Clockwise rotation advances page | Rotation +1 → page++; wraps 9→0 |
| TC-ENC-NAV-003 | Encoder rotation decrements page | Counter-clockwise rotation goes back | Rotation -1 → page--; wraps 0→9 |
| TC-ENC-NAV-004 | Encoder button press enters Large Display | Button down triggers mode entry | largeDisplayMode.active = true |
| TC-ENC-NAV-005 | Encoder button press exits Large Display | Button down in Large Display exits mode | largeDisplayMode.active = false |
| TC-ENC-NAV-006 | Encoder button debounce | Multiple presses filtered | Only one transition per unique press |
| TC-ENC-NAV-007 | Large Display value scrolling | Encoder rotation scrolls values | Position maps to valueIndex % paramCount |
| TC-ENC-NAV-008 | LED color reflects page | Page 0→red, Page 5→blue, etc. | LED color matches page color map |
| TC-ENC-NAV-009 | LED brightness in Large Display | Brightness increases with value index | Value 0→50%, Value N→100% |
| TC-ENC-NAV-010 | Gamepad buttons still work | Redundancy model preserved | Gamepad Y/A/B/X still navigate/toggle |

### 9.2 Integration Tests

| Test ID | Name | Purpose | Acceptance Criteria |
|---------|------|---------|------------------|
| TC-ENC-INT-001 | Encoder + gamepad simultaneous input | Both inputs work in same session | No conflicts; redundancy model works |
| TC-ENC-INT-002 | Mode transition timing | Encoder press → mode change <100ms | Respects FR-001 constraint |
| TC-ENC-INT-003 | Display flicker prevention | Mode transition doesn't flicker | BR-DISPLAY-FLICKER-001 satisfied |
| TC-ENC-INT-004 | Sensor data integrity | Navigation doesn't affect sensor reads | Sensor values accurate during page navigation |

### 9.3 Acceptance Tests (User-Facing)

| Test ID | Scenario | Steps | Expected Result |
|---------|----------|-------|-----------------|
| TAC-ENC-001 | Navigate pages with encoder | Rotate encoder dial clockwise 3 times | Page advances 0→1→2→3 |
| TAC-ENC-002 | Navigate pages backward | Rotate encoder dial counter-clockwise 2 times | Page goes back (e.g., 5→4→3) |
| TAC-ENC-003 | Enter Large Display Mode | Rotate to desired page, press encoder button | Large Display shows first parameter value |
| TAC-ENC-004 | Scroll parameter in Large Display | Rotate encoder dial clockwise 2 times | Parameter index advances (e.g., 0→1→2) |
| TAC-ENC-005 | Exit Large Display Mode | Press encoder button while in Large Display | Return to page view (standard mode) |
| TAC-ENC-006 | Circular page wrapping | Navigate from page 9 clockwise | Page wraps to 0 |
| TAC-ENC-007 | Circular value wrapping | Scroll from last value clockwise | Value wraps to 0 |
| TAC-ENC-008 | LED color changes per page | Navigate through pages 0→1→2...→9 | LED color changes matching page (red→orange→yellow...) |

---

## 10. Documentation Updates Required

| Document | Section | Change | Reason |
|----------|---------|--------|--------|
| docs/01-requirements.md | FR-001 | Add encoder input to requirement statement | Encoder navigation now covered by performance requirement |
| docs/02-architecture.md | Input Handling subsection | Add "Encoder Navigation Handler" section | Document new navigation logic integration |
| docs/06-business-rules.md | Navigation section | Add BR-ENC-NAV-001 through BR-ENC-NAV-004 | Define encoder navigation business rules |
| docs/07-ui.md | Navigation section (§1) | Expand button mapping table to include encoder equivalents | Show encoder rotation/press equivalents to gamepad buttons |
| docs/07-ui.md | Section 10 (Large Display Mode) | Add encoder behavior subsection | Document encoder scrolling in Large Display Mode |
| docs/08-api.md | Internal APIs | Add pollEncoder() integration points | Document how encoder position/button feed into navigation |
| docs/12-dependencies.md | Adafruit seesaw library | No change (already documented) | Feature uses existing encoder library |
| docs/14-reconstruction-plan.md | Integration Testing phase | Add encoder navigation test cases | Ensure encoder feature tested before release |

---

## 11. Acceptance Criteria

### 11.1 Functional Acceptance

- [ ] **AC-F-001:** Encoder rotation clockwise navigates to next page (circular wrap) in standard mode
- [ ] **AC-F-002:** Encoder rotation counter-clockwise navigates to previous page (circular wrap) in standard mode
- [ ] **AC-F-003:** Encoder button press enters Large Display Mode when in standard mode
- [ ] **AC-F-004:** Encoder button press exits Large Display Mode when already active
- [ ] **AC-F-005:** Encoder rotation clockwise scrolls to next parameter value in Large Display Mode
- [ ] **AC-F-006:** Encoder rotation counter-clockwise scrolls to previous parameter value in Large Display Mode
- [ ] **AC-F-007:** Parameter value scrolling wraps circular (last value → first value)
- [ ] **AC-F-008:** Gamepad buttons Y/A remain functional for page navigation (redundancy preserved)
- [ ] **AC-F-009:** Gamepad buttons B/X remain functional for parameter scrolling in Large Display (redundancy preserved)

### 11.2 Performance Acceptance

- [ ] **AC-P-001:** Input response time <100 ms (FR-001 compliance)
- [ ] **AC-P-002:** Display update latency <50 ms after encoder input
- [ ] **AC-P-003:** LED feedback updates <50 ms after encoder rotation
- [ ] **AC-P-004:** No display flicker during encoder navigation
- [ ] **AC-P-005:** No dropped encoder inputs during rapid rotation

### 11.3 Integration Acceptance

- [ ] **AC-I-001:** Encoder navigation works alongside gamepad input (no conflicts)
- [ ] **AC-I-002:** Mode transitions respect BR-DISPLAY-FLICKER-001 (no flicker)
- [ ] **AC-I-003:** Encoder button debounce prevents unintended rapid mode toggles
- [ ] **AC-I-004:** Sensor data integrity maintained during navigation (no data loss)
- [ ] **AC-I-005:** LED color/brightness feedback consistent across all 10 sensor pages

### 11.4 Documentation Acceptance

- [ ] **AC-D-001:** docs/07-ui.md updated with complete encoder navigation behavior
- [ ] **AC-D-002:** docs/06-business-rules.md includes all encoder navigation rules
- [ ] **AC-D-003:** docs/02-architecture.md documents encoder navigation integration points
- [ ] **AC-D-004:** All navigation equivalences (encoder vs. gamepad) clearly documented
- [ ] **AC-D-005:** Test cases (TC-ENC-*) and scenarios (TAC-ENC-*) included in test documentation

---

## 12. Design Decisions - CONFIRMED ✅

The following decisions have been confirmed by user review. Implementation will proceed with these parameters.

### 12.1 Encoder Position Mapping Strategy - ✅ DELTA-BASED STEPPING

**Decision:** Each rotation step (delta >= threshold) = ±1 page step

**Rationale:** Matches gamepad button feel (discrete stepping), provides predictable behavior consistent with existing button navigation model.

**Implementation Impact:**
- Track `lastEncoderPosition` to calculate delta each cycle
- `if (encoderValues.position - lastEncoderPosition >= STEP_THRESHOLD) currentPage++`
- `if (encoderValues.position - lastEncoderPosition <= -STEP_THRESHOLD) currentPage--`
- Circular wrapping via modulo: `currentPage = (currentPage + 10) % 10`
- No linear position-to-page mapping; purely event-driven like gamepad

---

### 12.2 Encoder Button Hold/Repeat Behavior - ✅ NO REPEAT

**Decision:** Single press = one mode transition, holding ignored after initial press

**Rationale:** Predictable behavior, prevents unintended rapid mode toggles, matches gamepad Y/B button feel.

**Implementation Impact:**
- Debounce detects rising edge (NOT pressed → pressed)
- Trigger transition on state change only
- No repeat timer or accelerated scrolling logic needed
- Release and re-press required for next transition

---

### 12.3 LED Brightness Scaling in Large Display Mode - ✅ LINEAR 50-100%

**Decision:** Brightness scales linearly from 50% to 100% based on parameter value index

**Rationale:** Smooth gradient feedback, readable at minimum brightness, provides clear visual position indicator.

**Formula:**
```
brightness = 128 + (valueIndex / paramCount[currentPage]) * 127
```
- valueIndex 0 → brightness 128 (50%)
- valueIndex N (last) → brightness 255 (100%)
- Smooth gradient across all values

**Implementation Impact:**
- Calculate brightness in updateEncoderLed() based on `largeDisplayMode.valueIndex`
- Brightness updated each poll cycle in Large Display mode
- Page color (hue) remains constant; only brightness changes

---

### 12.4 Gamepad Button Redundancy Model - ✅ FULL REDUNDANCY

**Decision:** Both encoder and gamepad buttons remain active simultaneously

**Rationale:** Maximum user flexibility, no conflicts between input methods, matches existing navigation design.

**Implementation Impact:**
- No mutual exclusion logic needed
- Gamepad Y/A/B/X continue to work alongside encoder
- Both inputs process independently in handleButtons()
- No 500ms cooldowns or input priority logic

---

### 12.5 Encoder Navigation Scope - ✅ UNIVERSAL

**Decision:** Encoder navigation works in all display modes

**Rationale:** Simplifies implementation, maximizes usability in all contexts.

**Implementation Impact:**
- No conditional guards based on display mode
- Encoder always navigates pages in standard mode
- Encoder always scrolls values in Large Display mode
- Works during startup I2C scan, error displays, etc.

---

### 12.6 Encoder Polling Frequency - ✅ 50 Hz SUFFICIENT

**Decision:** Current 50 Hz polling (once per main loop cycle) is adequate

**Rationale:** Seesaw MCU pre-computes position in background; we read aggregated value. No risk of missed updates even with fast rotation.

**Implementation Impact:**
- No polling frequency increase needed
- pollEncoder() called as-is in main loop
- No performance optimization required

---

## 13. Implementation Notes (Updated)

### 13.1 Code Integration Points

**File: `src/main.cpp`**

1. **Global state additions (~line 60):**
   ```cpp
   bool lastEncoderPressed = false;
   int32_t lastEncoderPosition = 0;
   const int ENCODER_DELTA_THRESHOLD = 1;  // Steps per transition
   ```

2. **pollEncoder() function (~line 1270):**
   - Already tracks `encoderValues.position` and `encoderValues.pressed`
   - No changes needed to polling layer

3. **handleButtons() function (~line 2556-2563) - MAJOR CHANGES:**
   
   **New encoder delta calculation logic:**
   ```cpp
   // Calculate encoder position delta
   int32_t encoderDelta = encoderValues.position - lastEncoderPosition;
   lastEncoderPosition = encoderValues.position;
   
   // Standard mode: page navigation via delta
   if (!largeDisplayMode.active) {
       if (encoderDelta >= ENCODER_DELTA_THRESHOLD) {
           currentPage = (currentPage + 1) % 10;
           needsPageRedraw = true;
       } else if (encoderDelta <= -ENCODER_DELTA_THRESHOLD) {
           currentPage = (currentPage - 1 + 10) % 10;
           needsPageRedraw = true;
       }
   } 
   // Large Display mode: value scrolling via delta
   else {
       uint8_t paramCount = getParameterCount(currentPage);
       if (encoderDelta >= ENCODER_DELTA_THRESHOLD) {
           largeDisplayMode.valueIndex = (largeDisplayMode.valueIndex + 1) % paramCount;
           largeDisplayMode.needsFullRedraw = true;
       } else if (encoderDelta <= -ENCODER_DELTA_THRESHOLD) {
           largeDisplayMode.valueIndex = (largeDisplayMode.valueIndex - 1 + paramCount) % paramCount;
           largeDisplayMode.needsFullRedraw = true;
       }
   }
   ```
   
   **Encoder button press debounce (rising edge detection):**
   ```cpp
   bool encoderPressed = encoderValues.pressed == 0;  // Active LOW
   if (encoderPressed && !lastEncoderPressed) {
       // Rising edge: button just pressed
       if (!largeDisplayMode.active) {
           // Enter Large Display Mode
           largeDisplayMode.active = true;
           largeDisplayMode.currentPage = currentPage;
           largeDisplayMode.valueIndex = 0;
           largeDisplayMode.needsFullRedraw = true;
       } else {
           // Exit Large Display Mode
           largeDisplayMode.active = false;
           currentPage = largeDisplayMode.currentPage;
           needsPageRedraw = true;
       }
   }
   lastEncoderPressed = encoderPressed;
   ```

4. **updateEncoderLed() function (~line 1000-1050) - ENHANCED:**
   
   **Standard mode: LED color reflects current page**
   ```cpp
   if (!largeDisplayMode.active) {
       uint32_t pageColors[] = {
           0xFF0000,  // Page 0: Red
           0xFF7F00,  // Page 1: Orange
           0xFFFF00,  // Page 2: Yellow
           0x00FF00,  // Page 3: Green
           0x00FFFF,  // Page 4: Cyan
           0x0000FF,  // Page 5: Blue
           0x7F00FF,  // Page 6: Purple
           0xFFFFFF,  // Page 7: White
           0xFF1493,  // Page 8: Pink
           0x87CEEB   // Page 9: Light Blue
       };
       setEncoderLedColor(pageColors[currentPage]);
       setEncoderLedBrightness(255);  // Full brightness in standard mode
   }
   ```
   
   **Large Display mode: LED brightness reflects value position**
   ```cpp
   else {
       uint8_t paramCount = getParameterCount(largeDisplayMode.currentPage);
       // Linear brightness scale: 50% to 100%
       uint8_t brightness = 128 + (largeDisplayMode.valueIndex * 127 / max(paramCount, 1));
       setEncoderLedBrightness(brightness);
       // Color remains same as page color
   }
   ```

5. **displayCurrentPage() function:**
   - Already handles mode transitions via `largeDisplayMode.active`
   - Already triggered by state changes in handleButtons()
   - No changes needed

### 13.2 Testing Strategy

1. **Unit tests first:** Validate modulo arithmetic and debounce logic in isolation
2. **Integration tests:** Confirm encoder + gamepad work together without conflicts
3. **Acceptance tests:** User-facing scenarios (TAC-ENC-001 through TAC-ENC-008)
4. **Performance validation:** Measure input → display latency against FR-001
5. **Hardware tests:** Verify LED color/brightness feedback on actual device

### 13.3 Rollout Plan

1. Implement core navigation logic (handleButtons updates)
2. Add unit tests and validate locally
3. Add integration tests (encoder + gamepad redundancy)
4. Implement LED feedback enhancements
5. Update documentation (docs/07-ui.md, docs/06-business-rules.md)
6. Acceptance testing on hardware
7. Commit as FEATURE-SPEC-011-implementation

---

## 14. Related Specifications

- **FEATURE-SPEC-007:** Large Display Mode (defines valueIndex, parameter scrolling)
- **FEATURE-SPEC-010:** Sensor Library Migration (defines sensor data APIs)
- **docs/01-requirements.md:** FR-001 (input response <100 ms)
- **docs/06-business-rules.md:** BR-DISPLAY-FLICKER-001 (no flicker on mode transitions)
- **docs/07-ui.md:** Navigation topology, button mapping, Large Display Mode behavior

---

## 15. Change Summary

| Category | Count | Impact |
|----------|-------|--------|
| New code functions | 1 | handleButtons() enhancement |
| Modified functions | 2 | pollEncoder(), updateEncoderLed() |
| New unit tests | 10 | TC-ENC-NAV-001 through TC-ENC-NAV-010 |
| New integration tests | 4 | TC-ENC-INT-001 through TC-ENC-INT-004 |
| New acceptance tests | 8 | TAC-ENC-001 through TAC-ENC-008 |
| Documentation files updated | 6 | docs/01,02,06,07,08,14 |
| Global variables added | 2 | lastEncoderPressed, lastEncoderPosition |
| No breaking changes | ✓ | Full backward compatibility |

---

**Specification Status:** ✅ COMPLETE - All Design Decisions Confirmed - Ready for Implementation

**Next Step:** Begin implementation in src/main.cpp using confirmed specifications (§12-13).


