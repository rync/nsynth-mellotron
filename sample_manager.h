#pragma once

#include "nsynth_types.h"
#include <map>
#include <memory>

// ============================================================================
// SAMPLE MANAGER
// ============================================================================
// Responsible for:
//  - Scanning /SAMPLES/{INSTRUMENT_NAME}/ directories on SD card
//  - Parsing WAV filenames to extract root MIDI notes
//  - Building metadata descriptors for quick lookup
//  - Providing efficient nearest-neighbor sample selection for pitch shifting
//  - Managing sample memory and streaming from SD card

class SampleManager {
public:
    SampleManager();
    ~SampleManager();

    // Initialize: scan SD card and build instrument library
    // Returns true if at least one instrument was successfully loaded
    bool initialize(const std::string& samples_root_path = "/SAMPLES");

    // Load a specific instrument by name
    // Returns nullptr if not found
    const InstrumentLibrary* load_instrument(const std::string& name);

    // Get currently loaded instrument
    const InstrumentLibrary* get_current_instrument() const {
        return current_instrument_;
    }

    // Find best sample for playback
    // Given target MIDI note, return the nearest sample descriptor
    // Selects lower sample if available and closest, else upper
    const SampleDescriptor* find_best_sample_for_note(uint8_t target_midi_note) const;

    // Utility: extract MIDI note from filename like "36_C2.wav"
    static bool parse_sample_filename(const std::string& filename, uint8_t& out_midi_note);

    // Get list of all available instrument names
    const std::vector<std::string>& get_instrument_names() const {
        return instrument_names_;
    }

    // Get a specific instrument library by index
    const InstrumentLibrary* get_instrument_by_index(size_t index) const;

private:
    std::map<std::string, InstrumentLibrary> instruments_;  // name -> library
    const InstrumentLibrary* current_instrument_;
    std::vector<std::string> instrument_names_;             // Ordered list

    // Helpers
    bool scan_instrument_directory_(const std::string& dir_path,
                                   InstrumentLibrary& out_library);
    bool load_wav_file_metadata_(const std::string& filepath,
                                SampleDescriptor& out_descriptor);
};

#endif  // SAMPLE_MANAGER_H
