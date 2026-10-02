#pragma once

#include "nsynth_types.h"
#include <string>
#include <memory>

// ============================================================================
// PRESET SYSTEM
// ============================================================================
// Handles:
//  - Saving/loading 144-slot (12x12) preset grid as JSON
//  - Touch matrix mapping (normalized XY → grid coordinates)
//  - Quick save/recall with visual feedback

class PresetSystem {
public:
    PresetSystem();
    ~PresetSystem();

    // Initialize: verify /PRESETS directory exists on SD card
    bool initialize(const std::string& presets_root_path = "/PRESETS");

    // Save current state to grid slot (X, Y) ∈ [0, 11]
    bool save_preset(uint8_t grid_x, uint8_t grid_y, const PresetState& state);

    // Load preset from grid slot
    // Returns nullptr if slot is empty or load failed
    std::unique_ptr<PresetState> load_preset(uint8_t grid_x, uint8_t grid_y);

    // Check if a slot contains a saved preset
    bool preset_exists(uint8_t grid_x, uint8_t grid_y) const;

    // Convert XY touch coordinates to grid coordinates
    // touch_x, touch_y ∈ [0.0, 1.0]
    void normalize_touch_coords(float touch_x, float touch_y,
                               uint8_t& out_grid_x, uint8_t& out_grid_y) const;

    // Get the filename for a grid slot
    std::string get_preset_filename(uint8_t grid_x, uint8_t grid_y) const;

    // Clear a preset slot
    bool delete_preset(uint8_t grid_x, uint8_t grid_y);

private:
    std::string presets_root_path_;

    // Helpers
    bool serialize_preset_to_json_(const PresetState& state, std::string& out_json);
    bool deserialize_preset_from_json_(const std::string& json, PresetState& out_state);
};

#endif  // PRESET_SYSTEM_H
