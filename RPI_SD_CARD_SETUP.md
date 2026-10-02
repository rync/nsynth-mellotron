# NSynth Mellotron on Raspberry Pi 3B+ - Complete SD Card Setup Guide

This guide walks you through preparing a Raspberry Pi 3B+ from scratch to run the NSynth Mellotron synthesizer firmware.

**Time Required:** 45 minutes
**Difficulty:** Intermediate

---

## Table of Contents

1. [What You'll Need](#what-youll-need)
2. [Step 1: Create Base Linux Image (10 min)](#step-1-create-base-linux-image)
3. [Step 2: Boot & Initial Configuration (5 min)](#step-2-boot--initial-configuration)
4. [Step 3: Install Dependencies (10 min)](#step-3-install-dependencies)
5. [Step 4: Prepare Directory Structure (5 min)](#step-4-prepare-directory-structure)
6. [Step 5: Prepare Sample Files (10 min)](#step-5-prepare-sample-files)
7. [Step 6: Configure Hardware Peripherals (5 min)](#step-6-configure-hardware-peripherals)
8. [Step 7: Build the Firmware (5 min)](#step-7-build-the-firmware)
9. [Step 8: First Run (5 min)](#step-8-first-run)
10. [Troubleshooting](#troubleshooting)

---

## What You'll Need

### Hardware
- **Raspberry Pi 3B+** with proper power supply (5V 2.5A minimum)
- **Micro SD Card** 16GB or larger (Class 10 recommended)
- **SD Card reader** for your desktop computer
- **USB-to-Serial adapter** (3.3V) for MIDI input
- **CS4271 audio codec** (or similar I2S audio board) with 3.5mm jack
- **I2C OLED display** (optional but recommended)
- **Rotary encoders** ×4 + **potentiometers** ×6 (optional, can test via MIDI)

### Software
- **Raspberry Pi Imager** or `dd` / **balenaEtcher**
- **SSH client** (built-in on Linux/Mac, PuTTY on Windows)
- **Text editor** (nano, vim, or SCP)
- **FFmpeg** (for converting audio samples)

### Your Code
- The complete NSynth Mellotron firmware from this repository
- Linux HAL layer (`hal/linux/nsynth_hal_linux.h`)
- Linux main (`main_linux.cpp`)

---

## Step 1: Create Base Linux Image

### 1a. Download Raspberry Pi OS

Go to **https://www.raspberrypi.com/software/operating-systems/**

Download **Raspberry Pi OS Lite (32-bit)** for Pi 3B+:
- Latest version recommended
- **NOT** the desktop variant (we don't need GUI, saves space/CPU)
- File size: ~300–400 MB

Example filename: `2024-10-raspberry-pi-os-lite-arm.img.xz`

### 1b. Flash to SD Card

**Option A: Using Raspberry Pi Imager (Easiest)**

1. Download & install **Raspberry Pi Imager** from https://www.raspberrypi.com/software/
2. Launch it
3. Click **"Choose Device"** → Select **Raspberry Pi 3**
4. Click **"Choose OS"** → **Raspberry Pi OS (Other)** → **Raspberry Pi OS Lite (32-bit)**
5. Click **"Choose Storage"** → Select your SD card reader
6. Click **"Next"**
7. Answer **"Would you like to use OS customisation?"** → YES
8. Configure:
   - **Hostname:** `nsynth` (or any name)
   - **Username:** `pi` (default)
   - **Password:** (set a strong password)
   - **WiFi SSID/Password:** (optional, needed for apt downloads)
   - **Locale/Timezone:** Your location
   - **SSH:** Enable (so you can remote in)
9. Click **"Save"** → **"Yes"** to write
10. Wait 5–10 minutes for flashing to complete

**Option B: Using dd (Linux/Mac)**

```bash
# Insert SD card, identify it:
lsblk
# Look for /dev/sdX (NOT /dev/sdX1 - the partition)

# Unmount any partitions:
sudo umount /dev/sdX*

# Decompress and write:
xzcat ~/Downloads/2024-10-raspberry-pi-os-lite-arm.img.xz | \
  sudo dd of=/dev/sdX bs=4M status=progress conv=fsync

# Wait for completion, then:
sudo eject /dev/sdX
```

**Option C: Using balenaEtcher (GUI, all platforms)**

1. Download from https://www.balena.io/etcher/
2. Launch it
3. **Select image** → choose the `.img.xz` file
4. **Select drive** → choose your SD card
5. **Flash** and wait

### Result

Your SD card now contains a bootable Raspberry Pi OS Lite with SSH enabled.

---

## Step 2: Boot & Initial Configuration

### 2a. Insert SD Card & Power On

1. Insert the flashed SD card into your Pi 3B+
2. Connect power (Pi will boot immediately)
3. Wait ~30 seconds for first-time setup

### 2b. Find Your Pi's IP Address

**Option A: Check your router**
- Log into your WiFi router's admin panel
- Look for a device named "nsynth" (or whatever hostname you set)
- Note its IP address (e.g., `192.168.1.100`)

**Option B: Using nmap**
```bash
# Scan your network for Raspberry Pi:
nmap -sn 192.168.1.0/24 | grep -i rasp
# Look for "Raspberry Pi Foundation" entries
```

**Option C: Serial console (if SSH won't work)**
- Connect UART pins (GPIO 14/15) to USB serial adapter
- Use `screen` or `PuTTY` at 115200 baud
- Login with `pi` / your password

### 2c. SSH into the Pi

```bash
# From your desktop:
ssh pi@192.168.1.100  # Replace IP with your Pi's address
# Enter your password when prompted

# You should see:
# pi@nsynth:~ $
```

### 2d. Expand Filesystem

```bash
# Expand to use full SD card:
sudo raspi-config

# Select: Advanced Options → Expand Filesystem
# Select: Finish → Reboot
# SSH back in after reboot
```

After reboot, verify:
```bash
df -h /
# Should show your full SD card size (e.g., 15G)
```

---

## Step 3: Install Dependencies

SSH into your Pi and run these commands:

```bash
# Update package lists:
sudo apt update

# Upgrade existing packages:
sudo apt upgrade -y

# Install ALSA (audio):
sudo apt install -y alsa-utils libasound2-dev

# Install I2C/SPI tools:
sudo apt install -y i2c-tools python3-smbus

# Install build tools:
sudo apt install -y build-essential cmake git

# Install MIDI utilities (optional but useful for debugging):
sudo apt install -y alsa-midi-utils

# Install libfatfs for SD card compatibility (optional):
sudo apt install -y libfatfs-dev

# Clean up:
sudo apt autoremove -y
```

**Total installation time:** ~5–10 minutes (depends on internet speed)

Verify installation:
```bash
# Check versions:
gcc --version         # Should be 10.x or higher
cmake --version       # Should be 3.18 or higher
aplay --version       # ALSA player (for testing audio)
```

---

## Step 4: Prepare Directory Structure

Create the folders needed for samples, presets, and code:

```bash
# Create main project directory:
mkdir -p ~/nsynth-mellotron
cd ~/nsynth-mellotron

# Create subdirectories:
mkdir -p samples/{Strings_Violin,Piano_Acoustic,Brass_Trumpet,Synth_Pad}
mkdir -p presets
mkdir -p firmware
mkdir -p logs

# Create symlink for easier access (optional):
ln -s ~/nsynth-mellotron/samples ~/samples
ln -s ~/nsynth-mellotron/presets ~/presets

# Verify structure:
tree ~/nsynth-mellotron
# Or:
ls -la ~/nsynth-mellotron/
```

Result:
```
~/nsynth-mellotron/
├── samples/
│   ├── Strings_Violin/
│   ├── Piano_Acoustic/
│   ├── Brass_Trumpet/
│   └── Synth_Pad/
├── presets/
├── firmware/
└── logs/
```

---

## Step 5: Prepare Sample Files

### 5a. Create Test WAV Files

On your **desktop** (not the Pi), create WAV files in the correct format:

**Using FFmpeg:**

```bash
# Create a simple test sample (e.g., from an MP3):
ffmpeg -i mysample.mp3 -ar 48000 -ac 1 -acodec pcm_s16le 60_C4.wav

# Create multiple samples for a full octave (C to B):
# MIDI notes: 60=C4, 62=D4, 64=E4, 65=F4, 67=G4, 69=A4, 71=B4

for note in 60 62 64 65 67 69 71; do
  ffmpeg -i mysample.mp3 -ar 48000 -ac 1 -acodec pcm_s16le \
    ${note}_note.wav
done
```

**Using Audacity (GUI method):**

1. Open Audacity
2. **File → Open** your audio file
3. **Audio Track → Audio Track → Mono** (convert to mono if stereo)
4. **Tracks → Sample Tracks → Set Sample Rate → 48000**
5. **File → Export → Export as WAV**
6. Choose: **Signed 16-bit PCM**
7. Save as: `60_C4.wav`

**Sample Duration:** 0.5–5 seconds recommended
- Shorter: faster loading, less latency
- Longer: more sustain texture, file size acceptable

### 5b. Prepare MIDI Note Mapping

Each sample file must be named `{MIDI_NOTE}_{NOTE_NAME}.wav`

**MIDI Note Numbering:**

| MIDI | Note | MIDI | Note | MIDI | Note |
|------|------|------|------|------|------|
| 36   | C2   | 60   | C4   | 84   | C6   |
| 48   | C3   | 72   | C5   | 96   | C7   |

**Examples:**
- `60_C4.wav` — Middle C (MIDI 60)
- `69_A4.wav` — A4 (MIDI 69)
- `36_C2.wav` — Low C (MIDI 36)

Create a full octave by sampling every 2–3 semitones:
- Minimum: `60_C4.wav` + `67_G4.wav` (two samples, pitch shifting fills gaps)
- Recommended: `60_C4.wav`, `62_D4.wav`, `64_E4.wav`, `65_F4.wav`, `67_G4.wav`, `69_A4.wav`, `71_B4.wav` (7 samples)

### 5c. Copy Samples to Pi

**Option A: Using SCP (over SSH)**

```bash
# From desktop, copy samples to Pi:
scp ~/audio_samples/*.wav pi@192.168.1.100:~/nsynth-mellotron/samples/Strings_Violin/

# Verify on Pi:
ssh pi@192.168.1.100 "ls -la ~/nsynth-mellotron/samples/Strings_Violin/"
```

**Option B: Using SD Card Directly**

1. Eject Pi's SD card
2. Insert into desktop card reader
3. Copy `.wav` files to `samples/Strings_Violin/`
4. Eject and reinsert into Pi

**Option C: Using USB Thumb Drive**

```bash
# On Pi:
mkdir -p /mnt/usb
sudo mount /dev/sda1 /mnt/usb
cp /mnt/usb/*.wav ~/nsynth-mellotron/samples/Strings_Violin/
sudo umount /mnt/usb
```

### 5d. Verify Samples on Pi

```bash
# SSH into Pi:
ssh pi@192.168.1.100

# List samples:
ls -lh ~/nsynth-mellotron/samples/Strings_Violin/

# Check WAV format:
file ~/nsynth-mellotron/samples/Strings_Violin/60_C4.wav
# Should show: "RIFF (little-endian) data, WAVE audio, mono 48000 Hz, 16 bit"
```

---

## Step 6: Configure Hardware Peripherals

### 6a. Enable I2S Audio Output

SSH into your Pi:

```bash
# Edit boot config:
sudo nano /boot/firmware/config.txt
```

Add these lines at the end:

```ini
# I2S Audio (CS4271 codec)
dtoverlay=i2s-mmap
dtparam=i2s=on

# Enable audio output
dtparam=audio=on

# Force audio to 3.5mm jack (not HDMI):
hdmi_drive=2

# Additional audio settings:
audio_pwm_mode=2
```

Save with **Ctrl+X**, then **Y**, then **Enter**.

### 6b. Enable I2C (for OLED display optional)

```bash
# In the same config.txt:
# Add/uncomment:
dtparam=i2c_arm=on
```

### 6c. Enable SPI (for SD card if not USB)

```bash
# In the same config.txt:
# Add/uncomment:
dtparam=spi=on
```

### 6d. Reboot to Apply Changes

```bash
sudo reboot

# Wait 30 seconds, then SSH back in
ssh pi@192.168.1.100
```

### 6e. Test Audio Output

```bash
# List ALSA devices:
aplay -l

# You should see something like:
# card 0: ALSA [ALSA], device 0: bcm2835 ALSA [bcm2835 ALSA]
# card 1: IQaudIO [IQaudIO], device 0: ...

# Test audio playback:
speaker-test -c 2 -r 48000 -t wav

# You should hear white noise from the 3.5mm jack
# Press Ctrl+C to stop
```

If no audio:
- Check CS4271 codec is connected to I2S pins (GPIO 18=BCLK, GPIO 19=LRCLK, GPIO 21=DOUT)
- Verify audio cable is plugged in
- Check speaker/headphones work with another source

### 6f. Configure MIDI Serial Input

```bash
# Check if USB MIDI adapter is detected:
ls /dev/ttyUSB*

# Should show /dev/ttyUSB0 if USB adapter is plugged in

# Set permissions (so non-root can access):
sudo usermod -a -G dialout pi
# Log out and back in for this to take effect
```

**Wiring USB-to-Serial (3.3V):**
- MIDI IN pin 4 (GND) → Serial GND
- MIDI IN pin 5 (5V via 220Ω) → Serial 5V (or 3.3V if 3.3V board)
- MIDI IN pin 3 (RxD) → Serial TxD
- MIDI IN pin 2 (TxD) → Serial RxD

### 6g. Configure GPIO for Potentiometers (Optional)

If you have analog potentiometers, use an ADS1115 I2C ADC:

```bash
# Check I2C address:
i2cdetect -y 1

# Should show the ADS1115 at 0x48 (or 0x49/0x4A/0x4B depending on jumpers)
```

The HAL code will automatically detect ADS1115 and read 6 channels.

### 6h. Configure GPIO for Encoders (Optional)

Rotary encoders connect to GPIO pins for quadrature counting.

Example wiring (will need to adjust based on your setup):
```
Encoder 0 (Corner A):  CLK → GPIO 17,  DT → GPIO 18
Encoder 1 (Corner B):  CLK → GPIO 27,  DT → GPIO 22
Encoder 2 (Corner C):  CLK → GPIO 5,   DT → GPIO 6
Encoder 3 (Corner D):  CLK → GPIO 12,  DT → GPIO 13
```

Edit `hal/linux/nsynth_hal_linux.h` and update the `encoder_pins_` initialization if different.

---

## Step 7: Build the Firmware

### 7a. Get the Source Code

**Option A: Clone from GitHub**

```bash
cd ~/nsynth-mellotron
git clone <your-repo-url> .
```

**Option B: Copy via SCP**

From your desktop:
```bash
scp -r ./nsynth-mellotron/* pi@192.168.1.100:~/nsynth-mellotron/
```

**Option C: Manual Transfer via USB**

Copy all `.h` and `.cpp` files to the Pi using a USB thumb drive.

### 7b. Prepare Linux CMakeLists.txt

Create/update `CMakeLists.txt` for Linux:

```cmake
cmake_minimum_required(VERSION 3.18)
project(nsynth_mellotron)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fno-exceptions -fno-rtti")

# Find required packages
find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(.)
include_directories(hal/linux)

# Source files
set(SOURCES
    main_linux.cpp
    audio_engine.cpp
    adsr.cpp
    sample_manager.cpp
    ui_controller.cpp
    midi_handler.cpp
    preset_system.cpp
)

# Create executable
add_executable(nsynth_mellotron ${SOURCES})

# Link libraries
target_link_libraries(nsynth_mellotron 
    PRIVATE ALSA::ALSA Threads::Threads
)

# Output directory
set_target_properties(nsynth_mellotron PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
```

Place this in `~/nsynth-mellotron/CMakeLists.txt`

### 7c. Build

```bash
cd ~/nsynth-mellotron
mkdir -p build
cd build

# Configure:
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build (will take 2–5 minutes on Pi 3B+):
cmake --build . -j4

# Verify:
ls -lh bin/nsynth_mellotron
```

### 7d. Test Run

```bash
# Run the synth:
./bin/nsynth_mellotron

# You should see:
# ╔════════════════════════════════════════════════════════╗
# ║  NSynth Mellotron Firmware - Raspberry Pi 3B+ Edition  ║
# ║  Real-time Sample Playback Synthesizer                 ║
# ╚════════════════════════════════════════════════════════╝
# 
# [Init] Initializing subsystems...
# [Init] Scanning samples...
# [Audio] Starting audio processing thread...
# [MIDI] Starting MIDI input thread...
# [Control] Starting UI/control thread...
# [Main] ✓ All threads running!

# To stop: Press Ctrl+C
```

---

## Step 8: First Run

### 8a. Connect Hardware

1. **Audio:** Plug speakers/headphones into 3.5mm jack
2. **MIDI:** Connect USB MIDI keyboard to USB port (or serial adapter to GPIO)
3. **Display (optional):** I2C OLED on GPIO 2/3

### 8b. Boot Synth

```bash
cd ~/nsynth-mellotron/build
./bin/nsynth_mellotron
```

Expected output:
```
[Init] Scanning samples from /root/nsynth-samples...
[Init] Loaded 7 samples for Strings_Violin
[Init] All subsystems initialized!
[Main] ✓ All threads running!
[Main] Connect MIDI keyboard to /dev/ttyUSB0
[Main] Press Ctrl+C to exit.
```

### 8c. Play a Note

- Press any key on your MIDI keyboard
- You should hear audio output from the speakers

If you don't hear anything:
- See [Troubleshooting](#troubleshooting) below

### 8d. Test Parameters

With potentiometers/encoders connected:
- **P1 (Potentiometer 1):** Changes the page (0–7)
- **Encoders:** Adjust parameters for selected page
- **Pitch Bend:** Changes pitch in real-time
- **Mod Wheel (CC1):** Modulates the selected parameter

---

## Step 9: Auto-Start on Boot (Optional)

To run the synth automatically when the Pi boots:

### 9a. Create Systemd Service

```bash
sudo nano /etc/systemd/system/nsynth.service
```

Paste:

```ini
[Unit]
Description=NSynth Mellotron Synthesizer
After=network.target sound.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/nsynth-mellotron
ExecStart=/home/pi/nsynth-mellotron/build/bin/nsynth_mellotron
Restart=always
RestartSec=5

# Allocate real-time priority (optional, for low latency)
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=50

[Install]
WantedBy=multi-user.target
```

Save with **Ctrl+X**, **Y**, **Enter**.

### 9b. Enable & Start

```bash
# Enable on boot:
sudo systemctl enable nsynth

# Start now:
sudo systemctl start nsynth

# Check status:
sudo systemctl status nsynth

# View logs:
journalctl -u nsynth -f
```

To disable auto-start:
```bash
sudo systemctl disable nsynth
sudo systemctl stop nsynth
```

---

## Troubleshooting

### No Audio Output

**Symptom:** MIDI notes trigger, but no sound

**Diagnose:**
```bash
# Check audio device exists:
aplay -l

# Test speaker output:
speaker-test -c 2 -r 48000 -t wav -l 1

# Check ALSA is working:
alsamixer  # (use arrow keys to navigate, check volume levels)
```

**Solutions:**
1. Verify CS4271 is on I2S bus:
   ```bash
   i2cdetect -y 1
   # CS4271 should appear at 0x10 or 0x11
   ```
2. Check GPIO wiring (BCLK, LRCLK, DOUT connections)
3. Ensure audio jack is inserted
4. Try different speakers/headphones

### MIDI Not Working

**Symptom:** MIDI keyboard plugged in but synth doesn't respond

**Diagnose:**
```bash
# Check USB device detected:
lsusb | grep -i midi

# Check serial port exists:
ls -la /dev/ttyUSB*

# Monitor incoming MIDI bytes:
od -tx1 < /dev/ttyUSB0
# (plug keyboard, press a key - should see 0x90 XXXX... output)
```

**Solutions:**
1. Verify USB cable is plugged in
2. Try a different USB port
3. Set serial port permissions:
   ```bash
   sudo chmod 666 /dev/ttyUSB0
   ```
4. Check if it's the correct serial port:
   - Unplug USB adapter
   - `ls /dev/ttyUSB*` (should be empty)
   - Plug back in
   - `ls /dev/ttyUSB*` (should reappear)

### High CPU Usage / Crackling Audio

**Symptom:** Audio stutters, CPU usage > 90%

**Solutions:**
1. Reduce polyphony (change `16` to `8` in `main_linux.cpp`):
   ```cpp
   if (!g_audio_engine.initialize(8)) {  // Was 16
   ```
2. Reduce sample size (change `256` to `128`):
   ```cpp
   const int BUFFER_SIZE = 128;  // Was 256
   ```
3. Close background processes:
   ```bash
   ps aux | grep -v grep | grep -v nsynth
   sudo systemctl stop nginx apache2 mysql  # If running
   ```
4. Increase real-time priority (in systemd service):
   ```ini
   CPUSchedulingPolicy=fifo
   CPUSchedulingPriority=50
   ```

### Samples Not Loading

**Symptom:** Synth runs but says "0 samples loaded"

**Diagnose:**
```bash
# Check samples folder exists:
ls -la ~/nsynth-mellotron/samples/Strings_Violin/

# Check WAV file format:
file ~/nsynth-mellotron/samples/Strings_Violin/60_C4.wav
# Should say: "RIFF (little-endian) data, WAVE audio, mono 48000 Hz, 16 bit"

# Verify read permissions:
stat ~/nsynth-mellotron/samples/Strings_Violin/60_C4.wav
```

**Solutions:**
1. Ensure samples are in correct location: `~/nsynth-mellotron/samples/INSTRUMENT_NAME/`
2. Rename samples to match format: `{MIDI_NOTE}_{NOTE_NAME}.wav`
3. Check WAV file is 48 kHz, 16-bit, mono:
   ```bash
   ffprobe ~/nsynth-mellotron/samples/Strings_Violin/60_C4.wav
   ```
4. Verify read permissions:
   ```bash
   chmod 644 ~/nsynth-mellotron/samples/*/*.wav
   ```

### Display Not Showing (I2C OLED)

**Symptom:** OLED display stays blank

**Diagnose:**
```bash
# Check I2C device detected:
i2cdetect -y 1
# Should show OLED at address 0x3C or 0x3D

# Check I2C is enabled:
grep "^dtparam=i2c_arm" /boot/firmware/config.txt
# Should show: dtparam=i2c_arm=on
```

**Solutions:**
1. Enable I2C in `config.txt` and reboot
2. Verify I2C wiring (SDA → GPIO 2, SCL → GPIO 3)
3. Check I2C slave address matches (`0x3C` default, can be `0x3D`)
4. Update address in `hal/linux/nsynth_hal_linux.h`:
   ```cpp
   initialize(..., 0x3D)  // Change from 0x3C
   ```

### Build Errors

**"alsa/asoundlib.h: No such file"**
```bash
sudo apt install libasound2-dev
```

**"CMake not found"**
```bash
sudo apt install cmake
```

**"g++: error: command not found"**
```bash
sudo apt install build-essential
```

---

## Quick Reference

### Useful Commands

```bash
# Check synth status
sudo systemctl status nsynth

# View live logs
journalctl -u nsynth -f

# Stop synth
sudo systemctl stop nsynth

# Rebuild after code changes
cd ~/nsynth-mellotron/build && cmake --build . -j4

# Check system resources
top -p $(pgrep nsynth_mellotron)

# Monitor audio
pacmd list-sinks  # ALSA/PulseAudio info

# Test MIDI
aseqdump  # Shows incoming MIDI events in real-time
```

### Directory Structure (Final)

```
~/nsynth-mellotron/
├── CMakeLists.txt              # Build config (Linux)
├── main_linux.cpp              # Main entry point
├── audio_engine.{h,cpp}        # DSP core
├── adsr.{h,cpp}                # Envelope
├── sample_manager.{h,cpp}      # SD card/samples
├── ui_controller.{h,cpp}       # UI parameters
├── midi_handler.{h,cpp}        # MIDI parsing
├── preset_system.{h,cpp}       # Save/load
├── nsynth_types.h              # Data structures
│
├── hal/
│   └── linux/
│       └── nsynth_hal_linux.h  # Linux HAL (ALSA, GPIO, I2C)
│
├── samples/
│   ├── Strings_Violin/
│   │   ├── 60_C4.wav
│   │   ├── 62_D4.wav
│   │   └── ...
│   ├── Piano_Acoustic/
│   └── Brass_Trumpet/
│
├── presets/
│   └── (auto-created on first save)
│
├── build/
│   └── bin/
│       └── nsynth_mellotron    # Compiled executable
│
└── logs/
    └── nsynth.log              # Optional logging
```

---

## Summary

You now have a fully functioning NSynth Mellotron synthesizer running on Raspberry Pi 3B+!

### What Works
✅ Real-time audio playback (16-voice polyphony)
✅ MIDI keyboard input (Note On/Off, Pitch Bend, Mod Wheel)
✅ Potentiometer/encoder control
✅ Preset save/load (144 slots)
✅ Tape saturation + peak limiting
✅ ADSR envelopes

### Next Steps
- Record a MIDI performance
- Create more sample sets
- Add reverb/delay (CPU budget available)
- Build a custom enclosure with controls
- Integrate with Ableton Live via USB MIDI

---

## Support

If you encounter issues:

1. **Check logs:** `journalctl -u nsynth -f`
2. **Test individually:**
   - Audio: `speaker-test`
   - MIDI: `aseqdump`
   - Files: `ls samples/*/`
3. **Ask for help:** Include terminal output and your hardware setup

**Happy synthesizing!** 🎹🎵
