# NSynth Super - Integration Guide

This document explains how to integrate the Mellotron firmware into the existing NSynth Super hardware.

---

## NSynth Super Hardware Overview

The Open NSynth Super is based on:

| Component | Spec | Purpose |
|-----------|------|---------|
| **MCU** | STM32H745XI (dual-core) | Main processor |
| **Audio Codec** | CS4271 | I2S audio I/O |
| **SD Card** | SDIO/SPI interface | Sample & preset storage |
| **Display** | 240×240 OLED (I2C) | UI feedback |
| **Input Controls** | 4 encoders, 6 pots, XY pad | Parameter control |
| **MIDI Input** | UART3 @ 31.25 kbaud | Keyboard input |
| **Audio I/O** | 1/4" line-out (stereo) | Speaker/mixer output |

---

## Current NSynth Super Firmware

The stock firmware:
- Uses **Linux userspace** on ARM Cortex-A53 (separate SoM)
- Runs **TensorFlow** autoencoder inference
- High latency (~500ms+) and complex dependencies
- External Python pipeline required

**Our new firmware:**
- Bare-metal STM32H7 (no OS)
- Mellotron sample-playback synthesis
- <5ms latency, deterministic real-time
- Minimal dependencies (FatFS only)

---

## Prerequisites

### Hardware Required
- Open NSynth Super board (PCB + components)
- ST-Link v2 or compatible programmer
- USB cable (for flashing)
- Micro SD card (tested up to 32GB)

### Software Required
- ARM GCC toolchain (arm-none-eabi-gcc)
- STM32CubeMX (for HAL generation)
- OpenOCD (for programming via ST-Link)
- Python 3.x (for utilities)
- CMake 3.16+

### Install Instructions

**macOS:**
```bash
brew install arm-none-eabi-gcc openocd cmake
```

**Ubuntu/Debian:**
```bash
sudo apt-get install gcc-arm-none-eabi openocd cmake
```

**Windows:**
- Download ARM GCC from https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm
- Download OpenOCD from http://openocd.org/
- Add to PATH

---

## Step-by-Step Integration

### Phase 1: Prepare Hardware Files (Using CubeMX)

**Goal:** Generate STM32 HAL layer matching NSynth Super hardware

1. **Download STM32CubeMX** (free from ST Microelectronics)

2. **Open NSynth Super Project**
   - If existing .ioc file available, use it
   - Otherwise, create new project for STM32H745XI

3. **Configure Peripherals**

   **I2S (Audio Output):**
   - Peripheral: I2S3 (connected to CS4271 codec)
   - Mode: Master Transmit
   - Frequency: 48 kHz
   - Data Format: 16-bit PCM
   - Enable DMA: I2S3_TX (DMA1 Channel)

   **SPI (SD Card):**
   - Peripheral: SPI1 or SPI2
   - Mode: Full Duplex Master
   - Prescaler: 2 (fast SPI clock)
   - Enable DMA for RX/TX

   **UART (MIDI):**
   - Peripheral: USART3
   - Baud Rate: 31250 (MIDI standard)
   - Data: 8 bits, 1 stop bit, no parity
   - Enable interrupt for RX

   **ADC (Potentiometers & XY Touch):**
   - Channels: 0–9 (6 pots + XY + extras)
   - Scan Mode: Continuous
   - DMA: Enabled, circular

   **Timers (Encoders):**
   - TIM1, TIM2, TIM3, TIM4
   - Mode: Encoder mode, count on TI1 & TI2
   - Prescaler: 1 (full resolution)

   **I2C (Display):**
   - Peripheral: I2C1
   - Speed: 400 kHz (fast mode)
   - Interrupt enabled

4. **Generate Code**
   - Click "Generate Code" in CubeMX
   - Select output folder: `hal/stm32_hal/`
   - Project format: Makefile (or CMake if available)

### Phase 2: Build Firmware

1. **Copy Generated HAL Files**
   ```bash
   cp -r hal/stm32_hal/Drivers/* hal/stm32_drivers/
   cp hal/stm32_hal/Inc/stm32h7xx_hal.h hal/
   ```

2. **Create CMakeLists.txt Overlay** (for NSynth)
   ```cmake
   # In CMakeLists.txt, add:
   
   # STM32 HAL Libraries
   target_link_libraries(nsynth_firmware PRIVATE
       -Lhal/stm32_drivers/
       -lstm32h7xx_hal
       -lstm32h7xx_ll
       -lc_nano
   )
   
   # FatFS for SD card
   target_include_directories(nsynth_firmware PRIVATE
       hal/fatfs/
   )
   target_sources(nsynth_firmware PRIVATE
       hal/fatfs/diskio.c
       hal/fatfs/ff.c
   )
   
   # Link script for bare-metal
   target_link_options(nsynth_firmware PRIVATE
       -Thal/STM32H745XIIx_FLASH.ld
   )
   ```

3. **Build**
   ```bash
   mkdir build && cd build
   cmake -DCMAKE_TOOLCHAIN_FILE=../hal/arm_toolchain.cmake ..
   cmake --build . -j4
   ```

   Output: `bin/nsynth_mellotron.elf`

### Phase 3: Flash Firmware

1. **Connect ST-Link to NSynth Super**
   ```
   ST-Link    NSynth Super (JTAG header)
   ─────────────────────────────────
   GND        GND
   3.3V       VCC
   SWDIO      SWDIO (or TMS)
   SWCLK      SWCLK (or TCK)
   ```

2. **Verify Connection**
   ```bash
   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
     -c "init" -c "targets" -c "exit"
   ```
   Should show: `STM32H745XI.cpu`

3. **Flash Binary**
   ```bash
   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
     -c "program bin/nsynth_mellotron.elf verify reset" \
     -c "exit"
   ```

4. **Power Cycle NSynth**
   - Unplug USB power
   - Wait 3 seconds
   - Plug back in
   - Display should show "Ready"

---

## Verify Installation

### Audio Output Test

1. Connect NSynth to audio interface (headphones/speakers)
2. Start MIDI sequencer or keyboard
3. Send MIDI Note On (e.g., C4 = MIDI 60)
4. Should hear sample playback
5. Adjust encoders/potentiometers
6. Parameter changes should affect sound

### Display Check

- Power on: "NSynth Mellotron" message appears
- Loading: "Loading Samples..." during init
- Ready: "Ready" with page name
- UI: Page/parameter info updates at ~60 Hz

### SD Card Check

Create test file on SD card:
```bash
echo "test" > /Volumes/SDCARD/test.txt
```

Firmware should read and recognize SD card. Check by:
- Adding test samples and verifying they load
- Saving a preset and checking file appears

---

## Troubleshooting Integration

### Firmware Won't Flash

**Problem:** "Failed to flash"

**Solutions:**
1. Check ST-Link drivers installed
2. Verify JTAG connections (GND, 3.3V, SWDIO, SWCLK)
3. Power cycle board before flashing
4. Try in DFU mode (hold BOOT button during reset)

### Firmware Crashes on Boot

**Problem:** LED flashes or system hangs

**Solutions:**
1. Check stack/heap sizes in linker script (may be too small)
2. Verify all interrupt vectors configured
3. Enable debug logging to UART for error messages
4. Use OpenOCD GDB to step through boot code

### Audio Not Working

**Problem:** No sound output

**Solutions:**
1. Verify I2S codec (CS4271) is initialized
2. Check audio connections (line-out to speakers)
3. Verify I2S DMA interrupts are firing
4. Use oscilloscope to check I2S signals

### SD Card Not Detected

**Problem:** "No samples found" error

**Solutions:**
1. Verify SPI clock is fast enough (~20 MHz)
2. Check SD card formatting (FAT32)
3. Ensure directory structure exists (`/SAMPLES/`)
4. Use FatFS tools to debug SD interface

### MIDI Not Responsive

**Problem:** Keyboard commands don't trigger notes

**Solutions:**
1. Verify UART3 is running at 31250 baud
2. Check MIDI cable connections
3. Scope MIDI input line for data
4. Test with external MIDI monitor software

---

## Integration Checkpoints

| Checkpoint | Status | Notes |
|------------|--------|-------|
| CubeMX HAL generation | ✓ | Create once, reuse |
| CMake build passes | ✓ | Should compile cleanly |
| ST-Link connection works | ✓ | OpenOCD finds target |
| Firmware flashes | ✓ | Binary writes to flash |
| Boot messages appear | ✓ | Display shows "Ready" |
| SD card mounts | ✓ | Samples directory found |
| MIDI input works | ✓ | Keyboard triggers notes |
| Audio output plays | ✓ | Speaker/headphones hear sound |
| Parameters respond | ✓ | Encoders/pots change sound |

---

## Simplified Path: Start with Simulator

If you don't have hardware yet, test the firmware in a simulator:

1. **Build desktop version**
   ```bash
   mkdir build_sim && cd build_sim
   cmake -DCMAKE_BUILD_TYPE=Debug ..
   cmake --build .
   ./bin/nsynth_firmware
   ```

2. **Run unit tests**
   ```bash
   ./bin/unit_tests
   ```

3. **Verify logic before hardware**
   - ADSR envelope behavior
   - Pitch shifting calculations
   - XY blending weights
   - Parameter routing

---

## File Structure After Integration

```
nsynth-mellotron-firmware/
├── CMakeLists.txt
├── main.cpp
├── nsynth_types.h
├── audio_engine.{h,cpp}
├── sample_manager.cpp         ← With real FatFS calls
├── adsr.{h,cpp}
├── ui_controller.{h,cpp}
├── midi_handler.{h,cpp}
├── preset_system.{h,cpp}
│
├── hal/
│   ├── nsynth_hal.h            ← Real HAL abstractions
│   ├── arm_toolchain.cmake     ← ARM GCC configuration
│   ├── STM32H745XIIx_FLASH.ld  ← Linker script
│   ├── stm32_hal/              ← CubeMX generated files
│   │   ├── Drivers/
│   │   ├── Inc/
│   │   └── Src/
│   └── fatfs/                  ← FatFS source
│       ├── ff.c
│       ├── ff.h
│       ├── diskio.c
│       └── diskio.h
│
├── tests/
│   └── unit_tests.cpp
│
├── README.md
├── BUILDING.md
├── ARCHITECTURE.md
├── SD_CARD_SETUP.md
└── NSYNTH_INTEGRATION.md       ← This file
```

---

## Next Steps

1. **Prepare Hardware:** Gather NSynth Super board + ST-Link
2. **Generate HAL:** Run CubeMX for your specific board revision
3. **Configure Build:** Update CMakeLists.txt with HAL paths
4. **Compile:** Build firmware binary
5. **Flash:** Use OpenOCD to write to STM32
6. **Test:** Verify audio, MIDI, and controls
7. **Load Samples:** Populate SD card with WAV files
8. **Enjoy:** Play your Mellotron synthesizer!

---

## Support & Debugging

### Enable Logging

Add UART debug output:

```cpp
// In main.cpp uart_debug_init():
HAL_UART_Transmit(&huart_debug, (uint8_t*)"NSynth Init\n", 12, 100);
```

### Connect Debugger

Use OpenOCD + GDB for live debugging:

```bash
# Terminal 1: Start OpenOCD
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg

# Terminal 2: Connect GDB
arm-none-eabi-gdb build/bin/nsynth_mellotron.elf
(gdb) target remote localhost:3333
(gdb) load
(gdb) break main
(gdb) continue
```

### Measure Performance

Use STM32 cycle counter for profiling:

```cpp
uint32_t start = DWT->CYCCNT;
// ... code to measure ...
uint32_t elapsed = DWT->CYCCNT - start;
float ms = (float)elapsed / (480000000 / 1000);  // 480 MHz clock
```

---

For hardware-specific questions, consult the [Open NSynth Super](https://github.com/google/open-nsynth-super) repository.
