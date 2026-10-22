"""
PlatformIO extra script - runs before the build

Automatically updates VERSION and BUILD_DATE in diagnostics.h before compilation.
"""

import os
import subprocess
import sys

try:
    Import("env")
except NameError:
    # Script called directly for testing
    pass

def before_build_callback(source, target, env):
    """Called by PlatformIO before building"""
    
    project_dir = env.get("PROJECT_DIR", os.getcwd())
    script_path = os.path.join(project_dir, "scripts", "update_build_info.py")
    
    print("\n" + "="*70)
    print("🔨 Running pre-build: Updating build information...")
    print("="*70)
    
    try:
        # Use python3 explicitly
        result = subprocess.run(
            ["python3", script_path],
            cwd=project_dir,
            capture_output=True,
            text=True,
            timeout=10
        )
        
        # Print output
        if result.stdout:
            for line in result.stdout.strip().split('\n'):
                print(f"   {line}")
        
        if result.stderr:
            for line in result.stderr.strip().split('\n'):
                print(f"   ⚠️  {line}", file=sys.stderr)
        
        if result.returncode != 0:
            print(f"❌ Pre-build script failed with exit code {result.returncode}")
            env.Exit(1)
            
    except subprocess.TimeoutExpired:
        print("❌ Pre-build script timed out")
        env.Exit(1)
    except FileNotFoundError:
        print(f"⚠️  Python3 not found, skipping version update")
    except Exception as e:
        print(f"❌ Error running pre-build script: {e}")
        env.Exit(1)
    
    print("="*70 + "\n")

# Register the before_build callback with PlatformIO
try:
    env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", before_build_callback)
except NameError:
    # Script not loaded by PlatformIO
    pass
