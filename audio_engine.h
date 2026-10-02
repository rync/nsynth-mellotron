#pragma once

#include "nsynth_types.h"
#include "adsr.h"
#include "sample_manager.h"
#include <array>
#include <memory>

// ============================================================================
// AUDIO ENGINE - REAL-TIME DSP CORE
// ============================================================================
// Runs in real-time audio interrupt handler (highest priority)
// Handles:
//  - Voice playback with resampling
//  - ADSR envelope modulation
//  - XY blending of 4 corner mixes
//  - Dynamics (tape saturation, peak limiter)
//  - DMA output buffering

class AudioEngine {
public:
    AudioEngine(SampleManager* sample_manager);
    ~AudioEngine();

    // Initialize audio subsystem (setup I2S DMA, buffers)
    bool initialize();

    // Voice management
    Voice* allocate_voice(Corner corner, uint8_t midi_note, float velocity);
    void release_voice(Voice* voice);
    void kill_all_voices();

    // Parameter updates (called from control thread)
    void set_corner_params(Corner corner, const CornerParameters& params);
    void set_global_params(const GlobalParameters& params);

    // Main real-time processing callback (called from I2S interrupt)
    // Fills output_buffer with AUDIO_BUFFER_SIZE samples [0, 1]
    void process_audio_block(float* output_buffer, int buffer_size);

    // Query active voice count
    int get_active_voice_count() const { return active_voice_count_; }

private:
    SampleManager* sample_manager_;
    
    // Voice pool
    std::array<Voice, MAX_POLYPHONY> voice_pool_;
    std::array<ADSREnvelope, MAX_POLYPHONY> envelope_pool_;
    int active_voice_count_;

    // Per-corner parameters
    std::array<CornerParameters, NUM_CORNERS> corner_params_;
    
    // Global parameters
    GlobalParameters global_params_;

    // Per-corner mixing buffers (pre-XY blend)
    std::array<std::vector<float>, NUM_CORNERS> corner_buffers_;

    // LFO state
    float lfo_phase_;  // 0.0–1.0
    float lfo_output_;

    // Output stage parameters
    float peak_limiter_threshold_linear_;  // Linear scale -1..+1
    float tape_sat_drive_;

    // Helpers
    float resample_linear_(const float* sample_data, float position,
                          uint32_t sample_count);
    float resample_cubic_(const float* sample_data, float position,
                         uint32_t sample_count);
    
    void process_lfo_block_(int buffer_size);
    void process_voice_block_(Voice* voice, ADSREnvelope* env,
                             float* corner_buffer, int buffer_size);
    void apply_xy_blending_(float* output_buffer, int buffer_size);
    void apply_tape_saturation_(float* sample, float drive);
    void apply_peak_limiter_(float* output_buffer, int buffer_size);
};

#endif  // AUDIO_ENGINE_H
