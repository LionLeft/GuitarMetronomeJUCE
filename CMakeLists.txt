cmake_minimum_required(VERSION 3.22)
project(GuitarMetronome VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# JUCE — update this path to wherever JUCE is installed on your machine
# e.g. C:/JUCE  or  C:/Users/YourName/JUCE
# ============================================================================
add_subdirectory("C:/Users/leyteris/CLionProjects/GuitarMetronome/JUCE")

# ============================================================================
# Console app — no GUI needed
# ============================================================================
juce_add_console_app(GuitarMetronome
    PRODUCT_NAME "GuitarMetronome"
)

# ============================================================================
# Source files
# ============================================================================
target_sources(GuitarMetronome PRIVATE
    main.cpp
    AudioEngine.cpp
    AudioEngine.h
)

# ============================================================================
# JUCE modules
# ============================================================================
target_link_libraries(GuitarMetronome PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_events
)

# ============================================================================
# Required JUCE compile definitions
# ============================================================================
target_compile_definitions(GuitarMetronome PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
