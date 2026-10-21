# FEATURE-SPEC-009: Impact Analysis - X-Button Change

## Summary
Changed ASC toggle button from **Y button** to **X button** to avoid conflicts with existing navigation and Large Display mode controls.

---

## 1. Affected Existing Requirements

| Requirement | Original State | Change | Impact |
|------------|----------------|--------|--------|
| **FR-002: Sensor Detection** | SCD30 detected at 0x61 | Add ASC query after START | LOW - Non-breaking addition |
| **FR-003: Display Architecture** | Page 4 shows CO2, Temp, Humidity | Add ASC status display | LOW - Non-breaking addition |
| **FR-006: Input Handling** | Y=nav, X=Large Display backward | X now toggles ASC in normal mode | MEDIUM - X button now dual-function |
| **FR-008: Large Display Mode** | X cycles backward, Y exits | No change to Large Display behavior | NONE - Fully compatible |

**Conclusion**: No existing requirements broken. All changes additive or clarifying existing dual-function buttons.

---

## 2. Domain Entities Affected

| Entity | Change | Scope |
|--------|--------|-------|
| **SCD30Values** | None | SCD30 measurements unchanged |
| **SCD30State** | NEW struct added | Configuration state tracking |
| **SensorStatus** | None | scd30 presence detection unchanged |

**Conclusion**: One new entity (Scd30State). All existing entities unmodified.

---

## 3. Business Rules Affected

| Rule ID | Type | Change |
|---------|------|--------|
| **BR-034** | NEW | ASC Status Display Rule |
| **BR-035** | NEW | X Button Toggle (Normal Mode Only) |
| **BR-036** | NEW | ASC State Consistency |
| **BR-037** | NEW | Sensor Lifecycle - ASC Initialization |

**Rationale**: 
- BR-035 explicitly states: "Only in normal mode when `!largeDisplayMode.active`"
- This prevents conflicts with Large Display backward cycling
- Large Display mode behavior unchanged

**Conclusion**: 4 new business rules added. No existing rules modified.

---

## 4. Database Structures Affected

**None.** 

ASC status is runtime state only (Scd30State struct in RAM). No persistent storage or EEPROM changes.

---

## 5. APIs Affected

| Function | Type | Impact |
|----------|------|--------|
| **setAsc(bool enable)** | NEW | Enable/disable ASC via I2C |
| **getAscStatus(bool& outEnabled)** | NEW | Query ASC status from sensor |
| **scd30_getAscStatus()** | NEW | Display string helper ("ON"/"OFF"/"--") |

**Existing APIs**: No changes to measurement reading functions.

**I2C Commands**:
- 0x53DA: SET ASC (Enable/Disable)
- 0x5301: GET ASC Status

**Conclusion**: 3 new functions. Zero breaking changes to existing API.

---

## 6. UI Components Affected

### SCD30 Page (Page 4)

#### Header Area
- **ADD**: ASC status badge (top-right corner)
  - "ASC: ON" (green) / "ASC: OFF" (orange) / "ASC: --" (grey)
  - Small font, minimal footprint

#### Footer Area
- **CHANGE**: Button help text from `"Y/A: Change page"` to `"X: Toggle ASC | Y/A: Change page"`
- **RATIONALE**: X button now has dedicated function in normal mode

#### Button Mapping
| Button | Before | After |
|--------|--------|-------|
| X (Normal) | Unused | **Toggle ASC** |
| X (Large Display) | Cycle Backward | Cycle Backward (unchanged) |
| Y (Normal) | Previous Page | Previous Page (unchanged) |
| Y (Large Display) | Exit Mode | Exit Mode (unchanged) |
| A | Next Page | Next Page (unchanged) |
| B | Enter Large Display | Enter Large Display (unchanged) |

**Conclusion**: One header badge added. Footer text updated. Button mapping clarified (no logic changes for Y/A/B).

---

## 7. Security & Authorization Rules

**None affected.** 

ASC control is a local sensor configuration with no security implications.

---

## 8. Tests Affected

### Modified Tests

| Test ID | Original | Change | Reason |
|---------|----------|--------|--------|
| **IT-002** | "Y button toggles ASC" | "X button toggles ASC" | Button reassigned |
| **IT-003** | "Y button exits Large Display" | "Y button exits Large Display" | No change; Y still exits |
| **AT-001** | "Toggle via Y button" | "Toggle via X button" | Button reassigned |

### New Tests Required
- **UT-001 to UT-004**: CRC, string formatting, debounce (unchanged)
- **IT-001, IT-004**: X/Y behavior verification (modified descriptions)
- **AT-002 to AT-004**: End-to-end toggle workflows (unchanged scope)

**Conclusion**: Test descriptions updated for X button. Test logic/coverage unchanged.

---

## 9. Documentation Files Affected

Files to be updated (same 6 as before, but with X-button references):

1. **docs/01-requirements.md**
   - Update FR-010 to reference X button
   - No structural changes

2. **docs/04-data-model.md**
   - Scd30State struct section unchanged
   - No structural changes

3. **docs/05-api.md**
   - Function documentation unchanged
   - Usage examples update to reference X button instead of Y
   - No API signature changes

4. **docs/06-business-rules.md**
   - BR-034: ASC Display (unchanged)
   - BR-035: "X Button Toggle" (renamed from Y, updated condition)
   - BR-036: ASC Consistency (unchanged)
   - BR-037: Lifecycle (unchanged)

5. **docs/07-ui.md**
   - Button mapping table for Page 4
   - Footer text: `"X: Toggle ASC | Y/A: Change page"`
   - No large structural changes

6. **docs/10-testing.md**
   - IT-002: Update to reference X button
   - AT-001: Update to reference X button
   - Test strategy descriptions only; no test count changes

---

## Compatibility Analysis

### With Large Display Mode
✅ **No Conflict**:
- X button toggle only active when `!largeDisplayMode.active`
- X button backward cycling only active when `largeDisplayMode.active`
- Clear separation maintained by conditional check

### With Existing Page Navigation
✅ **No Conflict**:
- Y button still navigates (previous page)
- A button still navigates (next page)
- X button is new in normal mode (was unused)

### With Sensor Polling
✅ **No Conflict**:
- ASC query fits into existing 500ms display cycle
- No new blocking operations
- Doesn't interfere with measurement polling (Slot 6 @ 1200ms)

---

## Breaking Changes

**NONE.**

This is a pure addition with optional button remapping for clarity.

---

## Risk Assessment

| Risk | Probability | Mitigation |
|------|-------------|------------|
| User presses X expecting Large Display cycle while in normal mode | MEDIUM | Clear footer help text: "X: Toggle ASC" |
| I2C command conflicts with sensor polling | LOW | Non-blocking state machine + 100ms debounce |
| Large Display X cycling "stolen" by ASC toggle | NONE | Conditional: only active in normal mode |
| Sensor not responding to ASC commands | MEDIUM | Error handling: display "ASC: --", log error |

---

## Summary of Changes

### Code Changes Required
- Add Scd30State struct
- Add 3 new functions (setAsc, getAscStatus, scd30_getAscStatus)
- Add X button event handler in SCD30 page input loop
- Add ASC badge to Page 4 header display
- Add ASC status query after SCD30 START

### Documentation Changes Required
- Update 6 documentation files (same ones as before)
- All changes are edits, not structural overhauls
- Button references: Y → X (where applicable)

### Breaking Changes
- **None**

### New Features
- ASC status display on SCD30 page
- Toggle capability via X button
- Error handling and eventual consistency pattern

---

## Conclusion

The change from Y button to X button:
1. ✅ Avoids all conflicts with existing navigation and Large Display mode
2. ✅ Provides clearer user intent separation (Y=navigation, X=configuration)
3. ✅ Requires NO changes to API signatures, data structures, or business logic
4. ✅ Requires only documentation/description updates and button event handler placement
5. ✅ Maintains full backward compatibility

**Impact Level: LOW** (button remapping, no functional changes)

