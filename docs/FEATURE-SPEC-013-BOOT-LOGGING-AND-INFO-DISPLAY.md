# FEATURE-SPEC-013: Boot Logging on TFT Display and System Information Page

**Status:** SPECIFICATION ONLY (NOT YET IMPLEMENTED)  
**Version:** 1.0  
**Date:** 2026-10-23  
**Related:** FEATURE-SPEC-012 (Logging), FR-002 (Display), FR-001 (Sensors)

---

## 1. EXECUTIVE SUMMARY

This feature adds two capabilities:

1. **Boot Logging to TFT Display**: Display boot initialization messages on the TFT screen in a terminal-like format (no timestamps), allowing visual confirmation of each boot stage.

2. **System Information Page**: Add a new display page (after boot completion) showing system runtime state, version, build date, error counts, and other diagnostics without interfering with the existing 10-page sensor navigation.

---

## 2. AFFECTED REQUIREMENTS

### Existing Requirements Impacted:

**FR-002: Real-Time Display of Sensor Data**
- Currently: TFT display is only initialized after setup() completes
- Impact: Must defer full TFT initialization to support boot logging
- Affected: Display subsystem must support dual modes (boot terminal vs. sensor pages)

**FR-003: Circular Pagination Navigation**
- Currently: 10 pages (indices 0-9) for sensor data
- Impact: Need to add new "info" page without breaking pagination logic
- Affected: Page navigation must handle 11 pages or use special-case handling for info page

**FR-001: Multi-Sensor Data Acquisition**
- Currently: I2C bus initialization happens early in setup()
- Impact: Must add I2C bus scan logging after bus initialization
- Affected: Sensor detection sequence must be visible on TFT during boot

**Related to diagnostics.h (VERSION, BUILD_DATE)**
- Currently: Version/build date only logged to Serial
- Impact: Must be accessible on display and require no changes to diagnostics.h
- Affected: Boot logging system must read VERSION and BUILD_DATE constants

---

## 3. DOMAIN ENTITIES AFFECTED

### New Entities:

**3.1 BootLogger Entity**
```
BootLogger {
  Mode: enum {DISABLED, SERIAL_ONLY, TFT_ONLY, SERIAL_AND_TFT}
  LineCount: uint8 (0-30, display capacity)
  LineBuffer[30]: char[64] (store most recent lines)
  IsReady: bool (TFT fully initialized and ready)
  StartTimeMs: uint32 (boot start timestamp)
}
```

**3.2 SystemInfoPage Entity**
```
SystemInfoPage {
  UptimeMs: uint32 (continuously updated)
  BootTimeMs: uint32 (when boot_complete reached)
  SoftwareVersion: string (from diagnostics.h VERSION)
  BuildDate: string (from diagnostics.h BUILD_DATE)
  ErrorCount: uint32 (aggregate of all error categories)
  I2cErrorCount: uint32 (from diagnostics.h)
  SlowSensorCount: uint32 (from diagnostics.h)
  FreeHeap: uint32 (current free memory)
  MinFreeHeap: uint32 (all-time minimum)
  SensorCount: uint8 (number of detected sensors)
  RefreshIntervalMs: uint32 (how often to update)
  IsVisible: bool (currently displayed)
}
```

### Existing Entities Modified:

**3.3 Display Subsystem**
- Change: Must support boot terminal mode before first sensor page
- Change: Page navigation may need to handle info page specially

**3.4 Setup Sequence (main.cpp setup())**
- Change: Must be reorganized into logged stages
- Change: Return values or status must be trackable for logging

---

## 4. BUSINESS RULES AFFECTED

### New Business Rules:

**BR-BOOT-001: Boot Sequence Logging Order**
```
Boot sequence is:
  1. Serial port initialized (115200 baud, 8N1)
  2. Diagnostics initialized (reboot reason, heap stats)
  3. TFT display initialized → ready to log boot messages
  4. I2C bus initialized (400 kHz, 100ms timeout)
  5. I2C bus scan (if enabled): discover all devices
  6. Sensor presence detection: list detected addresses
  7. Sensor initialization: per-sensor begin() calls logged
  8. Sensor registry initialization
  9. Initial sensor reads
  10. DS18B20 conversion (if present): wait message logged
  11. Final screen drawn
  12. Boot complete message, 1 second pause
  13. Show info page OR first sensor page (configurable)
```

**BR-BOOT-002: Boot Terminal Message Format**
```
Messages on TFT during boot:
  - No timestamps (timestamps only in Serial)
  - Format: "[CATEGORY] message text"
  - Categories: BOOT, SETUP, I2C, SENSOR, INIT, etc.
  - Maximum line length: 60 characters
  - Maximum visible lines: ~28 lines at default font size
  - Scrolling: New lines push old lines off top
  - Color: White text on black background (standard TFT color scheme)
```

**BR-BOOT-003: Boot Completion Criteria**
```
Boot is complete when:
  - All initialization stages finished
  - "BOOT COMPLETE" message displayed
  - 1000ms delay passed
  - First normal page will be shown next
```

**BR-INFO-001: Info Page Display**
```
Info page shows:
  - Software version (from VERSION)
  - Build date (from BUILD_DATE)
  - Current uptime (HH:MM:SS, updated every 500ms)
  - Heap status (free bytes / total bytes)
  - Error summary (count by type)
  - Sensor count
  - Not a numbered page (special display mode)
```

**BR-INFO-002: Info Page Navigation**
```
Options:
  A) Add 11th page (index 10) to PAGE_TITLES and navigation
  B) Special key combination (e.g., X + Y held) for info page
  C) Replace current page 0 temporarily, then return to sensor pages
  D) Add info page button (no spare buttons)
  
RECOMMENDATION: Option A (11th page) — cleanest, least disruptive
```

**BR-INFO-003: Error Count Aggregation**
```
ErrorCount = sum of:
  - diagStats.i2cErrors (from diagnostics.h)
  - diagStats.slowSensorReads (from diagnostics.h)
  - Custom error counters per sensor type (if added)
```

---

## 5. DATABASE STRUCTURES AFFECTED

### New Persistent Storage (if any):

**5.1 EEPROM / NVS (Non-Volatile Storage)**

Current state: No EEPROM usage detected in existing code.

Proposal for future: Store boot history (optional, not required for this feature):
```
NVS Namespace: "boot"
Keys:
  - boot_count (uint32)
  - last_boot_ms (uint32)
  - error_count (uint32)
  - uptime_ms (uint32)
```

**Status:** NOT REQUIRED for FEATURE-SPEC-013. Can be deferred to future feature.

### In-Memory Structures:

**5.2 DiagnosticStats (existing, from diagnostics.h)**

Used by boot logging:
```cpp
struct DiagnosticStats {
  uint32_t bootCount;           // Already tracked
  uint32_t totalRuntimeMs;      // Add: track boot time separately
  uint32_t minFreeHeap;         // Already tracked
  uint32_t maxHeapUsage;        // Already tracked
  uint32_t slowSensorReads;     // Already tracked
  uint32_t i2cErrors;           // Already tracked
  uint32_t lastHeapCheck;       // Already tracked
  uint32_t lastI2cCheck;        // Already tracked
  // NEW: Boot-specific tracking
  uint32_t bootCompleteTimeMs;  // Time when boot finished
};
```

---

## 6. API CHANGES

### New Public Functions:

**6.1 Boot Logger API**

```cpp
// Initialize boot logger (must be called after TFT is ready)
void bootLoggerInit(TFT_eSPI& tft);

// Add a line to boot log (called instead of/in addition to Serial)
void bootLogPrintf(const char* format, ...);

// Display boot log on TFT (rendering)
void bootLoggerRender();

// Get boot logger ready state
bool bootLoggerReady();

// Clear boot log and exit boot mode
void bootLoggerComplete();
```

**6.2 System Info Page API**

```cpp
// Initialize system info page
void initSystemInfoPage();

// Render system info page
void pageSystemInfo();

// Update system info values (uptime, heap, etc.)
void updateSystemInfoData();

// Get current uptime in milliseconds since boot
uint32_t getUptimeMs();

// Get uptime formatted as string HH:MM:SS
String formatUptimeString(uint32_t ms);
```

### Modified Public Functions:

**6.3 Changes to logPrintf() (diagnostics.h)**

Current behavior:
```cpp
logPrintf("[SETUP] message");  // → Serial output only
```

New behavior (NO CHANGES to logPrintf itself):
- logPrintf() continues to log to Serial with timestamps
- New bootLogPrintf() handles TFT output (no timestamps)
- Both can be called during boot phase

**Rationale:** Keep existing Serial logging unchanged; add parallel TFT output via new function.

### Modified Display Functions:

**6.4 Changes to drawCurrentPage()**

Current:
```cpp
void drawCurrentPage(bool force = false) {
  // Draw page 0-9
  pageArray[currentPage]();
}
```

New:
```cpp
void drawCurrentPage(bool force = false) {
  if (currentPage == 10) {
    pageSystemInfo();  // New info page
  } else {
    pageArray[currentPage]();  // Existing 10 pages
  }
}
```

**6.5 Navigation Logic**

Current pagination (PAGE_COUNT = 10):
```cpp
nextPage = (currentPage + 1) % PAGE_COUNT;
```

New pagination (PAGE_COUNT = 11):
```cpp
nextPage = (currentPage + 1) % PAGE_COUNT;  // 0→1, 9→10, 10→0
```

---

## 7. UI COMPONENTS AFFECTED

### 7.1 Boot Terminal Display

**Location:** TFT screen during setup()

**Layout:**
```
┌─────────────────────────────────────┐
│ SENSOR PAGES DASHBOARD - BOOT       │  ← Header (fixed)
├─────────────────────────────────────┤
│ [BOOT] Starting setup...            │
│ [DIAG] ===== REBOOT DIAGNOSTICS     │
│ [DIAG] Reboot Reason: POWER-ON      │
│ [DIAG] Free Heap: 272344 bytes      │
│ [SETUP] I2C bus initialized         │
│ [SETUP] Detecting sensors...        │
│ [I2C] Found: BME280 @ 0x77          │
│ [I2C] Found: INA219 @ 0x40          │
│ [I2C] Found: Gamepad @ 0x50         │
│ [SETUP] Initializing sensors...     │  ← Scrolls up as new lines added
│ [SETUP] Reading sensor cache...     │
│ ...                                 │
│ [SETUP] ===== BOOT COMPLETE ====    │
│                                     │
│                 [Wait 1 second]      │
└─────────────────────────────────────┘
```

**Colors:**
- Background: TFT_BLACK
- Text: TFT_WHITE
- Header: TFT_GREEN (same as drawHeader)

**Font:**
- Use existing default font size
- Fixed-width spacing for alignment

### 7.2 System Information Page

**Location:** Page index 10 (new 11th page)

**Layout:**
```
┌─────────────────────────────────────┐
│ SYSTEM INFORMATION                  │  ← Title bar
├─────────────────────────────────────┤
│                                     │
│ Version:  3.1.0                     │
│ Build:    2026-10-22T17:20:00Z      │
│                                     │
│ Uptime:   00:12:34  (live)          │
│ Boot Time: 3.2 seconds              │
│                                     │
│ Heap:     243520 / 276480 bytes     │
│ Min Free: 238912 bytes              │
│                                     │
│ Sensors:  9/10 detected             │
│                                     │
│ Errors:   Total: 2                  │
│           I2C:   1                  │
│           Slow:  1                  │
│                                     │
│ ← Y | Info Page | A →               │  ← Navigation hint
└─────────────────────────────────────┘
```

**Colors:**
- Labels: TFT_GREEN
- Values: TFT_WHITE / TFT_CYAN (for numbers)
- Errors: TFT_RED if count > 0

**Update Frequency:**
- Uptime: Every 500ms (same as sensor pages)
- Heap: Every 1000ms (reduce overhead)
- Error counts: Updated when errors occur

### 7.3 Navigation Integration

**Current navigation (10 pages):**
```
Y (prev)     A (next)
Page 0  ←→  Page 1  ←→  ...  ←→  Page 9
```

**New navigation (11 pages):**
```
Y (prev)     A (next)
Page 0  ←→  Page 1  ←→  ...  ←→  Page 9  ←→  Page 10 (Info)
                                           ↓
                                    (wraps back to Page 0)
```

**Behavior:**
- Page 9 + A button → Page 10 (Info)
- Page 10 + A button → Page 0 (Sensor Page 0)
- Page 10 + Y button → Page 9 (Sensor Page 9)
- All transitions smooth (no special cases)

---

## 8. SECURITY & AUTHORIZATION

### 8.1 No Security Requirements

This feature displays system diagnostics and firmware version — all non-sensitive information.

**Security Considerations:**
- Version display: Public information (no CVE disclosure concern for embedded device)
- Error counts: Debugging information only, not user-facing
- Uptime: Non-sensitive timing information
- Heap stats: System performance data (internal use only)

**Authorization:**
- No authorization needed (no user login, no role-based access)
- No secrets in output
- No sensitive sensor data on info page (only sensor COUNT, not values)

---

## 9. TEST REQUIREMENTS

### 9.1 Boot Logger Tests

| Test Case | Requirement | Acceptance Criteria |
|-----------|-------------|-------------------|
| BOOT-LOG-001 | TFT initialization before first log | First boot message appears on TFT, not black screen |
| BOOT-LOG-002 | Message scrolling | 28+ messages can be logged; older messages scroll off top |
| BOOT-LOG-003 | Format (no timestamps) | Messages show "[CATEGORY] text" without time |
| BOOT-LOG-004 | Serial logging continues | Serial output still has timestamps; TFT output does not |
| BOOT-LOG-005 | I2C discovery logged | All discovered sensors appear with address (0xHH) |
| BOOT-LOG-006 | Boot complete message | "BOOT COMPLETE" appears before 1s wait |
| BOOT-LOG-007 | 1 second delay | Exactly 1000ms pause after BOOT COMPLETE |
| BOOT-LOG-008 | First page display | After boot, sensor page 0 or info page shown |

### 9.2 System Info Page Tests

| Test Case | Requirement | Acceptance Criteria |
|-----------|-------------|-------------------|
| INFO-001 | Info page accessible | Y/A navigation from page 9 reaches page 10 |
| INFO-002 | Version displayed | SOFTWARE VERSION correctly shown |
| INFO-003 | Build date displayed | BUILD_DATE correctly shown |
| INFO-004 | Uptime updates | Uptime value increments every ~500ms |
| INFO-005 | Heap display | Free / Total heap values shown and valid |
| INFO-006 | Error count display | Total errors = I2C + Slow count |
| INFO-007 | Sensor count | Matches number of detected sensors from boot |
| INFO-008 | Navigation persistence | Y/A buttons navigate away and back correctly |

### 9.3 Integration Tests

| Test Case | Requirement | Acceptance Criteria |
|-----------|-------------|-------------------|
| INT-001 | Boot-to-sensor flow | Boot → Info page → Sensor pages (if configured) |
| INT-002 | Page count consistency | PAGE_COUNT=11, PAGE_TITLES has 11 entries |
| INT-003 | Serial logging intact | All Serial messages still logged with timestamps |
| INT-004 | TFT color scheme | Boot terminal consistent with sensor pages |
| INT-005 | Error recovery | Boot logging survives sensor initialization errors |
| INT-006 | Memory usage | Boot logging does not exceed heap budget (~10KB) |

### 9.4 Manual Testing Checklist

- [ ] Connect ESP32 to power
- [ ] Watch TFT display: boot messages appear
- [ ] Verify each stage logged: Serial init, Diag, TFT init, I2C, sensors
- [ ] Check I2C scan results match connected devices
- [ ] Wait for "BOOT COMPLETE" message
- [ ] Confirm 1 second pause
- [ ] System shows sensor page 0 or info page
- [ ] Navigate with Y/A to reach info page (page 10)
- [ ] Verify uptime increments every second
- [ ] Return to sensor pages
- [ ] Reboot and confirm process repeats

---

## 10. DOCUMENTATION CHANGES

### New Documentation Files:

**10.1 FEATURE-SPEC-013-IMPLEMENTATION-GUIDE.md** (to be created)
- Detailed implementation steps
- Code locations for each component
- Integration with existing setup() sequence

**10.2 BOOT-SEQUENCE-REFERENCE.md** (to be created)
- Complete boot flow diagram
- Error paths and recovery
- Timing requirements

### Existing Documentation to Update:

**10.3 docs/01-requirements.md**

Add new requirements:
```markdown
### FR-007: Boot-Time Diagnostic Logging (FEATURE-SPEC-013)

The system shall display boot initialization progress on the TFT display
in a terminal-like format without timestamps, enabling visual confirmation
of each boot stage.

Requirements:
- Boot messages appear on TFT after display initialization
- Format: "[CATEGORY] message" (no timestamps)
- I2C device discovery displayed with addresses
- Boot completion indicated by final message + 1s delay
- Serial logging continues independently with timestamps
```

Add new functional requirement:
```markdown
### FR-008: System Information Display (FEATURE-SPEC-013)

The system shall provide a dedicated display page (page 10) showing
runtime diagnostics including version, build date, uptime, memory usage,
and error statistics.

Requirements:
- Accessible via normal page navigation (Y/A buttons)
- Displays firmware version and build date
- Shows current uptime updated every 500ms
- Shows heap statistics (free / total)
- Shows error and sensor counts
- Integrated into 11-page pagination
```

**10.4 docs/02-architecture.md**

Add subsystem section:
```markdown
### Boot Logger Subsystem

Manages display of boot initialization messages on TFT during setup().
Activated before sensor initialization, disabled after boot complete.
Operates in parallel with Serial logging (no timestamps on TFT).

Components:
- bootLoggerInit(): Activate after TFT ready
- bootLogPrintf(): Add messages to boot log
- bootLoggerRender(): Render to TFT
- bootLoggerComplete(): Exit boot mode
```

**10.5 docs/06-business-rules.md**

Add boot-related business rules (see Section 4 above):
- BR-BOOT-001: Sequence order
- BR-BOOT-002: Message format
- BR-BOOT-003: Completion criteria
- BR-INFO-001: Info page content
- BR-INFO-002: Navigation options
- BR-INFO-003: Error aggregation

**10.6 docs/11-user-interface.md**

Add UI design section:
- Boot terminal layout and colors
- System info page layout
- Navigation flow diagram
- Font and text sizing

**10.7 docs/14-reconstruction-plan.md**

Update if boot sequence changes (likely):
- Add boot logger checkpoint
- Specify I2C bus recovery during boot
- Document sensor initialization rollback on error

---

## 11. IMPLEMENTATION CONSIDERATIONS

### 11.1 Phasing

**Phase 1: Core Boot Logger** (dependencies: existing logPrintf, TFT)
- bootLoggerInit() and bootLogPrintf()
- Integration into setup() sequence
- Tests: BOOT-LOG-001 through BOOT-LOG-007

**Phase 2: System Info Page** (dependencies: Phase 1, PAGE_COUNT update)
- pageSystemInfo() and supporting functions
- Navigation integration
- Tests: INFO-001 through INFO-008

**Phase 3: Integration Testing** (dependencies: Phase 1 + 2)
- Full boot-to-sensor flow
- Error cases
- Tests: INT-001 through INT-006

### 11.2 Configuration Options

**Runtime Options (for future):**
```cpp
enum BootLogTarget {
  BOOT_LOG_NONE,          // Disabled
  BOOT_LOG_SERIAL,        // Serial only
  BOOT_LOG_TFT,           // TFT only
  BOOT_LOG_BOTH           // Serial + TFT
};

bootLogTarget = BOOT_LOG_BOTH;  // Default: show on both
```

**At compile time (for now):**
```cpp
#define ENABLE_BOOT_LOGGING 1
#define BOOT_LOG_BUFFER_LINES 30
#define BOOT_LOG_BUFFER_LINE_LEN 64
```

### 11.3 Memory Footprint

**Estimated new RAM usage:**
```
Boot logger buffer:   30 lines × 64 chars = 1920 bytes
SystemInfoPage vars:  ~200 bytes
Total:                ~2.1 KB
```

**Acceptable:** Well within ESP32 heap budget (276 KB available).

### 11.4 Code Organization

**Suggested locations:**
- Boot logger functions: new file `src/boot_logger.h / .cpp`
- System info page: new file `src/pages/page_system_info.cpp`
- Changes to main.cpp: reorganize setup() sequence, add bootLogPrintf() calls
- Changes to navigation: update PAGE_COUNT, PAGE_TITLES, drawCurrentPage()

---

## 12. OPEN QUESTIONS

### Design Decisions Needed:

**Q1: After boot complete, show which page first?**

Options:
- A) System info page (page 10) — user sees diagnostics
- B) First sensor page (page 0) — normal operation immediately
- C) Configurable at runtime

**Recommendation:** Option A for diagnostic visibility; user can navigate to sensors with A button.

**Q2: How to handle boot errors that prevent normal startup?**

Example: TFT initialization fails. Can't display boot messages.

Fallback: Must still log to Serial. Design allows graceful degradation.

**Decision needed:** Should boot continue despite TFT failure, or hard-reset?

**Q3: Should boot logger save messages to NVS/EEPROM for crash analysis?**

Option: Store last boot log in persistent storage for post-mortem debugging.

**Recommendation:** Defer to future feature. Not required for initial FEATURE-SPEC-013.

**Q4: Sensor count on info page — include failed/offline sensors?**

Example: 10 sensors detected at boot, but 1 goes offline during operation.

Options:
- A) Show detected count (10) — what was found at boot
- B) Show current online count (9) — live status
- C) Show both "10/10" or "9/10"

**Recommendation:** Option C "9/10" for transparency.

**Q5: Should error counts persist across reboots?**

Current diagStats resets on every boot (not saved to NVS).

**Decision:** Keep error counts reset on boot (simpler). Total errors = errors since last boot.

**Q6: Info page refresh rate — sync with sensor pages (500ms) or use different rate?**

**Recommendation:** Use 500ms to keep consistent visual feel with sensor pages.

**Q7: Should boot sequence be abortable (e.g., press button to skip 1s delay)?**

**Recommendation:** No. Keep boot deterministic. 1s delay is minimal.

---

## 13. ACCEPTANCE CRITERIA (for entire feature)

- [ ] Boot messages appear on TFT during setup() without timestamps
- [ ] All boot stages logged: Serial, Diag, TFT, I2C, Sensors, complete
- [ ] I2C device discovery shows addresses of detected sensors
- [ ] "BOOT COMPLETE" message displayed; 1000ms delay observed
- [ ] System shows sensor page 0 or info page after boot
- [ ] Info page displays software version (3.1.0) correctly
- [ ] Info page displays build date correctly
- [ ] Uptime on info page updates smoothly every ~500ms
- [ ] Heap statistics displayed and valid
- [ ] Error count aggregation works correctly
- [ ] Navigation to info page via Y/A buttons works consistently
- [ ] Serial logging continues with timestamps (no regression)
- [ ] All 10 sensor pages remain functional and unmodified
- [ ] No performance degradation observed (boot time < 5 seconds)
- [ ] No memory leaks in boot logger or info page
- [ ] Documentation updated (01-requirements.md, 02-architecture.md, etc.)
- [ ] All unit and integration tests pass
- [ ] Manual testing checklist completed

---

## 14. REVISION HISTORY

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-10-23 | Specification | Initial draft — analysis only, no implementation |

---

## APPENDIX A: Boot Sequence Diagram

```
RESET / POWER-ON
    ↓
Serial.begin(115200) ← [BOOT] message to Serial only
    ↓
initDiagnostics()    ← [DIAG] messages to Serial only
    ↓
TFT.init()           ← TFT ready, can now display
    ↓
bootLoggerInit()     ← activate boot terminal on TFT
    ↓
Wire.begin()         ← [I2C] message on TFT & Serial
    ↓
I2C_ScanBus()        ← [I2C] Found: xxx @ 0xYY
    ↓
detectSensors()      ← [SENSOR] initialization per sensor
    ↓
sensorRegistry...    ← [SETUP] registry initialization
    ↓
readSensors()        ← [SETUP] initial read messages
    ↓
DS18B20_Conversion() ← [SETUP] wait message
    ↓
drawCurrentPage()    ← First display rendered
    ↓
[SETUP] BOOT COMPLETE message on TFT
    ↓
delay(1000)          ← 1 second pause
    ↓
bootLoggerComplete() ← exit boot terminal
    ↓
showInfoPage()       ← display page 10 (info)
    ↓
User navigation...   ← Y/A buttons work normally
```

---

## APPENDIX B: Error Handling Strategy

### During Boot:

**Scenario: Sensor initialization fails**
```
Before:
  setup() calls sensor.begin() → may return false
  No visibility until boot ends

After (with logging):
  [SENSOR] BME280 initialization: FAILED
  [SENSOR] Skipping BME280, continuing...
  Boot continues, user sees the problem on TFT
```

**Scenario: I2C bus scan hangs**
```
Current code: I2C scan is DISABLED (see setup() comment about boot hang)
With boot logging:
  [I2C] Scanning bus... (omitted, disabled for safety)
  [I2C] Bus scan disabled (boot safety)
  Continue to next stage
```

**Scenario: TFT initialization fails**
```
If TFT init fails during setup():
  - bootLoggerInit() called but TFT is dead
  - bootLogPrintf() calls fail silently (safety)
  - Serial logging continues
  - System hangs at drawCurrentPage() (existing behavior)
  - Future: add TFT error recovery
```

---

## APPENDIX C: Pagination Table

| Page | Title | Type | Renderer | Navigation |
|------|-------|------|----------|-----------|
| 0 | BME280 | Sensor | pageBme280() | ← Y \| A → |
| 1 | Gamepad | Sensor | pageGamepad() | ← Y \| A → |
| 2 | INA219 | Sensor | pageIna219() | ← Y \| A → |
| 3 | Encoder | Sensor | pageEncoder() | ← Y \| A → |
| 4 | SCD30 | Sensor | pageScd30() | ← Y \| A → |
| 5 | SI1145 | Sensor | pageSi1145() | ← Y \| A → |
| 6 | TSL2591 | Sensor | pageTsl2591() | ← Y \| A → |
| 7 | VEML6070 | Sensor | pageVeml6070() | ← Y \| A → |
| 8 | VEML6075 | Sensor | pageVeml6075() | ← Y \| A → |
| 9 | DS2484 | Sensor | pageDs2484() | ← Y \| A → |
| **10** | **SYSTEM** | **Info** | **pageSystemInfo()** | **← Y \| A →** |

Note: Page 10 wraps to Page 0 (A button) and from Page 0 to Page 10 (Y button).

---

END OF SPECIFICATION
