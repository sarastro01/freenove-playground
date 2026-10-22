#!/bin/bash
# Quick wrapper for Python serial monitor
# Usage: ./scripts/monitor.sh [--compact] [--search PATTERN] [--port PORT]

cd "$(dirname "$0")/.." || exit 1
python3 scripts/serial_monitor.py "$@"
