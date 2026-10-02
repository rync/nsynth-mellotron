# NSynth Mellotron - SD Card Setup Guide

This document describes how to organize sample files and presets on the SD card for the NSynth Mellotron firmware.

---

## Directory Structure

```
SD Card (FAT32 or exFAT)
│
├── /SAMPLES/                          # Sample library root
│   ├── Strings_Violin/                # Instrument folder (any name)
│   │   ├── 36_C2.wav                  # MIDI note 36 (C2) sample
│   │   ├── 39_D#2.wav                 # MIDI note 39 (D♯2) sample
│   │   ├── 43_G2.wav                  # MIDI note 43 (G2) sample
│   │   ├── 48_C3.wav
│   │   ├── 53_F3.wav
│   │   ├── 57_A3.wav                  # Up to 16 samples recommended
│   │   └── 62_D4.wav
│   │
│   ├── Piano_Acoustic/
│   │   ├── 36_C2.wav
│   │   ├── 39_D#2.wav
│   │   ├── 43_G2.wav
│   │   └── ...
│   │
│   ├── Brass_Trumpet/
│   │   ├── 36_C2.wav
│   │   └── ...
│   │
│   └── Bells_Glockenspiel/
│       └── ...
│
├── /PRESETS/                          # Preset grid (12×12 = 144 slots)
│   ├── PRESET_00_00.JSON              # Slot (0,0) - top-left
│   ├── PRESET_00_01.JSON              # Slot (0,1)
│   ├── PRESET_01_00.JSON              # Slot (1,0)
│   ├── ...
│   └── PRESET_11_11.JSON              # Slot (11,11) - bottom-right
│
└── /FIRMWARE/                         # Optional: firmware binaries
    └── nsynth_mellotron.bin           # (For future updates)
```

---

## Sample File Organization

### Filename Convention

Samples **must** follow this naming format:

```
{MIDI_NOTE}_{NOTE_NAME}.wav
```

**Examples:**
- `36_C2.wav` → MIDI note 36 = C2 (low C)
- `60_C4.wav` → MIDI note 60 = C4 (middle C)
- `69_A4.wav` → MIDI note 69 = A4 (concert pitch)
- `39_D#2.wav` → MIDI note 39 = D♯2 (D sharp 2)

### MIDI Note Number Reference

| Note | C | C♯ | D | D♯ | E | F | F♯ | G | G♯ | A | A♯ | B |
|------|---|----|----|----|----|---|----|---|----|----|----|----|
| 0    | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 |
| 1    | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 | 32 | 33 | 34 | 35 |
| 2    | **36** | 37 | 38 | 39 | 40 | 41 | 42 | **43** | 44 | 45 | 46 | 47 |
| 3    | 48 | 49 | 50 | 51 | 52 | 53 | 54 | 55 | 56 | 57 | 58 | 59 |
| 4    | **60** | 61 | 62 | 63 | 64 | 65 | 66 | 67 | 68 | **69** | 70 | 71 |
| 5    | 72 | 73 | 74 | 75 | 76 | 77 | 78 | 79 | 80 | 81 | 82 | 83 |
| 6    | 84 | 85 | 86 | 87 | 88 | 89 | 90 | 91 | 92 | 93 | 94 | 95 |
| 7    | 96 | 97 | 98 | 99 | 100| 101| 102| 103| 104| 105| 106| 107|

**Common Reference:**
- C2 = 36 (low note)
- C4 = 60 (middle C)
- A4 = 69 (concert pitch, 440 Hz)
- C8 = 108 (high note, near max MIDI range)

### WAV File Requirements

Each sample must be a **16-bit PCM WAV file** with these specs:

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sample Rate | 48 kHz | Exact (will not be resampled on load) |
| Bit Depth | 16-bit | Mono or stereo (stereo will be summed to mono) |
| Format | PCM | Not compressed (no MP3, FLAC, etc.) |
| Duration | 0.5–5 seconds | Recommended for playback latency |
| Max File Size | ~10 MB | Per sample |

**Creating WAV Files:**

Use **Audacity** (free) or similar:
1. Import audio file
2. Export → WAV, 48000 Hz, 16-bit PCM, Mono
3. Rename to `{MIDI_NOTE}_{NOTE_NAME}.wav`

Or command line (ffmpeg):
```bash
ffmpeg -i source.mp3 -ar 48000 -ac 1 -acodec pcm_s16le output.wav
```

---

## Instrument Library Structure

### Minimal Setup (1 instrument, 8 samples)

For a quick test, create one instrument:

```
/SAMPLES/TestViolin/
├── 36_C2.wav
├── 41_F2.wav
├── 45_A2.wav
├── 48_C3.wav
├── 52_E3.wav
├── 55_G3.wav
├── 60_C4.wav
└── 64_E4.wav
```

**Number of samples:** 7–16 per instrument is typical
- **Fewer samples** → More pitch-shifting, higher potential for artifacts
- **More samples** → Better quality, more SD card space

### Sample Selection Strategy

Choose samples across the full keyboard range:

**Low notes (C2–G2):**
- 36 (C2), 41 (F2), 43 (G2), 45 (A2)

**Mid-low (C3–G3):**
- 48 (C3), 50 (D3), 52 (E3), 55 (G3)

**Mid-high (C4–G4):**
- 60 (C4), 62 (D4), 64 (E4), 67 (G4)

**High (C5+):**
- 72 (C5), 76 (E5), 79 (G5)

---

## Preset Grid Layout

### 12×12 Touch Matrix Mapping

The XY touch pad maps to a 12×12 grid of preset slots:

```
    0   1   2   3   4   5   6   7   8   9  10  11   (X →)
  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
0 │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │
  ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
1 │12 │13 │14 │15 │16 │17 │18 │19 │20 │21 │22 │23 │
  ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
2 │24 │25 │26 │27 │28 │29 │30 │31 │32 │33 │34 │35 │
  ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
3 │36 │37 │38 │39 │40 │41 │42 │43 │44 │45 │46 │47 │
  ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
4 │48 │49 │50 │51 │52 │53 │54 │55 │56 │57 │58 │59 │
  ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
...
  ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
11│132│133│134│135│136│137│138│139│140│141│142│143│
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
  (Y ↓)
```

**Touch Zones:**
- **Top-left** → Preset 0
- **Top-right** → Preset 11
- **Bottom-left** → Preset 132
- **Bottom-right** → Preset 143

### Preset File Format (JSON)

Each preset is a JSON file:

```json
{
  "name": "Violin Pad",
  "timestamp_created": 1696263841,
  "timestamp_modified": 1696263901,
  "corners": [
    {
      "instrument_index": 0,
      "coarse_semitones": 0,
      "fine_cents": 0,
      "is_active": true,
      "attack_ms": 50.0,
      "decay_ms": 200.0,
      "sustain_level": 0.8,
      "release_ms": 1000.0,
      "loop_mode": 1,
      "loop_start_pct": 0.0,
      "loop_end_pct": 100.0,
      "lfo_pitch_depth_cents": 0.0,
      "lfo_amp_depth": 0.0,
      "gain_trim_db": 0.0
    },
    { ... },  // Corners B, C, D
    { ... },
    { ... }
  ],
  "global": {
    "master_trim_db": 0.0,
    "xy_position_x": 0.5,
    "xy_position_y": 0.5,
    "tape_saturation_drive": 0.2,
    "limiter_threshold_dbfs": -3.0,
    "limiter_ratio": 4.0,
    "lfo_frequency_hz": 5.0,
    "lfo_waveform": 0,
    "lfo_phase": 0.0,
    "midi_channel": 0,
    "pitch_bend_semitones": 2.0,
    "mod_wheel_depth": 1.0,
    "display_brightness_pct": 100,
    "pot_mode": 1,
    "xy_touch_sensitivity": 1.0
  }
}
```

---

## Prepare Your SD Card

### Step 1: Format SD Card

- **File System:** FAT32 (SD cards ≤32GB) or exFAT (>32GB)
- **Allocation Unit Size:** Default (4 KB)
- Use official SD card formatter or OS tools

### Step 2: Create Directory Structure

```bash
mkdir /Volumes/SDCARD/SAMPLES
mkdir /Volumes/SDCARD/PRESETS
mkdir /Volumes/SDCARD/FIRMWARE
```

### Step 3: Add Sample Instruments

Copy organized instrument folders:
```bash
cp -r ~/Samples/Violin_Samples/* /Volumes/SDCARD/SAMPLES/Strings_Violin/
cp -r ~/Samples/Piano_Samples/* /Volumes/SDCARD/SAMPLES/Piano_Acoustic/
```

Ensure files follow naming: `{MIDI_NOTE}_{NOTE_NAME}.wav`

### Step 4: Verify Structure

Eject and re-insert SD card to verify all files are written correctly. Use file manager to check:
- `/SAMPLES/` contains instrument subdirectories
- Each instrument folder has 7–16 WAV files
- Filenames follow convention (e.g., `60_C4.wav`)

---

## First-Time Startup

When firmware first boots:

1. **Sample Loading** (appears on display)
   - Scans `/SAMPLES/` directory
   - Loads all WAV metadata (not audio data)
   - Selects first instrument as default

2. **Preset System**
   - Checks `/PRESETS/` directory
   - Creates if missing (first save will create files)

3. **Ready**
   - UI displays "Ready"
   - Awaits MIDI input or encoder commands

---

## Troubleshooting

### Samples Not Loading

**Symptom:** "No samples found" error

**Fixes:**
1. Verify `/SAMPLES/` folder exists at SD card root
2. Check instrument folders are inside `/SAMPLES/`
3. Ensure WAV files use exact naming: `{MIDI_NOTE}_{NOTE_NAME}.wav`
4. Verify sample rate is 48 kHz (check with ffmpeg or Audacity)

### Playback Quality Issues

**Symptom:** Audio artifacts, distortion, or pitch bend problems

**Causes & Fixes:**
- WAV files not 16-bit PCM → Re-export with correct settings
- Sample rate not 48 kHz → Resample using ffmpeg or Audacity
- Too few samples per instrument → Add more samples across range
- SD card corruption → Reformat and reload samples

### Preset Loading Fails

**Symptom:** Presets don't save or load

**Fixes:**
1. Verify `/PRESETS/` folder exists
2. Check SD card has write permissions
3. Try formatting and reloading one preset manually
4. Clear /PRESETS and let firmware create fresh

---

## Sample Library Recommendations

### Minimum Setup (1 instrument, 8 samples)
```
/SAMPLES/DefaultInstrument/
├── 36_C2.wav    ← Lowest
├── 43_G2.wav
├── 48_C3.wav
├── 55_G3.wav
├── 60_C4.wav
├── 67_G4.wav
├── 72_C5.wav
└── 79_G5.wav    ← Highest
```

### Recommended Setup (4 instruments, 12 samples each)
- **Strings** (Violin, Cello, Ensemble)
- **Brass** (Trumpet, Horn, Trombone)
- **Woodwinds** (Flute, Oboe, Clarinet)
- **Percussion** (Piano, Bells, Marimba)

Each instrument: 12 samples spread across 3+ octaves

---

## Tips for Best Results

1. **Sample Quality:** Use clear, attack-defined recordings (short release time)
2. **Sample Consistency:** Same instrument family should sound cohesive
3. **Looping:** Mark natural loop points in WAV (CUE/SMPL chunks) for smooth sustain
4. **Presets:** Save good-sounding combinations frequently
5. **Backup:** Keep samples and presets backed up to computer

---

For detailed audio file creation instructions, see **BUILDING.md** → "Creating Sample Libraries".
