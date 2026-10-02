# NSynth Mellotron Firmware - Architecture & Design Patterns

## System Block Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    NSynth Super Hardware                         │
│  ┌──────────────────┐    ┌──────────────────┐  ┌────────────┐   │
│  │   STM32H7 MCU    │    │    SD Card Slot  │  │  Display   │   │
│  │  (480 MHz)       │    │  (FAT32/exFAT)   │  │  (OLED/LCD)│   │
│  │  • I2S (Audio)   │    │  • /SAMPLES      │  │  (SPI/I2C) │   │
│  │  • UART (MIDI)   │    │  • /PRESETS      │  │            │   │
│  │  • SPI (SD)      │    │  • Firmware      │  └────────────┘   │
│  │  • ADC (Pots)    │    │                  │                    │
│  │  • TIM (Enc)     │    └──────────────────┘  ┌────────────┐   │
│  │  • Touch (XY)    │                          │  UI Input  │   │
│  └──────────────────┘                          │  • 4x Enc  │   │
│                                                │  • 6x Pot  │   │
│                                                │  • XY Pad  │   │
│                                                │  • MIDI    │   │
│                                                └────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Thread Architecture

```
                         ┌──────────────────────────┐
                         │   Main Thread (Monitor)  │
                         │   - System health check  │
                         │   - Graceful shutdown    │
                         └──────────────────────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    │               │               │
         ┌──────────▼──────────┐   │   ┌──────────▼─────────┐
         │  Audio Interrupt    │   │   │  MIDI Event Thread │
         │  (Highest Priority) │   │   │  (Medium Priority) │
         │  • 10.67 kHz        │   │   │  • ~1 kHz rate     │
         │  • <5ms latency     │   │   │  • Serial parse    │
         │  • I2S ISR callback │   │   │  • Voice alloc     │
         │  • DSP/Mixing       │   │   │  • Pitch/CC mod    │
         └────────────────────┘   │   └────────────────────┘
                                  │
                    ┌─────────────▼──────────────┐
                    │  Control Thread (60Hz UI)  │
                    │  (Low Priority)            │
                    │  • Encoder/Pot polling     │
                    │  • Display refresh         │
                    │  • Param state machine     │
                    │  • Preset load/save        │
                    └────────────────────────────┘
                                  │
                    ┌─────────────▼──────────────┐
                    │  Shared State (Mutex)      │
                    │  • AudioEngine             │
                    │  • Parameters              │
                    │  • Voice pool              │
                    └────────────────────────────┘
```

---

## Data Flow Diagrams

### 1. MIDI Note On → Voice Allocation → Audio Output

```
Serial MIDI Input (31.25 kbps)
         │
         ▼
   ┌──────────────────────┐
   │  MIDI Event Thread   │
   │  - Parse byte        │
   │  - Running status    │
   │  - Extract note+vel  │
   └──────────────────────┘
         │
         ├─ Note On (0x90)
         │  └─ channel filter
         │     └─ velocity→float
         │
         ▼
   ┌──────────────────────┐
   │  AudioEngine         │
   │  allocate_voice()    │
   │  - Find idle voice   │
   │  - Set MIDI note     │
   │  - Select sample     │
   │  - Calc pitch ratio  │
   │  - Trigger envelope  │
   └──────────────────────┘
         │
         ▼
   ┌──────────────────────┐
   │  Voice Playback      │
   │  (next I2S block)    │
   │  - Resampling loop   │
   │  - ADSR envelope     │
   │  - XY blending       │
   │  - Dynamics          │
   │  - Output buffer     │
   └──────────────────────┘
         │
         ▼
   I2S DMA → Codec → Audio Jack
```

### 2. Parameter Update Flow (Potentiometer → Audio)

```
Control Thread (60 Hz UI)
         │
         ├─ Read analog pots (P1–P6)
         ├─ Apply catch-up logic
         │
         ▼
   ┌──────────────────────┐
   │  UIController        │
   │  update_ui_frame()   │
   │  - Current page      │
   │  - Route pots→param  │
   └──────────────────────┘
         │
         ├─ Page 0: Instrument selection
         ├─ Page 1: Coarse/fine tuning
         ├─ Page 2: ADSR times
         ├─ Page 5: Dynamics FX
         │  └─ Gain trim, Limiter, Tape sat
         │
         ▼
   ┌──────────────────────┐
   │  GlobalParameters /  │
   │  CornerParameters    │
   └──────────────────────┘
         │
         ▼ (Mutex lock)
   ┌──────────────────────┐
   │  Audio Engine        │
   │  set_*_params()      │
   │  - Cache updates     │
   │  - Apply next block  │
   └──────────────────────┘
         │
         ▼
   Real-time DSP (next I2S interrupt)
```

### 3. Looping & Release Behavior

```
Note On
  │
  ▼
Sustain Phase: Loop Start ←→ Loop End (continuous)
  │
  ├─ Loop Mode: ONE_SHOT
  │  └─ Play 0% → 100%, ignore release
  │
  ├─ Loop Mode: LOOP_FADE
  │  └─ Crossfade Start ↔ End
  │     Release → Envelope RELEASE
  │
  └─ Loop Mode: PLAY_TO_END
     └─ Loop Start ↔ End
        Release → Exit loop, play End → 100%
        → Envelope RELEASE
  │
  ▼
Release Phase (ADSR release time)
  │
  ▼
Envelope → 0
  │
  ▼
Voice IDLE (return to pool)
```

---

## XY Blending Algorithm

### Four-Corner Mix Calculation

```
┌─────────────────────────────────────────┐
│  XY Touch Position: (0.0–1.0, 0.0–1.0) │
└─────────────────────────────────────────┘
              │
              ▼
    ┌─────────────────────┐
    │  Calculate Weights  │
    │  W_A = (1-X)*(1-Y)  │  ← Top-Left
    │  W_B = X*(1-Y)      │  ← Top-Right
    │  W_C = (1-X)*Y      │  ← Bottom-Left
    │  W_D = X*Y          │  ← Bottom-Right
    └─────────────────────┘
              │
              ▼
    ┌─────────────────────┐
    │  Corner Mix Buffers │
    │  [Buf_A, Buf_B,     │
    │   Buf_C, Buf_D]     │
    └─────────────────────┘
              │
              ▼
    ┌─────────────────────┐
    │  Blend & Normalize  │
    │  output = sum(      │
    │   Buf_i * W_i       │
    │  ) / sum(W_i)       │
    │  (or max 6dB atten) │
    └─────────────────────┘
              │
              ▼
    Output Sample to DAC
```

### Visual Example

```
                Y=1.0
                  ▲
                  │
        Pad Zone  │  Zone C (X=0.5, Y=0.75)
                  │
    ┌─────────────┼─────────────┐
    │ (0,1) C=100%│C,D=50%  (1,1)│
    │             │D=100%        │
    │ Centered=  │             │
    │ ┌─────┐    │    ┌─────┐  │
    │ │ XY  │    │    │ All │  │
    │ │ Pad │    │    │ 25% │  │
    │ └─────┐    │    └─────┘  │
    │       │    │             │
    ├───────┼────┼─────────────┤ Y=0.5
    │       │    │             │
    │  A=50%│    │    B=50%   │
    │  C=50%│    │    D=50%   │
    │       │    │             │
    │(0,0)  │    │  (1,0)      │
    │A=100% │    │  B=100%    │
    └───────┴────┴─────────────┘
              X=0.0       X=1.0
```

---

## Voice State Machine

```
       ┌───────────────────┐
       │   Voice IDLE      │
       │ (available pool)  │
       └─────────┬─────────┘
                 │
        allocate_voice()
         Note On received
                 │
                 ▼
       ┌─────────────────┐
       │  ATTACK Phase   │◄──────┐
       │  (envelope)     │       │
       │  ↑ 0 → 1.0      │       │
       │  (t: attack_ms) │  retrigger
       │  │              │  without
       └──┼──────────────┘  release
          │
          ▼ (attack complete)
       ┌─────────────────┐
       │  DECAY Phase    │
       │  1.0 → sustain  │
       │  (t: decay_ms)  │
       └────────┬────────┘
                │
                ▼ (decay complete)
       ┌─────────────────┐
       │ SUSTAIN Phase   │
       │ Hold level 0–1.0│
       │ (indefinite)    │◄───┐
       │                 │    │ (while key pressed)
       └────────┬────────┘    │
                │             │
        release_voice()       │
        Note Off received     │
                │             │
                ├─────────────┘
                │
                ▼
       ┌─────────────────┐
       │ RELEASE Phase   │
       │ sustain → 0     │
       │ (t: release_ms) │
       └────────┬────────┘
                │
                ▼ (envelope reaches 0)
       ┌─────────────────┐
       │   Voice IDLE    │
       │ (return to pool)│
       └─────────────────┘
```

---

## Parameter Routing by Page

```
Page 0: CORNER_INSTRUMENTS
    P1: Page Select (0–7)
    P2: Corner A Instrument
    P3: Corner B Instrument
    P4: Corner C Instrument
    P5: Corner D Instrument

Page 1: CORNER_TUNING
    P1: Page Select
    P2: Select Corner (A–D)
    P3: Coarse Semitones (-24 to +24)
    P4: Fine Cents (-100 to +100)
    P5: (unused)

Page 2: ADSR_ENVELOPES
    P1: Page Select
    P2: Select Corner (A–D)
    P3: Attack (1–5000 ms)
    P4: Decay (1–10000 ms)
    P5: Sustain (0–100%)
    P6: Release (1–10000 ms)

Page 3: LOOPING_PLAYBACK
    P1: Page Select
    P2: Select Corner (A–D)
    P3: Loop Start % (0–100)
    P4: Loop End % (0–100)
    P5: Release Mode (One-Shot / Loop-Fade / Play-To-End)
    P6: (unused)

Page 4: PRESET_SAVE_RECALL
    P1: Page Select
    12×12 Touch Matrix:
    - Tap (<500ms) → Load preset
    - Hold (>1500ms) → Save preset

Page 5: DYNAMICS_FX
    P1: Page Select
    P2: (unused)
    P3: Master Trim Gain (±12 dB)
    P4: Limiter Threshold (-12 to 0 dBFS)
    P5: Tape Saturation Drive (0–100%)
    P6: Limiter Ratio (2, 4, 8, ∞)

Page 6: MODULATION_LFO
    P1: Page Select
    P2: Select Corner (A–D)
    P3: LFO Frequency (0.1–200 Hz)
    P4: LFO Waveform (Sine/Triangle/Square/Ramp/S&H)
    P5: LFO Pitch Depth (±200 cents)
    P6: LFO Amplitude Depth (0–100%)

Page 7: GLOBAL_SETTINGS
    P1: Page Select
    P2: (unused)
    P3: Display Brightness (0–100%)
    P4: XY Touch Sensitivity (0–100%)
    P5: Potentiometer Mode (Jump/Catch-Up/Relative)
    P6: MIDI Channel (1–16)
```

---

## Dynamics Processing Chain

```
Per-Voice Output
       │
       ▼
    [ADSR Envelope]  ×
    [Velocity 0–1.0] ×
       │
       ▼
    Per-Corner Sum
       │
    [XY Blending]
    [4-Way Mix]
       │
       ▼
    Master Output
       │
       ├─ [Master Trim Gain]
       │  (-inf to +12 dB)
       │
       ▼
    [Tape Saturation]
    f(x) = x - (drive × x³/3)
    Cubic soft-clip [-1, +1]
       │
       ▼
    [Peak Limiter]
    Brickwall compressor:
    • Threshold: -12 to 0 dBFS
    • Attack: 0.5 ms
    • Release: 50 ms
    • Ratio: ∞ (hard clip)
       │
       ▼
    I2S DAC Output
```

---

## Potentiometer Catch-Up Logic

### Problem
- Potentiometers are hardware analog inputs (cannot track software state)
- Switching UI pages creates discontinuous parameter values
- "Jump" mode causes sudden audio drops/pops

### Solution: Catch-Up Mode (Default)

```
Software Value [Locked]
       │
       ├─ Physical Pot Moves
       │  (e.g., 0.3 → 0.35)
       │
       ▼ Delta = |0.35 - 0.5| = 0.15
       │
       ├─ Delta < Threshold (0.02)?
       │  No → Remain locked
       │  │
       │  Yes → Pot has crossed software value
       │        → Unlock & sync to physical
       │
       ▼
New Software Value = Physical Value
(No discontinuity, smooth parameter)
```

### Modes Available

```
1. JUMP MODE
   ├─ Parameter updates immediately
   ├─ Causes audio pops on page switch
   └─ Use for: Non-audio parameters

2. CATCH_UP MODE (Default)
   ├─ Lock to last value until pot crosses
   ├─ Smooth transitions, no pops
   └─ Use for: Audio parameters

3. RELATIVE MODE
   ├─ Physical delta scales parameter
   ├─ ∆param = ∆physical × (max - min)
   └─ Use for: Knobs with fine/coarse control
```

---

## File Structure Recap

```
nsynth-mellotron-firmware/
├── README.md                # Main documentation
├── BUILDING.md              # Build & deployment guide
├── ARCHITECTURE.md          # This file
│
├── CMakeLists.txt           # Build configuration
│
├── nsynth_types.h           # Core types & constants
├── adsr.h / .cpp            # Envelope generator
├── audio_engine.h / .cpp    # Real-time DSP core
├── sample_manager.h / .cpp  # SD card sample loading
├── preset_system.h / .cpp   # Save/recall system
├── ui_controller.h / .cpp   # Parameter routing & UI
├── midi_handler.h / .cpp    # MIDI parsing
│
└── main.cpp                 # Firmware entry point & threads
```

---

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **Three-thread design** | Decoupling allows real-time audio + responsive UI |
| **Fixed-point arithmetic (16.16)** | Faster than float on embedded MCU; less code |
| **Linear ADSR only** | Simplicity; exponential curves optional later |
| **No dynamic memory** | Embedded safety; voice pool pre-allocated |
| **XY blending (normalized)** | Prevents accidental volume clipping |
| **Catch-up potentiometer mode** | Eliminates parameter jumps during page transitions |
| **JSON presets** | Human-readable; easy debug; standardizes save format |
| **Placeholder I/O routines** | Allows desktop testing; easy hardware integration |

---

## Performance Estimates

| Component | Time | Notes |
|-----------|------|-------|
| **Per-voice DSP** | ~500 µs | Resample + ADSR + LFO @ 48kHz |
| **XY blending** | ~50 µs | 4-corner sum + normalize |
| **Dynamics chain** | ~100 µs | Tape sat + limiter |
| **Total audio block** | ~5 ms | 256 samples @ 48kHz (slack: 5 ms available) |
| **MIDI parsing** | <100 µs | Running-status parsing |
| **UI update** | ~5 ms | Potentiometer polling, display refresh |

**CPU Budget (Cortex-M7 @ 480 MHz):**
- Audio ISR: 5 ms / 10.67 ms (47% utilization)
- MIDI thread: <1 ms per frame
- Control thread: <5 ms @ 60 Hz (idle most of the time)
- Headroom: ~40% for future features

---

For detailed implementation notes, see code comments in individual modules.
