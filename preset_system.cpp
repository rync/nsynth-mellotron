#include "preset_system.h"
#include <sstream>
#include <iomanip>
#include <ctime>

PresetSystem::PresetSystem()
    : presets_root_path_("/PRESETS") {}

PresetSystem::~PresetSystem() {}

bool PresetSystem::initialize(const std::string& presets_root_path) {
    presets_root_path_ = presets_root_path;
    
    // TODO: Verify /PRESETS directory exists on SD card
    // Create if needed using FatFS or HAL
    
    return true;  // Placeholder
}

bool PresetSystem::save_preset(uint8_t grid_x, uint8_t grid_y, 
                              const PresetState& state) {
    if (grid_x >= 12 || grid_y >= 12) {
        return false;
    }

    // Serialize state to JSON
    std::string json;
    if (!serialize_preset_to_json_(state, json)) {
        return false;
    }

    // Write to SD card
    std::string filename = get_preset_filename(grid_x, grid_y);
    // TODO: Open file and write JSON string using FatFS
    
    return true;  // Placeholder
}

std::unique_ptr<PresetState> PresetSystem::load_preset(uint8_t grid_x, 
                                                       uint8_t grid_y) {
    if (grid_x >= 12 || grid_y >= 12) {
        return nullptr;
    }

    // Check if file exists
    if (!preset_exists(grid_x, grid_y)) {
        return nullptr;
    }

    // Read JSON from SD card
    std::string filename = get_preset_filename(grid_x, grid_y);
    std::string json;
    // TODO: Open file and read JSON string using FatFS

    // Deserialize
    auto preset = std::make_unique<PresetState>();
    if (!deserialize_preset_from_json_(json, *preset)) {
        return nullptr;
    }

    return preset;
}

bool PresetSystem::preset_exists(uint8_t grid_x, uint8_t grid_y) const {
    if (grid_x >= 12 || grid_y >= 12) {
        return false;
    }

    // TODO: Check if file exists on SD card using FatFS
    return false;  // Placeholder
}

void PresetSystem::normalize_touch_coords(float touch_x, float touch_y,
                                         uint8_t& out_grid_x, 
                                         uint8_t& out_grid_y) const {
    // Map touch coordinates [0.0, 1.0] to grid [0, 11]
    out_grid_x = static_cast<uint8_t>(touch_x * 12.0f);
    out_grid_y = static_cast<uint8_t>(touch_y * 12.0f);
    
    // Clamp to valid range
    out_grid_x = std::min(out_grid_x, static_cast<uint8_t>(11));
    out_grid_y = std::min(out_grid_y, static_cast<uint8_t>(11));
}

std::string PresetSystem::get_preset_filename(uint8_t grid_x, uint8_t grid_y) const {
    std::ostringstream oss;
    oss << presets_root_path_ << "/PRESET_"
        << std::setfill('0') << std::setw(2) << static_cast<int>(grid_x) << "_"
        << std::setfill('0') << std::setw(2) << static_cast<int>(grid_y) << ".JSON";
    return oss.str();
}

bool PresetSystem::delete_preset(uint8_t grid_x, uint8_t grid_y) {
    if (grid_x >= 12 || grid_y >= 12) {
        return false;
    }

    std::string filename = get_preset_filename(grid_x, grid_y);
    // TODO: Delete file using FatFS
    return true;  // Placeholder
}

// ============================================================================
// Private Serialization Helpers
// ============================================================================

bool PresetSystem::serialize_preset_to_json_(const PresetState& state,
                                            std::string& out_json) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"name\": \"" << state.name << "\",\n";
    json << "  \"timestamp_created\": " << state.timestamp_created << ",\n";
    json << "  \"timestamp_modified\": " << state.timestamp_modified << ",\n";
    
    // Serialize corners
    json << "  \"corners\": [\n";
    for (int c = 0; c < NUM_CORNERS; ++c) {
        const auto& corner = state.corners[c];
        json << "    {\n";
        json << "      \"instrument_index\": " << static_cast<int>(corner.instrument_index) << ",\n";
        json << "      \"coarse_semitones\": " << static_cast<int>(corner.coarse_semitones) << ",\n";
        json << "      \"fine_cents\": " << static_cast<int>(corner.fine_cents) << ",\n";
        json << "      \"is_active\": " << (corner.is_active ? "true" : "false") << ",\n";
        json << "      \"attack_ms\": " << corner.attack_ms << ",\n";
        json << "      \"decay_ms\": " << corner.decay_ms << ",\n";
        json << "      \"sustain_level\": " << corner.sustain_level << ",\n";
        json << "      \"release_ms\": " << corner.release_ms << ",\n";
        json << "      \"loop_mode\": " << static_cast<int>(corner.loop_mode) << ",\n";
        json << "      \"loop_start_pct\": " << corner.loop_start_pct << ",\n";
        json << "      \"loop_end_pct\": " << corner.loop_end_pct << ",\n";
        json << "      \"lfo_pitch_depth_cents\": " << corner.lfo_pitch_depth_cents << ",\n";
        json << "      \"lfo_amp_depth\": " << corner.lfo_amp_depth << ",\n";
        json << "      \"gain_trim_db\": " << corner.gain_trim_db << "\n";
        json << "    }";
        if (c < NUM_CORNERS - 1) json << ",";
        json << "\n";
    }
    json << "  ],\n";

    // Serialize global parameters
    json << "  \"global\": {\n";
    json << "    \"master_trim_db\": " << state.global.master_trim_db << ",\n";
    json << "    \"xy_position_x\": " << state.global.xy_position_x << ",\n";
    json << "    \"xy_position_y\": " << state.global.xy_position_y << ",\n";
    json << "    \"tape_saturation_drive\": " << state.global.tape_saturation_drive << ",\n";
    json << "    \"limiter_threshold_dbfs\": " << state.global.limiter_threshold_dbfs << ",\n";
    json << "    \"limiter_ratio\": " << state.global.limiter_ratio << ",\n";
    json << "    \"lfo_frequency_hz\": " << state.global.lfo_frequency_hz << ",\n";
    json << "    \"lfo_waveform\": " << static_cast<int>(state.global.lfo_waveform) << ",\n";
    json << "    \"lfo_phase\": " << state.global.lfo_phase << ",\n";
    json << "    \"midi_channel\": " << static_cast<int>(state.global.midi_channel) << ",\n";
    json << "    \"pitch_bend_semitones\": " << state.global.pitch_bend_semitones << ",\n";
    json << "    \"mod_wheel_depth\": " << state.global.mod_wheel_depth << ",\n";
    json << "    \"display_brightness_pct\": " << static_cast<int>(state.global.display_brightness_pct) << ",\n";
    json << "    \"pot_mode\": " << static_cast<int>(state.global.pot_mode) << ",\n";
    json << "    \"xy_touch_sensitivity\": " << state.global.xy_touch_sensitivity << "\n";
    json << "  }\n";
    
    json << "}";
    
    out_json = json.str();
    return true;
}

bool PresetSystem::deserialize_preset_from_json_(const std::string& json,
                                                PresetState& out_state) {
    // TODO: Implement JSON parsing
    // For now, placeholder that returns false
    // In production, use a lightweight JSON library or custom parser
    
    // Could use libraries like:
    //  - nlohmann/json (header-only, easy to use)
    //  - cJSON (lightweight, POSIX)
    //  - rapidjson (fast)
    
    return false;  // Placeholder
}
