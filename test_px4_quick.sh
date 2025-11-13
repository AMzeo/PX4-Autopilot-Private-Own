#!/bin/sh
#
# PX4 SAMV71 Quick Test Script (2-3 minutes)
# Run this for a quick health check
#
# Usage: sh /fs/microsd/test_px4_quick.sh

echo "=== PX4 Quick Health Check ==="
echo ""

echo "1. Version:"
ver all | head -10
echo ""

echo "2. Core Modules:"
logger status | head -3
commander status | head -3
sensors status | head -5
echo ""

echo "3. Tasks (Top 10):"
top -n 1 | head -15
echo ""

echo "4. Memory:"
free
echo ""

echo "5. Parameters:"
echo "Total: $(param show | wc -l) parameters"
param get SYS_AUTOSTART
param get SYS_HAS_BARO
echo ""

echo "6. Storage:"
df | grep microsd
ls /fs/microsd/log 2>/dev/null && echo "Log dir: OK" || echo "Log dir: Missing"
echo ""

echo "7. I2C:"
i2c bus
echo ""

echo "8. Recent Errors:"
dmesg | grep -i "error\|fault" | tail -5
echo ""

echo "=== Quick Test Complete ==="
echo "✓ Check above for any unexpected errors"
echo "Run 'sh /fs/microsd/test_px4_full.sh' for comprehensive test"
