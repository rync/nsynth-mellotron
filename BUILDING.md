# Building & Deploying NSynth Mellotron Firmware

## Quick Start (Desktop/Testing)

### On Linux/macOS

```bash
# Clone or download the source code
cd nsynth-mellotron-firmware

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Compile
cmake --build . -j4

# Run (if implemented with mock hardware)
./bin/nsynth_firmware
```

### On Windows (Visual Studio)

```bash
# Configure
cmake -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . --config Release -j4

# Output: bin\Release\nsynth_firmware.exe
```

---

## Embedded Hardware Integration (NSynth Super)

The firmware is designed to run on the Open NSynth Super's STM32H7-series microcontroller.
Hardware-specific integration is required for:

1. **I2S Audio Interface** (DMA stereo output)
2. **SD Card SPI Interface** (FatFS/exFAT)
3. **Serial UART** (MIDI input @ 31.25kbps)
4. **Control Interface** (4 encoders, 6 potentiometers, XY resistive touch)
5. **Display Driver** (SPI/I2C to LCD)

### Hardware Abstraction Layer (HAL)

The firmware is structured to minimize hardware-specific code. Main integration points:

#### 1. Audio Output (audio_engine.cpp)

```cpp
// Mock implementation in main.cpp I2S interrupt:
extern "C" void i2s_dma_interrupt_handler(void* user_data) {
    static float output_buffer[AUDIO_BUFFER_SIZE];
    g_audio_state.audio_engine.process_audio_block(output_buffer, AUDIO_BUFFER_SIZE);
    
    // TODO: Replace with actual HAL call:
    // write_i2s_samples(output_buffer, AUDIO_BUFFER_SIZE);
}
```

**Integration Steps:**
1. Set up I2S peripheral (48kHz, stereo, 16-bit PCM)
2. Configure DMA double-buffering (256-sample blocks)
3. Register interrupt callback: `register_i2s_dma_interrupt(i2s_dma_interrupt_handler)`
4. Implement `write_i2s_samples()` to feed DMA buffer

#### 2. SD Card Access (sample_manager.cpp, preset_system.cpp)

The current implementation has placeholder file I/O:

```cpp
// TODO markers in implementation:
// - scan_instrument_directory_() for directory enumeration
// - load_wav_file_metadata_() for WAV header parsing
// - serialize/deserialize JSON with file I/O
```

**Integration Steps:**
1. Use **FatFS** (lightweight, portable RTOS-agnostic library)
2. Or STM32 HAL SDMMC + FatFS wrapper
3. Example FatFS usage:

```cpp
#include "ff.h"  // FatFS

bool scan_instrument_directory_(const std::string& dir_path,
                               InstrumentLibrary& out_library) {
    FRESULT res;
    DIR dj;
    FILINFO fno;
    
    res = f_opendir(&dj, dir_path.c_str());
    if (res != FR_OK) return false;
    
    while (true) {
        res = f_readdir(&dj, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;
        
        if (fno.fattrib & AM_DIR) continue;  // Skip directories
        if (fno.fname[strlen(fno.fname)-1] == 'v') {  // .wav file
            // Process WAV file...
        }
    }
    f_closedir(&dj);
    return true;
}
```

#### 3. Control Input (ui_controller.cpp)

Placeholder input reading:

```cpp
// In control_thread_fn():
ControlInput input = {};

// TODO: Read from hardware:
// read_corner_encoders(input.encoder_deltas);  // [-N, +N] per frame
// read_potentiometers(input.potentiometer_values);  // 0.0–1.0 normalized
// read_xy_touch(input.xy_touch_x, input.xy_touch_y, input.xy_touch_active);
```

**Encoder Interface:**
- 4 quadrature encoders (TIM peripheral with CH1/CH2 inputs)
- Read delta rotation per control loop frame

**Potentiometer Interface:**
- 6 ADC channels, 12-bit resolution
- Normalize to 0.0–1.0: `value / 4095.0f`

**XY Touch Pad:**
- Resistive analog touch panel
- X/Y axes via 2× ADC (or multiplexed)
- Normalize to 0.0–1.0 based on known panel dimensions

#### 4. Serial MIDI (midi_handler.cpp)

```cpp
// TODO: Replace with actual UART read:
// uint8_t midi_byte = read_serial_byte_if_available();
// if (midi_byte != 0xFF) {
//     g_audio_state.midi_handler.process_midi_byte(midi_byte);
// }
```

**UART Configuration:**
- 31250 baud (MIDI standard)
- 1 start bit, 8 data bits, 1 stop bit, no parity
- Example STM32 HAL:

```c
UART_HandleTypeDef huart3;
huart3.Instance = USART3;
huart3.Init.BaudRate = 31250;
huart3.Init.WordLength = UART_WORDLENGTH_8B;
huart3.Init.StopBits = UART_STOPBITS_1;
huart3.Init.Parity = UART_PARITY_NONE;

HAL_UART_Init(&huart3);
HAL_UART_Receive_IT(&huart3, &midi_byte_rx, 1);  // Enable RX interrupt
```

#### 5. Display Driver (ui_controller.cpp)

```cpp
// TODO: update_display_with_current_state(...);
```

Implement to refresh LCD with:
- Current page name
- Parameter labels and values (P1–P6)
- Page-specific graphics (knobs, sliders, preset grid)
- XY pad position

**Display Hardware** (assumed):
- Small OLED or LCD (e.g., 128×64 or 320×240)
- SPI or I2C interface
- Examples: SSD1306 (OLED), ILI9341 (LCD)

---

## Configuration & Customization

### Audio Parameters (nsynth_types.h)

```cpp
constexpr int SAMPLE_RATE = 48000;          // 48kHz
constexpr int AUDIO_BUFFER_SIZE = 256;      // 5.3ms latency
constexpr int MAX_POLYPHONY = 16;           // Voices
constexpr float TARGET_LATENCY_MS = 5.0f;   // Target
```

Increase `AUDIO_BUFFER_SIZE` if CPU is overloaded (higher latency trade-off).

### MIDI Configuration

```cpp
// In midi_handler.cpp initialization:
g_audio_state.midi_handler.set_midi_channel(0);  // Listen on channel 1
g_audio_state.midi_handler.set_pitch_bend_range(2.0f);  // ±2 semitones
```

### Potentiometer Mode (default: Catch-Up)

Change in ui_controller.cpp:

```cpp
global_params_.pot_mode = PotentiometerMode::JUMP;      // Immediate
// OR
global_params_.pot_mode = PotentiometerMode::CATCH_UP;  // Lock until crossing
// OR
global_params_.pot_mode = PotentiometerMode::RELATIVE;  // Delta scaling
```

---

## Compilation Flags

### Optimized Build

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j4
```

### Debug Build (with symbols)

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j4

# For GDB debugging:
gdb ./bin/nsynth_firmware
(gdb) run
```

### Embedded Flags (for bare-metal STM32)

Uncomment in CMakeLists.txt:

```cmake
set(MCU "-mcpu=cortex-m7")
set(FPU "-mfpu=fpv5-d16 -mfloat-abi=hard")
target_compile_options(nsynth_firmware PRIVATE ${MCU} ${FPU})
target_link_options(nsynth_firmware PRIVATE ${MCU} ${FPU})

# Optional: Link against STM32 HAL
# target_link_libraries(nsynth_firmware PRIVATE stm32h7xx_hal)
```

---

## Flashing to Hardware

### Using ST-Link (STM32 Official Programmer)

```bash
# Using st-flash (stm32flash tool):
st-flash write nsynth_firmware.bin 0x8000000

# Or using OpenOCD:
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg
# Then in GDB:
(gdb) target remote localhost:3333
(gdb) load
(gdb) continue
```

### Using Custom Bootloader

If NSynth Super has a serial bootloader:

```bash
# Example: STM32 ROM bootloader via UART
python stm32_flash.py --port /dev/ttyUSB0 --file nsynth_firmware.bin
```

---

## Testing Checklist

### Desktop (No Hardware)

- [x] Code compiles without errors
- [x] All includes resolve
- [x] Parameter structures initialize correctly
- [ ] Mock MIDI parsing (add unit tests)
- [ ] Preset JSON serialization/deserialization

### Hardware (With NSynth Super)

- [ ] I2S DMA interrupts fire (~10.67kHz @ 256-sample blocks)
- [ ] Audio output is clean (no clicks/pops)
- [ ] SD card samples load correctly
- [ ] Encoders/pots are responsive
- [ ] XY pad blending works (mix corners smoothly)
- [ ] MIDI Note On triggers voices
- [ ] Pitch bend modulates frequency
- [ ] All 8 pages navigate correctly
- [ ] Preset save/recall works

### Performance Monitoring

```cpp
// In control thread, log active voice count:
int active_voices = g_audio_state.audio_engine.get_active_voice_count();
if (active_voices > 0) {
    debug_log("Active voices: %d", active_voices);
}
```

---

## Troubleshooting

### Build Errors

**"C++17 features not available"**
- Update compiler (GCC 7+, Clang 5+)
- Or change `CMAKE_CXX_STANDARD` to 14 (remove structured bindings if needed)

**"CMake not found"**
```bash
# Install CMake
# macOS: brew install cmake
# Linux (Ubuntu): sudo apt-get install cmake
# Windows: Download from cmake.org
```

### Runtime Issues

**Audio Crackling/Dropout**
- Increase AUDIO_BUFFER_SIZE (higher latency)
- Reduce polyphony or disable LFO
- Profile CPU usage in control/MIDI threads

**SD Card Not Found**
- Check SD card formatting (FAT32/exFAT)
- Verify sample filenames match convention (`36_C2.wav`)
- Increase SPI clock if errors persist

**MIDI Not Responding**
- Verify 31.25kbps baud rate
- Check UART pin connections (TX)
- Test with external MIDI monitor

---

## Next Steps

1. **Prepare Hardware Abstraction Layer (HAL)**
   - Implement actual I2S, SD card, UART, ADC drivers
   - Integration with NSynth Super STM32 peripherals

2. **Test Individual Modules**
   - ADSR envelope generator
   - Audio mixing and resampling
   - Parameter routing

3. **Integration Testing**
   - Full firmware on real hardware
   - Live MIDI performance testing
   - Sample library playback

4. **Optimization**
   - Profile CPU/memory usage
   - Reduce audio latency if possible
   - Tune voice allocation algorithm

---

## Additional Resources

- **FatFS Library:** http://elm-chan.org/fsw/ff/
- **STM32 HAL Documentation:** https://www.st.com/
- **Embedded Audio:** "The Audio Programming Book" (Boulanger & Lazzarini)
- **Open NSynth Super:** https://github.com/google/open-nsynth-super

---

For questions or issues during integration, refer to the main README.md or consult the inline code comments.
