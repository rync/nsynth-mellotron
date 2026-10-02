#include "sample_manager.h"
#include "hal/nsynth_hal.h"
#include <algorithm>
#include <cstring>

// ============================================================================
// WAV FILE HEADER STRUCTURES
// ============================================================================

#pragma pack(1)
struct WAVHeader {
    // RIFF Chunk
    char riff_id[4];              // "RIFF"
    uint32_t riff_size;           // File size - 8
    char wave_id[4];              // "WAVE"
    
    // fmt Chunk
    char fmt_id[4];               // "fmt "
    uint32_t fmt_size;            // Size of format chunk (16 for PCM)
    uint16_t audio_format;        // 1 = PCM
    uint16_t num_channels;        // 1 = mono, 2 = stereo
    uint32_t sample_rate;         // 48000, etc.
    uint32_t byte_rate;           // sample_rate * num_channels * bits_per_sample / 8
    uint16_t block_align;         // num_channels * bits_per_sample / 8
    uint16_t bits_per_sample;     // 16, 24, 32
    
    // Optional: extended format chunk
    // uint16_t extended_size;    // Size of extended format
    // ...
};

struct DATAChunk {
    char data_id[4];              // "data"
    uint32_t data_size;           // Size of audio data in bytes
};

#pragma pack()

// ============================================================================
// IMPLEMENTATION: Sample Manager
// ============================================================================

SampleManager::SampleManager()
    : current_instrument_(nullptr) {}

SampleManager::~SampleManager() {
    // Cleanup if needed
}

bool SampleManager::initialize(const std::string& samples_root_path) {
    // Mount SD card first
    if (!HAL_SDCard::mount_sd_card()) {
        // SD card mount failed
        return false;
    }

    // Scan /SAMPLES directory for instrument folders
    if (HAL_SDCard::open_directory(samples_root_path.c_str()) != 0) {
        return false;  // /SAMPLES doesn't exist
    }

    const FILINFO* entry = nullptr;
    while ((entry = HAL_SDCard::read_directory()) != nullptr) {
        if (entry->fname[0] == '.') {
            continue;  // Skip "." and ".."
        }

        if (entry->fattrib & AM_DIR) {
            // This is a subdirectory (potential instrument)
            std::string instrument_name = entry->fname;
            
            // Build full path: /SAMPLES/InstrumentName
            std::string dir_path = samples_root_path + "/" + instrument_name;
            
            InstrumentLibrary instrument;
            instrument.name = instrument_name;
            
            // Scan this directory for .wav files
            if (scan_instrument_directory_(dir_path, instrument)) {
                if (!instrument.samples.empty()) {
                    // Successfully loaded samples for this instrument
                    instruments_[instrument_name] = instrument;
                    instrument_names_.push_back(instrument_name);
                }
            }
        }
    }

    HAL_SDCard::close_directory();

    return instruments_.size() > 0;
}

bool SampleManager::scan_instrument_directory_(const std::string& dir_path,
                                              InstrumentLibrary& out_library) {
    if (HAL_SDCard::open_directory(dir_path.c_str()) != 0) {
        return false;  // Directory open failed
    }

    const FILINFO* entry = nullptr;
    while ((entry = HAL_SDCard::read_directory()) != nullptr) {
        if (entry->fname[0] == '.') {
            continue;
        }

        if (entry->fattrib & AM_DIR) {
            continue;  // Skip subdirectories
        }

        // Check if file ends with .wav
        const char* fname = entry->fname;
        int len = std::strlen(fname);
        if (len < 4) continue;
        
        if (fname[len-4] != '.' || 
            (fname[len-3] != 'w' && fname[len-3] != 'W') ||
            (fname[len-2] != 'a' && fname[len-2] != 'A') ||
            (fname[len-1] != 'v' && fname[len-1] != 'V')) {
            continue;  // Not a .wav file
        }

        // Parse MIDI note from filename
        uint8_t midi_note = 0;
        if (!parse_sample_filename(fname, midi_note)) {
            continue;  // Could not extract MIDI note
        }

        // Load WAV metadata
        std::string full_path = dir_path + "/" + fname;
        SampleDescriptor descriptor;
        descriptor.filename = fname;
        descriptor.root_midi_note = midi_note;

        if (load_wav_file_metadata_(full_path, descriptor)) {
            out_library.samples.push_back(descriptor);
            out_library.total_sample_data_bytes += descriptor.sample_count * 2;  // 16-bit
        }
    }

    HAL_SDCard::close_directory();

    // Sort samples by MIDI note for efficient lookup
    std::sort(out_library.samples.begin(), out_library.samples.end(),
              [](const SampleDescriptor& a, const SampleDescriptor& b) {
                  return a.root_midi_note < b.root_midi_note;
              });

    return true;
}

bool SampleManager::load_wav_file_metadata_(const std::string& filepath,
                                           SampleDescriptor& out_descriptor) {
    // Open WAV file
    if (HAL_SDCard::open_file_read(filepath.c_str()) != 0) {
        return false;  // File open failed
    }

    // Read WAV header
    WAVHeader header;
    if (HAL_SDCard::read_file((uint8_t*)&header, sizeof(WAVHeader)) != sizeof(WAVHeader)) {
        HAL_SDCard::close_file();
        return false;  // Read failed
    }

    // Verify RIFF header
    if (std::memcmp(header.riff_id, "RIFF", 4) != 0 ||
        std::memcmp(header.wave_id, "WAVE", 4) != 0 ||
        std::memcmp(header.fmt_id, "fmt ", 4) != 0) {
        HAL_SDCard::close_file();
        return false;  // Not a valid WAV file
    }

    // Verify audio format
    if (header.audio_format != 1) {
        HAL_SDCard::close_file();
        return false;  // Not PCM format
    }

    if (header.bits_per_sample != 16) {
        HAL_SDCard::close_file();
        return false;  // Only 16-bit PCM supported
    }

    // Mono or stereo (we'll handle both, but prefer mono)
    if (header.num_channels != 1 && header.num_channels != 2) {
        HAL_SDCard::close_file();
        return false;  // Unsupported channel count
    }

    // Expected: 48kHz (or accept and resample)
    out_descriptor.sample_rate_original = static_cast<float>(header.sample_rate);

    // Skip to "data" chunk
    // fmt chunk is 16 bytes + 8 bytes header = 24 bytes total
    HAL_SDCard::seek_file(12 + 8 + header.fmt_size);

    // Read data chunk header
    DATAChunk data_chunk;
    uint8_t chunk_buffer[8];
    if (HAL_SDCard::read_file(chunk_buffer, 8) != 8) {
        HAL_SDCard::close_file();
        return false;  // Could not read data chunk header
    }

    std::memcpy(&data_chunk, chunk_buffer, 8);

    // Verify "data" chunk
    if (std::memcmp(data_chunk.data_id, "data", 4) != 0) {
        HAL_SDCard::close_file();
        return false;  // No data chunk found
    }

    // Calculate frame count (assuming 16-bit samples)
    // data_size is in bytes, divide by (bytes per sample * channels)
    uint32_t bytes_per_sample = 2;  // 16-bit
    uint32_t sample_count = data_chunk.data_size / (bytes_per_sample * header.num_channels);

    out_descriptor.sample_count = sample_count;
    out_descriptor.byte_offset = HAL_SDCard::tell_file();  // Current position = start of audio data

    // TODO: Parse optional CUE/SMPL chunks for loop markers
    // For now, default to full sample length
    out_descriptor.loop_start_frame = 0;
    out_descriptor.loop_end_frame = sample_count;

    HAL_SDCard::close_file();
    return true;
}

bool SampleManager::parse_sample_filename(const std::string& filename,
                                         uint8_t& out_midi_note) {
    // Expected format: "36_C2.wav" or "69_A4.wav"
    // Extract the leading numeric part
    
    size_t underscore_pos = filename.find('_');
    if (underscore_pos == std::string::npos || underscore_pos == 0) {
        return false;
    }

    std::string number_part = filename.substr(0, underscore_pos);
    
    // Try to parse as integer
    try {
        int note_value = std::stoi(number_part);
        if (note_value >= MIN_MIDI_NOTE && note_value <= MAX_MIDI_NOTE) {
            out_midi_note = static_cast<uint8_t>(note_value);
            return true;
        }
    } catch (...) {
        // Parsing failed
    }

    return false;
}

const InstrumentLibrary* SampleManager::load_instrument(const std::string& name) {
    auto it = instruments_.find(name);
    if (it != instruments_.end()) {
        current_instrument_ = &it->second;
        return current_instrument_;
    }
    return nullptr;
}

const InstrumentLibrary* SampleManager::get_current_instrument() const {
    return current_instrument_;
}

const SampleDescriptor* SampleManager::find_best_sample_for_note(
    uint8_t target_midi_note) const {
    
    if (!current_instrument_ || current_instrument_->samples.empty()) {
        return nullptr;
    }

    // Binary search for nearest sample (samples are sorted by MIDI note)
    const auto& samples = current_instrument_->samples;
    
    // Find first sample >= target_midi_note
    auto it = std::lower_bound(
        samples.begin(), 
        samples.end(), 
        target_midi_note,
        [](const SampleDescriptor& s, uint8_t note) {
            return s.root_midi_note < note;
        }
    );

    // Best match candidates
    const SampleDescriptor* best = nullptr;
    int best_distance = 128;

    // Check exact or next higher
    if (it != samples.end()) {
        int distance = it->root_midi_note - target_midi_note;
        if (distance < best_distance) {
            best_distance = distance;
            best = &(*it);
        }
    }

    // Check previous (lower) sample
    if (it != samples.begin()) {
        --it;
        int distance = target_midi_note - it->root_midi_note;
        if (distance < best_distance) {
            best = &(*it);
        }
    }

    return best;
}

const SampleDescriptor* SampleManager::find_best_sample_for_note_exhaustive(
    uint8_t target_midi_note) const {
    
    if (!current_instrument_ || current_instrument_->samples.empty()) {
        return nullptr;
    }

    // Linear search for nearest sample
    const SampleDescriptor* best_match = nullptr;
    int smallest_distance = 128;

    for (const auto& sample : current_instrument_->samples) {
        int distance = std::abs(static_cast<int>(target_midi_note) - 
                               static_cast<int>(sample.root_midi_note));
        
        if (distance < smallest_distance) {
            smallest_distance = distance;
            best_match = &sample;
        }
        
        // Early exit on exact match
        if (distance == 0) {
            break;
        }
    }

    return best_match;
}

const std::vector<std::string>& SampleManager::get_instrument_names() const {
    return instrument_names_;
}

const InstrumentLibrary* SampleManager::get_instrument_by_index(size_t index) const {
    if (index >= instrument_names_.size()) {
        return nullptr;
    }

    auto it = instruments_.find(instrument_names_[index]);
    if (it != instruments_.end()) {
        return &it->second;
    }

    return nullptr;
}

// ============================================================================
// REAL-TIME SAMPLE PLAYBACK
// ============================================================================
// This loads actual 16-bit PCM samples from SD card on-the-fly

int load_sample_chunk(const SampleDescriptor* sample, 
                      uint32_t frame_offset,
                      int16_t* out_buffer,
                      int frame_count) {
    if (!sample) {
        return 0;
    }

    // Open WAV file
    if (HAL_SDCard::open_file_read(sample->filename) != 0) {
        return 0;  // File open failed
    }

    // Seek to audio data start + frame offset
    uint32_t byte_offset = sample->byte_offset + (frame_offset * 2);  // 2 bytes per 16-bit sample
    if (HAL_SDCard::seek_file(byte_offset) != 0) {
        HAL_SDCard::close_file();
        return 0;
    }

    // Read frame_count samples
    int bytes_to_read = frame_count * 2;  // 16-bit = 2 bytes
    int bytes_read = HAL_SDCard::read_file((uint8_t*)out_buffer, bytes_to_read);

    HAL_SDCard::close_file();

    // Return number of frames actually read
    return bytes_read / 2;
}
