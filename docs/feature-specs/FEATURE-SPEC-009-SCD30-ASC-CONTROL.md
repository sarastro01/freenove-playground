# FEATURE-SPEC-009: SCD30 ASC (Automatic Self Calibration) Control & Display

## Feature Overview

Add capability to **display and toggle the SCD30's ASC (Automatic Self Calibration) status** on the SCD30 display page via the Gamepad **X button**. Users can monitor whether ASC is active and switch it on/off without needing serial commands.

**Key Design Decision:** Use **X button** (not Y button) to avoid conflicts:
- X button is free in normal mode (only used in Large Display mode for backward cycling)
- Y button remains for page navigation
- Clear, intentional separation between view/navigation (Y) and configuration (X)

---

## Affected Existing Requirements

### From FR-002: Sensor Detection
- ✅ SCD30 is already detected at startup (I2C address 0x61)
- **NEW**: Add ASC status query during initialization (after START command)

### From FR-003: Display Architecture
- ✅ Page 4 displays SCD30 parameters (CO2, Temperature, Humidity)
- **NEW**: Display ASC status as additional information on Page 4

### From FR-006: Input Handling
- ✅ Gamepad X button is currently used only in Large Display mode (backward cycle)
- ✅ Gamepad Y button is for page navigation and exiting Large Display
- **NEW**: X button in normal mode (when NOT in Large Display) toggles ASC on SCD30 page

### From FR-008: Large Display Mode
- ✅ Large Display mode cycles through parameters with B/X buttons
- **UNCHANGED**: Large Display mode behavior stays the same
  - X button still cycles backward in Large Display
  - Y button still exits Large Display
  - ASC toggle only available in normal page view (not in Large Display mode)

---

## Domain Entities Affected

### 1. SCD30Values Struct
**Location**: Line 95  
**Current State**:
```cpp
struct Scd30Values { 
  float co2; 
  float temp; 
  float humidity; 
};
```

**Impact**: 
- ❌ **NO CHANGE** - SCD30Values remains sensor measurement cache only
- ASC status is **runtime state**, not a sensor measurement
- New state handled by: `Scd30State` struct (see below)

### 2. NEW: Scd30State Struct
**Required**: Add runtime state tracking for SCD30 configuration

```cpp
struct Scd30State {
  bool ascEnabled;        // Current ASC status (true=ON, false=OFF)
  bool ascStatusValid;    // Whether we successfully queried ASC status
  uint32_t lastAscQuery;  // Timestamp of last ASC status query
  bool pendingAscWrite;   // Whether an ASC toggle is in progress
};
```

**Rationale**:
- Separates sensor **measurements** (Scd30Values) from sensor **configuration** (Scd30State)
- Follows existing architecture pattern (cf. LargeDisplayMode struct)
- Allows tracking in-flight I2C write operations
- Non-blocking state machine pattern: set pending flag, query status in next cycle

### 3. SensorStatus Struct
**Location**: Line 84-91  
**Current State**: Tracks which sensors are available (bool scd30;)

**Impact**:
- ✅ **NO CHANGE** - scd30 bool remains to indicate sensor presence
- ASC status is tracked separately in Scd30State

---

## Business Rules Affected

### BR-034: ASC Status Display Rule
**New**:
- Display "ASC: ON" or "ASC: OFF" on SCD30 page in normal (non-Large-Display) mode
- ASC status must be queried at startup (after SCD30 START command)
- ASC status must remain visible on page without blocking display updates
- If ASC query fails, display "ASC: --" (unknown state)

### BR-035: X Button Toggle on SCD30 Page (Normal Mode Only)
**New**:
- **Only in normal mode** (when `!largeDisplayMode.active || largeDisplayMode.currentPage != currentPage`):
  - X button press toggles ASC state: ON → OFF → ON
  - Send I2C SET ASC command (0x53DA with appropriate data)
  - Update local `scd30State.ascEnabled` immediately (optimistic)
  - Query status in next poll cycle to verify (eventual consistency)
  - Include 100ms debounce to prevent rapid I2C traffic

- **In Large Display mode on SCD30 page**: X button cycles backward (existing behavior)
  - No conflict: ASC toggle only active in normal mode
  - Users cannot toggle ASC while in Large Display mode (intentional)

### BR-036: ASC State Consistency
**New**:
- After sending ASC toggle command, re-query ASC status after ~500ms
- If query shows different state than expected: log warning, update display
- This handles race conditions and sensor errors transparently
- Local state and sensor state eventually consistent within 1 display cycle

### BR-037: Sensor Lifecycle - ASC Initialization
**Modified**:
- On startup: Send START command (existing), then queue ASC status query (~500ms later)
- On reset: Reset Scd30State to {false, false, 0, false}
- ASC status persists in sensor hardware across power cycles (sensor-side behavior)

---

## Data Structures & Changes

### Sensor Reading Schedule Impact
**Location**: Line 1534-1620 (Slot timing)

**Current**: Slot 6 (1200ms) reads SCD30 measurements  
**New**: No change to measurement polling schedule

**Additional**: ASC status query
- Runs on-demand after toggle (doesn't consume regular slot)
- Or low-priority background query at startup (~2 seconds after START)
- Must **NOT block** the 500ms display cycle

### Cache Variables
**New global**:
```cpp
Scd30State scd30State{};  // Initialize: {false, false, 0, false}
```

**Location**: After existing sensor cache declarations (~line 150)

---

## API Changes

### New I2C Commands for SCD30

#### 1. SET ASC Enable/Disable
```
Command: 0x53DA
Data: <High Byte=0x00> <Low Byte=0x01|0x00> <CRC>

Where:
  0x00 0x00 + CRC = Disable ASC
  0x00 0x01 + CRC = Enable ASC
```

#### 2. GET ASC Status
```
Command: 0x5301
Response: 3 bytes = <Data High> <Data Low> <CRC>
  0x00 0x01 = ASC is ON
  0x00 0x00 = ASC is OFF
```

### New Functions Required

#### 1. `bool setAsc(bool enable)`
**Purpose**: Send I2C command to enable/disable ASC  
**Input**: 
- `enable`: true = turn ON, false = turn OFF

**Output**:
- `true` if I2C write succeeded
- `false` if I2C write failed (NAK, transmission error, etc.)

**Implementation**:
- Use existing `scd30Command()` pattern with data bytes
- Calculate CRC8 before sending (use existing `scd30Crc()`)
- Set `scd30State.pendingAscWrite = true` initially
- Clear pending flag after I2C operation completes

**Error Handling**:
- If I2C fails: don't update `scd30State.ascEnabled` immediately
- Log to serial (debug level)
- Next ASC status query will reveal actual state

#### 2. `bool getAscStatus(bool& outEnabled)`
**Purpose**: Query current ASC status from sensor  
**Input**: None

**Output**:
- `true` if query succeeded, `outEnabled` contains status
- `false` if query failed, `outEnabled` unchanged

**Implementation**:
- Send 0x5301 command
- Read 3 bytes response (data high, data low, CRC)
- Validate CRC using `scd30Crc()`
- Extract status: bytes[1] & 0x01 → `outEnabled`
- Update `scd30State.ascStatusValid = true`
- Update `scd30State.lastAscQuery = millis()`

**Error Handling**:
- If I2C read fails: return false, don't modify state
- If CRC invalid: return false, set `scd30State.ascStatusValid = false`
- If both fail repeatedly: display "ASC: --" (unknown)

#### 3. `String scd30_getAscStatus()`
**Purpose**: Return ASC status as display string  
**Returns**:
- "ON" if `scd30State.ascEnabled && scd30State.ascStatusValid`
- "OFF" if `!scd30State.ascEnabled && scd30State.ascStatusValid`
- "--" if `!scd30State.ascStatusValid` (unknown)

**Used by**: Display rendering functions

---

## UI Changes

### SCD30 Page (Page 4) Display Layout

**Current State** (Normal Mode):
```
┌────────────────────────────┐
│      SCD30 CO2 Sensor      │  ← Page title
├────────────────────────────┤
│  CO2 Level:    850 ppm     │
│  Temperature:  23.5°C      │
│  Humidity:     45.2%       │
├────────────────────────────┤
│   Y/A: Change page | ...   │
└────────────────────────────┘
```

**New State** (Normal Mode - WITH ASC STATUS):
```
┌────────────────────────────┐
│      SCD30 CO2 Sensor      │  ← Page title
│                 ASC: ON    │  ← NEW: ASC status (top right)
├────────────────────────────┤
│  CO2 Level:    850 ppm     │
│  Temperature:  23.5°C      │
│  Humidity:     45.2%       │
├────────────────────────────┤
│   X: Toggle ASC | Y/A: Pg  │  ← MODIFIED: X now toggles ASC
└────────────────────────────┘
```

**Placement Rules**:
- ASC status appears in **top-right corner** of SCD30 page header
- Uses **small font** (existing header font size)
- Color: 
  - **TFT_GREEN** if ASC is ON
  - **TFT_ORANGE** if ASC is OFF
  - **TFT_DARKGREY** if status unknown ("--")
- Font: existing tft.setTextSize(1)

**Footer Text Change**:
- **Current**: `"Y/A: Change page | SCD30"`
- **New**: `"X: Toggle ASC | Y/A: Change page | SCD30"`

### Large Display Mode - SCD30

**Current Behavior** (Lines 2410-2430):
- X button cycles backward through parameters
- Y button exits Large Display mode

**New Behavior** - NO CHANGE:
- X button **still cycles backward** (existing behavior)
- Y button **still exits** Large Display mode (existing behavior)
- ASC toggle only available in normal page view (intentional design)

**Rationale**: 
- Large Display mode is for **viewing data**, not control
- X button already has a defined function in Large Display mode
- ASC toggle remains a deliberate action on the normal page

### Button Mapping Table - Page 4

| Button | Normal Mode | Large Display Mode |
|--------|------------|-------------------|
| A | Next Page | No Effect |
| B | Enter Large Display | Exit Large Display (actually, enter mode - toggle) |
| X | **NEW: Toggle ASC** | Cycle Backward |
| Y | Previous Page | Exit Large Display |

---

## Security & Authorization

### SEC-001: No Authorization Required
- **NEW**: ASC toggle is a local sensor configuration, no authentication needed
- Does not affect system security or access control
- No user roles or permissions apply to this feature

---

## Error Cases & Handling

### Case 1: I2C Write Fails During Toggle
```
User presses X → setAsc(true) sent → I2C write fails
→ Local state NOT updated (scd30State.ascEnabled unchanged)
→ Next query cycle reveals actual sensor state
→ Display shows actual state (no false feedback)
→ Debug log shows "ASC write failed"
```

### Case 2: I2C Read Fails During Status Query
```
scd30State.ascStatusValid = false
→ Display shows "ASC: --" (unknown state)
→ Retry on next sensor poll cycle
```

### Case 3: CRC Validation Fails
```
Query response received but CRC invalid
→ Set scd30State.ascStatusValid = false
→ Don't update ascEnabled state
→ Display shows "ASC: --"
→ Log warning with timestamp
```

### Case 4: Sensor Not Detected
```
SensorStatus.scd30 == false
→ Don't attempt ASC queries or writes
→ X button press has no effect on SCD30 page
→ ASC display area not shown
```

### Case 5: Rapid Button Presses
```
User presses X multiple times rapidly
→ 100ms debounce prevents I2C command flood
→ Only first press in debounce window takes effect
→ Subsequent presses ignored until debounce expires
```

---

## Acceptance Criteria

### AC-1: ASC Status Display
- [ ] ASC status badge visible in top-right of SCD30 page header (normal mode)
- [ ] Shows "ON" in green, "OFF" in orange, "--" in grey
- [ ] Status updates within 1 display cycle after query (500ms max)
- [ ] Status still visible in Large Display mode (informational only, not interactive)

### AC-2: X Button Toggle in Normal Mode
- [ ] X button press toggles ASC state (ON → OFF → ON)
- [ ] I2C SET command sent correctly with proper CRC
- [ ] Local state updates optimistically (user sees feedback immediately)
- [ ] Status verified in next sensor poll cycle

### AC-3: X Button in Large Display Mode
- [ ] X button still cycles backward through parameters (unchanged behavior)
- [ ] No conflict between Large Display backward cycling and ASC toggle
- [ ] ASC toggle not available while in Large Display mode (intentional)

### AC-4: Y Button Unchanged
- [ ] Y button still navigates to previous page (normal mode)
- [ ] Y button still exits Large Display mode
- [ ] No regression in existing page navigation

### AC-5: Error Handling
- [ ] Failed I2C writes don't corrupt local state
- [ ] Failed I2C reads display "ASC: --"
- [ ] Rapid button presses don't flood I2C bus (100ms debounce working)
- [ ] Sensor not present: no ASC toggle or display

### AC-6: Initialization
- [ ] ASC status queried after SCD30 START command completes
- [ ] Valid state displayed on first page load of SCD30 page
- [ ] Doesn't block 500ms display cycle

### AC-7: Documentation & Code
- [ ] New functions properly documented with I2C command details
- [ ] Business rules reference X button (not Y)
- [ ] UI documentation updated with button mapping table
- [ ] No regression in existing features

---

## Affected Components Summary

| Component | Type | Impact |
|-----------|------|--------|
| Scd30State | Data | NEW struct added |
| setAsc() | API | NEW function |
| getAscStatus() | API | NEW function |
| scd30_getAscStatus() | API | NEW function |
| Page 4 Display | UI | ASC badge added to header |
| Page 4 Footer | UI | Button help text changed (Y→X) |
| X Button Handler | Input | NEW: Toggle ASC in normal mode |
| Sensor Init | Lifecycle | ASC status query after START |
| Business Rules | BR | BR-034, BR-035, BR-036, BR-037 added |

---

## Implementation Checklist

### Phase 1: Data Structures
- [ ] Add Scd30State struct definition
- [ ] Initialize global scd30State variable
- [ ] Add test for struct initialization

### Phase 2: API Functions
- [ ] Implement setAsc(bool enable) with CRC calculation
- [ ] Implement getAscStatus(bool& outEnabled) with CRC validation
- [ ] Implement scd30_getAscStatus() display helper
- [ ] Add unit tests for each function

### Phase 3: Sensor Initialization
- [ ] Queue ASC status query after START command
- [ ] Verify no I2C timing conflicts with measurement polling

### Phase 4: Button Input Handling
- [ ] Add X button event handler for SCD30 page
- [ ] Implement 100ms debounce logic
- [ ] Test in normal mode and Large Display mode

### Phase 5: Display Updates
- [ ] Add ASC status badge to SCD30 page header
- [ ] Update footer with new button help text
- [ ] Test color scheme (green/orange/grey)

### Phase 6: Integration Testing
- [ ] Test full toggle cycle: press X → state changes → display updates
- [ ] Test Large Display mode: X button still cycles backward
- [ ] Test error scenarios (I2C failure, CRC error, sensor absent)
- [ ] Test rapid button presses (debounce working)

### Phase 7: Documentation
- [ ] Update 01-requirements.md with FR-010
- [ ] Update 04-data-model.md with Scd30State struct
- [ ] Update 05-api.md with new I2C commands and functions
- [ ] Update 06-business-rules.md with BR-034 through BR-037
- [ ] Update 07-ui.md with Page 4 button mapping table
- [ ] Update 10-testing.md with acceptance test cases

---

## Testing Strategy

### Unit Tests (UT)
- **UT-001**: scd30_getAscStatus() returns correct strings for all states
- **UT-002**: CRC calculation for SET ASC command
- **UT-003**: CRC validation for GET ASC response
- **UT-004**: Debounce timer correctly prevents rapid toggling

### Integration Tests (IT)
- **IT-001**: X button press triggers ASC query and display update
- **IT-002**: ASC toggle command sent with correct I2C format
- **IT-003**: Large Display X button cycling unaffected
- **IT-004**: Y button still exits Large Display, doesn't toggle ASC

### Acceptance Tests (AT)
- **AT-001**: User can toggle ASC on SCD30 page with X button
- **AT-002**: ASC status displayed correctly (ON/OFF/--)
- **AT-003**: Toggle works across multiple cycles without errors
- **AT-004**: No regression in existing page navigation (Y, A buttons)

---

## Open Questions

1. **ASC Display in Large Display Mode**: Should ASC status be shown as 4th selectable parameter in Large Display mode SCD30 page, or only informational (non-selectable) as currently designed?

2. **Initialization Timing**: After SCD30 START command, what's the minimum safe delay before querying ASC status? Is 500ms sufficient, or should we wait longer?

3. **Persistent State Across Power Cycles**: The SCD30 sensor retains ASC setting in hardware across resets. Should the firmware log or track this state for debugging purposes?

4. **Rapid Toggle Behavior**: If user presses X multiple times (bypassing debounce somehow), should we queue multiple toggle commands or only send the latest state?

5. **Display Priority During Init**: If SCD30 page is displayed while ASC status query is in-flight (during startup), should we show "ASC: --" or empty space until valid state received?

6. **Duplicate Status Queries**: Should we implement a smart cache where we don't query if last query < 100ms ago, to prevent excessive I2C traffic if multiple code paths trigger queries?

7. **Error Logging Level**: When ASC toggle or query fails, should this be logged as DEBUG, INFO, or WARNING level?

8. **Button Conflict Resolution**: If somehow Large Display mode is active on SCD30 page and user presses X, is cycling backward the intended behavior (not toggling ASC)?

9. **Testing Environment**: Will we test with an actual SCD30 sensor connected, or mock the I2C responses in unit tests?

---

## Design Rationale

### Why X Button Instead of Y?
1. **Clear Separation of Concerns**: Y = Navigation, X = Configuration
2. **Conflict-Free**: Y already used for page navigation and Large Display exit
3. **X is Free in Normal Mode**: Only used in Large Display mode (backward cycling)
4. **Logical Mapping**: X on Xbox controller typically represents a secondary action or menu

### Why Not Include ASC in Large Display Mode?
1. **Consistency**: Large Display mode for sensor **measurements**, not configuration
2. **User Intent**: Entering Large Display mode implies the user wants to view/zoom parameters
3. **Button Efficiency**: X already allocated in Large Display mode
4. **Design Pattern**: Configuration changes (like ASC toggle) belong on normal page view

### Why Eventual Consistency Instead of Blocking Verification?
1. **Non-Blocking**: Display refresh cycle must stay at 500ms
2. **Responsiveness**: User sees immediate feedback (optimistic update)
3. **Error Recovery**: If sensor state differs, next query corrects display
4. **Typical Pattern**: Similar to network applications (optimistic UI updates)

### Why Separate Scd30State and Scd30Values?
1. **Separation of Concerns**: Measurements ≠ Configuration
2. **Reusability**: ASC state can be queried without reading measurements
3. **Consistency**: LargeDisplayMode struct follows same pattern
4. **State Machine**: Pending writes tracked separately from current state

