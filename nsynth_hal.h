#pragma once

#include <cstdint>
#include <cstring>

// ============================================================================
// STM32 HARDWARE ABSTRACTION LAYER FOR NSYNTH SUPER
// ============================================================================
// This layer interfaces with existing NSynth Super hardware:
// - STM32H745 dual-core MCU
// - I2S audio codec (CS4271)
// - SPI SD card interface
// - Resistive XY touch panel (ADC)
// - Encoders (TIM quadrature inputs)
// - Potentiometers (ADC channels)

// Include STM32 HAL headers (user-provided)
// Minimal set required:
// extern "C" {
//     #include "stm32h7xx_hal.h"
//     #include "fatfs.h"
// }

extern I2S_HandleTypeDef hi2s;           // I2S audio interface (from CubeMX)
extern SPI_HandleTypeDef hspi1;          // SPI SD card interface
extern ADC_HandleTypeDef hadc1;          // ADC for pots/XY touch
extern TIM_HandleTypeDef htim1;          // Timer 1 (encoder input)
extern TIM_HandleTypeDef htim2;          // Timer 2 (encoder input)
extern TIM_HandleTypeDef htim3;          // Timer 3 (encoder input)
extern TIM_HandleTypeDef htim4;          // Timer 4 (encoder input)

// FatFS includes
extern "C" {
    #include "ff.h"                      // FatFS API
    #include "sd_diskio.h"              // SD card driver
}

// ============================================================================
// AUDIO I/O (I2S DMA)
// ============================================================================

namespace HAL_Audio {

// Double-buffering for I2S (prevent clicks/pops)
constexpr int I2S_BUFFER_SIZE = 512;    // 256 samples × 2 channels (L/R)
extern int16_t i2s_tx_buffer_a[I2S_BUFFER_SIZE];
extern int16_t i2s_tx_buffer_b[I2S_BUFFER_SIZE];
extern volatile bool i2s_half_complete;
extern volatile bool i2s_full_complete;

// Initialize I2S audio output (48kHz, 16-bit stereo)
// Called once at startup
void i2s_initialize() {
    // HAL_I2S_Init() already called by CubeMX
    // Start DMA transfer with callback
    HAL_I2SEx_TransmitReceive_DMA(&hi2s, 
        (uint16_t*)i2s_tx_buffer_a, 
        (uint16_t*)i2s_tx_buffer_a, 
        I2S_BUFFER_SIZE);
}

// ISR callback: Half transfer complete (first half of buffer done)
extern "C" void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
    i2s_half_complete = true;
}

// ISR callback: Full transfer complete (second half of buffer done)
extern "C" void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef* hi2s) {
    i2s_full_complete = true;
}

// Write audio samples to active I2S buffer
// float samples: mono float [-1.0, 1.0]
// count: number of samples (will output stereo: same on L/R)
void write_audio_samples(const float* samples, int count) {
    static bool use_buffer_a = true;
    int16_t* dest = use_buffer_a ? i2s_tx_buffer_a : i2s_tx_buffer_b;
    
    // Convert float to int16 stereo
    for (int i = 0; i < count; ++i) {
        int16_t sample_int = static_cast<int16_t>(samples[i] * 32767.0f);
        
        // Left channel
        dest[i * 2] = sample_int;
        
        // Right channel (same as left for mono)
        dest[i * 2 + 1] = sample_int;
    }
    
    use_buffer_a = !use_buffer_a;
}

} // namespace HAL_Audio

// ============================================================================
// CONTROL INPUT (ADC & ENCODERS)
// ============================================================================

namespace HAL_Input {

// Potentiometer ADC channels (6 pots on ADC1)
constexpr uint8_t POT_CHANNELS[6] = {
    ADC_CHANNEL_0,   // P1 - Page Select
    ADC_CHANNEL_1,   // P2
    ADC_CHANNEL_2,   // P3
    ADC_CHANNEL_3,   // P4
    ADC_CHANNEL_4,   // P5
    ADC_CHANNEL_5    // P6
};

// XY Touch pad (resistive, dual ADC channels)
constexpr uint8_t XY_TOUCH_X = ADC_CHANNEL_8;  // X-axis
constexpr uint8_t XY_TOUCH_Y = ADC_CHANNEL_9;  // Y-axis

// Encoder TIM handles (quadrature input)
// TIM1: Corner A, TIM2: Corner B, TIM3: Corner C, TIM4: Corner D
const TIM_HandleTypeDef* encoder_timers[4] = {
    &htim1, &htim2, &htim3, &htim4
};

// Initialize control inputs
void controls_initialize() {
    // Start ADC conversions
    HAL_ADC_Start_DMA(&hadc1, nullptr, 6);
    
    // Start encoder timers (quadrature counting)
    for (int i = 0; i < 4; ++i) {
        HAL_TIM_Encoder_Start(encoder_timers[i], TIM_CHANNEL_ALL);
    }
}

// Read potentiometer value (0–4095 for 12-bit ADC)
// Returns normalized 0.0–1.0
float read_potentiometer(int pot_index) {
    if (pot_index < 0 || pot_index >= 6) return 0.5f;
    
    // ADC data stored in DMA buffer
    extern uint32_t adc_dma_buffer[6];
    uint32_t raw = adc_dma_buffer[pot_index];
    
    return raw / 4095.0f;
}

// Read XY touch position
// Returns 0.0–1.0 for both X and Y
void read_xy_touch(float& out_x, float& out_y) {
    extern uint32_t adc_dma_buffer[6];
    
    // X and Y channels follow potentiometers
    uint32_t raw_x = adc_dma_buffer[6];  // After 6 pots
    uint32_t raw_y = adc_dma_buffer[7];
    
    out_x = raw_x / 4095.0f;
    out_y = raw_y / 4095.0f;
}

// Read encoder delta (rotation since last read)
// Returns: -N to +N for rotation steps
int read_encoder_delta(int corner_index) {
    if (corner_index < 0 || corner_index >= 4) return 0;
    
    TIM_HandleTypeDef* tim = (TIM_HandleTypeDef*)encoder_timers[corner_index];
    int16_t count = __HAL_TIM_GET_COUNTER(tim);
    
    // Reset counter for next read
    __HAL_TIM_SET_COUNTER(tim, 0);
    
    // Each quadrature step = 1; divide by 2 for detents (if applicable)
    return count / 2;
}

} // namespace HAL_Input

// ============================================================================
// SERIAL MIDI INPUT (UART)
// ============================================================================

namespace HAL_MIDI {

extern UART_HandleTypeDef huart3;  // UART3 for MIDI (31.25 kbaud)

constexpr int MIDI_RX_BUFFER_SIZE = 256;
uint8_t midi_rx_buffer[MIDI_RX_BUFFER_SIZE];
volatile int midi_write_idx = 0;
volatile int midi_read_idx = 0;

void midi_initialize() {
    // Start UART receive with interrupt
    HAL_UART_Receive_IT(&huart3, &midi_rx_buffer[0], 1);
}

// UART RX complete callback (called for each byte)
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart->Instance == USART3) {
        // Advance write pointer
        midi_write_idx = (midi_write_idx + 1) % MIDI_RX_BUFFER_SIZE;
        
        // Re-enable interrupt for next byte
        HAL_UART_Receive_IT(&huart3, 
            &midi_rx_buffer[midi_write_idx], 1);
    }
}

// Read next MIDI byte (if available)
// Returns 0xFF if no data available
uint8_t read_midi_byte() {
    if (midi_read_idx == midi_write_idx) {
        return 0xFF;  // No data
    }
    
    uint8_t byte = midi_rx_buffer[midi_read_idx];
    midi_read_idx = (midi_read_idx + 1) % MIDI_RX_BUFFER_SIZE;
    
    return byte;
}

} // namespace HAL_MIDI

// ============================================================================
// SD CARD FILE I/O (FatFS)
// ============================================================================

namespace HAL_SDCard {

// FatFS objects
FATFS sd_filesystem;
FIL sd_file;
DIR sd_dir;
FILINFO sd_fileinfo;

// Mount SD card (call once at startup)
bool mount_sd_card() {
    FRESULT res = f_mount(&sd_filesystem, "", 1);  // 1 = immediate mount
    
    if (res != FR_OK) {
        // Mount failed
        return false;
    }
    
    return true;
}

// Unmount SD card (call on shutdown)
void unmount_sd_card() {
    f_unmount("");
}

// Open file for reading
// path: full path, e.g., "/SAMPLES/Violin/36_C2.wav"
// Returns: 0 on success, error code otherwise
int open_file_read(const char* path) {
    FRESULT res = f_open(&sd_file, path, FA_READ);
    return (res == FR_OK) ? 0 : static_cast<int>(res);
}

// Read bytes from open file
int read_file(uint8_t* buffer, int count) {
    UINT bytes_read = 0;
    FRESULT res = f_read(&sd_file, buffer, count, &bytes_read);
    
    if (res != FR_OK) {
        return -1;
    }
    return bytes_read;
}

// Seek in open file (absolute position)
int seek_file(uint32_t offset) {
    FRESULT res = f_lseek(&sd_file, offset);
    
    if (res != FR_OK) {
        return -1;
    }
    return 0;
}

// Get current file position
uint32_t tell_file() {
    return f_tell(&sd_file);
}

// Get file size
uint32_t get_file_size() {
    return f_size(&sd_file);
}

// Close open file
void close_file() {
    f_close(&sd_file);
}

// List directory contents
// path: directory path, e.g., "/SAMPLES/Violin"
// Returns: 0 on success
int open_directory(const char* path) {
    FRESULT res = f_opendir(&sd_dir, path);
    return (res == FR_OK) ? 0 : static_cast<int>(res);
}

// Get next directory entry
// Returns: pointer to FILINFO on success, nullptr on end or error
const FILINFO* read_directory() {
    FRESULT res = f_readdir(&sd_dir, &sd_fileinfo);
    
    if (res != FR_OK || sd_fileinfo.fname[0] == 0) {
        return nullptr;  // End of directory
    }
    
    return &sd_fileinfo;
}

// Close directory
void close_directory() {
    f_closedir(&sd_dir);
}

} // namespace HAL_SDCard

// ============================================================================
// DISPLAY (OLED via I2C - optional, for status only)
// ============================================================================

namespace HAL_Display {

extern I2C_HandleTypeDef hi2c1;  // I2C for OLED/LCD

// Simple display update (not critical for audio)
// Typically displays current page, parameter values, etc.
void update_display(const char* line1, const char* line2) {
    // TODO: I2C OLED library calls
    // This is auxiliary; audio works without display
}

} // namespace HAL_Display

#endif  // NSYNTH_HAL_H
