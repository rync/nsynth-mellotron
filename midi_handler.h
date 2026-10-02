#pragma once

#include "nsynth_types.h"
#include "audio_engine.h"
#include <queue>
#include <cstdint>

// ============================================================================
// MIDI HANDLER
// ============================================================================
// Runs in MIDI event thread (medium priority)
// Parses Serial MIDI messages and triggers/releases voices
// Handles:
//  - Note On / Note Off
//  - Pitch Bend (±2 to ±12 semitones configurable)
//  - Modulation Wheel (CC 1)
//  - MIDI Channel filtering

struct MIDIMessage {
    uint8_t status;    // Status byte (top nibble = message type)
    uint8_t data1;     // Data byte 1 (note number or CC)
    uint8_t data2;     // Data byte 2 (velocity or CC value)
};

class MIDIHandler {
public:
    MIDIHandler(AudioEngine* engine);
    ~MIDIHandler();

    // Process a single byte from serial input
    void process_midi_byte(uint8_t byte);

    // Process a complete MIDI message
    void process_midi_message(const MIDIMessage& msg);

    // Set active MIDI channel (0–15)
    void set_midi_channel(uint8_t channel) { active_channel_ = channel; }

    // Pitch bend range
    void set_pitch_bend_range(float semitones) { pitch_bend_range_ = semitones; }

    // Query current pitch bend and mod wheel state
    float get_pitch_bend_semitones() const { return pitch_bend_semitones_; }
    float get_mod_wheel_value() const { return mod_wheel_value_; }

private:
    AudioEngine* audio_engine_;
    
    // MIDI state machine
    uint8_t running_status_;
    uint8_t midi_buffer_[3];
    uint8_t buffer_index_;
    bool in_sysex_;

    // Channel filtering
    uint8_t active_channel_;

    // CC/Pitch state
    float pitch_bend_semitones_;    // ±range based on configuration
    float mod_wheel_value_;         // 0.0–1.0
    float pitch_bend_range_;        // Typically 2, 12 semitones

    // Voice tracking (which corner gets next note)
    Corner next_corner_;
    int corner_rotation_;

    // Helpers
    void handle_note_on_(uint8_t midi_note, uint8_t velocity);
    void handle_note_off_(uint8_t midi_note);
    void handle_pitch_bend_(int16_t bend_value);    // ±8192
    void handle_cc_(uint8_t cc_num, uint8_t value);
};

#endif  // MIDI_HANDLER_H
