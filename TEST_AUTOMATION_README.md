# Automated PX4 Testing via Serial Port

This Python script automatically tests PX4 on your SAMV71 board via serial connection.

## Quick Start

### 1. Install Requirements

```bash
pip3 install pyserial
```

### 2. Connect Board

- Connect SAMV71-XULT to PC via USB
- Wait for board to boot (~5 seconds)

### 3. Run Automated Tests

```bash
# Find your serial port
ls /dev/ttyACM*   # USB CDC/ACM
ls /dev/ttyUSB*   # UART adapter

# Run tests
python3 test_px4_serial.py /dev/ttyACM0
```

**That's it!** The script will:
- Connect to the board
- Run 12 automated tests
- Display results in real-time
- Save detailed report to `px4_test_report.txt`

## Usage Examples

```bash
# Basic test (default settings)
python3 test_px4_serial.py /dev/ttyACM0

# Custom baud rate
python3 test_px4_serial.py /dev/ttyUSB0 --baud 115200

# Custom output file
python3 test_px4_serial.py /dev/ttyACM0 --output my_test.txt

# Longer timeout for slow responses
python3 test_px4_serial.py /dev/ttyACM0 --timeout 5.0

# Windows
python3 test_px4_serial.py COM3

# Get help
python3 test_px4_serial.py --help
```

## What It Tests

The script runs 12 automated tests:

1. **Echo Test** - Basic connectivity
2. **Version Info** - Checks for SAMV71 and PX4
3. **Logger Status** - Verifies logger is running
4. **Commander Status** - Checks flight controller
5. **Sensors Status** - Verifies sensor module
6. **Parameter Count** - Ensures 300+ parameters loaded
7. **Parameter Get** - Tests parameter retrieval
8. **Memory Status** - Checks free RAM
9. **Task List** - Verifies multiple tasks running
10. **Storage** - Checks microSD mount
11. **I2C Bus** - Verifies I2C bus 0
12. **System Messages** - Checks for hard faults

## Example Output

```
======================================================================
PX4 SAMV71 Automated Test Suite
======================================================================
Started: 2025-11-13 10:30:45
Port: /dev/ttyACM0
======================================================================

Waiting for board to boot (5 seconds)...

[1] Testing Basic Connectivity
  Running: Echo test... ✓ PASS

[2] Testing System Information
  Running: Version info... ✓ PASS

[3] Testing Logger Module
  Running: Logger status... ✓ PASS

[4] Testing Commander Module
  Running: Commander status... ✓ PASS

[5] Testing Sensors Module
  Running: Sensors status... ✓ PASS

[6] Testing Parameter System
  Running: Parameter count... ✓ PASS

[7] Testing Parameter Get
  Running: Get SYS_AUTOSTART... ✓ PASS

[8] Testing Memory Status
  Running: Free memory... ✓ PASS

[9] Testing Task List
  Running: Top command... ✓ PASS

[10] Testing Storage
  Running: microSD check... ✓ PASS

[11] Testing I2C Bus
  Running: I2C bus list... ✓ PASS

[12] Testing System Messages
  Running: dmesg... ✓ PASS

======================================================================
TEST SUMMARY
======================================================================
Total Tests: 12
✓ Passed: 12
✗ Failed: 0
Success Rate: 100.0%
======================================================================

Detailed report saved to: px4_test_report.txt
✓ Disconnected from /dev/ttyACM0
```

## Test Report

The detailed report (`px4_test_report.txt`) contains:
- Date and time of test
- Serial port and settings
- Each test's command
- Full response from board
- Pass/fail status for each test
- Summary statistics

Example report section:
```
[2] Version info
Command: ver all
Status: PASS
Response:
  HW arch: MICROCHIP_SAMV71_XULT_CLICKBOARDS
  PX4 git-hash: a8dcc1e41027c24146f31f1488991e9ce05d037a
  PX4 version: 1.17.0 40 (17891392)
  OS: NuttX
  OS version: Release 11.0.0 (184549631)
  MCU: SAMV70, rev. B
```

## Troubleshooting

### Permission Denied

```bash
# Linux - Add user to dialout group
sudo usermod -a -G dialout $USER
# Then logout and login again

# Or run with sudo (not recommended)
sudo python3 test_px4_serial.py /dev/ttyACM0
```

### Port Not Found

```bash
# Check available ports
ls /dev/tty* | grep -E "(ACM|USB)"

# On Windows
python3 -m serial.tools.list_ports

# Or check dmesg
dmesg | tail
```

### Connection Timeout

```bash
# Increase timeout
python3 test_px4_serial.py /dev/ttyACM0 --timeout 5.0

# Check board is powered and booted
# Wait 10 seconds after power-on before testing
```

### Module pyserial Not Found

```bash
# Install pyserial
pip3 install pyserial

# Or system-wide
sudo apt-get install python3-serial

# Verify installation
python3 -c "import serial; print(serial.__version__)"
```

### Tests Fail

If specific tests fail:

1. **Check board is fully booted** (wait 10 seconds)
2. **Try manual command** via minicom to verify board works
3. **Check the detailed report** for error messages
4. **Increase timeout** with `--timeout` option

Example manual test:
```bash
minicom -D /dev/ttyACM0 -b 115200
# Type: ver all
# Should see version output
```

### Board Not Responding

```bash
# Power cycle board
# Wait 10 seconds
# Try again

# Or check if another program has port open
lsof | grep ttyACM0
```

## Integration with CI/CD

You can use this in automated testing:

```bash
#!/bin/bash
# flash_and_test.sh

# Flash firmware
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program firmware.elf verify reset exit"

# Wait for boot
sleep 10

# Run automated tests
python3 test_px4_serial.py /dev/ttyACM0 --output test_results.txt

# Check exit code
if [ $? -eq 0 ]; then
    echo "✓ All tests passed"
    exit 0
else
    echo "✗ Some tests failed"
    cat test_results.txt
    exit 1
fi
```

## Customizing Tests

To add your own tests, edit `test_px4_serial.py`:

```python
# Add after Test 12
print("\n[13] Testing My Feature")
if self.run_test(
    "My test name",
    "my_command",
    wait_time=1.0,
    check_func=lambda r: (
        # Check response
        any('expected' in line for line in r),
        None  # Error message if check fails
    )
):
    tests_passed += 1
else:
    tests_failed += 1
```

## Exit Codes

- `0` - All tests passed
- `1` - One or more tests failed
- `130` - Interrupted by user (Ctrl+C)

Useful for scripting:
```bash
if python3 test_px4_serial.py /dev/ttyACM0; then
    echo "Tests passed!"
else
    echo "Tests failed!"
fi
```

## Comparison: Automated vs Manual Testing

| Aspect | Automated (This Script) | Manual (minicom) |
|--------|------------------------|------------------|
| Time | 2-3 minutes | 10-15 minutes |
| Consistency | Always same tests | Depends on user |
| Documentation | Auto-generated report | Must copy/paste |
| CI/CD Integration | Yes | No |
| Real-time interaction | No | Yes |
| Custom commands | Need to edit script | Type anything |

**Recommendation:** Use automated script for regular testing, manual for debugging specific issues.

## Support

If tests fail:
1. Check `px4_test_report.txt` for details
2. Try manual testing with minicom
3. Share report in GitHub issue
4. Include board serial output

## See Also

- `TESTING_GUIDE.md` - Manual testing procedures
- `QUICK_START.md` - Initial setup guide
- `README.md` - Full documentation
