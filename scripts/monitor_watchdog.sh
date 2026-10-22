#!/bin/bash
# Watchdog script to keep serial_monitor.py running
# Usage: ./scripts/monitor_watchdog.sh &
# Or: nohup ./scripts/monitor_watchdog.sh > nohup.out 2>&1 &

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

MONITOR_SCRIPT="$SCRIPT_DIR/serial_monitor.py"
MONITOR_PORT="/dev/ttyUSB0"
MONITOR_MODE="--compact"
PID_FILE="/tmp/serial_monitor.pid"
LOG_FILE="logs/serial/serial_monitor_watchdog.log"

# Ensure log directory exists
mkdir -p "$(dirname "$LOG_FILE")"

log_message() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOG_FILE"
}

start_monitor() {
    log_message "Starting serial monitor..."
    nohup python3 "$MONITOR_SCRIPT" --port "$MONITOR_PORT" "$MONITOR_MODE" > /dev/null 2>&1 &
    MPID=$!
    echo $MPID > "$PID_FILE"
    log_message "Monitor started with PID $MPID"
}

check_and_restart() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            return 0  # Process is running
        else
            log_message "Monitor (PID $PID) is not running, restarting..."
            start_monitor
            return 1
        fi
    else
        log_message "PID file not found, starting monitor..."
        start_monitor
        return 1
    fi
}

log_message "Watchdog started"

# Main loop: check monitor every 30 seconds
while true; do
    check_and_restart
    sleep 30
done
