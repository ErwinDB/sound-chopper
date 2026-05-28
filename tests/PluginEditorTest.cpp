// PluginEditorTest.cpp – tests for SoundChopperAudioProcessorEditor
//
// These tests exercise the editor by constructing it directly.
// Off-screen rendering is used for paint() so no display is needed.

#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EditorLayout.h"

using namespace EditorLayout;

// Set a choice parameter by index using the APVTS
static void setChoiceParam (juce::AudioProcessorValueTreeState& apvts,
                             const juce::String& id, int index)
{
    auto* param = apvts.getParameter (id);
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
}

// ============================================================================
//  Construction / destruction
// ============================================================================
TEST_CASE ("Editor constructs and destructs without crashing", "[editor]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    (void) ed;
}

TEST_CASE ("Editor has correct initial size", "[editor]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    REQUIRE (ed.getWidth()  == kWindowWidth);
    REQUIRE (ed.getHeight() > 0);
}

// ============================================================================
//  computeNumRows
// ============================================================================
TEST_CASE ("Editor computeNumRows in fixed-pattern mode", "[editor][rows]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);

    // Fixed-pattern (source=0): at least 1 (source) + 1 (pattern) + 1 (adv btn)
    const int rows = ed.computeNumRows();
    REQUIRE (rows >= 3);
}

TEST_CASE ("Editor computeNumRows in MIDI mode", "[editor][rows]")
{
    SoundChopperAudioProcessor proc;
    setChoiceParam (proc.apvts, ParamID::rhythmSource, 1);  // MIDI source

    SoundChopperAudioProcessorEditor ed (proc);

    const int rows = ed.computeNumRows();
    // 1 (source) + MidiModePanel::kNumRows + filterPanel.numRows() + 1 (adv btn)
    REQUIRE (rows >= 1 + MidiModePanel::kNumRows + 1);
}

// ============================================================================
//  computeMaxRows
// ============================================================================
TEST_CASE ("Editor computeMaxRows >= computeNumRows", "[editor][rows]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    REQUIRE (ed.computeMaxRows() >= ed.computeNumRows());
}

// ============================================================================
//  timerCallback
// ============================================================================
TEST_CASE ("Editor timerCallback does not crash", "[editor][timer]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);

    proc.midiNoteLedActive.store (false);
    proc.gateLedActive    .store (false);
    proc.bypassLedActive  .store (false);
    REQUIRE_NOTHROW (ed.timerCallback());

    proc.midiNoteLedActive.store (true);
    proc.gateLedActive    .store (true);
    proc.bypassLedActive  .store (true);
    REQUIRE_NOTHROW (ed.timerCallback());
}

// ============================================================================
//  parameterChanged
// ============================================================================
TEST_CASE ("Editor parameterChanged(rhythmSource) – switch to MIDI", "[editor][paramChanged]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    REQUIRE_NOTHROW (ed.parameterChanged (ParamID::rhythmSource, 1.0f));
}

TEST_CASE ("Editor parameterChanged(rhythmSource) – switch to fixed", "[editor][paramChanged]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    REQUIRE_NOTHROW (ed.parameterChanged (ParamID::rhythmSource, 0.0f));
}

TEST_CASE ("Editor parameterChanged(rhythmPattern) does not crash", "[editor][paramChanged]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    REQUIRE_NOTHROW (ed.parameterChanged (ParamID::rhythmPattern, 3.0f));
}

TEST_CASE ("Editor parameterChanged(filterEnabled, on) does not crash", "[editor][paramChanged]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    REQUIRE_NOTHROW (ed.parameterChanged (ParamID::filterEnabled, 1.0f));
}

TEST_CASE ("Editor parameterChanged(filterEnabled, off) does not crash", "[editor][paramChanged]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    REQUIRE_NOTHROW (ed.parameterChanged (ParamID::filterEnabled, 0.0f));
}

// ============================================================================
//  paint – off-screen rendering
// ============================================================================
static void paintEditor (SoundChopperAudioProcessorEditor& ed)
{
    juce::Image img (juce::Image::ARGB, ed.getWidth(), ed.getHeight(), true);
    juce::Graphics g (img);
    ed.paint (g);
}

TEST_CASE ("Editor paint does not crash", "[editor][paint]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    REQUIRE_NOTHROW (paintEditor (ed));
}

// ============================================================================
//  resized
// ============================================================================
TEST_CASE ("Editor resized in fixed-pattern mode does not crash", "[editor][resized]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);
    ed.setSize (kWindowWidth, windowHeight (ed.computeNumRows()));
    REQUIRE_NOTHROW (ed.resized());
}

TEST_CASE ("Editor resized in MIDI mode does not crash", "[editor][resized]")
{
    SoundChopperAudioProcessor proc;
    setChoiceParam (proc.apvts, ParamID::rhythmSource, 1);  // MIDI source

    SoundChopperAudioProcessorEditor ed (proc);
    ed.setSize (kWindowWidth, windowHeight (ed.computeNumRows()));
    REQUIRE_NOTHROW (ed.resized());
}

// ============================================================================
//  updateRhythmSourceVisibility (via parameterChanged)
// ============================================================================
TEST_CASE ("Editor switching rhythm source does not crash", "[editor][visibility]")
{
    SoundChopperAudioProcessor proc;
    SoundChopperAudioProcessorEditor ed (proc);

    // Switch MIDI → Fixed → MIDI
    ed.parameterChanged (ParamID::rhythmSource, 1.0f);
    ed.parameterChanged (ParamID::rhythmSource, 0.0f);
    ed.parameterChanged (ParamID::rhythmSource, 1.0f);
}
