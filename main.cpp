#include "nsynth_types.h"
#include "audio_engine.h"
#include "sample_manager.h"
#include "preset_system.h"
#include "ui_controller.h"
#include "midi_handler.h"
#include "hal/nsynth_hal.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

// ============================================================================
// GLOBAL STATE (Shared between threads with mutex protection)
// ============================================================================

SampleManager g_sample_manager;
AudioEngine g_audio_engine(&g_sample_manager);
PresetSystem g_preset_system;
UIController g_ui_controller(&g_audio_engine, &g_preset_system);
MIDIHandler g_midi_handler(&g_audio_engine);

std::mutex g_state_lock;
std::condition_variable g_state_cv;
volatile bool g_shutdown_requested = false;

// Audio buffer management
namespace {
    float g_audio_output_buffer[AUDIO_BUFFER_SIZE];
    volatile bool g_audio_buffer_ready = false;
}

// ============================================================================
// REAL-TIME AUDIO INTERRUPT HANDLER
// ============================================================================
// Called from I2S DMA interrupt (~10.67 kHz @ 256 samples/buffer)
// This is the hot path - must complete in <5ms

extern "C" void nsynth_audio_interrupt_handler() {
    // Check if half or full buffer complete
    if (HAL_Audio::i2s_half_complete || HAL_Audio::i2s_full_complete) {
        // Process audio block
        // Note: No mutexes here! Audio runs with cached parameters.
        // Control thread updates parameters, audio uses stale-but-safe copies.
        
        g_audio_engine.process_audio_block(g_audio_output_buffer, AUDIO_BUFFER_SIZE);
        
        // Write to I2S output
        HAL_Audio::write_audio_samples(g_audio_output_buffer, AUDIO_BUFFER_SIZE);
        
        // Clear flags
        HAL_Audio::i2s_half_complete = false;
        HAL_Audio::i2s_full_complete = false;
        
        g_audio_buffer_ready = true;
    }
}

// ============================================================================
// MIDI EVENT THREAD (Medium Priority)
// ============================================================================

void midi_thread_main() {
    HAL_MIDI::midi_initialize();
    
    while (!g_shutdown_requested) {
        // Read available MIDI bytes
        uint8_t midi_byte = HAL_MIDI::read_midi_byte();
        
        if (midi_byte != 0xFF) {  // 0xFF = no data available
            // Process this byte (state machine parsing)
            g_midi_handler.process_midi_byte(midi_byte);
        }
        
        // Yield to other threads
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

// ============================================================================
// CONTROL THREAD (Low Priority)
// ============================================================================

void control_thread_main() {
    HAL_Input::controls_initialize();
    HAL_Display::update_display("NSynth Mellotron", "Loading...");
    
    const auto frame_duration = std::chrono::milliseconds(16);  // ~60 Hz
    
    while (!g_shutdown_requested) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        
        // Read hardware inputs
        ControlInput input = {};
        
        // Read potentiometers (6 pots)
        for (int i = 0; i < 6; ++i) {
            input.potentiometer_values[i] = HAL_Input::read_potentiometer(i);
        }
        
        // Read encoders (4 corners)
        for (int i = 0; i < NUM_CORNERS; ++i) {
            input.encoder_deltas[i] = HAL_Input::read_encoder_delta(i);
        }
        
        // Read XY touch pad
        HAL_Input::read_xy_touch(input.xy_touch_x, input.xy_touch_y);
        input.xy_touch_active = (input.xy_touch_x > 0.05f && input.xy_touch_x < 0.95f) &&
                                (input.xy_touch_y > 0.05f && input.xy_touch_y < 0.95f);
        
        // Update UI state machine
        {
            std::lock_guard<std::mutex> lock(g_state_lock);
            g_ui_controller.update_ui_frame(input);
        }
        
        // Update display with current state
        std::string line1 = "Page: " + std::to_string(static_cast<int>(
            g_ui_controller.get_current_page()));
        std::string line2 = "Voices: " + std::to_string(
            g_audio_engine.get_active_voice_count());
        
        HAL_Display::update_display(line1.c_str(), line2.c_str());
        
        // Frame rate limiting
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            frame_end - frame_start);
        
        if (elapsed < frame_duration) {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }
    }
}

// ============================================================================
// FIRMWARE INITIALIZATION
// ============================================================================

bool firmware_initialize() {
    // Step 1: Initialize hardware abstraction layer
    // I2S, SPI, ADC, UART are configured by CubeMX
    
    // Step 2: Initialize audio system
    if (!g_audio_engine.initialize()) {
        HAL_Display::update_display("ERROR", "Audio init failed");
        return false;
    }
    
    // Step 3: Load samples from SD card
    HAL_Display::update_display("Loading", "Samples...");
    if (!g_sample_manager.initialize("/SAMPLES")) {
        HAL_Display::update_display("ERROR", "No samples found");
        // This is not necessarily fatal - firmware can run without samples
    }
    
    // Load default instrument (first one found)
    auto names = g_sample_manager.get_instrument_names();
    if (!names.empty()) {
        g_sample_manager.load_instrument(names[0]);
    }
    
    // Step 4: Initialize preset system
    if (!g_preset_system.initialize("/PRESETS")) {
        // Preset directory doesn't exist yet - will be created on first save
    }
    
    // Step 5: Initialize UI controller
    g_ui_controller.initialize();
    
    // Step 6: Initialize MIDI handler (no action, just set defaults)
    g_midi_handler.set_midi_channel(0);      // Listen on MIDI channel 1
    g_midi_handler.set_pitch_bend_range(2.0f);  // ±2 semitones

    // All systems initialized successfully
    HAL_Display::update_display("Ready", "NSynth Mellotron");
    return true;
}

// ============================================================================
// FIRMWARE SHUTDOWN
// ============================================================================

void firmware_shutdown() {
    g_shutdown_requested = true;
    
    // Kill all audio
    g_audio_engine.kill_all_voices();
    
    // Wait for threads to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Unmount SD card
    HAL_SDCard::unmount_sd_card();
    
    // Display shutdown message
    HAL_Display::update_display("Shutdown", "Complete");
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
// Called from STM32 CubeMX main() after HAL initialization

extern "C" int nsynth_mellotron_main() {
    // Initialize firmware components
    if (!firmware_initialize()) {
        // Initialization failed - loop indefinitely with error message
        while (1) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // Register I2S DMA interrupt handler
    // This is typically done in CubeMX stm32h7xx_it.c:
    // extern void nsynth_audio_interrupt_handler(void);
    // Then in DMA ISR or I2S callback, call nsynth_audio_interrupt_handler()
    
    // Start I2S audio output
    HAL_Audio::i2s_initialize();

    // Create and launch worker threads
    std::thread midi_thread(midi_thread_main);
    std::thread control_thread(control_thread_main);

    // Main thread: monitor system health
    int idle_loops = 0;
    while (!g_shutdown_requested) {
        // Periodically check system state
        {
            std::lock_guard<std::mutex> lock(g_state_lock);
            int active_voices = g_audio_engine.get_active_voice_count();
            
            // Could log or act on this data
            // For now, just use it to prevent optimization
            idle_loops += active_voices;
        }
        
        // Sleep to allow other threads to run
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // Join worker threads
    midi_thread.join();
    control_thread.join();

    // Shutdown
    firmware_shutdown();

    return 0;
}

// ============================================================================
// CUBEMX INTEGRATION
// ============================================================================
// Place this code in your CubeMX main.c or main.cpp:
//
// int main(void) {
//     HAL_Init();
//     SystemClock_Config();
//     MX_GPIO_Init();
//     MX_I2S_Init();
//     MX_SPI_Init();
//     MX_UART_Init();
//     MX_ADC_Init();
//     MX_TIM_Init();
//     MX_I2C_Init();
//     
//     // Enable FatFS
//     FATFS_Init();
//     
//     // Start NSynth firmware
//     nsynth_mellotron_main();
//     
//     return 0;
// }
//
// Also in stm32h7xx_it.c, add callback handlers:
//
// void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
//     nsynth_audio_interrupt_handler();
// }
//
// void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef* hi2s) {
//     nsynth_audio_interrupt_handler();
// }
//
// void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
//     // Handled in HAL_MIDI namespace
// }
//
// void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
//     // DMA complete, potentiometer data ready
// }
