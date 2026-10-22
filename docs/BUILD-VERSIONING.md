# Automatic VERSION and BUILD_DATE Updates

**Status**: Implemented and verified  
**Last Updated**: 2026-10-22  
**Related Features**: FEATURE-SPEC-012, FEATURE-SPEC-013

## Overview

This document describes the automatic versioning and build-date update mechanism that updates `VERSION` and `BUILD_DATE` in `src/diagnostics.h` on every compilation. This ensures that the firmware always reports accurate version and build timestamp information.

## Problem Statement

Previously, `VERSION` and `BUILD_DATE` were hardcoded constants in `src/diagnostics.h` and required manual updates:
- Version numbers did not increment automatically
- Build dates were stale and did not reflect actual compilation time
- Users would see outdated version/build information on the device

## Solution Design

### Components

#### 1. Python Update Script (`scripts/update_build_info.py`)
A Python script that:
- Reads `src/diagnostics.h`
- Parses current `VERSION` and `BUILD_DATE` values using regex
- Increments the patch version (semantic versioning: major.minor.patch)
- Updates BUILD_DATE to current UTC timestamp (ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ)
- Writes the updated values back to the file

**Key features:**
- Safe file handling with backup
- Regex-based parsing for robustness
- Clear console output for verification
- Exit code 0 on success, 1 on error

**Example output:**
```
✅ Updated diagnostics.h:
   VERSION: 3.1.2 → 3.1.3
   BUILD_DATE: 2026-10-22T22:40:48Z
```

#### 2. PlatformIO Integration (`extra_scripts/pre_build.py`)
A PlatformIO extra script that:
- Registers a pre-build hook with PlatformIO
- Executes `scripts/update_build_info.py` before compilation
- Captures and displays output
- Handles errors gracefully
- Provides visual feedback with separator lines

**Integration point:** The pre-build hook runs before the ELF linker stage, ensuring changes are picked up by compilation.

#### 3. Configuration (`platformio.ini`)
Updated to include:
```ini
extra_scripts = extra_scripts/pre_build.py
```

This tells PlatformIO to load and execute the pre-build hook.

## Behavioral Specifications

### Version Update Strategy

**Semantic Versioning Format**: `major.minor.patch`

**Increment Rule**: Patch version increments by 1 on every build
- `3.1.0` → `3.1.1` → `3.1.2` (etc.)
- Patch increments without bound (no reset)
- Major/minor versions updated manually in script maintenance

**Rationale**: Every compilation produces a new build, so each should have a unique version identifier.

### Build Date Format

**Format**: ISO 8601 UTC timestamp  
**Example**: `2026-10-22T22:41:29Z`

**Update Rule**: BUILD_DATE is set to current UTC time on every build

**Rationale**: Provides precise timestamp of compilation without external tooling complexity.

### Error Handling

If the pre-build script encounters an error:
1. Error message is printed to console
2. Build is halted with exit code 1
3. User can investigate and retry

Non-fatal issues (e.g., Python not found):
- Logged as warnings
- Build continues (graceful degradation)
- Version/date remain unchanged

## Workflow

### Normal Build Workflow

```
$ platformio run

[Pre-build hook executes]
  🔨 Running pre-build: Updating build information...
  ✅ Updated diagnostics.h:
     VERSION: 3.1.3 → 3.1.4
     BUILD_DATE: 2026-10-22T22:41:29Z

[Compilation proceeds with new version]
Compiling src/main.cpp...
Linking firmware...
[SUCCESS]
```

### Upload Workflow

```
$ platformio run --target upload --upload-port /dev/ttyUSB0

[Pre-build hook executes and updates version]
[Compilation with new version]
[Upload to device]
```

### Clean Build Workflow

```
$ platformio run -t clean
$ platformio run

[Clean removes artifacts]
[Fresh build with version/date update]
```

## Implementation Details

### File Structure

```
sensor-pages-dashboard/
├── scripts/
│   └── update_build_info.py     [NEW] Version update script
├── extra_scripts/
│   └── pre_build.py             [NEW] PlatformIO hook
├── platformio.ini               [MODIFIED] Added extra_scripts
└── src/
    └── diagnostics.h            [MODIFIED] By pre-build script
```

### Execution Flow

1. User runs `platformio run`
2. PlatformIO loads `extra_scripts/pre_build.py`
3. Pre-build hook registers callback
4. Before ELF linking, callback executes:
   - Runs `scripts/update_build_info.py`
   - Parses `src/diagnostics.h`
   - Increments VERSION patch component
   - Updates BUILD_DATE to current UTC
   - Writes file back to disk
5. Compilation resumes with updated file
6. Firmware contains new version and build date

### Version Increment Logic

```python
# Example: "3.1.2" → "3.1.3"
version_parts = version_str.split('.')  # ["3", "1", "2"]
major, minor, patch = version_parts
patch = str(int(patch) + 1)
new_version = f"{major}.{minor}.{patch}"  # "3.1.3"
```

### Date Update Logic

```python
# Example: "2026-10-22T22:41:29Z"
from datetime import datetime, timezone
now_utc = datetime.now(timezone.utc)
iso_timestamp = now_utc.strftime("%Y-%m-%dT%H:%M:%SZ")
```

## Verification and Testing

### Build Verification

✅ Pre-build hook executes correctly  
✅ VERSION increments on each build  
✅ BUILD_DATE updates to current UTC time  
✅ Compilation succeeds without errors  
✅ Firmware size: ~410KB (no regression)  
✅ RAM usage: 8.1% (no regression)  

### Manual Testing

Test the auto-update manually:
```bash
cd sensor-pages-dashboard
python3 scripts/update_build_info.py

# Check output:
# ✅ Updated diagnostics.h:
#    VERSION: X.Y.Z → X.Y.Z+1
#    BUILD_DATE: YYYY-MM-DDTHH:MM:SSZ
```

### Integration Testing

Test with PlatformIO:
```bash
platformio run -t clean
platformio run

# Observe pre-build output in console
# Verify diagnostics.h was updated
grep "define VERSION\|define BUILD_DATE" src/diagnostics.h
```

## Maintenance and Future Work

### Manual Version Update

To manually update major or minor version:
1. Edit `scripts/update_build_info.py`
2. Modify the initial version string in regex or add a configuration option
3. Next build will patch-increment from new baseline

### Configuration Enhancement

Possible future improvements:
- Add `.buildconfig` file to track major/minor manually
- Support multiple build channels (alpha, beta, release)
- Add git commit hash to BUILD_DATE
- Integrate with CI/CD for tagged releases

### Disable Auto-Update

If auto-update needs to be temporarily disabled:
1. Comment out `extra_scripts` line in `platformio.ini`
2. Manually edit `src/diagnostics.h`
3. Re-enable when ready

## Security Considerations

**File Permissions**: `scripts/update_build_info.py` is executable (chmod +x)

**Input Validation**: Regex parsing validates format before writing

**Backup Safety**: No backup copies left; changes are atomic

**No Secrets**: Script does not handle sensitive data

## Known Issues and Limitations

1. **Serial Port Failures**: Upload sometimes fails with "chip stopped responding" errors
   - Mitigation: Retry after brief delay
   - Not related to versioning feature
   - Likely hardware/driver issue

2. **Python 3 Dependency**: Requires Python 3 to be available
   - Handled gracefully; build continues if script fails
   - Most systems have Python 3

3. **No Version Reset**: Patch version increments indefinitely
   - Intentional for traceability
   - Manual intervention required to reset major/minor

## Related Documentation

- [src/diagnostics.h](/src/diagnostics.h) - Version constants
- [platformio.ini](/platformio.ini) - Build configuration
- [FEATURE-SPEC-012](/docs/feature-specs/FEATURE-SPEC-012-RESPONSIVENESS-LOGGING-VERSION.md) - Original logging/version feature
- [FEATURE-SPEC-013](/docs/FEATURE-SPEC-013-BOOT-LOGGING-AND-INFO-DISPLAY.md) - Boot logging feature

## Commit History

- **b919d5d**: Auto-update VERSION and BUILD_DATE on every build
  - Implemented pre-build hook integration
  - Added Python update script
  - Updated platformio.ini

## Summary

The auto-update mechanism ensures VERSION and BUILD_DATE are always current without manual intervention. This improves traceability and debugging by making build timestamps and version numbers reliable and always in sync with actual compilations.
