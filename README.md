# NSynth Super - Mellotron Alternative Firmware

A real-time, sample-playback synthesizer firmware for the Open NSynth Super hardware, replacing the default TensorFlow/Autoencoder engine with a deterministic Mellotron-style instrument.

## Project Overview

This firmware implements a four-corner audio mixer with independent sample playback, looping, ADSR envelopes, and effects processing—all running from an SD card without external TensorFlow dependencies.

**Key Features:**
- 16-voice polyphony with 4-corner XY blending
- Pitch-shifting resampling (linear/Hermite interpolation)
- Three looping modes (One-Shot, Loop-Fade, Play-To-End)
- Full ADSR envelope control per corner
- XY touch pad blending with weight calculation
- Tape saturation + peak limiter dynamics chain
- LFO modulation (Sine, Triangle, Square, Ramp, S&H)
- 12×12 preset grid for save/recall via touch matrix
- Potentiometer catch-up logic with mode selection
- MIDI Note On/Off, Pitch Bend, Mod Wheel support

---

## Architecture

### Three-Thread Design

The firmware follows a **decoupled controller-engine pattern** with three independent threads:

#### 1. **Audio Processing Thread** (Real-Time, Highest Priority)
- Runs in I2S DMA interrupt handler (~5ms blocks)
- Processes 16 voices with resampling and pitch shifting
- Applies ADSR envelopes, LFO modulation, and mixing
- Executes XY blending and dynamics/FX
- **Target Latency:** <5ms (AUDIO_BUFFER_SIZE = 256 samples @ 48kHz)

#### 2. **MIDI Event Thread** (Medium Priority, ~1kHz rate)
- Polls serial MIDI input (31.25kbps)
- Parses status/data bytes with running-status support
- Triggers voices on Note On (velocity-aware)
- Processes Pitch Bend (±range configurable) and CC (ModWheel, All Notes Off)
- Round-robin corner allocation across 4 instruments

#### 3. **Control Thread** (Low Priority, ~60-100Hz UI)
- Reads hardware controls (encoders, potentiometers, touch)
- Updates UI state machine and parameter routing
- Applies potentiometer catch-up logic
- Manages preset load/save workflow
- Refreshes display output

---

## Module Overview

### Core Components

#### **nsynth_types.h**
Fundamental data structures and enumerations:
- Sample descriptor (metadata from SD card)
- Voice pool state machine
- Parameter containers (per-corner and global)
- ADSR/LFO configuration
- Preset storage (JSON-serializable)
- Fixed-point utilities (16.16 format)

**Key Structs:**
- `Voice`: Playback state, ADSR envelope, sample descriptor
- `CornerParameters`: Instrument selection, tuning, ADSR, looping, gain
- `GlobalParameters`: Master trim, XY position, LFO state, dynamics threshold
- `PresetState`: Full system state snapshot (144 slots in 12×12 grid)

---

#### **audio_engine.h / .cpp**
Real-time DSP core (runs in interrupt handler):

**Features:**
- **Voice Management:** 16-voice pool with allocation/release
- **Resampling:** Linear and Hermite cubic interpolation
- **Pitch Shifting:** Speed ratio = 2^((midi_delta + cents/100) / 12)
- **ADSR Envelope:** Linear ramps with per-phase timing
- **Looping Logic:**
  - **One-Shot:** Plays 0% → 100%, ignores release
  - **Loop-Fade:** Crossfades Start ↔ End during sustain, exits on release
  - **Play-To-End:** Loops Start ↔ End, exits loop on release key
- **XY Blending:** W_A = (1-X)(1-Y), W_B = X(1-Y), W_C = (1-X)Y, W_D = XY
- **Dynamics Chain:**
  1. Per-corner ADSR × velocity
  2. XY blending (normalized 4-way sum)
  3. Master trim gain
  4. Tape saturation (cubic soft-clip)
  5. Peak limiter (brick-wall at threshold)

**Interface:**
```cpp
Voice* allocate_voice(Corner, uint8_t midi_note, float velocity);
void release_voice(Voice*);
void process_audio_block(float* buffer, int size);  // Called from I2S ISR
```

---

#### **sample_manager.h / .cpp**
SD card sample loading and metadata:

**Features:**
- Scans `/SAMPLES/{INSTRUMENT_NAME}/` directories
- Parses WAV filenames: `36_C2.wav` → MIDI note 36
- Builds descriptor array (sample rate, frame count, byte offset, loops)
- Nearest-neighbor pitch selection (minimize artifacts)
- Pitch-shift capping: ±6 semitones max per sample file

**Filename Convention:**
```
/SAMPLES/Strings_Violin/36_C2.wav     (MIDI note 36 = C2)
/SAMPLES/Strings_Violin/39_D#2.wav    (MIDI note 39 = D♯2)
/SAMPLES/Strings_Violin/43_G2.wav     (MIDI note 43 = G2)
```

**WAV File Assumptions:**
- 48kHz sample rate (or resampled on load)
- 16-bit signed PCM, mono
- Optional CUE/SMPL chunks for loop markers

---

#### **adsr.h / .cpp**
Envelope generator (linear segments):

**Timing (configurable):**
- **Attack:** 1–5000 ms (0 → 1.0 volume ramp)
- **Decay:** 1–10000 ms (1.0 → sustain ramp)
- **Sustain:** 0–100% (held indefinitely until release)
- **Release:** 1–10000 ms (sustain → 0 ramp)

**State Machine:**
```
IDLE → ATTACK → DECAY → SUSTAIN → RELEASE → IDLE
```

**Processing:** Per-sample-block output (e.g., 256 samples/block)

---

#### **preset_system.h / .cpp**
Save/recall with 12×12 touch matrix grid:

**Architecture:**
- Single Tap (<500ms): Load preset from `/PRESETS/PRESET_X_Y.JSON`
- Long Press (>1500ms): Overwrite slot with current state
- Touch coords (0.0–1.0) → grid index (0–11)
- JSON serialization (all parameters)

**Slot Mapping:**
```
(0,0) ─── (11,0)    Top row (left to right)
  │         │
(0,11) ── (11,11)   Bottom row
```

---

#### **ui_controller.h / .cpp**
Parameter state machine and control routing:

**8 Menu Pages:**

| Page | Purpose | Controls |
|------|---------|----------|
| 0 | Instruments | P2–P5 = Select instruments A–D |
| 1 | Tuning | P2 = corner, P3 = coarse (-24/+24 ST), P4 = fine (±100¢) |
| 2 | ADSR | P2 = corner, P3–P6 = A/D/S/R ms values |
| 3 | Looping | P2 = corner, P3/P4 = start/end %, P5 = mode |
| 4 | Presets | 12×12 touch matrix for save/recall |
| 5 | Dynamics | P3 = gain trim, P4 = limiter threshold, P5 = tape drive |
| 6 | LFO | P3 = freq (0.1–200 Hz), P4 = waveform, P5 = depth |
| 7 | Settings | P3 = brightness, P4 = touch sensitivity, P5 = pot mode |

**Potentiometer Modes:**
- **Jump:** Immediate value update on any movement
- **Catch-Up:** Lock until physical pot crosses software value
- **Relative:** Delta scaling from last position

---

#### **midi_handler.h / .cpp**
Serial MIDI parsing and voice triggering:

**Supported Messages:**
- **Note On (0x90):** Allocate voice (velocity-aware)
- **Note Off (0x80):** Release voice
- **Pitch Bend (0xE0):** ±8192 → ±semitones (configurable ±2–±12)
- **CC 1 (Mod Wheel):** LFO depth scaling
- **CC 120/123:** All Sounds Off / All Notes Off

**Voice Allocation:** Round-robin across 4 corners (fills corner A first, then B, C, D)

**MIDI State Machine:**
- Running status support (compact message format)
- SysEx rejection (not used)
- Per-channel filtering

---

### Main Orchestration (main.cpp)

**Initialization:**
```cpp
firmware_initialize()
├─ sample_manager.initialize("/SAMPLES")
├─ audio_engine.initialize()  // I2S, DMA setup
├─ preset_system.initialize("/PRESETS")
├─ ui_controller.initialize()
└─ midi_handler setup
```

**Thread Layout:**
```
Main Thread (Monitoring)
├── I2S Interrupt (Audio Real-Time)
│   └─ audio_engine.process_audio_block()
├── MIDI Thread (1kHz polling)
│   └─ midi_handler.process_midi_byte()
└── Control Thread (60Hz UI)
    └─ ui_controller.update_ui_frame()
```

**Synchronization:** `std::mutex` guards `GlobalAudioState` for parameter updates between threads.

---

## Building & Deployment

### Prerequisites
- C++17 compiler (GCC/Clang)
- CMake 3.16+
- Optional: STM32 HAL (for actual hardware)

### Build Steps

```bash
# Create build directory
mkdir build && cd build

# Configure project
cmake ..

# Compile
cmake --build .

# Output: ./bin/nsynth_firmware
```

### For Embedded Hardware (STM32)

Uncomment HAL configuration in `CMakeLists.txt`:
```cmake
# set(MCU "-mcpu=cortex-m7")
# set(FPU "-mfpu=fpv5-d16 -mfloat-abi=hard")
# target_link_libraries(nsynth_firmware PRIVATE stm32h7xx_hal)
```

Then flash to NSynth Super via ST-Link or similar programmer.

---

## SD Card Layout

```
/SAMPLES/
├─ Strings_Violin/
│  ├─ 36_C2.wav
│  ├─ 39_D#2.wav
│  └─ 43_G2.wav
├─ Piano_Acoustic/
│  ├─ 36_C2.wav
│  ├─ 39_D#2.wav
│  └─ ...
└─ Brass_Trumpet/
   └─ ...

/PRESETS/
├─ PRESET_00_00.JSON
├─ PRESET_00_01.JSON
└─ PRESET_11_11.JSON
```

---

## Performance & Constraints

| Metric | Target | Notes |
|--------|--------|-------|
| **Latency** | <5ms | AUDIO_BUFFER_SIZE=256 @ 48kHz |
| **Polyphony** | 16 voices | Across 4 corners |
| **Pitch Range** | ±6 semitones | Per sample before resampling |
| **LFO Rate** | 0.1–200 Hz | Sine, Triangle, Square, Ramp, S&H |
| **ADSR Timing** | 1ms–10s | Per phase |
| **Sample Rate** | 48 kHz | Fixed |
| **Display Refresh** | 60 Hz | ~16ms per frame |
| **MIDI Latency** | <10ms | Voice allocation + envelope |

---

## Future Enhancements

- [ ] Wavetable synthesis layer (complement samples)
- [ ] Arpeggiator with pattern sequencing
- [ ] MIDI Learn for CC remapping
- [ ] Analog Step Sequencer input
- [ ] USB MIDI support (in addition to serial)
- [ ] Internal sampler recording
- [ ] Stereo effects (delay, reverb)
- [ ] Touch matrix sensitivity per-zone tuning
- [ ] Firmware update via SD card
- [ ] State machine visualization on display

---

## Debugging Tips

1. **Active Voice Count:** Monitor in control thread for stuck notes
2. **I2S Buffer Underrun:** Indicates audio engine overload
3. **MIDI Parsing Errors:** Check 31.25kbps baud rate
4. **SD Card Issues:** Verify FAT32/exFAT and filename format
5. **Pitch Artifacts:** Lower `MAX_PITCH_SHIFT_SEMITONES` or use more samples

---

## References

- **NSynth Super Hardware:** https://github.com/google/open-nsynth-super
- **MIDI Specification:** https://www.midi.org/specifications
- **Resampling Theory:** https://en.wikipedia.org/wiki/Sample-rate_conversion
- **Envelope Generation:** "The Audio Programming Book" (Boulanger & Lazzarini)

---

## License

**[Specify your license here]**

This firmware is provided as-is for educational and experimental use on the Open NSynth Super platform.

---

## Contact & Support

For issues, questions, or contributions, please file an issue or pull request in the project repository.
