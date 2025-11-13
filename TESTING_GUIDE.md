# PX4 SAMV71 Testing Guide

This guide explains how to test PX4 functionality on your SAMV71-XULT board.

## Prerequisites

- SAMV71-XULT board with PX4 flashed
- Serial connection (UART1 or USB CDC/ACM)
- Terminal software (minicom, screen, or similar)

## Method 1: Automated Testing (Recommended)

### Step 1: Copy Test Scripts to Board

**Option A: Via microSD card reader on PC**
```bash
# Mount microSD card on PC
# Copy test scripts
cp test_px4_quick.sh /media/microsd/
cp test_px4_full.sh /media/microsd/
sync
# Eject and insert into SAMV71 board
```

**Option B: Via serial console (copy/paste)**
```bash
# Connect to board
minicom -D /dev/ttyACM0 -b 115200

# On NSH prompt, create script:
cat > /fs/microsd/test_px4_quick.sh << 'EOF'
[paste quick test script content]
EOF

cat > /fs/microsd/test_px4_full.sh << 'EOF'
[paste full test script content]
EOF
```

### Step 2: Run Quick Test (2-3 minutes)

```bash
nsh> sh /fs/microsd/test_px4_quick.sh
```

Watch output for:
- ✅ Version shows SAMV71 and PX4 1.17.0
- ✅ Modules show "Running" status
- ✅ ~20-25 tasks active
- ✅ ~360KB RAM free
- ✅ microSD mounted

### Step 3: Run Full Test (5-10 minutes)

```bash
nsh> sh /fs/microsd/test_px4_full.sh > /fs/microsd/test_results.txt
```

Wait for completion, then:
```bash
nsh> cat /fs/microsd/test_results.txt
```

**Or copy results to PC for analysis:**
- Remove microSD from board
- Insert into PC
- Open `test_results.txt`

## Method 2: Manual Testing

If automated scripts don't work, run these commands manually:

### Essential Tests (5 minutes)

```bash
# 1. System info
ver all

# 2. Module status
logger status
commander status
sensors status

# 3. Resources
top
free

# 4. Parameters
param show | head -20
param get SYS_AUTOSTART

# 5. Storage
ls /fs/microsd/log
df

# 6. I2C
i2c bus

# 7. Messages
dmesg | tail -20
```

## Method 3: Copy Output for Analysis

### Using minicom with logging

```bash
# Start minicom with logging
minicom -D /dev/ttyACM0 -b 115200 -C test_output.log

# Run commands on board
nsh> ver all
nsh> logger status
nsh> commander status
[... run all test commands ...]

# Exit minicom (Ctrl+A, X)
# View log file
cat test_output.log
```

### Using screen with logging

```bash
# Start screen with logging
screen -L -Logfile test_output.log /dev/ttyACM0 115200

# Run commands
[... run test commands ...]

# Exit screen (Ctrl+A, K)
# View log
cat test_output.log
```

### Using script command

```bash
# Start recording
script -c "minicom -D /dev/ttyACM0 -b 115200" test_session.log

# Run commands on board
# Exit minicom when done

# View recording
cat test_session.log
```

## Expected Results

### ✅ Success Indicators

**System Boot:**
- No hard fault messages
- NuttX 11.0.0 boots
- NSH prompt appears

**Modules:**
- Logger: "Running in mode: all"
- Commander: "Disarmed"
- Sensors: Shows gyro/accel configuration
- EKF2: Initialized
- MAVLink: Running

**Resources:**
- Flash: ~880KB / 2MB (42%)
- RAM: ~22KB / 384KB (6%)
- Tasks: 20-25 active
- Free RAM: ~360KB

**Parameters:**
- 394 parameters loaded
- Can set/get parameters
- No corruption errors

**Storage:**
- microSD mounted at /fs/microsd
- Log directory exists
- Can create/delete files

### ⚠️ Expected Warnings (OK)

These are normal without sensor hardware:

```
WARN [SPI_I2C] ak09916: no instance started (no device on bus?)
WARN [SPI_I2C] dps310: no instance started (no device on bus?)
ERROR [gps] invalid device (-d) /dev/ttyS2
ERROR [rc_input] invalid device (-d) /dev/ttyS4
WARN [dataman] Could not open data manager file
```

These are expected because:
- Sensors not physically connected
- GPS/RC UARTs not configured
- Dataman file created on first run

### ❌ Failure Indicators

These indicate problems:

```
arm_hardfault: Hard Fault
Assertion failed
Kernel panic
Bus fault
Memory pool exhausted
Task X crashed
Stack overflow
```

If you see any of these, share the full dmesg output.

## Sharing Results with Support

If you need help analyzing results:

1. **Capture full output:**
```bash
nsh> sh /fs/microsd/test_px4_full.sh > /fs/microsd/test_results.txt
```

2. **Copy to PC** (remove microSD and read file)

3. **Share relevant sections:**
   - Full version info (ver all)
   - Module status (logger, commander, sensors)
   - Error messages (dmesg)
   - Resource usage (top, free)

4. **Or paste serial console output** directly in chat/issue

## Troubleshooting

### Script won't run

**Issue:** `test_px4_quick.sh: not found`

**Solution:**
```bash
# Check file exists
ls /fs/microsd/*.sh

# Make executable
chmod +x /fs/microsd/test_px4_quick.sh

# Run with explicit interpreter
sh /fs/microsd/test_px4_quick.sh
```

### Output too long for screen

**Solution:** Redirect to file
```bash
nsh> sh /fs/microsd/test_px4_full.sh > /fs/microsd/results.txt
nsh> cat /fs/microsd/results.txt | more
```

### Serial console disconnects

**Solution:** Use USB CDC/ACM for more stable connection
```bash
# Wait for PX4 to boot (~5 seconds)
minicom -D /dev/ttyACM0 -b 115200
```

### Board hangs during test

**Solution:**
- Power cycle board
- Run dmesg to check for errors
- Try quick test first, then full test
- Run commands individually to isolate issue

## Quick Reference Card

Copy this to keep handy:

```bash
# Quick health check
ver all && logger status && commander status && top -n 1 && free

# Parameter check
param show | wc -l && param get SYS_AUTOSTART

# Storage check
df && ls /fs/microsd/log

# Error check
dmesg | grep -i "error\|fault\|warn"

# Full test
sh /fs/microsd/test_px4_full.sh > /fs/microsd/test_results.txt
```

## Testing Checklist

Use this to verify your system:

- [ ] System boots without hard faults
- [ ] `ver all` shows SAMV71 and PX4 1.17.0
- [ ] `logger status` shows "Running"
- [ ] `commander status` shows "Disarmed"
- [ ] `sensors status` shows gyro/accel configured
- [ ] `top` shows ~20-25 tasks, <50% CPU usage
- [ ] `free` shows ~360KB RAM free
- [ ] `param show | wc -l` shows 394 parameters
- [ ] `ls /fs/microsd/log` shows log directory
- [ ] `i2c bus` shows Bus 0
- [ ] `dmesg` shows no hard faults or panics
- [ ] Can set/get parameters successfully
- [ ] Can create files on microSD
- [ ] System stable for >5 minutes

If all items checked, **your PX4 port is working correctly!** ✅

---

**Next Steps After Testing:**
- Configure SPI for ICM20689 IMU
- Set up flash parameter storage (MTD)
- Configure PWM outputs
- Add additional UART ports
- Connect real sensors and test

See [README.md](README.md) for contribution guidelines.
