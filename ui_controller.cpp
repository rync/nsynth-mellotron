#include "ui_controller.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>

UIController::UIController(AudioEngine* engine, PresetSystem* preset_system)
    : audio_engine_(engine),
      preset_system_(preset_system),
      last_xy_x_(0.5f),
      last_xy_y_(0.5f),
      last_touch_time_(0),
      last_frame_time_(0) {
    
    // Initialize parameter values to defaults
    for (int i = 0; i < NUM_CORNERS; ++i) {
        all_corners_params_[i].instrument_index = 0;
        all_corners_params_[i].coarse_semitones = 0;
        all_corners_params_[i].fine_cents = 0;
        all_corners_params_[i].is_active = true;
        all_corners_params_[i].attack_ms = 10.0f;
        all_corners_params_[i].decay_ms = 100.0f;
        all_corners_params_[i].sustain_level = 0.8f;
        all_corners_params_[i].release_ms = 500.0f;
        all_corners_params_[i].loop_mode = LoopMode::LOOP_FADE;
        all_corners_params_[i].loop_start_pct = 0.0f;
        all_corners_params_[i].loop_end_pct = 100.0f;
        all_corners_params_[i].lfo_pitch_depth_cents = 0.0f;
        all_corners_params_[i].lfo_amp_depth = 0.0f;
        all_corners_params_[i].gain_trim_db = 0.0f;
    }

    global_params_.master_trim_db = 0.0f;
    global_params_.xy_position_x = 0.5f;
    global_params_.xy_position_y = 0.5f;
    global_params_.tape_saturation_drive = 0.0f;
    global_params_.limiter_threshold_dbfs = -3.0f;
    global_params_.limiter_ratio = 4.0f;
    global_params_.lfo_frequency_hz = 5.0f;
    global_params_.lfo_waveform = LFOWaveform::SINE;
    global_params_.lfo_phase = 0.0f;
    global_params_.midi_channel = 0;
    global_params_.pitch_bend_semitones = 2.0f;
    global_params_.mod_wheel_depth = 1.0f;
    global_params_.display_brightness_pct = 100;
    global_params_.pot_mode = PotentiometerMode::CATCH_UP;
    global_params_.xy_touch_sensitivity = 1.0f;

    std::fill(last_pot_values_.begin(), last_pot_values_.end(), 0.5f);
    std::fill(pot_caught_up_.begin(), pot_caught_up_.end(), true);
    std::fill(pot_target_values_.begin(), pot_target_values_.end(), 0.5f);
}

UIController::~UIController() {}

void UIController::initialize() {
    ui_state_.current_page = UIPage::CORNER_INSTRUMENTS;
    ui_state_.current_corner = static_cast<uint8_t>(Corner::A);
    update_active_parameters_();
    sync_to_audio_engine_();
}

void UIController::update_ui_frame(const ControlInput& input) {
    int64_t now = std::time(nullptr);
    last_frame_time_ = now;

    // Handle page navigation (P1 = page select)
    handle_page_navigation_(input);

    // Handle corner encoders
    handle_corner_encoders_(input);

    // Handle potentiometers (P2–P6)
    handle_potentiometers_(input);

    // Handle XY pad
    handle_xy_pad_(input);

    // Handle preset matrix (Page 5 specific)
    if (ui_state_.current_page == UIPage::PRESET_SAVE_RECALL) {
        // Touch detection happens in display driver, passed via input
        if (input.preset_save_request) {
            handle_preset_touch(input.preset_recall_x, input.preset_recall_y, false);
        }
    }

    // Sync parameter changes to audio engine
    sync_to_audio_engine_();

    // Update display (would call display driver here)
    // update_display_();
}

void UIController::handle_page_navigation_(const ControlInput& input) {
    // P1 controls page selection via catch-up
    float p1_value = input.potentiometer_values[0];
    
    // Map P1 range [0.0, 1.0] to page [0, 7]
    uint8_t target_page = static_cast<uint8_t>(p1_value * (UIPage::NUM_PAGES - 1));
    
    if (target_page != static_cast<uint8_t>(ui_state_.current_page)) {
        ui_state_.current_page = static_cast<UIPage>(target_page);
        update_active_parameters_();
    }
}

void UIController::handle_corner_encoders_(const ControlInput& input) {
    // Four encoders map to corner selection or primary parameter
    // For now, use to select active corner for editing
    for (int i = 0; i < NUM_CORNERS; ++i) {
        if (input.encoder_deltas[i] != 0) {
            ui_state_.current_corner = i;
            break;  // Only process one encoder per frame
        }
    }
}

void UIController::handle_potentiometers_(const ControlInput& input) {
    // P2–P6 control active parameters based on current page
    for (int p = 1; p < 6; ++p) {  // P1 is page select
        float physical_value = input.potentiometer_values[p];
        float& software_value = ui_state_.active_parameter_values[p];

        // Apply catch-up logic based on pot mode
        if (global_params_.pot_mode == PotentiometerMode::CATCH_UP) {
            apply_catch_up_logic_(p, physical_value, software_value);
        } else if (global_params_.pot_mode == PotentiometerMode::JUMP) {
            software_value = physical_value;
        } else if (global_params_.pot_mode == PotentiometerMode::RELATIVE) {
            float delta = physical_value - last_pot_values_[p];
            software_value = std::max(0.0f, std::min(1.0f, software_value + delta));
        }

        last_pot_values_[p] = physical_value;
    }
}

void UIController::handle_xy_pad_(const ControlInput& input) {
    if (input.xy_touch_active) {
        global_params_.xy_position_x = input.xy_touch_x;
        global_params_.xy_position_y = input.xy_touch_y;
        last_xy_x_ = input.xy_touch_x;
        last_xy_y_ = input.xy_touch_y;
        last_touch_time_ = std::time(nullptr);
    }
}

void UIController::update_active_parameters_() {
    // Map page and potentiometer positions to actual parameters
    // This is the core parameter routing logic
    
    switch (ui_state_.current_page) {
        case UIPage::CORNER_INSTRUMENTS: {
            // P2: Instrument A, P3: B, P4: C, P5: D
            for (int c = 0; c < NUM_CORNERS; ++c) {
                uint8_t pot_idx = c + 2;
                if (pot_idx < 6) {
                    all_corners_params_[c].instrument_index = 
                        static_cast<uint8_t>(ui_state_.active_parameter_values[pot_idx] * 255);
                }
            }
            break;
        }

        case UIPage::CORNER_TUNING: {
            // P2: select corner, P3: coarse, P4: fine
            Corner selected = static_cast<Corner>(ui_state_.current_corner);
            int c = static_cast<int>(selected);
            
            // Coarse semitones [-24, +24]
            all_corners_params_[c].coarse_semitones = 
                static_cast<int8_t>((ui_state_.active_parameter_values[3] - 0.5f) * 48);
            
            // Fine cents [-100, +100]
            all_corners_params_[c].fine_cents = 
                static_cast<int8_t>((ui_state_.active_parameter_values[4] - 0.5f) * 200);
            break;
        }

        case UIPage::ADSR_ENVELOPES: {
            Corner selected = static_cast<Corner>(ui_state_.current_corner);
            int c = static_cast<int>(selected);
            
            // P3: Attack (1–5000ms), P4: Decay (1–10000ms)
            // P5: Sustain (0–1.0), P6: Release (1–10000ms)
            all_corners_params_[c].attack_ms = 
                1.0f + ui_state_.active_parameter_values[3] * 4999.0f;
            all_corners_params_[c].decay_ms = 
                1.0f + ui_state_.active_parameter_values[4] * 9999.0f;
            all_corners_params_[c].sustain_level = 
                ui_state_.active_parameter_values[5];
            break;
        }

        case UIPage::LOOPING_PLAYBACK: {
            Corner selected = static_cast<Corner>(ui_state_.current_corner);
            int c = static_cast<int>(selected);
            
            // P3: Loop Start %, P4: Loop End %, P5: Release Mode
            all_corners_params_[c].loop_start_pct = 
                ui_state_.active_parameter_values[3] * 100.0f;
            all_corners_params_[c].loop_end_pct = 
                ui_state_.active_parameter_values[4] * 100.0f;
            
            int mode = static_cast<int>(ui_state_.active_parameter_values[5] * 2);
            all_corners_params_[c].loop_mode = static_cast<LoopMode>(mode);
            break;
        }

        case UIPage::PRESET_SAVE_RECALL: {
            // Touch matrix handled separately
            break;
        }

        case UIPage::DYNAMICS_FX: {
            // P3: Gain Trim, P4: Limiter Threshold, P5: Tape Saturation
            global_params_.master_trim_db = 
                (ui_state_.active_parameter_values[3] - 0.5f) * 24.0f;  // ±12dB
            global_params_.limiter_threshold_dbfs = 
                -12.0f + ui_state_.active_parameter_values[4] * 12.0f;   // -12 to 0 dBFS
            global_params_.tape_saturation_drive = 
                ui_state_.active_parameter_values[5];
            break;
        }

        case UIPage::MODULATION_LFO: {
            // P3: LFO Frequency, P4: LFO Waveform, P5: Pitch Depth, P6: Amp Depth
            global_params_.lfo_frequency_hz = 
                0.1f + ui_state_.active_parameter_values[3] * 199.9f;
            
            int waveform = static_cast<int>(ui_state_.active_parameter_values[4] * 4);
            global_params_.lfo_waveform = static_cast<LFOWaveform>(waveform);
            
            Corner selected = static_cast<Corner>(ui_state_.current_corner);
            int c = static_cast<int>(selected);
            all_corners_params_[c].lfo_pitch_depth_cents = 
                (ui_state_.active_parameter_values[5] - 0.5f) * 400.0f;  // ±200
            break;
        }

        case UIPage::GLOBAL_SETTINGS: {
            // P3: Brightness, P4: Touch Sensitivity, P5: Pot Mode
            global_params_.display_brightness_pct = 
                static_cast<uint8_t>(ui_state_.active_parameter_values[3] * 100);
            global_params_.xy_touch_sensitivity = 
                ui_state_.active_parameter_values[4];
            
            int pot_mode = static_cast<int>(ui_state_.active_parameter_values[5] * 2);
            global_params_.pot_mode = static_cast<PotentiometerMode>(pot_mode);
            break;
        }

        case UIPage::NUM_PAGES:
        default:
            break;
    }
}

void UIController::apply_catch_up_logic_(int pot_index, float physical_value,
                                        float& software_value) {
    const float CATCH_UP_THRESHOLD = 0.02f;  // 2% dead zone
    
    float delta = std::abs(physical_value - software_value);
    
    if (delta < CATCH_UP_THRESHOLD) {
        // Close enough, lock to physical value
        software_value = physical_value;
        pot_caught_up_[pot_index] = true;
    } else if (pot_caught_up_[pot_index]) {
        // Already caught up, update immediately
        software_value = physical_value;
    }
    // Otherwise, remain locked to software_value until physical crosses it
}

void UIController::sync_to_audio_engine_() {
    if (!audio_engine_) return;

    // Send updated parameters to audio engine
    for (int c = 0; c < NUM_CORNERS; ++c) {
        audio_engine_->set_corner_params(static_cast<Corner>(c), all_corners_params_[c]);
    }
    audio_engine_->set_global_params(global_params_);
}

void UIController::handle_preset_touch(uint8_t grid_x, uint8_t grid_y, 
                                       bool is_long_press) {
    if (is_long_press) {
        // Save current state to this slot
        PresetState state;
        state.name = "Preset_" + std::to_string(grid_x) + "_" + std::to_string(grid_y);
        state.corners = all_corners_params_;
        state.global = global_params_;
        state.timestamp_created = std::time(nullptr);
        state.timestamp_modified = std::time(nullptr);
        
        preset_system_->save_preset(grid_x, grid_y, state);
    } else {
        // Load preset from this slot
        auto preset = preset_system_->load_preset(grid_x, grid_y);
        if (preset) {
            all_corners_params_ = preset->corners;
            global_params_ = preset->global;
            sync_to_audio_engine_();
        }
    }
}

std::string UIController::get_parameter_label(int param_index) const {
    // Return human-readable label for a parameter
    // This varies by page
    
    const char* labels[] = {
        "Page",
        "Reserved",
        "P2",
        "P3",
        "P4",
        "P5",
        "P6"
    };
    
    if (param_index >= 0 && param_index < 7) {
        return std::string(labels[param_index]);
    }
    return "?";
}

std::string UIController::get_parameter_value_string(int param_index) const {
    std::ostringstream oss;
    
    if (param_index == 0) {
        // Page label
        const char* page_names[] = {
            "Instruments",
            "Tuning",
            "ADSR",
            "Looping",
            "Presets",
            "Dynamics",
            "LFO",
            "Settings"
        };
        return page_names[static_cast<int>(ui_state_.current_page)];
    }
    
    // Parameter values
    float value = ui_state_.active_parameter_values[param_index];
    oss << std::fixed << std::setprecision(2) << (value * 100.0f) << "%";
    return oss.str();
}
