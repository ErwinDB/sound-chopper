# ---------------------------------------------------------------------------
# Fetch Catch2 via CPM
# ---------------------------------------------------------------------------
CPMAddPackage(
    NAME Catch2
    GIT_TAG v3.4.0
    GITHUB_REPOSITORY catchorg/Catch2
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/catch2
    OPTIONS
        "CATCH_INSTALL_DOCS OFF"
        "CATCH_INSTALL_EXTRAS OFF"
)

# ---------------------------------------------------------------------------
# Unit-test executable
#
# We compile the plugin's non-wrapper source files directly into the test
# binary.  PluginEditor.cpp is also included so that the editor class is
# available for editor tests.  BinaryData (background image) is linked so
# that <BinaryData.h> in PluginEditor.cpp resolves correctly.
#
# JucePlugin_Name and a few other JucePlugin_* macros are defined here
# because they are used in PluginProcessor.h / PluginProcessor.cpp; in a
# normal plugin build the plugin-client wrapper defines them automatically.
# ---------------------------------------------------------------------------
juce_add_console_app(SoundChopper_Tests
    PRODUCT_NAME "SoundChopper Tests"
)

target_sources(SoundChopper_Tests
    PRIVATE
        Main.cpp
        PluginProcessorTest.cpp
        EditorLayoutTest.cpp
        LedIndicatorTest.cpp
        NoteValueButtonTest.cpp
        FilterPanelTest.cpp
        AdvancedPanelTest.cpp
        MidiModePanelTest.cpp
        PluginEditorTest.cpp
        ${CMAKE_SOURCE_DIR}/src/PluginProcessor.cpp
        ${CMAKE_SOURCE_DIR}/src/PluginEditor.cpp
)

target_include_directories(SoundChopper_Tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(SoundChopper_Tests
    PRIVATE
        # Minimal set of JucePlugin_* macros required by PluginProcessor.h
        JucePlugin_Name="SoundChopper"
        JucePlugin_Manufacturer="ErwinDB"
        JucePlugin_ManufacturerCode=0x45727769
        JucePlugin_PluginCode=0x5343686f
        JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_EditorRequiresKeyboardFocus=0
        JucePlugin_Version=0.1.0
        JucePlugin_VersionCode=0x00010000
        JucePlugin_VersionString="0.1.0"
        JucePlugin_Latency=0
        JucePlugin_MaxNumInputChannels=2
        JucePlugin_MaxNumOutputChannels=2
        # Feature flags – must match the main plugin target
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        SHOW_DRY_WET=0
        SHOW_FILTER_PARAMS=1
)

target_link_libraries(SoundChopper_Tests
    PRIVATE
        SoundChopperBinaryData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        Catch2::Catch2
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

include(CTest)
include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(SoundChopper_Tests)
