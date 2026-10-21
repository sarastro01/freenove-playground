# Documentation Update Plan
## FEATURE-SPEC-009: X-Button Implementation Guide

---

## Overview
Six documentation files need updates to integrate the X-button change for ASC toggle. All changes are **button reference updates only** — no structural changes required.

---

## File 1: docs/01-requirements.md

### Section to Update
Locate: **## FR-010: SCD30 ASC Control & Display**

### Changes Required
```
OLD:  "Press Y button on SCD30 page to toggle ASC status"
NEW:  "Press X button on SCD30 page to toggle ASC status"

OLD:  "User presses Y → ASC toggles between ON/OFF"
NEW:  "User presses X → ASC toggles between ON/OFF"
```

### Context
- FR-010 is new requirement (added in previous spec)
- Update all Y-button references to X-button
- Keep all other FR-010 content unchanged

---

## File 2: docs/04-data-model.md

### Section to Update
Locate: **## Section 2.5a: SCD30 Configuration State**

### Changes Required
```
OLD:  struct Scd30State { ... }  // Full struct stays same
NEW:  struct Scd30State { ... }  // NO CHANGES NEEDED
```

**Status:** ✅ **NO CHANGES** — This file only contains the data structure, which is button-agnostic

---

## File 3: docs/05-api.md

### Section 1 to Update
Locate: **### setAsc(bool enable)**

#### Changes Required
```
OLD:  "Called when user presses Y button on SCD30 page"
NEW:  "Called when user presses X button on SCD30 page"

OLD:  "// User presses Y: Toggle ASC
       setAsc(!scd30State.ascEnabled);"
NEW:  "// User presses X: Toggle ASC
       setAsc(!scd30State.ascEnabled);"
```

### Section 2 to Update
Locate: **### Usage Example**

#### Changes Required
```
OLD:  "Y Button Handler:
       if (xPressed && !lastX && currentPage == 4) {"
NEW:  "X Button Handler:
       if (xPressed && !lastX && currentPage == 4 && !largeDisplayMode.active) {"
```

### Context
- Function signatures stay exactly the same
- Only usage/example descriptions change

---

## File 4: docs/06-business-rules.md

### Section 1 to Update
Locate: **## BR-035: X Button Toggle on SCD30 Page (Normal Mode Only)**

#### Changes Required
```
Rule ID stays: BR-035
Title stays: X Button Toggle on SCD30 Page
Condition stays: "Only in normal mode"

NO CHANGES NEEDED — Already written for X button!
```

**Status:** ✅ **NO CHANGES** — This was already written for X-button in the new spec

### Verify
- BR-034: ASC Status Display Rule ........... ✅ No changes needed
- BR-035: X Button Toggle .................. ✅ Already X-button
- BR-036: ASC State Consistency ............ ✅ No changes needed
- BR-037: Sensor Lifecycle ................. ✅ No changes needed

---

## File 5: docs/07-ui.md

### Section 1 to Update
Locate: **## Page 4: SCD30 CO2 Sensor**

#### Subsection: User Actions / Button Mapping

##### Changes Required
```
OLD:  | Y      | Toggle ASC                    |
      | Y/A    | Change page                   |

NEW:  | X      | Toggle ASC                    |
      | Y      | Previous page                 |
      | A      | Next page                     |
      | Y/A    | Change page                   |
```

### Section 2 to Update
Locate: **### Footer Display**

#### Changes Required
```
OLD:  "Y/A: Change page | SCD30"
NEW:  "X: Toggle ASC | Y/A: Change page | SCD30"
```

### Context
- Add X button to button mapping table
- Update footer help text to mention X button
- Visual layout stays the same

---

## File 6: docs/10-testing.md

### Section 1 to Update
Locate: **## Integration Tests (IT)**

Subsection: **IT-002: X Button Toggle on SCD30 Page**

#### Changes Required
```
OLD:  Test Name: "Y Button Toggles ASC on SCD30 Page"
      Steps: "1. Navigate to SCD30 page (Page 4)
              2. Press Y button
              ..."

NEW:  Test Name: "X Button Toggles ASC on SCD30 Page"
      Steps: "1. Navigate to SCD30 page (Page 4)
              2. Press X button
              ..."
```

### Section 2 to Update
Locate: **## Acceptance Tests (AT)**

Subsection: **AT-001: User Can Toggle ASC**

#### Changes Required
```
OLD:  "Given user is on SCD30 page
       When user presses Y button
       Then ASC state toggles"

NEW:  "Given user is on SCD30 page (normal mode)
       When user presses X button
       Then ASC state toggles"
```

### Context
- Test descriptions only — test logic unchanged
- Test coverage and acceptance criteria unchanged
- Large Display mode tests (IT-003) stay same (Y still exits)

---

## Summary of Changes by File

| File | Changes | Impact |
|------|---------|--------|
| **01-requirements.md** | Update FR-010: Y→X | Button references only |
| **04-data-model.md** | None needed | Data structures unchanged |
| **05-api.md** | Update usage examples: Y→X | Examples only, no API changes |
| **06-business-rules.md** | None needed | Already written for X-button |
| **07-ui.md** | Add X to button table, update footer | UI descriptions only |
| **10-testing.md** | IT-002, AT-001: Y→X | Test descriptions only |

**Total Changes: ~15 lines across 4 files**
**Breaking Changes: ZERO**

---

## Implementation Order

1. ✅ **01-requirements.md** — FR-010 button references
2. ✅ **05-api.md** — Usage examples and function descriptions
3. ✅ **07-ui.md** — Button mapping table and footer
4. ✅ **10-testing.md** — IT-002 and AT-001 descriptions
5. ✅ **04-data-model.md** — Verify (no changes needed)
6. ✅ **06-business-rules.md** — Verify (already correct)

---

## Verification Checklist

After updates, verify:

- [ ] All Y-button references in FR-010 changed to X
- [ ] X button conditional in API examples includes "!largeDisplayMode.active"
- [ ] UI button mapping table shows X for Toggle ASC
- [ ] Footer text includes "X: Toggle ASC"
- [ ] IT-002 test name and steps reference X button
- [ ] AT-001 test steps reference X button
- [ ] Large Display mode tests (IT-003, etc.) still reference Y for exit
- [ ] No breaking changes introduced
- [ ] All cross-references still valid
- [ ] Consistency check: All 4 new business rules (BR-034–037) still referenced

---

## Quick Find & Replace Guide

If using text editor Find & Replace (for reference only — review each change):

**DO NOT use blind find/replace** — Verify each change!

Pattern 1: "Y button" → "X button" (in SCD30 context only)
Pattern 2: "Press Y" → "Press X" (in SCD30 ASC context only)
Pattern 3: "yPressed && !lastY" in SCD30 input handler → Check context carefully

**Recommended: Manual review of each change for safety**

