# NSynth Mellotron Firmware - Quick Start Guide

Get the synthesizer running in the shortest time possible.

---

## 5-Minute Setup (Testing on Desktop)

### Prerequisites
- C++17 compiler (GCC 7+, Clang 5+, or MSVC 2017+)
- CMake 3.16+
- ~2 MB disk space

### Build & Run

```bash
# Clone/download the project
git clone <repo> nsynth-mellotron
cd nsynth-mellotron

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j4

# Run tests
./bin/unit_tests

# Output: All tests passed ✓
```

**That's it!** The firmware core logic is verified. No hardware needed for testing.

---

## 30-Minute Setup (With Actual Hardware)

### What You Need
- NSynth Super hardware board
- ST-Link v2 USB programmer (~$20)
- USB cable for ST-Link
- Micro SD card (≤32 GB, FAT32)
- Audio speakers/headphones

### Setup Steps

#### 1. Prepare Hardware Files (5 min)

```bash
# Copy existing NSynth Super CubeMX project (if available)
# OR generate new one for STM32H745XI with:
# - I2S3 for audio output (48 kHz, 16-bit)
# - SPI1/SPI2 for SD card
# - USART3 for MIDI (31250 baud)
# - ADC1 for potentiometers (ch 0-9)
# - TIM1-4 for encoders (quadrature mode)
# - I2C1 for display

# Copy generated HAL to project:
cp -r <cubemx_project>/Drivers hal/stm32_drivers/
cp <cubemx_project>/STM32H745XIIx_FLASH.ld hal/
```

#### 2. Compile (5 min)

```bash
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../hal/arm_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j4

# Output: bin/nsynth_mellotron.elf
```

#### 3. Flash Hardware (10 min)

```bash
# Connect ST-Link to NSynth board (GND, 3.3V, SWDIO, SWCLK)

# Verify connection
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init" -c "targets" -c "exit"

# Flash firmware
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program bin/nsynth_mellotron.elf verify reset" -c "exit"

# Power cycle board → "Ready" message appears
```

#### 4. Prepare SD Card (10 min)

**Format:**
- Use official SD card formatter or OS tools
- File system: FAT32 (if ≤32GB) or exFAT
- Allocation unit: Default

**Create directories:**
```bash
# On SD card root:
mkdir SAMPLES
mkdir PRESETS
```

**Add samples:**
```bash
# Create one test instrument:
mkdir SAMPLES/TestViolin

# Add 8-16 WAV files:
# - Each 48 kHz, 16-bit mono, 0.5-5 seconds
# - Named: {MIDI_NOTE}_{NOTE_NAME}.wav
# Examples: 36_C2.wav, 60_C4.wav, 69_A4.wav

# Or download pre-made sample pack from:
# https://freesound.org/browse/
```

#### 5. Test

1. Insert SD card into NSynth
2. Power on → display shows "Ready"
3. Connect MIDI keyboard to serial MIDI input
4. Play notes → hear audio
5. Turn encoders → parameters change
6. Long-press XY pad to save preset

---

## Troubleshooting

### Desktop Build Fails

**Error:** `C++17 not available`
- Update compiler: `sudo apt-get install gcc-arm-none-eabi`
- Or change `CMAKE_CXX_STANDARD` to 14 in CMakeLists.txt

**Error:** `CMake not found`
- Install: `brew install cmake` (macOS) or `sudo apt-get install cmake` (Linux)

### Hardware Flash Fails

**Error:** `Could not find OpenOCD`
- Install: `brew install openocd` (macOS) or `apt-get install openocd` (Linux)

**Error:** `No ST-Link device found`
- Check USB cable (use data cable, not power-only)
- Verify JTAG header connections
- Try different USB port

**Error:** `Program returned non-zero exit`
- Possible flash memory already populated
- Try: `openocd ... -c "reset halt" ... -c "exit"`

### Audio Not Working

**No sound output:**
1. Check speakers connected and volume up
2. Verify SD card has samples in `/SAMPLES/`
3. Test with MIDI keyboard on channel 1

**Sound is distorted:**
1. Check potentiometer P3 (gain) is not maxed
2. Reduce tape saturation (Page 5, P5)
3. Check sample files are 16-bit PCM

### MIDI Not Responding

**Keyboard not triggering notes:**
1. Verify MIDI cable connected to serial port
2. Test MIDI baud rate: should be 31250
3. Try MIDI monitor app to verify signal reaching board

---

## Minimal Working Setup

**Absolute minimum to hear sound:**

```
SD Card Structure:
├── /SAMPLES/
│   └── Default/
│       └── 60_C4.wav    ← Single 48kHz WAV file
└── /PRESETS/            ← Empty, will populate on first save
```

**Workflow:**
1. Power on NSynth
2. Connect MIDI keyboard
3. Send MIDI Note 60 (middle C)
4. Hear sample playback
5. Adjust parameters with pots/encoders

**That's all you need to make sound!**

---

## Next Steps for More

### Add More Instruments
- Create folders: `/SAMPLES/Strings_Violin/`, `/SAMPLES/Piano_Acoustic/`, etc.
- Add 8-16 samples per instrument (across MIDI range 36-84)
- Switch instruments via encoder on Page 1

### Configure ADSR
- Page 2: Set attack, decay, sustain, release times
- Page 3: Set looping mode (One-Shot, Loop-Fade, Play-To-End)
- Page 4: Adjust dynamics (gain, limiter, saturation)

### Use Effects
- Page 5: Enable tape saturation (adds warmth/distortion)
- Page 4: Adjust limiter threshold (prevents clipping)
- Page 6: Add LFO modulation (vibrato/tremolo)

### Save Presets
- Page 5: Long-press XY pad to save current settings
- Tap to recall saved preset

### Fine Tuning
- Page 1: Adjust coarse/fine tuning per corner
- Page 7: Adjust MIDI channel, pot sensitivity, display brightness

---

## File Reference

| File | Purpose |
|------|---------|
| `main.cpp` | Firmware entry + thread coordination |
| `audio_engine.h/cpp` | Real-time DSP core |
| `sample_manager.cpp` | SD card sample loading (with FatFS) |
| `adsr.h/cpp` | Envelope generator |
| `ui_controller.h/cpp` | Parameter routing |
| `midi_handler.h/cpp` | MIDI parsing |
| `preset_system.h/cpp` | Save/recall |
| `hal/nsynth_hal.h` | Hardware abstraction |
| `tests/unit_tests.cpp` | Core logic tests |

---

## Performance Expectations

| Metric | Expected | Status |
|--------|----------|--------|
| **Latency** | <5 ms | Real-time audio |
| **Polyphony** | 16 voices | 4 corners × 4 voices each |
| **Pitch Range** | MIDI 0–127 | Full keyboard |
| **Sample Quality** | 48 kHz, 16-bit | Broadcast quality |
| **CPU Usage** | ~50% | Headroom for future features |

---

## Support Resources

- **Technical Details:** See `README.md` and `ARCHITECTURE.md`
- **Hardware Integration:** See `NSYNTH_INTEGRATION.md`
- **SD Card Setup:** See `SD_CARD_SETUP.md`
- **Building Guide:** See `BUILDING.md`

---

## Common Questions

**Q: Do I need to use the original NSynth Super Linux firmware?**  
A: No. This replaces it completely with a bare-metal firmware.

**Q: Can I go back to the original firmware?**  
A: Yes, if you backed up the original firmware binary. Otherwise, contact Magenta/Google for restoration files.

**Q: What if I don't have MIDI keyboard?**  
A: You can still test with MIDI software on a computer connected via USB MIDI adapter.

**Q: Can I add effects like reverb/delay?**  
A: Yes, they fit in the available CPU budget. Add them to the dynamics chain in `audio_engine.cpp`.

**Q: Will this work with other sample formats (MP3, FLAC)?**  
A: No, only 16-bit WAV at 48 kHz. Convert using ffmpeg: `ffmpeg -i input.mp3 -ar 48000 -ac 1 -acodec pcm_s16le output.wav`

---

## Quick Debugging Checklist

```
☐ Firmware compiles without errors
☐ Unit tests pass (./bin/unit_tests)
☐ ST-Link connected and detectable
☐ Firmware flashes successfully
☐ Display shows boot messages
☐ SD card detected (/SAMPLES found)
☐ Samples load (at least one instrument)
☐ MIDI input cable connected
☐ MIDI channel set correctly
☐ Audio output volume up
☐ First MIDI note triggers sound
☐ Parameters respond to encoders/pots
```

Once all checked, you have a working NSynth Mellotron!

---

## Next: Deep Dive

After getting basic functionality working:

1. **Customize DSP:** Add reverb, delay, filters in `audio_engine.cpp`
2. **Expand UI:** Add more parameter pages for new effects
3. **Optimize Performance:** Profile CPU usage and reduce if needed
4. **Add Features:** Sequencer, arpeggiator, scale snapping, etc.

---

**Ready to make music!** 🎹
