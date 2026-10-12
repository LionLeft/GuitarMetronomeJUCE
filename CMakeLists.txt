cmake_minimum_required(VERSION 3.22)
project(GuitarMetronomeJUCE VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# JUCE — update this path to wherever JUCE is installed on your machine
# e.g. C:/JUCE  or  C:/Users/YourName/JUCE
# ============================================================================
add_subdirectory("JUCE")

# ============================================================================
# Console app — no GUI needed
# ============================================================================
juce_add_console_app(GuitarMetronomeJUCE
    PRODUCT_NAME "GuitarMetronomeJUCE"
)

# ============================================================================
# Source files
# ============================================================================
target_sources(GuitarMetronomeJUCE PRIVATE
    main.cpp
    AudioEngine.cpp
    AudioEngine.h
)

# ============================================================================
# JUCE modules
# ============================================================================
target_link_libraries(GuitarMetronomeJUCE PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_events
)

# ============================================================================
# Required JUCE compile definitions
# ============================================================================
target_compile_definitions(GuitarMetronomeJUCE PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
