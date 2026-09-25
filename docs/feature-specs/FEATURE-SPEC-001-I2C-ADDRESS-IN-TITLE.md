# Feature Specification: I2C Address in Page Titles

**Feature ID:** FEAT-001  
**Status:** Analysis Phase (No Implementation Yet)  
**Created:** 2026-09-21  
**Requested By:** User  
**Priority:** Medium  
**Complexity:** Low–Medium  

---

## 1. Executive Summary

**Current State:**
- Page titles display only the sensor name: "BME280", "INA219", "DS18B20", etc.
- I2C addresses are displayed in a separate row below the title with the label "I2C address:" or equivalent
- The title is purely textual, hardcoded in a const char* array

**Requested Change:**
Include the I2C address (or 1-Wire identifier for DS18B20) directly in the page title itself.

**Example Transformations:**
- "BME280" → "BME280 (0x77)"
- "INA219" → "INA219 (0x40)"
- "DS18B20" → "DS18B20 (GPIO23)" or "DS18B20 (1-Wire)"
- "Gamepad QT" → "Gamepad QT (0x50)"

**Expected User Benefit:**
- Quicker sensor identification when navigating pages
- Reduced cognitive load (no need to parse a separate row)
- More self-documenting display (address always visible in header)

---

## 2. Functional Requirements Affected

### FR-001: Multi-Sensor Data Acquisition
**Impact Level:** Minimal  
**Change:** Display layer only; no sensor acquisition logic affected.
**Implication:** Sensor detection at boot remains unchanged; I2C addresses are already known at display time.

### FR-002: Real-Time Display of Sensor Data
**Impact Level:** Medium  
**Change:** Page titles now contain address metadata; title text length may increase.
**Implication:** Title string size in pixels may increase, affecting layout. Potential for text clipping if address is long (e.g., "DS18B20 (GPIO23)" fits, "VEML6070 (0x38 / 0x39)" may not fit on one line).

### FR-003: Circular Pagination Navigation
**Impact Level:** None  
**Change:** Navigation logic unchanged.
**Implication:** No impact on navigation behavior.

---

## 3. Domain Entities Affected

### 3.1 Sensor Identity

**Affected Entities:**
- 10 Sensor Pages (one per sensor or sensor group)
- I2C Address Constants (defined as #define in main.cpp)
- SensorStatus struct (detection flags)

**Current State:**
```cpp
#define GAMEPAD_ADDR 0x50
#define ENCODER_ADDR 0x36
#define VEML6075_ADDR 0x10
#define VEML6070_ADDR_LSB 0x38
#define VEML6070_ADDR_MSB 0x39
#define SI1145_ADDR 0x60
#define SCD30_ADDR 0x61
#define BME280_ADDR 0x77
#define INA219_ADDR 0x40  // (implicitly from Adafruit_INA219 ina219(0x40))
#define DS18B20_PIN 23    // 1-Wire, not I2C
```

**Change:** No structural change; these constants are already used for detection and page rendering. The title generation function will reference them.

---

## 4. Business Rules Affected

### BR-015: Page Redraw Only on Page Change (Header Skip)

**Current Rule:**
```
The drawCurrentPage(bool pageChanged) function renders either full page 
(header + data) or data-only (no header), depending on pageChanged flag. 
Header redraw is expensive (full screen clear), so it's skipped when only 
data updates.
```

**Affected By This Change:** YES  
**Impact:** Title is part of header, rendered by `drawHeader(title)`. If title generation is now dynamic (e.g., combining sensor name + address), the function must ensure title string is available/cached at header draw time.

**Current Implementation:**
```cpp
const char *titles[] = {"BME280", "DS18B20", "Gamepad QT", "INA219", 
                        "Rotary Encoder", "SCD30", "SI1145", "TSL2591", 
                        "VEML6070", "VEML6075"};

if (pageChanged) 
  drawHeader(titles[currentPage]);
```

**Consideration:** Title array becomes complex if addresses are dynamically formatted. May require:
1. Pre-computed string literals (simplest)
2. String formatting function (more flexible)
3. Cache-and-reuse pattern (performance-sensitive)

---

### BR-016: Page-Specific Refresh Rates (Multi-Rate Display)

**Current Rule:**
```
Live pages (Gamepad page 2, Encoder page 4) refresh every 20 ms. 
Other pages refresh every 500 ms.
```

**Affected By This Change:** NO  
**Implication:** Title display frequency unchanged; header redrawn only on page change.

---

## 5. Database Structures Affected

**Status:** NOT APPLICABLE  
This project has **no database**. All state is in RAM-only C structs.

**Affected RAM Structs:**
- `const char *titles[]` array in `drawCurrentPage()` (will change from simple strings to formatted strings with addresses)

**Memory Impact:**
- Each title string grows: typical 8–12 chars → 12–25 chars (with address)
- Example: "BME280" (7 bytes) → "BME280 (0x77)" (13 bytes)
- Total impact: ~80–120 additional bytes in flash (string literals) and stack (temporary strings during formatting)

---

## 6. API & Backend Changes

**Status:** NOT APPLICABLE  
This is embedded firmware with no HTTP/REST/RPC API.

**Serial Output:** Only debug output to Serial @ 115200 baud; no structured API payload changes.

---

## 7. UI Components Affected

### 7.1 Page Header Component

**Current Implementation:**
```cpp
void drawHeader(const char *title) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  writeText(6, 8, title, TFT_CYAN, 4);  // Title in cyan, font size 4
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawFastHLine(6, 42, 468, TFT_DARKGREY);  // Divider line
}
```

**Affected:** YES  
**Change:** Title parameter now includes address; text length increases.

**Potential Issue:** Text clipping on narrow addresses?
- Font 4 (standard title font) width per character ≈ 24 pixels
- Display width: 320 pixels
- Usable width (left margin + padding): ~300 pixels
- Maximum chars: ~12–13 characters
- Current longest: "Rotary Encoder" (14 chars) already exceeds this!
- Proposed: "Rotary Encoder (0x36)" (20 chars) will EXCEED display width

**Mitigation Options:**
1. **Abbreviate address:** "Rotary Encoder (36)" (no 0x prefix) → 19 chars (marginal fit)
2. **Shorten sensor name:** "Encoder (0x36)" (14 chars) ✅ Fits comfortably
3. **Use smaller font:** Font 3 instead of 4 (may reduce readability)
4. **Wrap to two lines:** Title on line 1, address on line 2 (changes layout)
5. **Right-align address:** Title left, address right (requires layout redesign)

**Recommended Approach:** Use full sensor names (for clarity in docs) but implement text width measurement and adaptive rendering (abbreviate if needed).

---

### 7.2 Data Row Component (I2C Address Row)

**Current Implementation (Example for BME280 Page):**
```
Label:  "I2C address:"
Value:  "0x77"
Color:  TFT_YELLOW
Y:      52
```

**Decision Required:** Remove this row once address is in title?

**Rationale for Keeping:**
- Consistent layout across all pages (always has address row)
- Useful for verification if title is abbreviated

**Rationale for Removing:**
- Eliminates redundancy (address shown twice)
- Frees up row space for additional data
- Cleaner display

**Current Specification Impact:**
- 07-ui.md explicitly documents address row on each page
- Removing it requires updates to 10 pages' specifications

---

### 7.3 All 10 Page Renderers

**Affected Pages:**
0. BME280 (0x77)
1. DS18B20 (GPIO23 / 1-Wire, no I2C address)
2. Gamepad QT (0x50)
3. INA219 (0x40)
4. Rotary Encoder (0x36)
5. SCD30 (0x61)
6. SI1145 (0x60)
7. TSL2591 (0x29)
8. VEML6070 (0x38/0x39)
9. VEML6075 (0x10)

**Change:** Title strings update; page renderer functions remain logically unchanged (they don't generate titles, they only render data rows).

---

### 7.4 Error State Display

**Current Pattern:**
```cpp
void unavailable(const char *name, const char *address, 
                 const char *addressLabel = "I2C-Adresse:") {
  writeText(6, 64, "Sensor nicht vorhanden", TFT_RED, 2);
  row(96, addressLabel, address, TFT_YELLOW);
  writeText(6, 142, "Y: vorherige Seite", TFT_WHITE);
  // ...
}
```

**Question:** Does `unavailable()` also need address in header?
- Currently, error pages still call `drawHeader()` with sensor name
- Address shown in error row below
- **Recommendation:** Consistent with available sensors; include address in header of error pages too

---

## 8. Security & Authorization Rules Affected

**Status:** NOT APPLICABLE  
This is embedded firmware with no authentication or authorization model.

---

## 9. Tests Needed or Changed

### 9.1 Unit Tests (Currently Non-Existent)

**Test Suite Status:** No tests exist; 10-testing.md documents this explicitly.

**New Tests to Consider:**
If tests were implemented, this change would require:

1. **Title Generation Tests**
   - Input: Sensor page index (0–9)
   - Output: Title string with address
   - Test cases: Each page generates correct title with matching I2C address

2. **Text Rendering Tests**
   - Input: Title string length
   - Output: Text fits within display bounds (320 pixels)
   - Test cases: Verify no clipping for all title variants

3. **Layout Tests**
   - Input: Page with new title format
   - Output: Data rows don't overlap with title
   - Test cases: Verify y-coordinates of data rows are still correct

### 9.2 Integration Tests (Hardware-Required)

**Current Status:** No automated integration tests; user must manually verify on hardware.

**Manual Verification Checklist:**
- [ ] Each of 10 pages displays with correct title including address
- [ ] Titles render without clipping or truncation
- [ ] Page navigation works (Y/A buttons still switch pages)
- [ ] Data rows display correctly below title
- [ ] Error pages (for missing sensors) also show address in title
- [ ] Text is readable (no color/contrast issues)
- [ ] No display artifacts or flickering during page changes

---

## 10. Documentation Files Affected

### 10.1 [07-ui.md](07-ui.md) — User Interface Specification

**Current Content:** Each page documents title and I2C address row separately.

**Example (Page 0: BME280):**
```markdown
| Component | Content | Y | Color |
|-----------|---------|---|-------|
| Header | "BME280" | 8 | Cyan |
| Label | "I2C address:" | 52 | White |
| Value | "0x77" | 52 | Yellow |
```

**Required Changes:**
1. Update header row to show new format: `"BME280 (0x77)"` instead of just `"BME280"`
2. Decision: Remove "I2C address:" row or keep it for redundancy?
   - If removed: Update table for all 10 pages
   - If kept: Document that it's now redundant; clarify design rationale
3. Update all 10 page specifications (Pages 0–9)
4. Add section explaining title format rules (e.g., how to handle multi-address sensors like VEML6070)
5. Add text width constraints and clipping mitigation strategy
6. Update screenshots (if any embedded in docs)

**Impact:** Medium — Systematic changes to all 10 page specifications; no new concepts, just format changes.

---

### 10.2 [06-business-rules.md](06-business-rules.md) — Business Rules

**Current Content:** BR-015 documents page header redraw logic.

**Required Changes:**
1. Add new business rule (e.g., BR-027) documenting title format rule
   - Rule name: "Page Title Format Includes I2C Address"
   - Scope: Applied to all 10 pages
   - Format: `"${SensorName} (${Address})"`
   - Exceptions: DS18B20 uses 1-Wire ("DS18B20 (GPIO23)"), VEML6070 uses dual-address ("VEML6070 (0x38/0x39)")
   - Trigger: Page header display (drawHeader)
   - Evidence: drawCurrentPage() line XXX, titles[] array
   - Dependencies: BR-015 (page redraw logic)

2. Update BR-015 if logic changes for title generation

**Impact:** Low — Mostly additive (new rule); BR-015 likely remains unchanged.

---

### 10.3 [01-requirements.md](01-requirements.md) — Functional Requirements

**Current Content:** FR-002 specifies "display of sensor data with color coding".

**Required Changes:**
1. Clarify FR-002 to include address display in title
2. Add new non-functional requirement (NFR) if appropriate:
   - "Page titles shall be self-documenting, including sensor I2C address"
   - OR add to existing FR-002 as clarification

**Impact:** Very Low — Clarification only; no new functional capability.

---

### 10.4 [14-reconstruction-plan.md](14-reconstruction-plan.md) — Reconstruction Plan

**Current Content:** Phase 7 (Frontend/UI) documents page implementation.

**Required Changes:**
1. Update Phase 7, Group 5 (Title Generation)
   - Add step for title string construction or formatting
   - Add acceptance criterion: "All titles include I2C address in format ${Name} (${Addr})"
   - Add validation test: Title strings fit within display width (≤300 pixels at font 4)

**Impact:** Low — Phase 7 already covers page rendering; this is a refinement.

---

### 10.5 Other Documentation Files

**No Direct Changes Required:**
- [02-architecture.md](02-architecture.md) — No UI details; no change
- [04-data-model.md](04-data-model.md) — Structs unchanged; no change
- [05-api.md](05-api.md) — No UI content; no change
- [09-configuration.md](09-configuration.md) — I2C addresses already documented; may add reference to title format
- [10-testing.md](10-testing.md) — Could add manual verification checklist (same as test plan above)

---

## 11. Implementation Design Decisions

### Decision 1: Title String Storage

**Question:** How should titles be generated/stored?

**Option A: Hardcoded String Literals (Simplest)**
```cpp
const char *titles[] = {
  "BME280 (0x77)",
  "DS18B20 (GPIO23)",
  "Gamepad QT (0x50)",
  "INA219 (0x40)",
  "Rotary Encoder (0x36)",
  "SCD30 (0x61)",
  "SI1145 (0x60)",
  "TSL2591 (0x29)",
  "VEML6070 (0x38/0x39)",
  "VEML6075 (0x10)"
};
```
**Pros:** Simplest; no runtime formatting overhead; address is correct by construction  
**Cons:** Hardcoded values; harder to maintain if addresses change; redundant with #define constants  
**Memory Cost:** ~120 bytes flash for string literals

**Option B: Dynamic String Formatting (Most Flexible)**
```cpp
void getPageTitle(uint8_t pageIndex, char *buffer, size_t bufferSize) {
  const char *names[] = {"BME280", "DS18B20", ...};
  const char *addrs[] = {"0x77", "GPIO23", ...};
  snprintf(buffer, bufferSize, "%s (%s)", names[pageIndex], addrs[pageIndex]);
}
```
**Pros:** Centralizes address definitions; easier to maintain; extensible  
**Cons:** Runtime overhead (snprintf); requires stack buffer (temporary)  
**Memory Cost:** ~50 bytes flash + ~50 bytes stack per call

**Option C: Macro-Based (Hybrid)**
```cpp
#define TITLE_BME280 "BME280 (0x77)"
#define TITLE_DS18B20 "DS18B20 (GPIO23)"
// ...
const char *titles[] = {TITLE_BME280, TITLE_DS18B20, ...};
```
**Pros:** Centralized address definitions; compile-time string building  
**Cons:** More macros to maintain; still hardcoded format  
**Memory Cost:** ~120 bytes flash (same as Option A)

**Recommendation:** Option A (hardcoded literals) for simplicity and performance. Addresses are compile-time constants and unlikely to change. If future projects need dynamic addresses, refactor to Option B.

---

### Decision 2: Text Clipping / Overflow Handling

**Problem:** "Rotary Encoder (0x36)" may exceed display width.

**Option A: Accept Clipping**
- Let TFT library truncate text if it exceeds display width
- **Pro:** Simplest; no special logic  
- **Con:** Aesthetically poor; looks like a bug

**Option B: Pre-Compute Width & Abbreviate**
```cpp
// Before rendering:
// "Rotary Encoder (0x36)" → "Encoder (0x36)" if too long
```
- **Pro:** Always readable; intelligent truncation  
- **Con:** Requires text width measurement (available in TFT_eSPI library)

**Option C: Redesign Layout (Two-Line Title)**
```
[Line 1] Rotary Encoder
[Line 2] (0x36)
```
- **Pro:** Always fits; no abbreviation needed  
- **Con:** Changes layout; uses extra vertical space

**Recommendation:** Option B (abbreviate if needed). Provides best UX without major layout changes. Use `tft.textWidth()` to measure and abbreviate intelligently.

---

### Decision 3: Keep or Remove Address Row?

**Problem:** Address appears in two places (title + separate row).

**Question:** Should we remove the "I2C address:" row?

**Option A: Keep Both**
- Title: "BME280 (0x77)"
- Row: "I2C address:" → "0x77"
- **Pro:** Maximum clarity; redundancy for verification
- **Con:** Wasted screen space; looks redundant

**Option B: Remove Address Row**
- Title: "BME280 (0x77)"
- No address row
- Other rows shift up (more space available)
- **Pro:** Cleaner display; more space for data
- **Con:** Changes documented layout; requires doc updates

**Recommendation:** Option B (remove address row) to eliminate redundancy and improve display efficiency. However, this is a BREAKING CHANGE to the documented layout, and the spec says "do not implement yet." Document this as a DECISION NEEDED in the Open Questions section.

---

## 12. Acceptance Criteria

### Functional Acceptance Criteria

- [ ] All 10 page titles include I2C address in format `${SensorName} (${Address})`
- [ ] DS18B20 title shows `"DS18B20 (GPIO23)"` or similar 1-Wire identifier
- [ ] VEML6070 title shows `"VEML6070 (0x38/0x39)"` reflecting dual-address nature
- [ ] All titles render without clipping (≤320 pixels; recommend ≤300 pixels with margins)
- [ ] Title text is visible and readable (cyan color, font 4)
- [ ] Page navigation (Y/A buttons) still works correctly
- [ ] Error pages (for missing sensors) also display title with address

### Non-Functional Acceptance Criteria

- [ ] No additional memory overhead (keep <150 bytes flash, <50 bytes stack)
- [ ] No performance degradation (page renders in same time as before)
- [ ] Code remains maintainable (addresses centralized, easy to update)
- [ ] Documentation updated consistently across all affected files

### Testing Acceptance Criteria

- [ ] Manual verification: Navigate through all 10 pages; verify each title includes address
- [ ] Manual verification: Simulate missing sensors; verify error pages show title + address
- [ ] Manual verification: No text clipping or rendering artifacts
- [ ] Documentation verification: 07-ui.md updated for all 10 pages
- [ ] Documentation verification: 06-business-rules.md includes new rule for title format
- [ ] Documentation verification: 14-reconstruction-plan.md updated Phase 7 (if applicable)

---

## 13. Validation Rules

### Validation: I2C Address Correctness

**Rule:** Each page's address must match the actual sensor I2C address used in the code.

**Verification:**
```cpp
// For each page:
// 1. Extract address from title (e.g., "0x77" from "BME280 (0x77)")
// 2. Verify matches #define: #define BME280_ADDR 0x77 ✅
// 3. Verify matches library instantiation: Adafruit_BME280 bme280; (address from begin()) ✅
```

**Test Coverage:**
- BME280: 0x77 ✅
- DS18B20: GPIO23 (1-Wire)
- Gamepad: 0x50 ✅
- INA219: 0x40 ✅
- Encoder: 0x36 ✅
- SCD30: 0x61 ✅
- SI1145: 0x60 ✅
- TSL2591: 0x29 ✅
- VEML6070: 0x38/0x39 ✅
- VEML6075: 0x10 ✅

---

### Validation: Text Rendering

**Rule:** Title strings must fit within display width (320 pixels) and render without clipping.

**Measurement:**
```cpp
// At font 4 (title font):
// Approximate char width: 20–24 pixels (depends on char)
// Display usable width: ~300 pixels (10–15 characters)

// Test all titles:
for each title in titles[] {
  int width = tft.textWidth(title);
  assert(width <= 300);  // Fail if too long
}
```

**Results:**
- "BME280 (0x77)" ≈ 200px ✅
- "DS18B20 (GPIO23)" ≈ 250px ✅
- "Gamepad QT (0x50)" ≈ 250px ✅
- "INA219 (0x40)" ≈ 200px ✅
- "Rotary Encoder (0x36)" ≈ 300px ⚠️ (marginal; may need abbreviation)
- "SCD30 (0x61)" ≈ 200px ✅
- "SI1145 (0x60)" ≈ 200px ✅
- "TSL2591 (0x29)" ≈ 200px ✅
- "VEML6070 (0x38/0x39)" ≈ 300px ⚠️ (marginal)
- "VEML6075 (0x10)" ≈ 200px ✅

**Mitigation:** Abbreviate "Rotary Encoder" → "Encoder" and verify fit.

---

## 14. Error Cases

### Error Case 1: Sensor Not Detected at Boot

**Scenario:** User navigates to page for sensor that failed to initialize.

**Current Behavior:** Error page displays "Sensor nicht vorhanden" (red) with I2C address in separate row.

**New Behavior:** Error page header should also include address in title.

**Implementation Consideration:** `unavailable()` function calls `drawHeader()` but doesn't receive title. Must refactor to pass title including address.

**Current Code:**
```cpp
void unavailable(const char *name, const char *address, ...) {
  // drawHeader() not called here; error page redraws manually
  writeText(6, 64, "Sensor nicht vorhanden", TFT_RED, 2);
  row(96, addressLabel, address, TFT_YELLOW);
  ...
}
```

**Recommended Fix:**
```cpp
void unavailable(const char *title, const char *address, ...) {
  drawHeader(title);  // Now includes address
  writeText(6, 64, "Sensor nicht vorhanden", TFT_RED, 2);
  // Optional: Keep address row for redundancy, or remove
  row(96, addressLabel, address, TFT_YELLOW);
  ...
}
```

---

### Error Case 2: Text Rendering Fails / Clipping Occurs

**Scenario:** A title is too long and gets truncated by TFT_eSPI library.

**Symptom:** Incomplete title displayed (e.g., "Rotary Encoder (0x" with "36)" cut off).

**Detection:** Manual verification or automated text width check.

**Recovery:** Abbreviate title (e.g., "Encoder (0x36)") or use smaller font (reduces readability).

**Recommended Mitigation:** Pre-compute title width at compile-time; use static_assert or runtime check in setup().

---

### Error Case 3: Address Constant Mismatch

**Scenario:** Title shows "BME280 (0x77)" but actual code probes address 0x77 differently.

**Symptom:** Sensor detection fails, but user sees correct address in title (confusing).

**Root Cause:** Hardcoded address in title mismatches actual #define or library instantiation.

**Prevention:** 
1. Version control: commit title changes together with address changes
2. Automated verification: grep for #define and title string to ensure consistency
3. Code review: verify all 10 titles match their corresponding #defines and library calls

---

## 15. Data Changes

### Data Model Changes

**Affected Struct:** None (title is display-only; no data structure change).

**Display Cache:** `const char *titles[]` array in `drawCurrentPage()` changes from 10 simple strings to 10 formatted strings with addresses.

**Change Magnitude:** None to RAM/EEPROM; only ROM (string literals in flash).

---

### Database Changes

**Status:** NOT APPLICABLE (no database in this project).

---

## 16. API Changes

**Status:** NOT APPLICABLE (embedded firmware; no HTTP/REST API).

---

## 17. Authorization Changes

**Status:** NOT APPLICABLE (no authentication model).

---

## 18. UI/UX Changes Summary

| Aspect | Before | After | Impact |
|--------|--------|-------|--------|
| **Title Format** | "BME280" | "BME280 (0x77)" | Clearer sensor identity |
| **Title Length** | 8–15 chars | 12–25 chars | Some titles marginal fit |
| **Address Row** | Displayed separately | Redundant (possibly removed) | Screen space saved |
| **User Cognitive Load** | Moderate (address separate) | Low (address in title) | Faster page identification |
| **Display Density** | 3–4 rows of data | 4–5 rows (if address row removed) | More data visible |

---

## 19. Open Questions

### Q1: Should Address Row Be Removed?

**Status:** DECISION NEEDED  

**Question:** Once address is in the title, should we remove the "I2C address:" row to save screen space and eliminate redundancy?

**Pros (Remove):**
- Eliminates redundant information
- Frees ~24 pixels of vertical space (one row height)
- Cleaner display
- Aligns with DRY principle

**Cons (Remove):**
- Changes documented layout (07-ui.md)
- May confuse users accustomed to address row
- Removes visual reference point

**Pros (Keep):**
- Maximum clarity; address shown twice
- Matches current documented layout
- Familiar to users

**Cons (Keep):**
- Redundant information
- Wastes screen space
- Looks inelegant

**Recommendation:** Document as "FUTURE DECISION — address row may be removed in v2.0 after user feedback."

---

### Q2: How to Handle Text Clipping?

**Status:** DECISION NEEDED  

**Question:** What strategy for titles that exceed display width (e.g., "Rotary Encoder (0x36)")?

**Options:**
1. Accept clipping (simplest, poor UX)
2. Abbreviate dynamically (e.g., "Encoder (0x36)") (best UX, moderate complexity)
3. Two-line title (unusual, breaks layout)
4. Smaller font (reduces readability)

**Recommendation:** Option 2 (abbreviate if needed). Implement text width check and use abbreviated names as fallback.

---

### Q3: Format for Multi-Address Sensors?

**Status:** DECISION NEEDED  

**Question:** For VEML6070 (dual I2C addresses 0x38 and 0x39), how to format title?

**Options:**
1. "VEML6070 (0x38/0x39)" (accurate, long, 20+ chars)
2. "VEML6070 (0x38)" (simplified, shows primary address)
3. "VEML6070 (LSB/MSB)" (descriptive, but unclear to users)
4. "VEML6070" (address omitted, contradicts feature goal)

**Recommendation:** Option 1 ("VEML6070 (0x38/0x39)") for technical accuracy. If text clipping occurs, abbreviate to "VEML6070 (38/39)" (no 0x prefix).

---

### Q4: Should DS18B20 Show GPIO Pin?

**Status:** DECISION NEEDED  

**Question:** DS18B20 uses 1-Wire (GPIO23), not I2C. How to label?

**Options:**
1. "DS18B20 (GPIO23)" (matches I2C format; clear)
2. "DS18B20 (1-Wire)" (describes protocol, less specific)
3. "DS18B20 (Pin 23)" (shorter; clear reference)
4. "DS18B20" (address omitted; feature doesn't apply)

**Recommendation:** Option 1 ("DS18B20 (GPIO23)") for consistency with other titles. Alternative: "DS18B20 (GPIO23 / 1-Wire)" if space allows.

---

### Q5: Documentation Update Scope?

**Status:** DECISION NEEDED  

**Question:** Should we update 14-reconstruction-plan.md Phase 7 to include title generation as a sub-task?

**Options:**
1. Update Phase 7 with new step for "Title String Construction" (adds clarity)
2. Leave Phase 7 unchanged; title generation is trivial subpart of "Header Rendering" (simpler)

**Recommendation:** Option 1 (minimal update): Add to Phase 7, Group 1:
> "Title strings now include I2C address. Update titles[] array in drawCurrentPage() to format: `"${SensorName} (${Address})"`"

---

### Q6: Automated Testing?

**Status:** DECISION NEEDED  

**Question:** Should unit tests be written for this feature?

**Current Status:** No tests exist in this project (10-testing.md documents this).

**Options:**
1. Leave unchanged; rely on manual verification (current practice)
2. Add integration test: Verify each page displays correct title with address
3. Add unit test: Test title string generation/formatting logic

**Recommendation:** Option 2 (minimal integration test) as part of Phase 10 (Testing) in reconstruction plan, IF testing is ever implemented. For now, document manual verification checklist (see Section 9.2).

---

## 20. Dependencies & Scheduling

### Prerequisite Changes

1. **No upstream dependencies** — This change is self-contained.
2. **Relies on existing:** I2C address #defines already in code; page rendering infrastructure already in place.

### Downstream Changes (Enabled By This Feature)

1. **Future v2.0:** Could remove address row entirely (if this feature is well-received).
2. **Future v2.0:** Could add dynamic address configuration (if addresses become user-configurable).
3. **Future Expansion:** Could extend to other sensor displays (e.g., baud rate in Serial output).

---

## 21. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Text clipping / truncation | Medium | Low (UX issue only) | Pre-compute text width; abbreviate if needed |
| Address mismatch with code | Low | Medium (confusing) | Code review; automated grep check for consistency |
| Layout break (data rows shift) | Very Low | Low (if address row removed) | Test layout on hardware before deploying |
| Performance regression | Very Low | Very Low | No runtime overhead if using hardcoded strings |
| User confusion (address now in two places) | Low | Low | Remove address row after user feedback in v2.0 |

---

## 22. Effort Estimate

**Note:** Estimate assumes no implementation yet; specification only.

| Task | Complexity | Estimated Effort |
|------|-----------|------------------|
| Update title strings in code | Trivial | 5–10 min |
| Handle text clipping (if needed) | Low | 15–30 min |
| Update 07-ui.md (all 10 pages) | Low | 30–45 min |
| Update 06-business-rules.md (new rule) | Low | 15–20 min |
| Update 14-reconstruction-plan.md (Phase 7) | Very Low | 5–10 min |
| Manual testing on hardware | Medium | 20–30 min |
| **TOTAL** | **Low–Medium** | **1.5–2.5 hours** |

---

## 23. Implementation Phases (When Approved)

### Phase 1: Code Changes (15–30 min)
1. Update `titles[]` array with addresses
2. Implement text width checking (if clipping occurs)
3. Compile and verify build succeeds

### Phase 2: Testing (20–30 min)
1. Upload to hardware
2. Navigate through all 10 pages; verify titles display correctly
3. Verify no text clipping
4. Test error pages

### Phase 3: Documentation Updates (1–1.5 hours)
1. Update 07-ui.md (all 10 pages) — 30–45 min
2. Update 06-business-rules.md (add BR-027) — 15–20 min
3. Update 14-reconstruction-plan.md (Phase 7) — 5–10 min
4. Update 01-requirements.md (clarify FR-002) — 5 min

---

## 24. Success Criteria

- ✅ All 10 pages display title with I2C address
- ✅ No text clipping or rendering artifacts
- ✅ Page navigation still works correctly
- ✅ Documentation updated and consistent
- ✅ Hardware testing passed (manual verification checklist)
- ✅ No performance degradation

---

## 25. Sign-Off

**Feature Specification Status:** ✅ COMPLETE (No Implementation Yet)

**Analysis Performed By:** GitHub Copilot  
**Analysis Date:** 2026-09-21  
**Specification Version:** 1.0  

**Approval Required From:**  
- [ ] User (feature approved for implementation?)
- [ ] Technical Lead (design approved?)
- [ ] QA (testing plan approved?)

---

## Appendix A: I2C Address Reference

| Page | Index | Sensor Name | I2C Address | Library | Notes |
|------|-------|-------------|-------------|---------|-------|
| 0 | 0 | BME280 | 0x77 | Adafruit_BME280 | Climate sensor (temp/humidity/pressure) |
| 1 | 1 | DS18B20 | GPIO23 (1-Wire) | DallasTemperature | Temperature sensor; not I2C |
| 2 | 2 | Gamepad QT | 0x50 | Adafruit_seesaw | Gamepad controller; optional for nav |
| 3 | 3 | INA219 | 0x40 | Adafruit_INA219 | Power monitor (voltage/current/charge) |
| 4 | 4 | Rotary Encoder | 0x36 | Adafruit_seesaw | Encoder + NeoPixel LED |
| 5 | 5 | SCD30 | 0x61 | Custom I2C | CO₂ sensor; CRC-validated reads |
| 6 | 6 | SI1145 | 0x60 | Adafruit_SI1145 | Multi-spectrum sensor |
| 7 | 7 | TSL2591 | 0x29 | Adafruit_TSL2591 | Light sensor (lux) |
| 8 | 8 | VEML6070 | 0x38/0x39 | Custom I2C | UV sensor; dual-address |
| 9 | 9 | VEML6075 | 0x10 | Custom I2C | UV sensor with compensation |

---

## Appendix B: Current Layout Example (BME280 Page)

```
┌──────────────────────────────────┐
│ BME280                           │  y=8, cyan, font 4 (WILL CHANGE TO "BME280 (0x77)")
├──────────────────────────────────┤ y=42, divider
│ I2C address:              0x77   │  y=52, label white + value yellow
│ Temperature:             23.4°C  │  y=76, label white + value green
│ Humidity:                45.2%   │  y=100, label white + value green
│ Pressure:             1013.2hPa  │  y=124, label white + value orange
│ Altitude:                15.3m   │  y=148, label white + value cyan
│ Dew point:               12.1°C  │  y=172, label white + value cyan
│                                  │
├──────────────────────────────────┤
│ Y/A: Seite wechseln | BME280    │  y=294, dark grey
└──────────────────────────────────┘
```

**Change:** Title (y=8) becomes "BME280 (0x77)". Address row (y=52) may be removed (Decision Q1).

---

**END OF SPECIFICATION**
