#!/usr/bin/env python3
"""
PX4 SAMV71 Serial Test Script

This script connects to the PX4 board via serial port, runs test commands,
and generates a test report.

Requirements:
    pip install pyserial

Usage:
    python3 test_px4_serial.py /dev/ttyACM0
    python3 test_px4_serial.py /dev/ttyUSB0 --baud 115200
    python3 test_px4_serial.py COM3  # Windows
"""

import serial
import time
import sys
import argparse
from datetime import datetime

class PX4Tester:
    def __init__(self, port, baudrate=115200, timeout=2):
        """Initialize serial connection to PX4 board"""
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        self.results = []

    def connect(self):
        """Connect to serial port"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE
            )
            time.sleep(0.5)  # Wait for connection to stabilize

            # Clear any existing data
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()

            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            return True
        except serial.SerialException as e:
            print(f"✗ Failed to connect to {self.port}: {e}")
            return False

    def send_command(self, command, wait_time=1.0, max_lines=100):
        """Send command and read response"""
        if not self.ser or not self.ser.is_open:
            return None

        try:
            # Clear buffer
            self.ser.reset_input_buffer()

            # Send command with newline
            cmd = command.strip() + '\n'
            self.ser.write(cmd.encode('utf-8'))
            self.ser.flush()

            # Wait for response
            time.sleep(wait_time)

            # Read all available data
            response = []
            lines_read = 0

            while self.ser.in_waiting > 0 and lines_read < max_lines:
                line = self.ser.readline()
                try:
                    decoded = line.decode('utf-8', errors='ignore').strip()
                    if decoded:  # Only add non-empty lines
                        response.append(decoded)
                        lines_read += 1
                except:
                    pass

            return response

        except Exception as e:
            print(f"Error sending command '{command}': {e}")
            return None

    def run_test(self, name, command, wait_time=1.0, check_func=None):
        """Run a test command and optionally verify result"""
        print(f"  Running: {name}...", end=' ', flush=True)

        response = self.send_command(command, wait_time)

        if response is None:
            print("✗ FAILED (no response)")
            self.results.append({
                'name': name,
                'command': command,
                'status': 'FAILED',
                'response': [],
                'error': 'No response'
            })
            return False

        # Check result if checker function provided
        passed = True
        error = None

        if check_func:
            try:
                passed, error = check_func(response)
            except Exception as e:
                passed = False
                error = f"Check function error: {e}"

        status = "✓ PASS" if passed else "✗ FAIL"
        print(status)

        if error:
            print(f"    Error: {error}")

        self.results.append({
            'name': name,
            'command': command,
            'status': 'PASS' if passed else 'FAIL',
            'response': response,
            'error': error
        })

        return passed

    def run_all_tests(self):
        """Run complete test suite"""
        print("\n" + "="*70)
        print("PX4 SAMV71 Automated Test Suite")
        print("="*70)
        print(f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"Port: {self.port}")
        print("="*70 + "\n")

        tests_passed = 0
        tests_failed = 0

        # Give board time to boot if just flashed
        print("Waiting for board to boot (5 seconds)...")
        time.sleep(5)

        # Test 1: Basic connectivity
        print("\n[1] Testing Basic Connectivity")
        if self.run_test(
            "Echo test",
            "echo PX4_TEST",
            wait_time=0.5,
            check_func=lambda r: (any('PX4_TEST' in line for line in r), None)
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 2: Version information
        print("\n[2] Testing System Information")
        if self.run_test(
            "Version info",
            "ver all",
            wait_time=1.0,
            check_func=lambda r: (
                any('SAMV71' in line or 'SAMV70' in line for line in r) and
                any('PX4' in line for line in r),
                None if any('SAMV7' in line for line in r) else "SAMV71 not found in output"
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 3: Logger status
        print("\n[3] Testing Logger Module")
        if self.run_test(
            "Logger status",
            "logger status",
            wait_time=1.0,
            check_func=lambda r: (
                any('Running' in line or 'running' in line for line in r),
                None if any('Running' in line for line in r) else "Logger not running"
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 4: Commander status
        print("\n[4] Testing Commander Module")
        if self.run_test(
            "Commander status",
            "commander status",
            wait_time=1.0,
            check_func=lambda r: (
                any('Disarmed' in line or 'Armed' in line for line in r),
                None
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 5: Sensors status
        print("\n[5] Testing Sensors Module")
        if self.run_test(
            "Sensors status",
            "sensors status",
            wait_time=1.0,
            check_func=lambda r: (
                any('gyro' in line.lower() or 'accel' in line.lower() for line in r),
                None
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 6: Parameter system
        print("\n[6] Testing Parameter System")
        if self.run_test(
            "Parameter count",
            "param show | wc -l",
            wait_time=2.0,
            check_func=lambda r: (
                any(line.strip().isdigit() and int(line.strip()) > 300 for line in r),
                None if any(line.strip().isdigit() for line in r) else "No parameter count found"
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 7: Parameter get
        print("\n[7] Testing Parameter Get")
        if self.run_test(
            "Get SYS_AUTOSTART",
            "param get SYS_AUTOSTART",
            wait_time=0.5,
            check_func=lambda r: (len(r) > 0, None)
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 8: Memory status
        print("\n[8] Testing Memory Status")
        if self.run_test(
            "Free memory",
            "free",
            wait_time=0.5,
            check_func=lambda r: (
                any('free' in line.lower() or 'avail' in line.lower() for line in r),
                None
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 9: Task list
        print("\n[9] Testing Task List")
        if self.run_test(
            "Top command",
            "top -n 1",
            wait_time=1.5,
            check_func=lambda r: (
                len(r) > 5,  # Should have multiple tasks
                None if len(r) > 5 else "Too few tasks running"
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 10: Storage
        print("\n[10] Testing Storage")
        if self.run_test(
            "microSD check",
            "ls /fs/microsd",
            wait_time=0.5,
            check_func=lambda r: (
                len(r) > 0 and not any('error' in line.lower() for line in r),
                None
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 11: I2C bus
        print("\n[11] Testing I2C Bus")
        if self.run_test(
            "I2C bus list",
            "i2c bus",
            wait_time=0.5,
            check_func=lambda r: (
                any('Bus 0' in line or 'bus 0' in line for line in r),
                None if any('0' in line for line in r) else "I2C bus 0 not found"
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Test 12: System messages
        print("\n[12] Testing System Messages")
        if self.run_test(
            "dmesg",
            "dmesg | tail -10",
            wait_time=1.0,
            check_func=lambda r: (
                len(r) > 0 and not any('hard fault' in line.lower() for line in r),
                "Hard fault detected!" if any('hard fault' in line.lower() for line in r) else None
            )
        ):
            tests_passed += 1
        else:
            tests_failed += 1

        # Summary
        print("\n" + "="*70)
        print("TEST SUMMARY")
        print("="*70)
        print(f"Total Tests: {tests_passed + tests_failed}")
        print(f"✓ Passed: {tests_passed}")
        print(f"✗ Failed: {tests_failed}")
        print(f"Success Rate: {(tests_passed/(tests_passed+tests_failed)*100):.1f}%")
        print("="*70 + "\n")

        return tests_passed, tests_failed

    def save_report(self, filename):
        """Save detailed test report to file"""
        with open(filename, 'w') as f:
            f.write("="*70 + "\n")
            f.write("PX4 SAMV71 Test Report\n")
            f.write("="*70 + "\n")
            f.write(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Port: {self.port}\n")
            f.write(f"Baudrate: {self.baudrate}\n")
            f.write("="*70 + "\n\n")

            for i, result in enumerate(self.results, 1):
                f.write(f"[{i}] {result['name']}\n")
                f.write(f"Command: {result['command']}\n")
                f.write(f"Status: {result['status']}\n")

                if result['error']:
                    f.write(f"Error: {result['error']}\n")

                f.write("Response:\n")
                for line in result['response']:
                    f.write(f"  {line}\n")

                f.write("\n" + "-"*70 + "\n\n")

            # Summary
            passed = sum(1 for r in self.results if r['status'] == 'PASS')
            failed = sum(1 for r in self.results if r['status'] == 'FAIL')

            f.write("="*70 + "\n")
            f.write("SUMMARY\n")
            f.write("="*70 + "\n")
            f.write(f"Total Tests: {len(self.results)}\n")
            f.write(f"Passed: {passed}\n")
            f.write(f"Failed: {failed}\n")
            f.write(f"Success Rate: {(passed/len(self.results)*100):.1f}%\n")

        print(f"Detailed report saved to: {filename}")

    def disconnect(self):
        """Close serial connection"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"✓ Disconnected from {self.port}")

def main():
    parser = argparse.ArgumentParser(
        description='Automated testing for PX4 on SAMV71',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 test_px4_serial.py /dev/ttyACM0
  python3 test_px4_serial.py /dev/ttyUSB0 --baud 115200
  python3 test_px4_serial.py COM3 --output test_report.txt
  python3 test_px4_serial.py /dev/ttyACM0 --quick
        """
    )

    parser.add_argument('port', help='Serial port (e.g., /dev/ttyACM0, COM3)')
    parser.add_argument('--baud', type=int, default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('--output', default='px4_test_report.txt',
                       help='Output report filename (default: px4_test_report.txt)')
    parser.add_argument('--timeout', type=float, default=2.0,
                       help='Serial timeout in seconds (default: 2.0)')

    args = parser.parse_args()

    # Create tester instance
    tester = PX4Tester(args.port, args.baud, args.timeout)

    # Connect
    if not tester.connect():
        sys.exit(1)

    try:
        # Run tests
        passed, failed = tester.run_all_tests()

        # Save report
        tester.save_report(args.output)

        # Exit code based on results
        sys.exit(0 if failed == 0 else 1)

    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(130)

    except Exception as e:
        print(f"\n✗ Error during testing: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

    finally:
        tester.disconnect()

if __name__ == '__main__':
    main()
