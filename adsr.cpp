#include "adsr.h"
#include <algorithm>

ADSREnvelope::ADSREnvelope()
    : current_state_(VoiceState::IDLE),
      envelope_output_(0.0f),
      sample_accumulator_(0.0f),
      attack_samples_(SAMPLE_RATE * 0.01f),       // 10ms default
      decay_samples_(SAMPLE_RATE * 0.1f),         // 100ms default
      release_samples_(SAMPLE_RATE * 0.1f),       // 100ms default
      sustain_level_(1.0f) {}

void ADSREnvelope::set_attack_ms(float attack_ms) {
    attack_ms = std::max(1.0f, attack_ms);
    attack_samples_ = (SAMPLE_RATE / 1000.0f) * attack_ms;
}

void ADSREnvelope::set_decay_ms(float decay_ms) {
    decay_ms = std::max(1.0f, decay_ms);
    decay_samples_ = (SAMPLE_RATE / 1000.0f) * decay_ms;
}

void ADSREnvelope::set_sustain_level(float level) {
    sustain_level_ = std::max(0.0f, std::min(1.0f, level));
}

void ADSREnvelope::set_release_ms(float release_ms) {
    release_ms = std::max(1.0f, release_ms);
    release_samples_ = (SAMPLE_RATE / 1000.0f) * release_ms;
}

void ADSREnvelope::trigger(float velocity) {
    current_state_ = VoiceState::ATTACK;
    sample_accumulator_ = 0.0f;
    envelope_output_ = 0.0f;  // Start from zero
}

void ADSREnvelope::release() {
    if (current_state_ != VoiceState::IDLE) {
        current_state_ = VoiceState::RELEASE;
        sample_accumulator_ = 0.0f;
    }
}

void ADSREnvelope::reset() {
    current_state_ = VoiceState::IDLE;
    envelope_output_ = 0.0f;
    sample_accumulator_ = 0.0f;
}

float ADSREnvelope::process_block(int buffer_size) {
    if (current_state_ == VoiceState::IDLE) {
        return envelope_output_ = 0.0f;
    }

    // Process each sample in the block and update state as needed
    float per_sample_increment = 1.0f / buffer_size;
    
    switch (current_state_) {
        case VoiceState::ATTACK: {
            sample_accumulator_ += buffer_size;
            if (sample_accumulator_ >= attack_samples_) {
                // Attack complete, move to decay
                current_state_ = VoiceState::DECAY;
                sample_accumulator_ = 0.0f;
                envelope_output_ = 1.0f;
            } else {
                // Linear ramp during attack
                envelope_output_ = sample_accumulator_ / attack_samples_;
            }
            break;
        }

        case VoiceState::DECAY: {
            sample_accumulator_ += buffer_size;
            if (sample_accumulator_ >= decay_samples_) {
                // Decay complete, move to sustain
                current_state_ = VoiceState::SUSTAIN;
                sample_accumulator_ = 0.0f;
                envelope_output_ = sustain_level_;
            } else {
                // Linear ramp from 1.0 to sustain level
                float decay_progress = sample_accumulator_ / decay_samples_;
                envelope_output_ = 1.0f - (decay_progress * (1.0f - sustain_level_));
            }
            break;
        }

        case VoiceState::SUSTAIN: {
            // Hold sustain level indefinitely
            envelope_output_ = sustain_level_;
            break;
        }

        case VoiceState::RELEASE: {
            sample_accumulator_ += buffer_size;
            if (sample_accumulator_ >= release_samples_) {
                // Release complete, silence
                current_state_ = VoiceState::IDLE;
                sample_accumulator_ = 0.0f;
                envelope_output_ = 0.0f;
            } else {
                // Linear ramp from current level to zero
                float release_progress = sample_accumulator_ / release_samples_;
                envelope_output_ = sustain_level_ * (1.0f - release_progress);
            }
            break;
        }

        case VoiceState::IDLE:
        default:
            envelope_output_ = 0.0f;
            break;
    }

    return envelope_output_;
}
