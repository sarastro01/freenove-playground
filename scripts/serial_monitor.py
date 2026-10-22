#!/usr/bin/env python3
"""
Persistent Serial Monitor for Sensor Pages Dashboard
Captures firmware output with timestamps, categorizes I2C errors, and logs to disk.
Version: 3.1.0
Build: 2026-10-22
FEATURE-SPEC-012 Implementation
"""

import serial
import sys
import os
import time
import re
from datetime import datetime
from pathlib import Path

class SerialMonitor:
    """Monitor serial port, log to disk, categorize I2C errors, provide search."""
    
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200, compact=False):
        self.port = port
        self.baudrate = baudrate
        self.compact = compact
        self.ser = None
        self.line_count = 0
        self.error_count = 0
        self.i2c_error_count = 0
        self.slow_i2c_count = 0
        self.log_file = None
        self.log_path = Path('logs/serial')
        self.log_path.mkdir(parents=True, exist_ok=True)
        self.current_log_date = None
        self.setup_log_file()
    
    def setup_log_file(self):
        """Create or reopen log file with today's date."""
        today = datetime.now().strftime('%Y-%m-%d')
        if self.current_log_date != today:
            if self.log_file:
                self.log_file.close()
            log_filename = self.log_path / f'serial_monitor_{today}.log'
            self.log_file = open(log_filename, 'a', buffering=1)
            self.current_log_date = today
    
    def get_i2c_severity(self, line):
        """Categorize I2C timing line: OK, SLOW, BUS_ERROR, or CRITICAL."""
        if 'Error 263' in line or 'ERROR' in line:
            return 'CRITICAL'
        
        if '[I2C-TIMING]' not in line:
            return None
        
        # Extract time, errors, bus status
        time_match = re.search(r'time:(\d+)\s*ms', line)
        errors_match = re.search(r'errors:(\d+)', line)
        bus_match = re.search(r'bus:(\w+)', line)
        
        if time_match and errors_match:
            time_ms = int(time_match.group(1))
            errors = int(errors_match.group(1))
            bus_status = bus_match.group(1) if bus_match else 'unknown'
            
            # Categorize
            if 'locked' in bus_status:
                return 'CRITICAL' if time_ms >= 100 else 'BUS_ERROR'
            elif errors > 0:
                return 'BUS_ERROR'
            elif time_ms >= 50:
                return 'SLOW'
            else:
                return 'OK'
        
        return None
    
    def format_line(self, raw_line):
        """Format line with timestamp and categorize severity."""
        timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
        formatted = f'[{timestamp}] {raw_line}'
        
        severity = self.get_i2c_severity(raw_line)
        
        # Return formatted line and severity
        return formatted, severity
    
    def apply_color(self, line, severity):
        """Apply ANSI color codes based on severity."""
        if severity == 'CRITICAL':
            return f'\033[1;31m{line}\033[0m'  # Bold red
        elif severity == 'BUS_ERROR':
            return f'\033[31m{line}\033[0m'    # Red
        elif severity == 'SLOW':
            return f'\033[33m{line}\033[0m'    # Yellow
        elif severity == 'OK':
            return f'\033[34m{line}\033[0m'    # Blue
        else:
            return line                         # No color
    
    def should_display(self, severity):
        """Determine if line should be displayed based on mode."""
        if not self.compact:
            return True  # Normal mode: show all
        
        # Compact mode: show only critical, slow, and bus errors
        return severity in ['CRITICAL', 'BUS_ERROR', 'SLOW']
    
    def update_statistics(self, raw_line, severity):
        """Update running statistics."""
        self.line_count += 1
        
        if 'ERROR' in raw_line or 'Error' in raw_line:
            self.error_count += 1
        
        if severity == 'BUS_ERROR':
            self.i2c_error_count += 1
        elif severity == 'SLOW':
            self.slow_i2c_count += 1
    
    def connect(self):
        """Connect to serial port."""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f'✓ Connected to {self.port}')
            return True
        except serial.SerialException as e:
            print(f'✗ Failed to connect to {self.port}: {e}')
            return False
    
    def run(self):
        """Main monitoring loop."""
        if not self.connect():
            return False
        
        mode = 'COMPACT MODE' if self.compact else 'NORMAL MODE'
        print(f'\n🔌 Serial Monitor ({mode})')
        print(f'   Port: {self.port}')
        print(f'   Baudrate: {self.baudrate}')
        print(f'   Log directory: {self.log_path}')
        print(f'   Log file: {self.log_path / f"serial_monitor_{datetime.now().strftime("%Y-%m-%d")}.log"}')
        if self.compact:
            print(f'   Mode: COMPACT (only critical messages shown)')
        print('─' * 80)
        
        try:
            while True:
                # Check if date changed (for log rotation)
                self.setup_log_file()
                
                try:
                    line = self.ser.readline().decode('utf-8', errors='ignore').rstrip()
                    if not line:
                        continue
                    
                    formatted, severity = self.format_line(line)
                    self.update_statistics(line, severity)
                    
                    # Write to log file (always)
                    self.log_file.write(formatted + '\n')
                    
                    # Display to terminal (conditional based on mode)
                    if self.should_display(severity):
                        colored_line = self.apply_color(formatted, severity)
                        print(colored_line)
                    
                    # Print statistics every 200 lines in compact mode
                    if self.compact and self.line_count % 200 == 0:
                        stats = f'[{datetime.now().strftime("%H:%M")}] 📊 Lines: {self.line_count}, Errors: {self.error_count}, I2C-Errors: {self.i2c_error_count}, Slow-I2C: {self.slow_i2c_count}'
                        print(f'\033[36m{stats}\033[0m')  # Cyan
                
                except UnicodeDecodeError:
                    continue
        except KeyboardInterrupt:
            print('\n✓ Monitor stopped by user')
            self.ser.close()
            return True
        except Exception as e:
            print(f'✗ Error: {e}')
            return False
    
    def search(self, pattern):
        """Search all log files for pattern."""
        print(f'\n🔍 Searching for pattern: "{pattern}"\n')
        
        found = False
        for log_file in sorted(self.log_path.glob('serial_monitor_*.log')):
            with open(log_file, 'r') as f:
                for line_num, line in enumerate(f, 1):
                    if pattern.lower() in line.lower():
                        print(f'{log_file.name}:{line_num}: {line.rstrip()}')
                        found = True
        
        if not found:
            print(f'No matches found for "{pattern}"')
        
        return True

def main():
    """Parse arguments and run monitor."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Persistent Serial Monitor for Sensor Pages Dashboard (FEATURE-SPEC-012)'
    )
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial port (default: /dev/ttyUSB0)')
    parser.add_argument('--baudrate', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--compact', action='store_true', help='Compact mode: filter terminal to critical only')
    parser.add_argument('--search', type=str, help='Search log files for pattern and exit')
    
    args = parser.parse_args()
    
    monitor = SerialMonitor(
        port=args.port,
        baudrate=args.baudrate,
        compact=args.compact
    )
    
    if args.search:
        return monitor.search(args.search)
    else:
        return monitor.run()

if __name__ == '__main__':
    success = main()
    sys.exit(0 if success else 1)
