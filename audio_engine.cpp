#include "audio_engine.h"
#include <algorithm>
#include <cmath>

AudioEngine::AudioEngine(SampleManager* sample_manager)
    : sample_manager_(sample_manager),
      active_voice_count_(0),
      lfo_phase_(0.0f),
      lfo_output_(0.0f),
      peak_limiter_threshold_linear_(0.95f),  // -0.45 dBFS
      tape_sat_drive_(0.0f) {
    
    // Initialize corner buffers
    for (auto& buffer : corner_buffers_) {
        buffer.resize(AUDIO_BUFFER_SIZE, 0.0f);
    }

    // Initialize voice pool
    for (int i = 0; i < MAX_POLYPHONY; ++i) {
        voice_pool_[i].state = VoiceState::IDLE;
        voice_pool_[i].playback_position = 0.0f;
        voice_pool_[i].playback_ratio = 1.0f;
        voice_pool_[i].envelope_value = 0.0f;
        voice_pool_[i].envelope_sample_count = 0.0f;
        voice_pool_[i].start_sample_index = 0;
        voice_pool_[i].release_sample_index = -1;
    }
}

AudioEngine::~AudioEngine() {}

bool AudioEngine::initialize() {
    // TODO: Initialize I2S peripheral, DMA, codec
    // This is highly hardware-specific to NSynth Super
    // Expected setup:
    //  - I2S master mode, 48kHz, stereo (L/R outputs)
    //  - DMA double-buffering with interrupt on half/full buffer
    //  - Codec configured for audio output

    return true;  // Placeholder
}

Voice* AudioEngine::allocate_voice(Corner corner, uint8_t midi_note, float velocity) {
    // Find first idle voice
    for (int i = 0; i < MAX_POLYPHONY; ++i) {
        if (voice_pool_[i].state == VoiceState::IDLE) {
            Voice* voice = &voice_pool_[i];
            voice->assigned_corner = corner;
            voice->midi_note = midi_note;
            voice->velocity = std::max(0.0f, std::min(1.0f, velocity));
            voice->playback_position = 0.0f;
            voice->envelope_sample_count = 0.0f;
            voice->release_sample_index = -1;

            // Find best sample for this note
            voice->current_sample = sample_manager_->find_best_sample_for_note(midi_note);
            if (!voice->current_sample) {
                voice->state = VoiceState::IDLE;
                return nullptr;  // No suitable sample
            }

            // Calculate playback ratio (pitch shifting)
            int sample_root_note = voice->current_sample->root_midi_note;
            int semitone_delta = midi_note - sample_root_note;
            CornerParameters& params = corner_params_[static_cast<int>(corner)];
            semitone_delta += params.coarse_semitones;
            float cent_offset = params.fine_cents;

            voice->playback_ratio = std::pow(SEMITONE_RATIO, semitone_delta + (cent_offset / 100.0f));

            // Clamp pitch shift ratio (max ±6 semitones)
            float max_ratio = std::pow(SEMITONE_RATIO, 6.0f);
            voice->playback_ratio = std::max(1.0f / max_ratio, 
                                           std::min(max_ratio, voice->playback_ratio));

            // Trigger envelope
            envelope_pool_[i].trigger(velocity);
            voice->state = VoiceState::ATTACK;
            active_voice_count_++;

            return voice;
        }
    }

    return nullptr;  // All voices busy
}

void AudioEngine::release_voice(Voice* voice) {
    if (!voice || voice->state == VoiceState::IDLE) {
        return;
    }

    int voice_index = voice - &voice_pool_[0];
    if (voice_index >= 0 && voice_index < MAX_POLYPHONY) {
        envelope_pool_[voice_index].release();
        // Voice state will transition to RELEASE via envelope
    }
}

void AudioEngine::kill_all_voices() {
    for (int i = 0; i < MAX_POLYPHONY; ++i) {
        voice_pool_[i].state = VoiceState::IDLE;
        envelope_pool_[i].reset();
    }
    active_voice_count_ = 0;
}

void AudioEngine::set_corner_params(Corner corner, const CornerParameters& params) {
    corner_params_[static_cast<int>(corner)] = params;
}

void AudioEngine::set_global_params(const GlobalParameters& params) {
    global_params_ = params;
    peak_limiter_threshold_linear_ = std::pow(10.0f, params.limiter_threshold_dbfs / 20.0f);
    tape_sat_drive_ = params.tape_saturation_drive;
}

void AudioEngine::process_audio_block(float* output_buffer, int buffer_size) {
    // Clear corner buffers
    for (auto& buffer : corner_buffers_) {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }

    // Process LFO for this block
    process_lfo_block_(buffer_size);

    // Process all active voices
    active_voice_count_ = 0;
    for (int i = 0; i < MAX_POLYPHONY; ++i) {
        Voice& voice = voice_pool_[i];
        ADSREnvelope& env = envelope_pool_[i];

        if (voice.state != VoiceState::IDLE) {
            int corner_idx = static_cast<int>(voice.assigned_corner);
            process_voice_block_(&voice, &env, corner_buffers_[corner_idx].data(), buffer_size);
            
            if (voice.state != VoiceState::IDLE) {
                active_voice_count_++;
            }
        }
    }

    // Apply XY blending and mix corners to output
    apply_xy_blending_(output_buffer, buffer_size);

    // Apply master trim
    float master_gain = std::pow(10.0f, global_params_.master_trim_db / 20.0f);
    for (int i = 0; i < buffer_size; ++i) {
        output_buffer[i] *= master_gain;
    }

    // Apply tape saturation
    if (tape_sat_drive_ > 0.01f) {
        for (int i = 0; i < buffer_size; ++i) {
            apply_tape_saturation_(&output_buffer[i], tape_sat_drive_);
        }
    }

    // Apply peak limiter (safety stage)
    apply_peak_limiter_(output_buffer, buffer_size);
}

void AudioEngine::process_lfo_block_(int buffer_size) {
    // Update LFO phase
    float lfo_freq_hz = global_params_.lfo_frequency_hz;
    float phase_increment = lfo_freq_hz / SAMPLE_RATE;
    lfo_phase_ += phase_increment * buffer_size;
    while (lfo_phase_ >= 1.0f) {
        lfo_phase_ -= 1.0f;
    }

    // Generate LFO waveform
    float phase = lfo_phase_;
    switch (global_params_.lfo_waveform) {
        case LFOWaveform::SINE:
            lfo_output_ = std::sin(2.0f * 3.14159265f * phase);
            break;
        case LFOWaveform::TRIANGLE:
            if (phase < 0.5f) {
                lfo_output_ = 4.0f * phase - 1.0f;
            } else {
                lfo_output_ = 3.0f - 4.0f * phase;
            }
            break;
        case LFOWaveform::SQUARE:
            lfo_output_ = (phase < 0.5f) ? 1.0f : -1.0f;
            break;
        case LFOWaveform::RAMP:
            lfo_output_ = 2.0f * phase - 1.0f;
            break;
        case LFOWaveform::RANDOM_SH:
            // Sample & hold at phase boundaries
            if (phase < 0.01f) {  // Rising edge
                static float random_value = 0.0f;
                random_value = (rand() % 1000) / 500.0f - 1.0f;
                lfo_output_ = random_value;
            }
            break;
    }
}

void AudioEngine::process_voice_block_(Voice* voice, ADSREnvelope* env,
                                      float* corner_buffer, int buffer_size) {
    if (!voice || !env || !voice->current_sample || voice->state == VoiceState::IDLE) {
        return;
    }

    // Update envelope
    float env_value = env->process_block(buffer_size);
    voice->envelope_value = env_value;

    // If envelope is done, mark voice idle
    if (env->get_state() == VoiceState::IDLE) {
        voice->state = VoiceState::IDLE;
        return;
    } else {
        voice->state = env->get_state();
    }

    // Get corner parameters for pitch/LFO modulation
    int corner_idx = static_cast<int>(voice->assigned_corner);
    CornerParameters& params = corner_params_[corner_idx];

    // Process each sample in block
    const SampleDescriptor* sample = voice->current_sample;
    uint32_t sample_frame_count = sample->sample_count;
    
    for (int s = 0; s < buffer_size; ++s) {
        // Apply looping
        float playback_pos = voice->playback_position;
        LoopMode loop_mode = params.loop_mode;
        
        float loop_start_frame = sample_frame_count * (params.loop_start_pct / 100.0f);
        float loop_end_frame = sample_frame_count * (params.loop_end_pct / 100.0f);

        // Handle loop wrapping
        if (voice->state == VoiceState::SUSTAIN) {
            // In sustain, always loop
            if (playback_pos >= loop_end_frame) {
                playback_pos = loop_start_frame + 
                    std::fmod(playback_pos - loop_start_frame, 
                             loop_end_frame - loop_start_frame);
            }
        } else if (voice->state == VoiceState::RELEASE && 
                   loop_mode == LoopMode::PLAY_TO_END) {
            // Exit loop on release, play to end
            if (playback_pos >= loop_end_frame) {
                // Continue to end
            }
        }

        // Bounds check
        if (playback_pos >= sample_frame_count) {
            voice->state = VoiceState::IDLE;
            break;
        }

        // Resample and fetch audio sample
        // TODO: Actual sample data loading from SD card
        // For now, placeholder value
        float sample_value = 0.0f;  // resample_linear_(...);

        // Apply LFO modulation
        float lfo_mod = lfo_output_;
        float pitch_mod_cents = params.lfo_pitch_depth_cents * lfo_mod * global_params_.mod_wheel_depth;
        float playback_ratio_modulated = voice->playback_ratio * std::pow(SEMITONE_RATIO, pitch_mod_cents / 100.0f);

        // Advance playback position
        voice->playback_position += playback_ratio_modulated;

        // Apply envelope and velocity
        float sample_out = sample_value * env_value * voice->velocity;

        // Mix into corner buffer
        corner_buffer[s] += sample_out;
    }
}

void AudioEngine::apply_xy_blending_(float* output_buffer, int buffer_size) {
    // XY blending: corners weighted by XY position
    // W_A = (1-X)*(1-Y)  [top-left]
    // W_B = X*(1-Y)      [top-right]
    // W_C = (1-X)*Y      [bottom-left]
    // W_D = X*Y          [bottom-right]

    float x = global_params_.xy_position_x;
    float y = global_params_.xy_position_y;

    float w_a = (1.0f - x) * (1.0f - y);
    float w_b = x * (1.0f - y);
    float w_c = (1.0f - x) * y;
    float w_d = x * y;

    // Normalize to prevent clipping
    float sum = w_a + w_b + w_c + w_d;
    if (sum > 0.0f) {
        w_a /= sum;
        w_b /= sum;
        w_c /= sum;
        w_d /= sum;
    }

    // Mix corners into output
    for (int i = 0; i < buffer_size; ++i) {
        output_buffer[i] = 
            corner_buffers_[0][i] * w_a +
            corner_buffers_[1][i] * w_b +
            corner_buffers_[2][i] * w_c +
            corner_buffers_[3][i] * w_d;
    }
}

void AudioEngine::apply_tape_saturation_(float* sample, float drive) {
    // Cubic soft-clip: f(x) = x - (drive * x^3 / 3)
    float x = *sample;
    float x_cubed = x * x * x;
    *sample = x - (drive * x_cubed / 3.0f);
    
    // Bound to [-1.0, 1.0]
    *sample = std::max(-1.0f, std::min(1.0f, *sample));
}

void AudioEngine::apply_peak_limiter_(float* output_buffer, int buffer_size) {
    // Brickwall compressor: threshold, fast attack, release
    // Simple implementation: hard clip if above threshold
    for (int i = 0; i < buffer_size; ++i) {
        float sample = output_buffer[i];
        float abs_sample = std::abs(sample);
        
        if (abs_sample > peak_limiter_threshold_linear_) {
            output_buffer[i] = (sample >= 0.0f ? 1.0f : -1.0f) * peak_limiter_threshold_linear_;
        }
    }
}

// ============================================================================
// Interpolation Helpers
// ============================================================================

float AudioEngine::resample_linear_(const float* sample_data, float position,
                                   uint32_t sample_count) {
    // Linear interpolation
    if (!sample_data || position >= sample_count) {
        return 0.0f;
    }

    int idx = static_cast<int>(position);
    float frac = position - idx;

    if (idx + 1 >= static_cast<int>(sample_count)) {
        return sample_data[idx];
    }

    float s0 = sample_data[idx];
    float s1 = sample_data[idx + 1];
    return s0 + frac * (s1 - s0);
}

float AudioEngine::resample_cubic_(const float* sample_data, float position,
                                  uint32_t sample_count) {
    // Hermite cubic interpolation (higher quality)
    // TODO: Implement 4-point cubic interpolation
    return resample_linear_(sample_data, position, sample_count);  // Fallback
}
