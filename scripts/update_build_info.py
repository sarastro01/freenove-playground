#!/usr/bin/env python3
"""
Auto-update VERSION and BUILD_DATE in diagnostics.h

This script is called by PlatformIO before each build to automatically update:
1. BUILD_DATE to current UTC timestamp (ISO 8601 format)
2. VERSION by incrementing the patch version

Usage: python3 scripts/update_build_info.py
"""

import os
import re
from datetime import datetime, timezone
from pathlib import Path

def parse_version(version_str):
    """Parse semantic version string (major.minor.patch)"""
    parts = version_str.split('.')
    if len(parts) != 3:
        raise ValueError(f"Invalid version format: {version_str}")
    return tuple(int(p) for p in parts)

def format_version(major, minor, patch):
    """Format semantic version tuple"""
    return f"{major}.{minor}.{patch}"

def increment_patch_version(version_str):
    """Increment patch version (e.g., 3.1.0 -> 3.1.1)"""
    major, minor, patch = parse_version(version_str)
    patch += 1
    return format_version(major, minor, patch)

def get_current_build_date():
    """Get current timestamp in ISO 8601 format (UTC)"""
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

def update_diagnostics_h(script_dir):
    """Update VERSION and BUILD_DATE in diagnostics.h"""
    
    # Find diagnostics.h
    diagnostics_path = Path(script_dir).parent / "src" / "diagnostics.h"
    
    if not diagnostics_path.exists():
        print(f"❌ Error: {diagnostics_path} not found")
        return False
    
    # Read current content
    with open(diagnostics_path, 'r') as f:
        content = f.read()
    
    # Extract current VERSION
    version_match = re.search(r'#define VERSION "([^"]+)"', content)
    if not version_match:
        print("❌ Error: VERSION not found in diagnostics.h")
        return False
    
    current_version = version_match.group(1)
    new_version = increment_patch_version(current_version)
    
    # Get new BUILD_DATE
    new_build_date = get_current_build_date()
    
    # Update VERSION
    content = re.sub(
        r'#define VERSION "[^"]+"',
        f'#define VERSION "{new_version}"',
        content
    )
    
    # Update BUILD_DATE
    content = re.sub(
        r'#define BUILD_DATE "[^"]+"',
        f'#define BUILD_DATE "{new_build_date}"',
        content
    )
    
    # Write updated content
    with open(diagnostics_path, 'w') as f:
        f.write(content)
    
    print(f"✅ Updated diagnostics.h:")
    print(f"   VERSION: {current_version} → {new_version}")
    print(f"   BUILD_DATE: {new_build_date}")
    
    return True

def main():
    """Main entry point"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    if update_diagnostics_h(script_dir):
        return 0
    else:
        return 1

if __name__ == "__main__":
    exit(main())
