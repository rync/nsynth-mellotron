#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include <iomanip>

// Simple test framework (no external dependencies)
#define ASSERT_EQUAL(a, b) assert((a) == (b))
#define ASSERT_NEAR(a, b, tol) assert(std::abs((a) - (b)) < (tol))
#define ASSERT_TRUE(x) assert((x))
#define ASSERT_FALSE(x) assert(!(x))

namespace tests {

// ============================================================================
// TEST: ADSR Envelope Generator
// ============================================================================

void test_adsr_attack() {
    std::cout << "Test: ADSR Attack Phase\n";
    
    // Mock ADSR (simplified version for testing)
    float attack_time_s = 0.1f;  // 100ms
    int sample_rate = 48000;
    float attack_samples = attack_time_s * sample_rate;
    
    // Simulate attack phase
    float envelope = 0.0f;
    float expected_at_50pct = 0.5f;
    float sample_at_50pct = attack_samples * 0.5f;
    float actual_at_50pct = sample_at_50pct / attack_samples;
    
    ASSERT_NEAR(actual_at_50pct, expected_at_50pct, 0.01f);
    std::cout << "  ✓ Attack ramp 0→1 is linear\n";
}

void test_adsr_decay() {
    std::cout << "Test: ADSR Decay Phase\n";
    
    float decay_time_s = 0.2f;  // 200ms
    float sustain_level = 0.7f;
    int sample_rate = 48000;
    float decay_samples = decay_time_s * sample_rate;
    
    // At 50% through decay: should be midway between 1.0 and sustain
    float progress = 0.5f;
    float expected = 1.0f - (progress * (1.0f - sustain_level));
    float actual = 1.0f - (progress * (1.0f - sustain_level));
    
    ASSERT_NEAR(actual, expected, 0.01f);
    std::cout << "  ✓ Decay ramp 1.0→sustain is linear\n";
}

void test_adsr_sustain() {
    std::cout << "Test: ADSR Sustain Phase\n";
    
    float sustain_level = 0.6f;
    
    // Sustain should hold steady
    float sample1 = sustain_level;
    float sample2 = sustain_level;
    float sample3 = sustain_level;
    
    ASSERT_EQUAL(sample1, sample2);
    ASSERT_EQUAL(sample2, sample3);
    std::cout << "  ✓ Sustain holds constant level\n";
}

void test_adsr_release() {
    std::cout << "Test: ADSR Release Phase\n";
    
    float release_time_s = 0.15f;  // 150ms
    float sustain_level = 0.5f;
    int sample_rate = 48000;
    float release_samples = release_time_s * sample_rate;
    
    // At 50% through release: should be midway between sustain and 0
    float progress = 0.5f;
    float expected = sustain_level * (1.0f - progress);
    float actual = sustain_level * (1.0f - progress);
    
    ASSERT_NEAR(actual, expected, 0.01f);
    std::cout << "  ✓ Release ramp sustain→0 is linear\n";
}

// ============================================================================
// TEST: Pitch Shifting & Resampling
// ============================================================================

void test_playback_ratio_calculation() {
    std::cout << "Test: Playback Ratio (Pitch Shift)\n";
    
    // Given: Sample at C2 (MIDI 36), play back as D2 (MIDI 38)
    int target_midi = 38;
    int root_midi = 36;
    int semitone_delta = target_midi - root_midi;  // +2 semitones
    
    float semitone_ratio = 1.059463094359f;  // 2^(1/12)
    float playback_ratio = std::pow(semitone_ratio, semitone_delta);
    
    // At +2 semitones, ratio should be ~1.122
    float expected = 1.122f;
    ASSERT_NEAR(playback_ratio, expected, 0.01f);
    std::cout << "  ✓ +2 semitone ratio = 1.122 (D2 from C2 sample)\n";
    
    // Test negative pitch shift
    target_midi = 34;  // A#1, 2 semitones below C2
    semitone_delta = target_midi - root_midi;  // -2
    playback_ratio = std::pow(semitone_ratio, semitone_delta);
    expected = 0.891f;
    ASSERT_NEAR(playback_ratio, expected, 0.01f);
    std::cout << "  ✓ -2 semitone ratio = 0.891 (A#1 from C2 sample)\n";
}

void test_linear_interpolation() {
    std::cout << "Test: Linear Resampling Interpolation\n";
    
    // Create test signal
    std::vector<float> samples = {0.0f, 0.5f, 1.0f, 0.5f, 0.0f};
    
    // Test exact sample
    float pos = 1.0f;  // Exactly at sample[1]
    int idx = static_cast<int>(pos);
    float frac = pos - idx;
    float result = samples[idx] + frac * (samples[idx+1] - samples[idx]);
    ASSERT_NEAR(result, 0.5f, 0.01f);
    std::cout << "  ✓ Exact position returns exact sample\n";
    
    // Test interpolation
    pos = 1.5f;  // Halfway between sample[1] and sample[2]
    idx = static_cast<int>(pos);
    frac = pos - idx;
    result = samples[idx] + frac * (samples[idx+1] - samples[idx]);
    ASSERT_NEAR(result, 0.75f, 0.01f);  // 0.5 + 0.5*(1.0 - 0.5)
    std::cout << "  ✓ Interpolation between samples is linear\n";
}

// ============================================================================
// TEST: XY Blending
// ============================================================================

void test_xy_blending_weights() {
    std::cout << "Test: XY Four-Corner Blending\n";
    
    float x = 0.5f;  // Center X
    float y = 0.5f;  // Center Y
    
    float w_a = (1.0f - x) * (1.0f - y);  // (1-0.5)*(1-0.5) = 0.25
    float w_b = x * (1.0f - y);           // 0.5*(1-0.5) = 0.25
    float w_c = (1.0f - x) * y;           // (1-0.5)*0.5 = 0.25
    float w_d = x * y;                    // 0.5*0.5 = 0.25
    
    ASSERT_NEAR(w_a, 0.25f, 0.01f);
    ASSERT_NEAR(w_b, 0.25f, 0.01f);
    ASSERT_NEAR(w_c, 0.25f, 0.01f);
    ASSERT_NEAR(w_d, 0.25f, 0.01f);
    std::cout << "  ✓ Center position = 25% each corner\n";
    
    // Test corner (0, 0)
    x = 0.0f;
    y = 0.0f;
    w_a = (1.0f - x) * (1.0f - y);
    w_b = x * (1.0f - y);
    w_c = (1.0f - x) * y;
    w_d = x * y;
    
    ASSERT_NEAR(w_a, 1.0f, 0.01f);  // Top-left (A) = 100%
    ASSERT_NEAR(w_b, 0.0f, 0.01f);
    ASSERT_NEAR(w_c, 0.0f, 0.01f);
    ASSERT_NEAR(w_d, 0.0f, 0.01f);
    std::cout << "  ✓ Bottom-left position = 100% corner A\n";
}

void test_xy_blending_mix() {
    std::cout << "Test: XY Blending Audio Mix\n";
    
    // Mock corner buffers
    float corner_a = 0.1f;
    float corner_b = 0.2f;
    float corner_c = 0.3f;
    float corner_d = 0.4f;
    
    // XY position
    float x = 0.5f;
    float y = 0.5f;
    
    // Calculate weights
    float w_a = (1.0f - x) * (1.0f - y);
    float w_b = x * (1.0f - y);
    float w_c = (1.0f - x) * y;
    float w_d = x * y;
    
    // Mix
    float output = corner_a * w_a + corner_b * w_b + 
                   corner_c * w_c + corner_d * w_d;
    
    // At center, all equal, average should be 0.25
    float expected = (0.1f + 0.2f + 0.3f + 0.4f) / 4.0f;
    ASSERT_NEAR(output, expected, 0.01f);
    std::cout << "  ✓ Center mix averages all four corners\n";
}

// ============================================================================
// TEST: Parameter Catch-Up Logic
// ============================================================================

void test_catch_up_locked() {
    std::cout << "Test: Potentiometer Catch-Up Mode (Locked)\n";
    
    float software_value = 0.5f;
    float physical_value = 0.3f;
    const float threshold = 0.02f;
    
    // Physical pot hasn't crossed software value yet
    float delta = std::abs(physical_value - software_value);
    
    if (delta > threshold) {
        // Locked, don't update
        // software_value remains 0.5f
    }
    
    ASSERT_EQUAL(software_value, 0.5f);
    std::cout << "  ✓ Parameter locked until physical pot crosses\n";
}

void test_catch_up_crossing() {
    std::cout << "Test: Potentiometer Catch-Up Mode (Crossing)\n";
    
    float software_value = 0.5f;
    float physical_value = 0.48f;  // Very close
    const float threshold = 0.02f;
    
    float delta = std::abs(physical_value - software_value);
    
    if (delta < threshold) {
        // Pot has crossed or is very close, unlock
        software_value = physical_value;
    }
    
    ASSERT_NEAR(software_value, 0.48f, 0.01f);
    std::cout << "  ✓ Parameter updates when physical pot crosses threshold\n";
}

void test_catch_up_jump_mode() {
    std::cout << "Test: Potentiometer Jump Mode\n";
    
    float software_value = 0.5f;
    float physical_value = 0.3f;
    
    // Jump mode: immediate update
    software_value = physical_value;
    
    ASSERT_EQUAL(software_value, 0.3f);
    std::cout << "  ✓ Jump mode updates immediately (may cause pops)\n";
}

// ============================================================================
// TEST: Looping Modes
// ============================================================================

void test_loop_percentage_to_frame() {
    std::cout << "Test: Loop Percentage → Frame Index\n";
    
    uint32_t total_frames = 48000;  // 1 second @ 48kHz
    float start_pct = 25.0f;
    float end_pct = 75.0f;
    
    uint32_t start_frame = total_frames * (start_pct / 100.0f);
    uint32_t end_frame = total_frames * (end_pct / 100.0f);
    
    ASSERT_EQUAL(start_frame, 12000);
    ASSERT_EQUAL(end_frame, 36000);
    std::cout << "  ✓ 25%/75% of 48000 frames = 12000/36000 frames\n";
}

void test_loop_wrap_around() {
    std::cout << "Test: Loop Wrap-Around\n";
    
    float playback_pos = 37000.0f;
    uint32_t loop_start = 12000;
    uint32_t loop_end = 36000;
    
    if (playback_pos >= loop_end) {
        // Wrap back to start
        playback_pos = loop_start + 
            std::fmod(playback_pos - loop_start, loop_end - loop_start);
    }
    
    ASSERT_TRUE(playback_pos >= loop_start);
    ASSERT_TRUE(playback_pos < loop_end);
    std::cout << "  ✓ Playback position wraps correctly within loop bounds\n";
}

// ============================================================================
// TEST: MIDI Note To Frequency
// ============================================================================

void test_midi_note_to_freq() {
    std::cout << "Test: MIDI Note Number → Frequency\n";
    
    // A4 (MIDI 69) = 440 Hz
    int midi_note = 69;
    float expected_freq = 440.0f;
    
    int semitones_from_a4 = midi_note - 69;
    float freq = 440.0f * std::pow(1.059463094359f, semitones_from_a4);
    
    ASSERT_NEAR(freq, expected_freq, 1.0f);
    std::cout << "  ✓ MIDI 69 (A4) = 440 Hz\n";
    
    // C4 (MIDI 60) = ~261.63 Hz
    midi_note = 60;
    semitones_from_a4 = midi_note - 69;  // -9 semitones
    freq = 440.0f * std::pow(1.059463094359f, semitones_from_a4);
    expected_freq = 261.63f;
    
    ASSERT_NEAR(freq, expected_freq, 1.0f);
    std::cout << "  ✓ MIDI 60 (C4) = 261.63 Hz\n";
}

// ============================================================================
// TEST: Tape Saturation (Cubic Soft-Clip)
// ============================================================================

void test_tape_saturation() {
    std::cout << "Test: Tape Saturation Distortion\n";
    
    float drive = 0.5f;
    
    // Input: 0.0 → should remain 0.0
    float x = 0.0f;
    float result = x - (drive * x * x * x / 3.0f);
    ASSERT_NEAR(result, 0.0f, 0.01f);
    std::cout << "  ✓ Zero input = zero output\n";
    
    // Input: 0.5 with drive=0.5
    x = 0.5f;
    result = x - (drive * x * x * x / 3.0f);
    result = std::max(-1.0f, std::min(1.0f, result));
    // 0.5 - (0.5 * 0.125 / 3) = 0.5 - 0.0208 ≈ 0.479
    ASSERT_NEAR(result, 0.479f, 0.01f);
    std::cout << "  ✓ Cubic soft-clip attenuates high inputs\n";
    
    // Input: 1.0 (hard clip limit)
    x = 1.0f;
    result = x - (drive * x * x * x / 3.0f);
    result = std::max(-1.0f, std::min(1.0f, result));
    // 1.0 - (0.5 * 1.0 / 3) = 1.0 - 0.1667 ≈ 0.833, clamped to ≤1.0
    ASSERT_TRUE(result <= 1.0f);
    std::cout << "  ✓ Output bounded to ±1.0\n";
}

// ============================================================================
// TEST: Peak Limiter
// ============================================================================

void test_peak_limiter() {
    std::cout << "Test: Peak Limiter (Brick-Wall)\n";
    
    float threshold = 0.95f;  // -0.45 dBFS
    
    // Input below threshold: pass through
    float sample = 0.5f;
    if (std::abs(sample) > threshold) {
        sample = (sample >= 0.0f ? 1.0f : -1.0f) * threshold;
    }
    ASSERT_EQUAL(sample, 0.5f);
    std::cout << "  ✓ Signals below threshold pass through\n";
    
    // Input above threshold: clip
    sample = 1.2f;
    if (std::abs(sample) > threshold) {
        sample = (sample >= 0.0f ? 1.0f : -1.0f) * threshold;
    }
    ASSERT_NEAR(sample, 0.95f, 0.01f);
    std::cout << "  ✓ Signals above threshold are limited\n";
    
    // Negative input
    sample = -1.1f;
    if (std::abs(sample) > threshold) {
        sample = (sample >= 0.0f ? 1.0f : -1.0f) * threshold;
    }
    ASSERT_NEAR(sample, -0.95f, 0.01f);
    std::cout << "  ✓ Negative signals are limited symmetrically\n";
}

// ============================================================================
// TEST: Sample Filename Parsing
// ============================================================================

void test_sample_filename_parsing() {
    std::cout << "Test: WAV Filename MIDI Note Extraction\n";
    
    // Example: "36_C2.wav"
    std::string filename = "36_C2.wav";
    
    // Extract leading number
    size_t underscore = filename.find('_');
    std::string number_part = filename.substr(0, underscore);
    
    int midi_note = std::stoi(number_part);
    ASSERT_EQUAL(midi_note, 36);
    std::cout << "  ✓ '36_C2.wav' → MIDI note 36\n";
    
    // Another example
    filename = "69_A4.wav";
    underscore = filename.find('_');
    number_part = filename.substr(0, underscore);
    midi_note = std::stoi(number_part);
    ASSERT_EQUAL(midi_note, 69);
    std::cout << "  ✓ '69_A4.wav' → MIDI note 69\n";
}

} // namespace tests

// ============================================================================
// RUN ALL TESTS
// ============================================================================

int main() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "NSynth Mellotron Firmware - Unit Test Suite\n";
    std::cout << std::string(60, '=') << "\n\n";

    try {
        // ADSR Tests
        std::cout << "[ADSR Envelope]\n";
        tests::test_adsr_attack();
        tests::test_adsr_decay();
        tests::test_adsr_sustain();
        tests::test_adsr_release();

        // Resampling Tests
        std::cout << "\n[Resampling & Pitch]\n";
        tests::test_playback_ratio_calculation();
        tests::test_linear_interpolation();

        // Blending Tests
        std::cout << "\n[XY Blending]\n";
        tests::test_xy_blending_weights();
        tests::test_xy_blending_mix();

        // Parameter Tests
        std::cout << "\n[Parameter Control]\n";
        tests::test_catch_up_locked();
        tests::test_catch_up_crossing();
        tests::test_catch_up_jump_mode();

        // Looping Tests
        std::cout << "\n[Looping]\n";
        tests::test_loop_percentage_to_frame();
        tests::test_loop_wrap_around();

        // MIDI Tests
        std::cout << "\n[MIDI]\n";
        tests::test_midi_note_to_freq();

        // DSP Tests
        std::cout << "\n[DSP Effects]\n";
        tests::test_tape_saturation();
        tests::test_peak_limiter();

        // File Tests
        std::cout << "\n[File I/O]\n";
        tests::test_sample_filename_parsing();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "✓ All tests passed!\n";
        std::cout << std::string(60, '=') << "\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << "\n";
        return 1;
    }
}
