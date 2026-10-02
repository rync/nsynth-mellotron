#pragma once

#include "nsynth_types.h"
#include "audio_engine.h"
#include "preset_system.h"
#include <array>

// ============================================================================
// UI CONTROLLER
// ============================================================================
// Runs in control thread (low priority)
// Manages:
//  - Page state machine and navigation
//  - Parameter editing with potentiometer handling
//  - Catch-up / jump / relative mode logic
//  - Display refresh and touch input processing
//  - Preset save/recall workflow

class UIController {
public:
    UIController(AudioEngine* engine, PresetSystem* preset_system);
    ~UIController();

    // Initialize with default page and parameter values
    void initialize();

    // Process one frame of UI input/output (called ~60-100Hz from control thread)
    void update_ui_frame(const ControlInput& input);

    // Query current state for display rendering
    UIPage get_current_page() const { return ui_state_.current_page; }
    uint8_t get_current_corner() const { return ui_state_.current_corner; }
    const CornerParameters& get_corner_params(Corner corner) const {
        return all_corners_params_[static_cast<int>(corner)];
    }
    const GlobalParameters& get_global_params() const { return global_params_; }

    // Get parameter description for display (page-specific label text)
    std::string get_parameter_label(int param_index) const;
    std::string get_parameter_value_string(int param_index) const;

    // Handle preset matrix touch (from Page 5)
    void handle_preset_touch(uint8_t grid_x, uint8_t grid_y, bool is_long_press);

private:
    AudioEngine* audio_engine_;
    PresetSystem* preset_system_;

    // State
    UIState ui_state_;
    std::array<CornerParameters, NUM_CORNERS> all_corners_params_;
    GlobalParameters global_params_;

    // Parameter catch-up tracking
    std::array<float, 6> last_pot_values_;    // Last frame's pot values
    std::array<bool, 6> pot_caught_up_;       // Has this pot caught up to target?
    std::array<float, 6> pot_target_values_;  // Target values (soft parameters)

    // Touch/gesture tracking
    float last_xy_x_, last_xy_y_;
    int64_t last_touch_time_;

    // Display state
    int64_t last_frame_time_;

    // Helpers
    void handle_page_navigation_(const ControlInput& input);
    void handle_corner_encoders_(const ControlInput& input);
    void handle_potentiometers_(const ControlInput& input);
    void handle_xy_pad_(const ControlInput& input);
    void update_active_parameters_();
    void apply_catch_up_logic_(int pot_index, float physical_value, float& software_value);
    void sync_to_audio_engine_();
};

#endif  // UI_CONTROLLER_H
