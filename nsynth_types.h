#pragma once

#include <cstdint>
#include <cmath>
#include <array>
#include <vector>
#include <string>

// ============================================================================
// FUNDAMENTAL CONSTANTS & CONFIGURATION
// ============================================================================

constexpr int SAMPLE_RATE = 48000;          // 48kHz audio
constexpr int AUDIO_BUFFER_SIZE = 256;      // Per-sample-block size
constexpr int MAX_POLYPHONY = 16;           // 16 voices max
constexpr int NUM_CORNERS = 4;              // 4-corner sampler layout
constexpr float TARGET_LATENCY_MS = 5.0f;   // <5ms latency target

// Pitch & Tuning
constexpr float A4_FREQ_HZ = 440.0f;
constexpr int MIDI_NOTE_A4 = 69;
constexpr float SEMITONE_RATIO = 1.059463094359f;  // 2^(1/12)
constexpr int MIN_MIDI_NOTE = 0;
constexpr int MAX_MIDI_NOTE = 127;

// ============================================================================
// ENUM TYPES
// ============================================================================

enum class Corner : uint8_t {
    A = 0,  // Top-Left
    B = 1,  // Top-Right
    C = 2,  // Bottom-Left
    D = 3   // Bottom-Right
};

enum class UIPage : uint8_t {
    CORNER_INSTRUMENTS = 0,
    CORNER_TUNING = 1,
    ADSR_ENVELOPES = 2,
    LOOPING_PLAYBACK = 3,
    PRESET_SAVE_RECALL = 4,
    DYNAMICS_FX = 5,
    MODULATION_LFO = 6,
    GLOBAL_SETTINGS = 7,
    NUM_PAGES = 8
};

enum class LoopMode : uint8_t {
    ONE_SHOT = 0,
    LOOP_FADE = 1,      // Crossfade during sustain
    PLAY_TO_END = 2     // Loop sustain, exit on release
};

enum class LFOWaveform : uint8_t {
    SINE = 0,
    TRIANGLE = 1,
    SQUARE = 2,
    RAMP = 3,
    RANDOM_SH = 4  // Sample & Hold
};

enum class PotentiometerMode : uint8_t {
    JUMP = 0,       // Immediate update
    CATCH_UP = 1,   // Wait for crossing
    RELATIVE = 2    // Proportional scaling
};

enum class VoiceState : uint8_t {
    IDLE = 0,
    ATTACK = 1,
    DECAY = 2,
    SUSTAIN = 3,
    RELEASE = 4
};

// ============================================================================
// AUDIO SAMPLE & METADATA STRUCTURES
// ============================================================================

struct SampleDescriptor {
    std::string filename;           // e.g., "36_C2.wav"
    uint8_t root_midi_note;         // MIDI note this sample was recorded at
    uint32_t sample_count;          // Total frames in WAV file
    uint32_t byte_offset;           // Offset in SD card file
    uint32_t loop_start_frame;      // Optional loop start marker
    uint32_t loop_end_frame;        // Optional loop end marker
    float sample_rate_original;     // Original sample rate (usually 48kHz)
};

struct InstrumentLibrary {
    std::string name;                                   // e.g., "String_Violin"
    std::vector<SampleDescriptor> samples;              // All multisamples
    uint32_t total_sample_data_bytes;                   // Total uncompressed
};

// ============================================================================
// PARAMETER STATE STRUCTURES
// ============================================================================

struct CornerParameters {
    // Instrument & Tuning
    uint8_t instrument_index;       // Index into library list
    int8_t coarse_semitones;        // [-24, +24]
    int8_t fine_cents;              // [-100, +100]
    bool is_active;                 // Corner on/off toggle

    // ADSR Envelope (milliseconds)
    float attack_ms;                // 1–5000 ms
    float decay_ms;                 // 1–10000 ms
    float sustain_level;            // 0.0–1.0
    float release_ms;               // 1–10000 ms

    // Looping
    LoopMode loop_mode;
    float loop_start_pct;           // 0–100%
    float loop_end_pct;             // 0–100%

    // LFO targeting this corner
    float lfo_pitch_depth_cents;    // ±200 cents
    float lfo_amp_depth;            // 0.0–1.0

    // Dynamics
    float gain_trim_db;             // Gainstaging trim
};

struct GlobalParameters {
    // Master mix
    float master_trim_db;           // -inf to +12 dB
    
    // XY Pad state (continuous)
    float xy_position_x;            // 0.0–1.0
    float xy_position_y;            // 0.0–1.0

    // Dynamics & FX
    float tape_saturation_drive;    // 0.0–1.0 (cubic soft-clip)
    float limiter_threshold_dbfs;   // -12 to 0 dBFS
    float limiter_ratio;            // 2.0, 4.0, 8.0, ∞

    // LFO Global
    float lfo_frequency_hz;         // 0.1–200 Hz
    LFOWaveform lfo_waveform;
    float lfo_phase;                // 0.0–1.0 (normalized)

    // MIDI & Control
    uint8_t midi_channel;           // 0–15
    float pitch_bend_semitones;     // ±2 to ±12 (configurable)
    float mod_wheel_depth;          // 0.0–1.0
    
    // UI/Display
    uint8_t display_brightness_pct; // 0–100%
    PotentiometerMode pot_mode;
    float xy_touch_sensitivity;     // 0.0–1.0

    // Preset Grid
    uint8_t last_loaded_preset_x;
    uint8_t last_loaded_preset_y;
};

// ============================================================================
// VOICE & POLYPHONY STRUCTURES
// ============================================================================

struct Voice {
    // Playback State
    VoiceState state;
    Corner assigned_corner;
    uint8_t midi_note;              // The note currently playing
    float velocity;                 // 0.0–1.0

    // Sample Playback
    const SampleDescriptor* current_sample;
    float playback_position;        // Fixed-point: frame index (float for interpolation)
    float playback_ratio;           // Resampling speed (for pitch shifting)

    // Envelope
    float envelope_value;           // Current envelope output [0.0, 1.0]
    float envelope_sample_count;    // Accumulator for ADSR state machine

    // Time tracking
    int64_t start_sample_index;     // When this voice was triggered
    int64_t release_sample_index;   // When key-release was received (-1 if not released)
};

// ============================================================================
// PRESET STRUCTURE
// ============================================================================

struct PresetState {
    std::string name;
    std::array<CornerParameters, NUM_CORNERS> corners;
    GlobalParameters global;
    int64_t timestamp_created;      // Unix timestamp
    int64_t timestamp_modified;
};

// ============================================================================
// UI STATE & CONTROL INTERFACE
// ============================================================================

struct ControlInput {
    // Encoders (4x corners)
    std::array<int32_t, NUM_CORNERS> encoder_deltas;   // Rotation delta per frame

    // Potentiometers (P1–P6)
    std::array<float, 6> potentiometer_values;         // 0.0–1.0 normalized

    // XY Pad
    float xy_touch_x;               // 0.0–1.0
    float xy_touch_y;               // 0.0–1.0
    bool xy_touch_active;

    // Buttons / touch matrix (if applicable)
    bool preset_save_request;
    uint8_t preset_recall_x, preset_recall_y;
};

struct UIState {
    UIPage current_page;
    uint8_t current_corner;         // For page-specific edits
    std::array<float, 6> active_parameter_values;      // Cached pot values per page
};

// ============================================================================
// FIXED-POINT UTILITIES
// ============================================================================

// Fixed-point type: 16.16 format (16 bits integer, 16 bits fraction)
using Fixed32 = int32_t;

inline Fixed32 float_to_fixed32(float value) {
    return static_cast<Fixed32>(value * 65536.0f);
}

inline float fixed32_to_float(Fixed32 value) {
    return static_cast<float>(value) / 65536.0f;
}

inline Fixed32 fixed32_add(Fixed32 a, Fixed32 b) {
    return a + b;
}

inline Fixed32 fixed32_mul(Fixed32 a, Fixed32 b) {
    return static_cast<Fixed32>((static_cast<int64_t>(a) * b) >> 16);
}

#endif  // NSYNTH_TYPES_H
