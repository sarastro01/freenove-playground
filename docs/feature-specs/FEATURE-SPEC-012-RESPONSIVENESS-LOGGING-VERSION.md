# FEATURE-SPEC-012: Responsiveness, Enhanced Logging, Display Diagnostics & Versioning

**Status:** Design Phase (Awaiting Decisions)  
**Created:** 2026-10-22  
**Version:** 1.0 (Draft)  
**Feature ID:** FEATURE-SPEC-012  
**Related Specs:** FEATURE-SPEC-007, FEATURE-SPEC-009, FEATURE-SPEC-010, FEATURE-SPEC-011

---

## Executive Summary

This feature addresses four interconnected requirements to improve system responsiveness, diagnostics, and operational transparency:

1. **Eliminate blocking delay() calls from loop()** to achieve <100ms input response
2. **Add timestamped logging** ([HH:MM:SS.sss] format) for improved diagnostics
3. **Investigate and log spontaneous display switches** and encoder/gamepad activity
4. **Embed version and build information** into firmware with boot-time display

---

## 1. Functional Behavior

### 1.1 Responsiveness: Eliminate delay() from loop()

**Current State:**
- `delay()` found at 4 locations in codebase:
  - Line 1273: `delay(850)` in DS18B20 conversion wait (DS2484 sensor read)
  - Line 2466: `delay(3000)` in I2C scan display
  - Line 2590: `delay(100)` in Serial initialization
  - Line 2648: `delay(1000)` in setup() after initialization

**Functional Requirement FR-RESP-001:**
- No `delay()` calls shall execute within `loop()` function (line 2661+)
- Input response time (button press to action) must be <100 ms
- Current architecture uses time-based slot scheduling (SENSOR_SLOT_INTERVAL_MS = 200 ms)
- Gamepad/Encoder polling runs every INPUT_REFRESH_MS = 10 ms

**Detailed Analysis:**

| Location | Function | Duration | Context | Issue | Resolution |
|----------|----------|----------|---------|-------|-----------|
| Line 1273 | readDs18b20Async() | 850 ms | Sensor read (async) | NOT in loop() | **NO CHANGE** |
| Line 2466 | performI2cScan() | 3000 ms | Setup phase | NOT in loop() | **NO CHANGE** |
| Line 2590 | setup() → wait for serial | 100 ms | Setup phase | NOT in loop() | **NO CHANGE** |
| Line 2648 | setup() → post-init wait | 1000 ms | Setup phase | NOT in loop() | **NO CHANGE** |

**Finding:** No `delay()` calls currently execute within `loop()` function. System already responsive.

**Validation:** Add static analysis check in CI/CD to catch future delay() additions to loop().

---

### 1.2 Enhanced Logging with Timestamps

**Current State:**
- Serial.printf() used throughout codebase
- No consistent timestamp format
- Log messages lack temporal correlation

**Functional Requirement FR-LOG-001:**
- Every Serial output must prepend timestamp format: `[HH:MM:SS.sss]`
- Examples:
  ```
  [00:00:15.342] [DS2484] Found sensor 1: Family=28...
  [00:00:15.412] [INA219] Reading voltage: 5.087 V
  [00:01:23.801] [GAMEPAD] Button Y pressed
  ```

**Implementation Pattern:**
- Create wrapper function: `void logPrintf(const char* format, ...)`
  - Gets current time from `millis()` since boot
  - Converts to HH:MM:SS.sss format
  - Prepends timestamp to output
  - Calls Serial.printf() with formatted string

**Time Calculation:**
- `millis()` returns ms since boot
- Convert to HH:MM:SS.sss:
  ```
  uint32_t ms = millis();
  uint8_t hours = (ms / 3600000) % 24;
  uint8_t minutes = (ms / 60000) % 60;
  uint8_t seconds = (ms / 1000) % 60;
  uint16_t milliseconds = ms % 1000;
  // Format: [HH:MM:SS.sss]
  ```

**Performance Impact Analysis FR-LOG-002:**

| Metric | Current | With Logging | Impact | Notes |
|--------|---------|--------------|--------|-------|
| Serial output rate | ~50-100 msg/min | ~50-100 msg/min | **Minimal** | No additional messages |
| Per-message overhead | 0 μs | ~50-100 μs | **Low** | Time calculation + formatting |
| Memory (stack) | Baseline | +~50 bytes | **Negligible** | Temporary format buffer |
| Memory (code) | Baseline | +~500 bytes | **Minor** | logPrintf() function |
| Loop cycle time | ~100-200 ms | ~100-200 ms | **None** | Logging not on loop critical path |
| Boot time | ~5 seconds | ~5 seconds | **None** | Logging only during operation |

**Potential Issues:**

| Issue | Severity | Mitigation |
|-------|----------|-----------|
| Serial output becomes bottleneck if high volume | LOW | Baud rate: 115200 (~14.4 KB/s sufficient for expected log rate) |
| Formatting time on performance sensors | MEDIUM | Use fast time calculation; avoid floating point |
| Memory on limited heap | LOW | Heap = 4 MB; ~500 bytes negligible |
| Float formatting in printf | HIGH | Use integer-only arithmetic for timestamps |

---

### 1.3 Display Spontaneous Switch Investigation & Logging

**Current Symptom:**
- Display switches pages without user interaction
- Occurs spontaneously, pattern not obvious
- Encoder and Gamepad not visibly activated

**Functional Requirement FR-DISP-001:**
- Log every `currentPage` assignment with reason
- Log every encoder rotation event
- Log every gamepad button press/release
- Track Large Display Mode entry/exit

**Logging Points (Display Switches):**

```cpp
// Line 2721 (Gamepad Button Y) → Previous page
[HH:MM:SS.sss] [INPUT] GAMEPAD Button Y pressed (change: page 2 → page 1)

// Line 2729 (Gamepad Button A) → Next page
[HH:MM:SS.sss] [INPUT] GAMEPAD Button A pressed (change: page 2 → page 3)

// Encoder (FEATURE-SPEC-011) → Page navigation
[HH:MM:SS.sss] [INPUT] ENCODER rotation CLOCKWISE (delta: +1, change: page 2 → page 3)
[HH:MM:SS.sss] [INPUT] ENCODER rotation COUNTER-CLOCKWISE (delta: -1, change: page 2 → page 1)

// Large Display Mode entry/exit
[HH:MM:SS.sss] [MODE] Large Display enter (page 2, valueIdx 0)
[HH:MM:SS.sss] [MODE] Large Display exit (back to page 2)

// Gamepad Button B (Large Display cycle forward)
[HH:MM:SS.sss] [INPUT] GAMEPAD Button B pressed (Large Display: valueIdx 0 → 1, page 2)

// Gamepad Button X (Large Display cycle backward)
[HH:MM:SS.sss] [INPUT] GAMEPAD Button X pressed (Large Display: valueIdx 1 → 0, page 2)
```

**Logging Points (Encoder Activity):**

```cpp
// Boot
[HH:MM:SS.sss] [ENCODER] Initialized (addr: 0x36, LED: RGB NeoPixel)

// Every poll cycle (or when delta != 0)
[HH:MM:SS.sss] [ENCODER] Poll: position=142, button=0, battery=255

// When rotation detected
[HH:MM:SS.sss] [ENCODER] Rotation: position 142 → 143 (delta: +1)

// When button state changes
[HH:MM:SS.sss] [ENCODER] Button: PRESSED (was: 0, now: 1)
[HH:MM:SS.sss] [ENCODER] Button: RELEASED (was: 1, now: 0)
```

**Logging Points (Gamepad Activity):**

```cpp
// Boot
[HH:MM:SS.sss] [GAMEPAD] Initialized (addr: 0x50, polling: 40 Hz)

// Button state changes (not every poll, only when state changes)
[HH:MM:SS.sss] [GAMEPAD] Button: Y=PRESSED (was: 0, now: 1)
[HH:MM:SS.sss] [GAMEPAD] Button: Y=RELEASED (was: 1, now: 0)
[HH:MM:SS.sss] [GAMEPAD] Button: A=PRESSED (was: 0, now: 1)
[HH:MM:SS.sss] [GAMEPAD] Button: B=PRESSED (was: 0, now: 1)
[HH:MM:SS.sss] [GAMEPAD] Button: X=PRESSED (was: 0, now: 1)
```

**Analysis Strategy:**
- Collect 24-hour log of display switches
- Correlate with encoder/gamepad logs
- Hypotheses:
  1. **Encoder electrical noise** → stray rotations from capacitive coupling
  2. **Gamepad phantom events** → I2C read errors causing false button states
  3. **Timer overflow** → if using signed comparisons (e.g., `(int32_t)(now - nextMs)`)
  4. **Interrupt-driven input** → seesaw encoder library might have interrupt handler

---

### 1.4 Version and Build Information

**Functional Requirement FR-VER-001:**
- Embed semantic version number in firmware
- Record build timestamp
- Display on boot via Serial output
- Make accessible via API or special display page (optional)

**Version Format:**
- Semantic versioning: `MAJOR.MINOR.PATCH`
- Example: `3.1.0` (after merging documentation sync PR #1)
- Updated during release builds (via build script or platform.ini)

**Build Information:**
- Build timestamp: ISO 8601 format `YYYY-MM-DDTHH:MM:SSZ`
- Compiler: `arm-none-eabi-g++` (Arduino/PlatformIO default)
- ESP32 variant: Detect from `ESP.getEfuseMac()` or `sdkconfig.h`

**Boot Output Example:**
```
[00:00:00.000] ========================================
[00:00:00.001] Sensor Pages Dashboard
[00:00:00.002] Version: 3.1.0
[00:00:00.003] Build Date: 2026-10-22T16:54:00Z
[00:00:00.004] Compiler: arm-none-eabi-g++
[00:00:00.005] ESP32-WROOM-32 Detected
[00:00:00.006] ========================================
```

**Implementation Options:**

| Option | Mechanism | Pros | Cons |
|--------|-----------|------|------|
| A) Hardcoded | `#define VERSION "3.1.0"` in header | Simple, no build deps | Manual update required |
| B) Build script | Generate header via build script | Automated, dynamic | Adds complexity |
| C) Git-based | Read from git tags/commits | Single source of truth | Requires git at compile time |
| D) CI/CD injected | GitHub Actions writes version | Best automation | Not applicable (local build) |

**Recommended:** Option A (hardcoded) with Option C (git info at build time via script)

---

## 2. Files to Be Updated

### 2.1 Source Files

| File | Changes | Scope |
|------|---------|-------|
| `src/main.cpp` | Add logPrintf() wrapper; replace Serial.printf() calls | Full file (~2850 lines) |
| `src/diagnostics.h` | Add version constants; enhance boot logging | Add ~20 lines |
| `src/main.cpp` (loop section) | Add input logging; display switch logging | Lines 2661+ |
| `src/sensors/EncoderSensor.cpp` | Add encoder rotation/button logging | Full file (~271 lines) |
| `src/sensors/GamepadSensor.cpp` | Add gamepad activity logging | Full file (~249 lines) |

### 2.2 Configuration Files

| File | Changes | Scope |
|------|---------|-------|
| `platformio.ini` | Add build info flags (optional) | ~50 lines |
| `.github/workflows/build.yml` | Inject version/date (if used) | ~80 lines |

### 2.3 Documentation Files

| File | Changes | Scope |
|------|---------|-------|
| `docs/02-architecture.md` | Section on logging subsystem | ~50-80 lines |
| `docs/feature-specs/FEATURE-SPEC-012-...md` | This specification | ~500 lines |

---

## 3. Validation Rules

### 3.1 Responsiveness (FR-RESP-001)

| Rule | Validation Method |
|------|------------------|
| VR-RESP-001: No delay() in loop() | Static code scan; grep -n "delay(" src/main.cpp verify all post-loop() |
| VR-RESP-002: Button → action <100 ms | Measure from Serial log timestamps; encoder/gamepad press to page change |
| VR-RESP-003: Loop cycle <200 ms | Monitor elapsed time per slot; verify no accumulation |

### 3.2 Logging (FR-LOG-001, FR-LOG-002)

| Rule | Validation Method |
|------|------------------|
| VR-LOG-001: All Serial output has timestamp | Regex match on log: `^\[(\d{2}):(\d{2}):(\d{2})\.(\d{3})\]` |
| VR-LOG-002: Timestamp format valid | Verify HH [00-23], MM [00-59], SS [00-59], sss [000-999] |
| VR-LOG-003: Time monotonic | timestamps always increase or equal to previous |
| VR-LOG-004: Performance overhead <10% | Measure loop cycle time before/after; should be negligible |
| VR-LOG-005: No heap leaks from logging | Monitor heap every 10 sec; should remain stable |

### 3.3 Display Diagnostics (FR-DISP-001)

| Rule | Validation Method |
|------|------------------|
| VR-DISP-001: Page change logged with reason | Every line "change: page X → Y" has cause (GAMEPAD/ENCODER/AUTO) |
| VR-DISP-002: Encoder activity logged | Grep for "\[ENCODER\]" in log; verify rotation/button events |
| VR-DISP-003: Gamepad activity logged | Grep for "\[GAMEPAD\]" in log; verify button presses |
| VR-DISP-004: 24-hour log reproducible | Run 24 hours; correlate spontaneous switches with encoder/gamepad |

### 3.4 Versioning (FR-VER-001)

| Rule | Validation Method |
|------|------------------|
| VR-VER-001: Version number present in code | Grep for `VERSION` constant; must match SemVer pattern |
| VR-VER-002: Build date present | Grep for `BUILD_DATE` constant; must be ISO 8601 |
| VR-VER-003: Boot output includes version | Serial monitor first output shows version/date |
| VR-VER-004: Version matches git tag (optional) | git describe should resolve to same version |

---

## 4. Authorization & Security

### 4.1 No Authorization Required
- All changes are local/firmware-internal
- No external API calls or authentication
- Serial logging is developer-facing (USB connection only)

### 4.2 Security Considerations
- Timestamps leak uptime information (minor info leakage)
- Serial output unencrypted (not a concern for local USB)
- No new attack surface introduced

---

## 5. Error Cases

### 5.1 Logging Errors

| Error | Cause | Handling |
|-------|-------|----------|
| Serial output buffer full | Rapid logging from multiple sources | Rate-limit non-critical logs OR increase Serial buffer |
| Invalid millis() on overflow | millis() rolls over every ~50 days | Handle via modulo; not critical for <1 week operation |
| Formatter stack overflow | Very long format strings | Limit format string length; use fixed buffers |

### 5.2 Display Diagnosis Errors

| Error | Cause | Handling |
|-------|-------|----------|
| False positives from noise | Electrical noise on encoder | Add hysteresis; require 2 consecutive deltas in same direction |
| Button debounce failures | I2C read glitches | Log raw button state changes; analyze for patterns |
| Log buffer exhaustion | 24-hour capture fills storage | Use ring buffer; overwrite oldest entries |

### 5.3 Version Errors

| Error | Cause | Handling |
|-------|-------|----------|
| Version mismatch after merge | Manual update forgotten | CI/CD validation to block commits without version bump |
| Build date in future | Clock skew in CI/CD | Use UTC; validate within ±1 hour of CI/CD server time |
| Version format invalid | Typo in SemVer | Regex validation in code; compilation warning if invalid |

---

## 6. Data Changes

### 6.1 Global State Additions

| Variable | Type | Purpose | Scope |
|----------|------|---------|-------|
| `VERSION` | const char* | Firmware version string | diagnostics.h (compile-time) |
| `BUILD_DATE` | const char* | Build timestamp ISO 8601 | diagnostics.h (compile-time) |
| `bootTimestampMs` | uint32_t | Recorded at boot for reference | main.cpp (runtime) |

### 6.2 No Database or File Changes
- No new files created on SD card
- No EEPROM modifications
- All changes in RAM or code

---

## 7. API Changes

### 7.1 New Function

```cpp
// Wrapper for timestamped logging
void logPrintf(const char* format, ...);
// Usage: logPrintf("[GAMEPAD] Button Y pressed\n");
// Output: [HH:MM:SS.sss] [GAMEPAD] Button Y pressed
```

### 7.2 Modified Serial Output
- All existing `Serial.printf()` calls replaced with `logPrintf()` (internal change)
- No external API affected
- Output format changed (timestamps added) - may break log parsing tools

---

## 8. UI Changes

### 8.1 Serial Monitor Output

**Before:**
```
SCD30 read time: 8234 µs
[DS2484] Found sensor 1: Family=28...
[INA219] Reading voltage: 5.087 V
```

**After:**
```
[00:00:15.234] SCD30 read time: 8234 µs
[00:00:15.242] [DS2484] Found sensor 1: Family=28...
[00:00:15.301] [INA219] Reading voltage: 5.087 V
```

### 8.2 Boot Sequence
- New boot banner with version/date (5-6 lines)
- Should display within first 100 ms after power-on
- Format: ASCII box drawing or plain text

### 8.3 TFT Display
- No changes (all diagnostics via Serial)
- Spontaneous switch diagnosis does NOT appear on TFT
- Optional: Add version/build info to "System Info" page (future)

---

## 9. Acceptance Criteria

### AC-1: Responsiveness (FR-RESP-001)
```
✓ Static analysis confirms: no delay() in loop() function
✓ Button press → page change latency <100 ms (measured via timestamps)
✓ Loop cycle time stable ~200 ms (no accumulation)
```

### AC-2: Logging (FR-LOG-001, FR-LOG-002)
```
✓ Every Serial line starts with timestamp [HH:MM:SS.sss]
✓ Timestamp format valid (regex passes all samples)
✓ Loop cycle time with logging vs without <5% difference
✓ Heap memory stable during 1-hour logging run
✓ Serial output readable at 115200 baud (no corruption)
```

### AC-3: Display Diagnostics (FR-DISP-001)
```
✓ Every page change logged with source (GAMEPAD/ENCODER/AUTO) and before→after pages
✓ Encoder activity logged: rotations and button presses
✓ Gamepad activity logged: button state changes
✓ 24-hour test run captures no unexplained page switches OR
  → All switches correlate to encoder/gamepad logs
```

### AC-4: Versioning (FR-VER-001)
```
✓ Version string present in code and matches git tag
✓ Build date accurate to within ±5 minutes of actual build time
✓ Boot output displays version and build date
✓ Version incremented after each feature merge (manual or CI/CD)
```

---

## 10. Open Questions

### Q1: Logging Volume & Baud Rate
- **Question:** Will ~100 logs/min at 115200 baud cause bottleneck or data loss?
- **Impact:** Determines if additional buffering or rate-limiting needed
- **Recommendation:** Test with actual baud rate; profile Serial write times

### Q2: Display Spontaneous Switch Root Cause
- **Question:** Is this electrical noise, I2C glitch, or firmware bug?
- **Impact:** Solution depends on root cause
- **Recommendation:** Implement FR-DISP-001 logging first; analyze 24-hour capture

### Q3: Encoder vs Gamepad Redundancy
- **Question:** Should both encoder and gamepad remain functional simultaneously?
- **Current Decision (from FEATURE-SPEC-011):** Yes, full redundancy model
- **Impact:** More complex logic, but more reliable
- **Verification Needed:** Confirm user intent (is dual input expected?)

### Q4: Version Number Semantics
- **Question:** What triggers MAJOR/MINOR/PATCH bump?
  - MAJOR: Breaking API changes?
  - MINOR: New features (like this one)?
  - PATCH: Bug fixes?
- **Current Assumption:** This is MINOR bump (3.1.0 after 3.0.0)
- **Recommendation:** Define versioning policy in README

### Q5: Performance Logging Overhead - Practical Impact
- **Question:** How many Serial.printf() calls per second in steady state?
- **Current Estimate:** ~50-100 calls/min (sensor reads + occasional input)
- **Impact:** Low overhead expected, but needs measurement
- **Recommendation:** Add timestamp wrapper; profile before/after; report results

### Q6: 24-Hour Logging Capture Method
- **Question:** How to capture 24-hour log reliably?
  - Serial monitor → file (requires terminal open)
  - SD card logging (not currently supported)
  - Memory ring buffer (limited by heap)
- **Impact:** Different approaches have different trade-offs
- **Recommendation:** Use terminal redirect initially; implement SD card logging if needed

### Q7: Large Display Mode & Display Switches
- **Question:** Should entering Large Display Mode be logged as a "display switch"?
- **Current Design:** Yes (separate from page change logs)
- **Impact:** More granular diagnostics but more verbose
- **Recommendation:** Keep separate for clarity

### Q8: Encoder Position Wrapping (FEATURE-SPEC-011 interaction)
- **Question:** How is encoder.position value managed across boots?
  - Does it persist in seesaw EEPROM?
  - Or reset to 0 at boot?
- **Impact:** Affects predictability of page navigation via encoder
- **Recommendation:** Investigate seesaw library behavior; document in implementation

---

## 11. Implementation Notes

### 11.1 Timestamp Calculation Optimization
```cpp
// Avoid floating point; use integer-only arithmetic
inline void formatTimestamp(char* buffer, size_t buflen) {
  uint32_t ms = millis();
  uint8_t hours = (ms / 3600000) % 24;
  uint8_t minutes = (ms / 60000) % 60;
  uint8_t seconds = (ms / 1000) % 60;
  uint16_t milliseconds = ms % 1000;
  snprintf(buffer, buflen, "[%02u:%02u:%02u.%03u]", 
           hours, minutes, seconds, milliseconds);
}
```

### 11.2 logPrintf Implementation Pattern
```cpp
void logPrintf(const char* format, ...) {
  char timestamp[14];  // "[HH:MM:SS.sss]" = 13 chars + null
  formatTimestamp(timestamp, sizeof(timestamp));
  
  char buffer[256];  // Limit total message size
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  Serial.print(timestamp);
  Serial.print(" ");
  Serial.print(buffer);
}
```

### 11.3 Logging Categorization
- `[BOOT]` - Startup messages
- `[DIAG]` - Diagnostics (heap, timing)
- `[SENSOR]` - Sensor reads and timing
- `[INPUT]` - User input (gamepad, encoder)
- `[MODE]` - Display mode changes
- `[I2C]` - I2C communication
- `[ERROR]` - Error conditions
- `[VERSION]` - Version/build info

### 11.4 Search Strategy for Spontaneous Switches
1. Capture 24-hour log to file
2. Extract all lines with "currentPage" or "page change"
3. Filter for changes NOT preceded by encoder/gamepad logs
4. Analyze timing of "orphan" changes
5. Look for patterns: clock-based, random, or correlated with other events

---

## 12. Testing Strategy

### 12.1 Unit Tests (Not Applicable)
- No discrete units to test (all integrated in loop)

### 12.2 Integration Tests
- **Test Responsiveness:**
  - Measure latency: press encoder → log "rotation" → log "page change"
  - Should be <50 ms between events
  
- **Test Logging:**
  - Capture 1-hour log
  - Verify every line has valid timestamp
  - Verify timestamps monotonic (never decrease)
  
- **Test Versioning:**
  - Build firmware
  - Open Serial monitor
  - Verify version banner appears in first 100 ms

### 12.3 System Tests
- **24-hour Stability Test:**
  - Run firmware 24 hours
  - Capture all Serial output
  - Monitor heap, no manual input
  - Verify no spontaneous page switches
  - Verify log file is well-formed
  
- **Input Stress Test:**
  - Rapidly press encoder/gamepad buttons
  - Verify every press logged
  - Verify page navigation responsive
  - Verify no missed events

### 12.4 Performance Tests
- **Serial Throughput:**
  - Measure time to output 1000 timestamped lines
  - Verify no data loss at 115200 baud
  
- **Memory Profiling:**
  - Monitor heap before/after logging enabled
  - Should be stable (no leaks)
  - Should be <1 MB additional usage

---

## 13. Deployment & Release

### 13.1 Release Checklist
- [ ] All 4 requirements implemented
- [ ] Code compiles with zero warnings
- [ ] Responsiveness validation passed (no delay in loop)
- [ ] Logging validation passed (timestamps on all output)
- [ ] 24-hour display diagnostics run completed (report spontaneous switches)
- [ ] Version number updated to 3.1.0
- [ ] Build date set correctly
- [ ] Boot output tested on hardware
- [ ] Serial output tested at 115200 baud
- [ ] Documentation updated (feature-spec, architecture, API)
- [ ] Pull request created and reviewed
- [ ] Merged to master

### 13.2 Rollback Plan
If issues discovered post-deployment:
- Revert to previous version via `git revert <commit>`
- Previous working version: commit 936a468 (after documentation sync)
- Recovery time: <5 minutes (rebuild and upload)

---

## 14. Success Metrics

| Metric | Target | Success Criteria |
|--------|--------|-----------------|
| Button response time | <100 ms | Measured from log timestamps |
| Log readability | 100% | All timestamps valid format |
| Diagnostic coverage | 100% | Every page switch has logged cause |
| Spontaneous switches | 0 | After 24-hour run, if unexplained: investigation required |
| Version accuracy | ±5 min | Build date within tolerance |
| Performance impact | <5% | Loop cycle time change |
| Memory overhead | <1 MB | Heap growth during operation |
| Baud rate stability | 0 errors | Serial output at 115200 no corruption |

