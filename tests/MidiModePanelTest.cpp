// MidiModePanelTest.cpp – tests for MidiModePanel.h
//
// MidiModePanel groups the MIDI-input controls and three LED indicators.
// We test construction, updateLeds() reflecting processor atomics, and
// the resized() layout code.

#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "MidiModePanel.h"

// ============================================================================
//  Construction
// ============================================================================
TEST_CASE ("MidiModePanel constructs without crashing", "[midimodepanel]")
{
    SoundChopperAudioProcessor proc;
    MidiModePanel mp (proc, proc.apvts);
    (void) mp;
}

TEST_CASE ("MidiModePanel kNumRows is 5", "[midimodepanel]")
{
    REQUIRE (MidiModePanel::kNumRows == 5);
}

// ============================================================================
//  updateLeds
// ============================================================================
TEST_CASE ("MidiModePanel updateLeds with all LEDs off", "[midimodepanel][leds]")
{
    SoundChopperAudioProcessor proc;
    MidiModePanel mp (proc, proc.apvts);

    proc.midiNoteLedActive.store (false);
    proc.gateLedActive    .store (false);
    proc.bypassLedActive  .store (false);

    REQUIRE_NOTHROW (mp.updateLeds());
}

TEST_CASE ("MidiModePanel updateLeds with all LEDs on", "[midimodepanel][leds]")
{
    SoundChopperAudioProcessor proc;
    MidiModePanel mp (proc, proc.apvts);

    proc.midiNoteLedActive.store (true);
    proc.gateLedActive    .store (true);
    proc.bypassLedActive  .store (true);

    REQUIRE_NOTHROW (mp.updateLeds());
}

TEST_CASE ("MidiModePanel updateLeds toggles correctly", "[midimodepanel][leds]")
{
    SoundChopperAudioProcessor proc;
    MidiModePanel mp (proc, proc.apvts);

    for (int i = 0; i < 4; ++i)
    {
        const bool state = (i % 2 == 0);
        proc.midiNoteLedActive.store (state);
        proc.gateLedActive    .store (!state);
        proc.bypassLedActive  .store (state);
        REQUIRE_NOTHROW (mp.updateLeds());
    }
}

// ============================================================================
//  resized
// ============================================================================
TEST_CASE ("MidiModePanel resized does not crash", "[midimodepanel]")
{
    SoundChopperAudioProcessor proc;
    MidiModePanel mp (proc, proc.apvts);
    mp.setSize (500, 400);
    REQUIRE_NOTHROW (mp.resized());
}

TEST_CASE ("MidiModePanel resized with zero size does not crash", "[midimodepanel]")
{
    SoundChopperAudioProcessor proc;
    MidiModePanel mp (proc, proc.apvts);
    mp.setSize (0, 0);
    REQUIRE_NOTHROW (mp.resized());
}
