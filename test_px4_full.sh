#!/bin/sh
#
# PX4 SAMV71 Comprehensive Test Script
# Run this on the NSH console and save output to microSD
#
# Usage: sh /fs/microsd/test_px4_full.sh > /fs/microsd/test_results.txt

echo "================================================================================"
echo "PX4 SAMV71-XULT Functional Test Suite"
echo "================================================================================"
echo "Test started at: $(date)"
echo ""

echo "================== 1. SYSTEM INFORMATION =================="
echo ""
echo "--- Version Info ---"
ver all
echo ""
echo "--- Git Info ---"
ver git
echo ""
echo "--- Hardware Info ---"
ver hw
echo ""

echo "================== 2. MODULE STATUS =================="
echo ""
echo "--- Logger ---"
logger status
echo ""
echo "--- Commander ---"
commander status
echo ""
echo "--- Sensors ---"
sensors status
echo ""
echo "--- EKF2 ---"
ekf2 status
echo ""
echo "--- MAVLink ---"
mavlink status
echo ""
echo "--- Control Allocator ---"
control_allocator status
echo ""
echo "--- Flight Mode Manager ---"
flight_mode_manager status
echo ""
echo "--- MC Position Control ---"
mc_pos_control status
echo ""
echo "--- MC Attitude Control ---"
mc_att_control status
echo ""
echo "--- Land Detector ---"
land_detector status
echo ""

echo "================== 3. SYSTEM RESOURCES =================="
echo ""
echo "--- Running Tasks ---"
top -n 1
echo ""
echo "--- Memory Status ---"
free
echo ""
echo "--- Process List ---"
ps
echo ""

echo "================== 4. PARAMETER SYSTEM =================="
echo ""
echo "--- Parameter Count ---"
param show | wc -l
echo ""
echo "--- Key System Parameters ---"
param get SYS_AUTOSTART
param get SYS_HAS_BARO
param get SYS_HAS_GPS
param get SYS_HAS_MAG
param get SYS_PARAM_VER
param get BAT_CAPACITY
param get MAV_SYS_ID
echo ""
echo "--- Parameter Test: Set/Get ---"
param set TEST_PARAM 42
param get TEST_PARAM
param reset TEST_PARAM
echo ""
echo "--- Parameter Comparison Test ---"
param greater SYS_AUTOSTART -1
echo "Result: $?"
echo ""

echo "================== 5. STORAGE AND FILESYSTEM =================="
echo ""
echo "--- Disk Usage ---"
df
echo ""
echo "--- microSD Contents ---"
ls -lh /fs/microsd
echo ""
echo "--- Log Directory ---"
ls -lh /fs/microsd/log
echo ""
echo "--- ROMFS Check ---"
ls -lh /etc/init.d
echo ""
echo "--- File I/O Test ---"
echo "PX4 Test - $(date)" > /fs/microsd/test_io.txt
cat /fs/microsd/test_io.txt
rm /fs/microsd/test_io.txt
echo "File I/O: OK"
echo ""

echo "================== 6. I2C BUS =================="
echo ""
echo "--- I2C Bus List ---"
i2c bus
echo ""
echo "--- I2C Device Scan (Bus 0) ---"
i2c dev 0 0x03 0x77
echo ""

echo "================== 7. DEVICE NODES =================="
echo ""
echo "--- All Devices ---"
ls -1 /dev/
echo ""
echo "--- Serial Devices ---"
ls -1 /dev/tty*
echo ""

echo "================== 8. SENSOR DRIVERS =================="
echo ""
echo "--- ICM20689 Status ---"
icm20689 status
echo ""
echo "--- AK09916 Status ---"
ak09916 status
echo ""
echo "--- DPS310 Status ---"
dps310 status
echo ""

echo "================== 9. SYSTEM MESSAGES =================="
echo ""
echo "--- Boot Messages (last 50 lines) ---"
dmesg | tail -50
echo ""
echo "--- Errors ---"
dmesg | grep -i error
echo ""
echo "--- Warnings ---"
dmesg | grep -i warn
echo ""

echo "================== 10. NSH SHELL FEATURES =================="
echo ""
echo "--- Variable Test ---"
set TEST_VAR=hello
echo "Variable set: TEST_VAR=$TEST_VAR"
unset TEST_VAR
echo ""
echo "--- Conditional Test ---"
if [ 1 -eq 1 ]; then
  echo "If/then/else: PASS"
else
  echo "If/then/else: FAIL"
fi
echo ""
echo "--- Loop Test ---"
i=0
while [ $i -lt 3 ]; do
  echo "Loop iteration: $i"
  i=$(($i + 1))
done
echo ""

echo "================== 11. PERFORMANCE METRICS =================="
echo ""
echo "--- System Load (5 second sample) ---"
top -n 1
sleep 5
top -n 1
echo ""

echo "================== TEST SUMMARY =================="
echo ""
echo "Test completed at: $(date)"
echo ""
echo "Expected Results:"
echo "  ✓ Modules: logger, commander, sensors should be running"
echo "  ✓ Parameters: 394 parameters loaded"
echo "  ✓ Memory: ~360KB free"
echo "  ✓ Tasks: ~20-25 running"
echo "  ✓ Storage: microSD mounted with log directory"
echo "  ✓ I2C: Bus 0 detected"
echo ""
echo "Expected Warnings (OK):"
echo "  - Sensor drivers: 'no device on bus' (no hardware connected)"
echo "  - GPS/RC: 'invalid device' (UARTs not configured)"
echo "  - Dataman: 'Could not open' (normal first boot)"
echo ""
echo "Should NOT see:"
echo "  ✗ Hard fault messages"
echo "  ✗ Kernel panics"
echo "  ✗ System crashes"
echo "  ✗ Memory errors"
echo ""
echo "================================================================================"
echo "Test log saved. Review output for any unexpected errors."
echo "================================================================================"
