#pragma once

#include "nsynth_types.h"

// ============================================================================
// ADSR ENVELOPE GENERATOR
// ============================================================================
// Linear ADSR with per-sample-block calculation and state machine.
// Handles attack, decay, sustain, and release phases independently per voice.

class ADSREnvelope {
public:
    ADSREnvelope();

    // Configuration
    void set_attack_ms(float attack_ms);
    void set_decay_ms(float decay_ms);
    void set_sustain_level(float level);       // 0.0–1.0
    void set_release_ms(float release_ms);

    // State machine
    void trigger(float velocity = 1.0f);       // Start at attack phase
    void release();                             // Move to release phase
    void reset();                               // Return to idle

    // Per-sample-block processing
    // Returns envelope output [0.0, 1.0] for this buffer block
    float process_block(int buffer_size);

    // Query state
    VoiceState get_state() const { return current_state_; }
    float get_output() const { return envelope_output_; }
    bool is_active() const { return current_state_ != VoiceState::IDLE; }

private:
    // State machine
    VoiceState current_state_;
    float envelope_output_;         // Current envelope value

    // Time tracking (in samples)
    float sample_accumulator_;
    float attack_samples_;
    float decay_samples_;
    float release_samples_;

    // Parameters
    float sustain_level_;

    // Helpers
    void update_phase_durations_();
};

#endif  // ADSR_H
