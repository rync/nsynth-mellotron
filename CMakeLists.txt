cmake_minimum_required(VERSION 3.16)
project(nsynth_mellotron_firmware C CXX)

# ============================================================================
# PROJECT CONFIGURATION
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")  # Embedded
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")                        # Optimize for speed

# For embedded/bare-metal, uncomment:
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --specs=nosys.specs")

# ============================================================================
# SOURCE FILES
# ============================================================================

set(SOURCES
    main.cpp
    nsynth_types.h
    adsr.h
    adsr.cpp
    audio_engine.h
    audio_engine.cpp
    sample_manager.h
    sample_manager.cpp
    preset_system.h
    preset_system.cpp
    ui_controller.h
    ui_controller.cpp
    midi_handler.h
    midi_handler.cpp
)

# ============================================================================
# BUILD TARGETS
# ============================================================================

# Main firmware executable
add_executable(nsynth_firmware ${SOURCES})

# Compiler options
target_compile_options(nsynth_firmware PRIVATE
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

# Link options
target_link_options(nsynth_firmware PRIVATE
    -Wl,--gc-sections
)

# Libraries (if using pthread on Linux/desktop for testing)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(nsynth_firmware PRIVATE pthread)
endif()

# ============================================================================
# OPTIONAL: Hardware-Specific Flags
# ============================================================================

# For STM32 targets (example: STM32H7 series on NSynth Super)
# Uncomment and modify as needed:

# set(MCU "-mcpu=cortex-m7")
# set(FPU "-mfpu=fpv5-d16 -mfloat-abi=hard")
# target_compile_options(nsynth_firmware PRIVATE ${MCU} ${FPU})
# target_link_options(nsynth_firmware PRIVATE ${MCU} ${FPU})

# ============================================================================
# OUTPUT
# ============================================================================

# Set output directory
set_target_properties(nsynth_firmware PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Print build info
message(STATUS "Building NSynth Mellotron Firmware")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# ============================================================================
# OPTIONAL: Unit Tests
# ============================================================================

# Uncomment to enable testing:
# enable_testing()
# add_subdirectory(tests)
