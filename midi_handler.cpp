#include "midi_handler.h"
#include <algorithm>

MIDIHandler::MIDIHandler(AudioEngine* engine)
    : audio_engine_(engine),
      running_status_(0),
      buffer_index_(0),
      in_sysex_(false),
      active_channel_(0),
      pitch_bend_semitones_(0.0f),
      mod_wheel_value_(0.0f),
      pitch_bend_range_(2.0f),
      next_corner_(Corner::A),
      corner_rotation_(0) {
    
    std::fill(midi_buffer_, midi_buffer_ + 3, 0);
}

MIDIHandler::~MIDIHandler() {}

void MIDIHandler::process_midi_byte(uint8_t byte) {
    // Handle MIDI byte-by-byte parsing
    
    if (byte & 0x80) {
        // Status byte
        if (byte == 0xF0) {
            // SysEx start
            in_sysex_ = true;
            buffer_index_ = 0;
            return;
        } else if (byte == 0xF7) {
            // SysEx end
            in_sysex_ = false;
            return;
        } else if (byte >= 0xF8 && byte <= 0xFE) {
            // Realtime messages (clock, start, stop, etc.)
            // These can appear between data bytes, so handle separately
            return;
        }

        // Regular channel message
        in_sysex_ = false;
        running_status_ = byte;
        buffer_index_ = 0;
    } else if (!in_sysex_) {
        // Data byte
        midi_buffer_[buffer_index_] = byte;
        buffer_index_++;

        // Determine how many bytes this message needs
        uint8_t msg_type = running_status_ & 0xF0;
        int expected_length = 2;  // Most messages are 2-3 bytes
        
        if (msg_type == 0xC0 || msg_type == 0xD0) {
            // Program Change, Channel Pressure (1 data byte)
            expected_length = 1;
        }

        // Process message when complete
        if (buffer_index_ >= expected_length) {
            MIDIMessage msg;
            msg.status = running_status_;
            msg.data1 = midi_buffer_[0];
            msg.data2 = (expected_length > 1) ? midi_buffer_[1] : 0;
            
            process_midi_message(msg);
            
            // Keep running status for next message
            buffer_index_ = 0;
        }
    }
}

void MIDIHandler::process_midi_message(const MIDIMessage& msg) {
    uint8_t msg_type = msg.status & 0xF0;
    uint8_t channel = msg.status & 0x0F;

    // Filter by channel
    if (channel != active_channel_) {
        return;
    }

    switch (msg_type) {
        case 0x90: {  // Note On
            if (msg.data2 == 0) {
                // Velocity 0 = Note Off
                handle_note_off_(msg.data1);
            } else {
                handle_note_on_(msg.data1, msg.data2);
            }
            break;
        }

        case 0x80: {  // Note Off
            handle_note_off_(msg.data1);
            break;
        }

        case 0xE0: {  // Pitch Bend
            // Pitch bend is 14-bit (data1 = LSB, data2 = MSB)
            int16_t bend_value = (msg.data1 & 0x7F) | ((msg.data2 & 0x7F) << 7);
            // Center is 0x2000 (8192), range ±8192
            bend_value -= 8192;
            handle_pitch_bend_(bend_value);
            break;
        }

        case 0xB0: {  // Control Change
            handle_cc_(msg.data1, msg.data2);
            break;
        }

        case 0xC0: {  // Program Change
            // Not used in this implementation
            break;
        }

        case 0xD0: {  // Channel Pressure
            // Could use for velocity-like pressure modulation
            break;
        }

        case 0xA0: {  // Polyphonic Key Pressure
            // Could use for per-note pressure
            break;
        }

        default:
            break;
    }
}

void MIDIHandler::handle_note_on_(uint8_t midi_note, uint8_t velocity) {
    if (!audio_engine_) return;

    // Allocate voice on next corner (round-robin)
    Corner corner = next_corner_;
    
    // Convert velocity to 0.0–1.0
    float vel_float = velocity / 127.0f;

    // Try to allocate
    Voice* voice = audio_engine_->allocate_voice(corner, midi_note, vel_float);
    
    if (voice) {
        // Rotate to next corner for next note
        corner_rotation_ = (corner_rotation_ + 1) % NUM_CORNERS;
        next_corner_ = static_cast<Corner>(corner_rotation_);
    }
}

void MIDIHandler::handle_note_off_(uint8_t midi_note) {
    if (!audio_engine_) return;

    // Find and release voice playing this note
    // Note: In this design, we don't track which voice plays which note,
    // so this is a limitation. A more sophisticated voice manager would
    // maintain voice-to-note mappings.
    
    // For now, this is a no-op. Production code would track active notes.
}

void MIDIHandler::handle_pitch_bend_(int16_t bend_value) {
    // bend_value is ±8192
    // Map to ±pitch_bend_range_ semitones
    pitch_bend_semitones_ = (bend_value / 8192.0f) * pitch_bend_range_;
}

void MIDIHandler::handle_cc_(uint8_t cc_num, uint8_t value) {
    // CC value is 0–127, normalize to 0.0–1.0
    float normalized = value / 127.0f;

    switch (cc_num) {
        case 1: {  // Modulation Wheel
            mod_wheel_value_ = normalized;
            break;
        }

        case 7: {  // Channel Volume
            // Could map to master trim
            break;
        }

        case 64: {  // Sustain Pedal
            // Could gate note release
            break;
        }

        case 120: {  // All Sounds Off
            if (!audio_engine_) break;
            audio_engine_->kill_all_voices();
            break;
        }

        case 123: {  // All Notes Off
            if (!audio_engine_) break;
            audio_engine_->kill_all_voices();
            break;
        }

        default:
            // Ignore other CCs
            break;
    }
}
