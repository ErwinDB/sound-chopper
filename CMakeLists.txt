cmake_minimum_required(VERSION 3.22)

project(SoundChopper VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sets the minimum deployment target
# macOS 10.13 is the minimum required by JUCE 8.x
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
# Builds a binary that works on Apple Silicon (arm64) and Intel-based Macs
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)

# ---------------------------------------------------------------------------
# CPM / JUCE
# ---------------------------------------------------------------------------
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)

include(cmake/CPM.cmake)

CPMAddPackage(
  NAME
    JUCE
  GIT_TAG
    8.0.13
  GITHUB_REPOSITORY
    juce-framework/JUCE
  SOURCE_DIR
    ${LIB_DIR}/juce
)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(SoundChopper
    COMPANY_NAME                "ErwinDB"
    PLUGIN_MANUFACTURER_CODE    "Erwi"
    PLUGIN_CODE                 "SCho"
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "SoundChopper"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE   # Reserved for future MIDI-rhythm input
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
)

# ---------------------------------------------------------------------------
# Generate JuceHeader.h
# ---------------------------------------------------------------------------
juce_generate_juce_header(SoundChopper)

# ---------------------------------------------------------------------------
# Binary resources (background image, etc.)
# ---------------------------------------------------------------------------
juce_add_binary_data(SoundChopperBinaryData
    SOURCES
        resources/images/background.jpg
)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
target_sources(SoundChopper
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(SoundChopper
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        # Set to 1 to show the Dry/Wet slider in the editor (hidden by default;
        # the parameter is always active and defaults to 100 %).
        SHOW_DRY_WET=0
        # Set to 1 to show the filter parameters in the editor (hidden by default).
        SHOW_FILTER_PARAMS=1
)

# ---------------------------------------------------------------------------
# Link libraries
# All modules needed for current features AND the full TODO list are included:
#   juce_audio_basics      – core audio types
#   juce_audio_devices     – audio I/O
#   juce_audio_formats     – reading/writing audio files
#   juce_audio_plugin_client – plugin wrapper glue
#   juce_audio_processors  – AudioProcessor, APVTS, parameters
#   juce_audio_utils       – AudioAppComponent helpers
#   juce_core              – String, File, Timer, etc.
#   juce_data_structures   – ValueTree, UndoManager
#   juce_dsp               – DSP filters/algorithms (future filter feature)
#   juce_events            – MessageManager, event loop
#   juce_graphics          – image / path drawing (future custom UI)
#   juce_gui_basics        – Component, Button, Slider, etc. (future UI)
#   juce_gui_extra         – extra widgets, e.g. CodeEditorComponent
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

target_link_libraries(SoundChopper
    PRIVATE
        SoundChopperBinaryData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
