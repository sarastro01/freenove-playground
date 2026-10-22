#!/bin/bash
# Persistent Serial Monitor Launcher
# Starts and keeps serial_monitor.py running indefinitely
# Usage: ./scripts/start_monitor_persistent.sh &

set +e
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

PORT="/dev/ttyUSB0"
MONITOR="python3 scripts/serial_monitor.py --port $PORT --compact"
LOGDIR="logs/serial"
LOGFILE="$LOGDIR/persistent_monitor.log"

mkdir -p "$LOGDIR"

log_msg() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >> "$LOGFILE"
}

log_msg "=== Starting persistent monitor ==="

# Main loop: keep monitor running
while true; do
    log_msg "Starting monitor process..."
    $MONITOR >> "$LOGDIR/serial_monitor_2026-10-22.log" 2>&1
    EXIT_CODE=$?
    log_msg "Monitor exited with code $EXIT_CODE, restarting in 5 seconds..."
    sleep 5
done
