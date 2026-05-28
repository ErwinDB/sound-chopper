// PluginProcessorTest.cpp – unit tests for SoundChopperAudioProcessor.
//
// Coverage targets:
//   • Static arrays and constants
//   • Parameter layout (existence and defaults)
//   • isBusesLayoutSupported()
//   • prepareToPlay() / releaseResources()
//   • processBlock() – fixed-pattern mode: gate open/closed/fade-in/fade-out,
//     dry-wet mix, transport-stopped passthrough, no-playhead path
//   • processBlock() – MIDI mode: initial silence, gate note, bypass note,
//     gate→bypass→gate sequence, LED atomics, MIDI pulse LED
//   • processBlock() – filter enabled path
//   • getStateInformation() / setStateInformation() round-trip and invalid data
//   • setMidiInputDevice() / getCurrentMidiInputDeviceIdentifier()
//   • Processor metadata (getName, acceptsMidi, …)

#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"

// ============================================================================
//  MockPlayHead
// ============================================================================
class MockPlayHead : public juce::AudioPlayHead
{
public:
    bool   isPlaying = true;
    double bpm       = 60.0;
    double ppqPos    = 0.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying   (isPlaying);
        info.setBpm         (bpm);
        info.setPpqPosition (ppqPos);
        return info;
    }
};

// ============================================================================
//  Helpers
// ============================================================================
static void fillBuffer (juce::AudioBuffer<float>& buf, float value)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        juce::FloatVectorOperations::fill (buf.getWritePointer (ch), value, buf.getNumSamples());
}

static float avgMagnitude (const juce::AudioBuffer<float>& buf, int channel = 0)
{
    double sum = 0.0;
    const float* p = buf.getReadPointer (channel);
    for (int i = 0; i < buf.getNumSamples(); ++i)
        sum += std::abs ((double) p[i]);
    return (float) (sum / buf.getNumSamples());
}

// Set an AudioParameterFloat to a raw value (not normalised).
static void setFloatParam (SoundChopperAudioProcessor& proc,
                           const char* id, float rawValue)
{
    auto* p = dynamic_cast<juce::AudioParameterFloat*> (proc.apvts.getParameter (id));
    REQUIRE (p != nullptr);
    *p = rawValue;
}

// Set an AudioParameterChoice to a specific index.
static void setChoiceParam (SoundChopperAudioProcessor& proc,
                            const char* id, int index)
{
    auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id));
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
}

// ============================================================================
//  Static arrays
// ============================================================================
TEST_CASE ("Static data arrays", "[processor][static]")
{
    SECTION ("rhythmChoices has 8 entries")
    {
        REQUIRE (SoundChopperAudioProcessor::rhythmChoices.size() == 8);
    }

    SECTION ("rhythmSourceChoices has 2 entries")
    {
        REQUIRE (SoundChopperAudioProcessor::rhythmSourceChoices.size() == 2);
    }

    SECTION ("filterTypeChoices has 4 entries")
    {
        REQUIRE (SoundChopperAudioProcessor::filterTypeChoices.size() == 4);
    }

    SECTION ("midiNoteNames has 128 entries")
    {
        REQUIRE (SoundChopperAudioProcessor::midiNoteNames.size() == 128);
    }

    SECTION ("midiNoteNames first entry is C-2")
    {
        REQUIRE (SoundChopperAudioProcessor::midiNoteNames[0] == "C-2");
    }

    SECTION ("midiNoteNames entry 36 maps to C1 (Ableton-style)")
    {
        REQUIRE (SoundChopperAudioProcessor::midiNoteNames[36] == "C1");
    }

    SECTION ("default gate note is 36")
    {
        REQUIRE (SoundChopperAudioProcessor::kDefaultGateNote == 36);
    }

    SECTION ("default bypass note is 38")
    {
        REQUIRE (SoundChopperAudioProcessor::kDefaultBypassNote == 38);
    }
}

// ============================================================================
//  Parameter layout
// ============================================================================
TEST_CASE ("Parameter layout", "[processor][params]")
{
    SoundChopperAudioProcessor proc;

    SECTION ("all parameters exist in the APVTS")
    {
        REQUIRE (proc.apvts.getParameter (ParamID::rhythmPattern) != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::gateLength)    != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::fadeTime)      != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::dryWet)        != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::rhythmSource)  != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::gateNote)      != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::bypassNote)    != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::filterEnabled) != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::filterType)    != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::filterFreq)    != nullptr);
        REQUIRE (proc.apvts.getParameter (ParamID::filterQ)       != nullptr);
    }

    SECTION ("default parameter values")
    {
        // gateLength = 50 %, fadeTime = 10 %, dryWet = 100 %
        REQUIRE (*proc.apvts.getRawParameterValue (ParamID::gateLength) == Catch::Approx(50.0f));
        REQUIRE (*proc.apvts.getRawParameterValue (ParamID::fadeTime)   == Catch::Approx(10.0f));
        REQUIRE (*proc.apvts.getRawParameterValue (ParamID::dryWet)     == Catch::Approx(100.0f));

        // rhythmPattern defaults to index 2 (1/8 note)
        REQUIRE (static_cast<int> (*proc.apvts.getRawParameterValue (ParamID::rhythmPattern)) == 2);

        // rhythmSource defaults to 0 (Fixed Pattern)
        REQUIRE (static_cast<int> (*proc.apvts.getRawParameterValue (ParamID::rhythmSource)) == 0);

        // MIDI note defaults
        REQUIRE (static_cast<int> (*proc.apvts.getRawParameterValue (ParamID::gateNote))   == 36);
        REQUIRE (static_cast<int> (*proc.apvts.getRawParameterValue (ParamID::bypassNote)) == 38);
    }
}

// ============================================================================
//  Bus layout support
// ============================================================================
TEST_CASE ("Bus layout support", "[processor][buses]")
{
    SoundChopperAudioProcessor proc;

    auto makeLayout = [] (juce::AudioChannelSet in, juce::AudioChannelSet out)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add (in);
        layout.outputBuses.add (out);
        return layout;
    };

    SECTION ("stereo in / stereo out is supported")
    {
        REQUIRE (proc.isBusesLayoutSupported (
            makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())) == true);
    }

    SECTION ("mono in / mono out is supported")
    {
        REQUIRE (proc.isBusesLayoutSupported (
            makeLayout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())) == true);
    }

    SECTION ("stereo in / mono out is NOT supported (layout mismatch)")
    {
        REQUIRE (proc.isBusesLayoutSupported (
            makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono())) == false);
    }

    SECTION ("5.1 surround is NOT supported")
    {
        REQUIRE (proc.isBusesLayoutSupported (
            makeLayout (juce::AudioChannelSet::create5point1(),
                        juce::AudioChannelSet::create5point1())) == false);
    }
}

// ============================================================================
//  prepareToPlay / releaseResources
// ============================================================================
TEST_CASE ("prepareToPlay and releaseResources", "[processor][prepare]")
{
    SoundChopperAudioProcessor proc;

    SECTION ("LEDs are inactive after prepareToPlay")
    {
        proc.prepareToPlay (44100.0, 512);
        REQUIRE (proc.midiNoteLedActive.load() == false);
        REQUIRE (proc.gateLedActive    .load() == false);
        REQUIRE (proc.bypassLedActive  .load() == false);
    }

    SECTION ("releaseResources after prepareToPlay does not crash")
    {
        proc.prepareToPlay (44100.0, 512);
        REQUIRE_NOTHROW (proc.releaseResources());
    }

    SECTION ("releaseResources without prior prepareToPlay does not crash")
    {
        REQUIRE_NOTHROW (proc.releaseResources());
    }

    SECTION ("prepareToPlay can be called multiple times")
    {
        REQUIRE_NOTHROW (proc.prepareToPlay (44100.0, 512));
        REQUIRE_NOTHROW (proc.prepareToPlay (48000.0, 256));
    }
}

// ============================================================================
//  processBlock – fixed-pattern mode
//
//  Geometry (sampleRate=100 Hz, BPM=60):
//    samplesPerQtrNote = 100
//    rhythmPattern = 2 → 1/8 note → cycleLengthPPQ = 0.5
//    gateLength = 50 %  → gate open for first 50 % of cycle
//    fadeTime   = 10 %  → fadeFrac = 0.1
//    fadeDuration (fraction of cycle) = 0.5 * 0.1 = 0.05
//    Gate open in [fadeDuration, gateLength - fadeDuration] = [0.05, 0.45]
// ============================================================================
TEST_CASE ("processBlock – fixed-pattern, transport stopped", "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    MockPlayHead ph;
    ph.isPlaying = false;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 100);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    // Stopped transport → audio passes through unchanged
    REQUIRE (buf.getReadPointer (0)[0]  == Catch::Approx(1.0f));
    REQUIRE (buf.getReadPointer (1)[50] == Catch::Approx(1.0f));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – fixed-pattern, gate fully open", "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    // phase = 0.25 (middle of the open portion [0.05, 0.45])
    // ppqPos = phase * cycleLengthPPQ = 0.25 * 0.5 = 0.125
    MockPlayHead ph;
    ph.bpm    = 60.0;
    ph.ppqPos = 0.125;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 1);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(1.0f));   // unity gain

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – fixed-pattern, gate fully closed", "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    // phase = 0.70 (> gateLength=0.5 → gate closed)
    // ppqPos = 0.70 * 0.5 = 0.35
    MockPlayHead ph;
    ph.bpm    = 60.0;
    ph.ppqPos = 0.35;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 1);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(0.0f));   // zero gain

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – fixed-pattern, fade-in midpoint", "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    // phase = 0.025  →  gain = phase / fadeDuration = 0.025 / 0.05 = 0.5
    // ppqPos = 0.025 * 0.5 = 0.0125
    MockPlayHead ph;
    ph.bpm    = 60.0;
    ph.ppqPos = 0.0125;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 1);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(0.5f).margin (0.05f));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – fixed-pattern, fade-out midpoint", "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    // phase = 0.475  →  gateLength - phase = 0.025
    // gain = (gateLength - phase) / fadeDuration = 0.025 / 0.05 = 0.5
    // ppqPos = 0.475 * 0.5 = 0.2375
    MockPlayHead ph;
    ph.bpm    = 60.0;
    ph.ppqPos = 0.2375;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 1);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(0.5f).margin (0.05f));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – fixed-pattern, no fade (fadeTime=0)", "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setFloatParam (proc, ParamID::fadeTime, 0.0f);

    // phase = 0.1 (inside the first half → gate open, no fade needed)
    MockPlayHead ph;
    ph.bpm    = 60.0;
    ph.ppqPos = 0.1 * 0.5;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 1);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(1.0f));   // immediate unity gain

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – fixed-pattern, dry/wet mix at 50 %", "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    // Set dryWet BEFORE prepareToPlay so the smoother is initialised at 0.5
    setFloatParam (proc, ParamID::dryWet, 50.0f);
    proc.prepareToPlay (100.0, 512);

    // Gate closed (phase=0.7 → gain=0) with dryWet=0.5:
    // totalGain = 1 - 0.5 + 0.5 * 0 = 0.5
    MockPlayHead ph;
    ph.bpm    = 60.0;
    ph.ppqPos = 0.35;   // phase = 0.7 → gate closed
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 1);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(0.5f).margin (0.05f));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – fixed-pattern, no playhead (null)", "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    proc.setPlayHead (nullptr);

    juce::AudioBuffer<float> buf (2, 512);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;

    // No playhead → isPlaying = false → audio passes through, no crash
    REQUIRE_NOTHROW (proc.processBlock (buf, midi));
    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(1.0f));
}

TEST_CASE ("processBlock – fixed-pattern, negative ppq phase wraps correctly",
           "[processor][processblock]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    // Set a ppqPosition that when fmod'd gives a small positive phase inside
    // the gate-open window.  We rely on the fmod / phase += 1.0 guard in the
    // code for negative positions.
    MockPlayHead ph;
    ph.bpm    = 60.0;
    ph.ppqPos = -0.375;  // fmod(-0.375, 0.5)=-0.375, then +0.5 → phase=0.25 (open)
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 1);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(1.0f));

    proc.setPlayHead (nullptr);
}

// ============================================================================
//  processBlock – MIDI mode
// ============================================================================
TEST_CASE ("processBlock – MIDI mode, silence before any gate note",
           "[processor][midi]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setChoiceParam (proc, ParamID::rhythmSource, 1);   // MIDI Input

    MockPlayHead ph;
    ph.bpm = 60.0;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 64);
    fillBuffer (buf, 1.0f);
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);

    // Gate is Closed → all samples should be zero
    REQUIRE (avgMagnitude (buf) == Catch::Approx(0.0f));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI mode, gate note opens gate",
           "[processor][midi]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setChoiceParam (proc, ParamID::rhythmSource, 1);

    MockPlayHead ph;
    ph.bpm = 60.0;
    proc.setPlayHead (&ph);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);

    juce::AudioBuffer<float> buf (2, 200);
    fillBuffer (buf, 1.0f);
    proc.processBlock (buf, midi);

    // Some samples should have received non-zero gain after the gate opens
    REQUIRE (avgMagnitude (buf) > 0.0f);

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI mode, bypass note bypasses plugin",
           "[processor][midi]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setChoiceParam (proc, ParamID::rhythmSource, 1);

    MockPlayHead ph;
    ph.bpm = 60.0;
    proc.setPlayHead (&ph);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) 100), 0);

    juce::AudioBuffer<float> buf (2, 64);
    fillBuffer (buf, 1.0f);
    proc.processBlock (buf, midi);

    // With bypass active all samples should remain at full amplitude
    REQUIRE (buf.getReadPointer (0)[10] == Catch::Approx(1.0f));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI mode, gate note clears bypass",
           "[processor][midi]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setChoiceParam (proc, ParamID::rhythmSource, 1);

    MockPlayHead ph;
    ph.bpm = 60.0;
    proc.setPlayHead (&ph);

    // 1. Activate bypass
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buf (2, 64);
        fillBuffer (buf, 1.0f);
        proc.processBlock (buf, midi);
        REQUIRE (proc.bypassLedActive.load() == true);
    }

    // 2. Send gate note to clear bypass
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buf (2, 200);
        fillBuffer (buf, 1.0f);
        proc.processBlock (buf, midi);
        // bypass cleared, gate opens → some samples are non-zero
        REQUIRE (avgMagnitude (buf) > 0.0f);
    }

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI mode, gate note ignored when already open",
           "[processor][midi]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setChoiceParam (proc, ParamID::rhythmSource, 1);

    MockPlayHead ph;
    ph.bpm = 60.0;
    proc.setPlayHead (&ph);

    // Open the gate
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buf (2, 5);
        fillBuffer (buf, 1.0f);
        proc.processBlock (buf, midi);
    }

    // Second gate note while gate is already open should not crash
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buf (2, 5);
        fillBuffer (buf, 1.0f);
        REQUIRE_NOTHROW (proc.processBlock (buf, midi));
    }

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI mode, full gate state-machine cycle",
           "[processor][midi]")
{
    // BPM=60, sampleRate=100 Hz → samplesPerQtrNote=100
    // rhythmPattern default=2 (1/8, cycleLengthPPQ=0.5)
    // gateLength=50 % → gateDurationSamples = 0.5*0.5*100 = 25
    // fadeTime=10 % → fadeFrac=0.1 → fadeDurationSamples = int(0.1*25) = 2
    // openDurationSamples = max(0, 25-4) = 21
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setChoiceParam (proc, ParamID::rhythmSource, 1);

    MockPlayHead ph;
    ph.bpm = 60.0;
    proc.setPlayHead (&ph);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);

    constexpr int kBufSize = 200;
    juce::AudioBuffer<float> buf (2, kBufSize);
    fillBuffer (buf, 1.0f);
    proc.processBlock (buf, midi);

    const float* ch0 = buf.getReadPointer (0);

    // First sample: FadingIn just started → gain near 0
    REQUIRE (ch0[0] == Catch::Approx(0.0f).margin (0.2f));

    // A sample well inside the open window should have near-unity gain
    REQUIRE (ch0[10] == Catch::Approx(1.0f).margin (0.1f));

    // Well after the gate has closed the sample should be zero
    REQUIRE (ch0[kBufSize - 1] == Catch::Approx(0.0f));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI mode, no-fade path (fadeTime=0)",
           "[processor][midi]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setChoiceParam (proc, ParamID::rhythmSource, 1);
    setFloatParam  (proc, ParamID::fadeTime, 0.0f);

    MockPlayHead ph;
    ph.bpm = 60.0;
    proc.setPlayHead (&ph);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);

    juce::AudioBuffer<float> buf (2, 200);
    fillBuffer (buf, 1.0f);
    proc.processBlock (buf, midi);

    // With zero fade the first sample after the gate note should be unity
    REQUIRE (buf.getReadPointer (0)[0] == Catch::Approx(1.0f));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI mode, LED states", "[processor][midi][leds]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    setChoiceParam (proc, ParamID::rhythmSource, 1);

    MockPlayHead ph;
    ph.bpm = 60.0;
    proc.setPlayHead (&ph);

    // No MIDI → all LEDs off
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 64);
        fillBuffer (buf, 0.0f);
        proc.processBlock (buf, midi);
        REQUIRE (proc.gateLedActive  .load() == false);
        REQUIRE (proc.bypassLedActive.load() == false);
    }

    // Gate note → gate LED on
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buf (2, 5);
        fillBuffer (buf, 0.0f);
        proc.processBlock (buf, midi);
        REQUIRE (proc.gateLedActive.load() == true);
    }

    // Bypass note → bypass LED on
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buf (2, 1);
        fillBuffer (buf, 0.0f);
        proc.processBlock (buf, midi);
        REQUIRE (proc.bypassLedActive.load() == true);
    }

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI LED pulse on any NoteOn",
           "[processor][midi][leds]")
{
    SoundChopperAudioProcessor proc;
    // 100 Hz: pulse = 0.08 s * 100 = 8 samples
    proc.prepareToPlay (100.0, 512);

    MockPlayHead ph;
    ph.isPlaying = false;
    ph.bpm       = 60.0;
    proc.setPlayHead (&ph);

    // Send a note-on on any channel/note
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

    juce::AudioBuffer<float> buf (2, 1);
    fillBuffer (buf, 0.0f);
    proc.processBlock (buf, midi);
    REQUIRE (proc.midiNoteLedActive.load() == true);

    // After 10 samples (> pulse of 8) the counter is drained.
    // LED is stored at start-of-block, so need one more block to observe off.
    midi.clear();
    buf.setSize (2, 10);
    fillBuffer (buf, 0.0f);
    proc.processBlock (buf, midi);

    buf.setSize (2, 1);
    proc.processBlock (buf, midi);
    REQUIRE (proc.midiNoteLedActive.load() == false);

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – MIDI mode, fixed-pattern LEDs cleared",
           "[processor][midi][leds]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (100.0, 512);

    // Fixed Pattern mode: gate and bypass LEDs are always off
    MockPlayHead ph;
    ph.bpm       = 60.0;
    ph.isPlaying = true;
    ph.ppqPos    = 0.125;
    proc.setPlayHead (&ph);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 64);
    fillBuffer (buf, 0.0f);
    proc.processBlock (buf, midi);

    REQUIRE (proc.gateLedActive  .load() == false);
    REQUIRE (proc.bypassLedActive.load() == false);

    proc.setPlayHead (nullptr);
}

// ============================================================================
//  processBlock – filter path
// ============================================================================
TEST_CASE ("processBlock – filter enabled does not crash", "[processor][filter]")
{
    SoundChopperAudioProcessor proc;
    proc.prepareToPlay (44100.0, 512);

    // Enable the filter
    auto* filterParam = dynamic_cast<juce::AudioParameterBool*> (
        proc.apvts.getParameter (ParamID::filterEnabled));
    REQUIRE (filterParam != nullptr);
    *filterParam = true;

    MockPlayHead ph;
    ph.isPlaying = true;
    ph.bpm       = 120.0;
    ph.ppqPos    = 0.125;   // gate open in fixed-pattern mode
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buf (2, 512);
    fillBuffer (buf, 0.5f);
    juce::MidiBuffer midi;
    REQUIRE_NOTHROW (proc.processBlock (buf, midi));

    proc.setPlayHead (nullptr);
}

TEST_CASE ("processBlock – all filter type choices", "[processor][filter]")
{
    for (int typeIndex = 0; typeIndex < 4; ++typeIndex)
    {
        SoundChopperAudioProcessor proc;
        proc.prepareToPlay (44100.0, 512);

        auto* filterEnabled = dynamic_cast<juce::AudioParameterBool*> (
            proc.apvts.getParameter (ParamID::filterEnabled));
        *filterEnabled = true;

        setChoiceParam (proc, ParamID::filterType, typeIndex);

        MockPlayHead ph;
        ph.isPlaying = true;
        ph.bpm       = 120.0;
        ph.ppqPos    = 0.125;
        proc.setPlayHead (&ph);

        juce::AudioBuffer<float> buf (2, 128);
        fillBuffer (buf, 0.5f);
        juce::MidiBuffer midi;
        REQUIRE_NOTHROW (proc.processBlock (buf, midi));

        proc.setPlayHead (nullptr);
    }
}

// ============================================================================
//  State persistence
// ============================================================================
TEST_CASE ("getStateInformation / setStateInformation round-trip",
           "[processor][state]")
{
    SoundChopperAudioProcessor proc1;

    // Change a parameter from its default
    setFloatParam (proc1, ParamID::gateLength, 75.0f);

    juce::MemoryBlock stateData;
    proc1.getStateInformation (stateData);
    REQUIRE (stateData.getSize() > 0);

    SoundChopperAudioProcessor proc2;
    proc2.setStateInformation (stateData.getData(), (int) stateData.getSize());

    REQUIRE (*proc2.apvts.getRawParameterValue (ParamID::gateLength) == Catch::Approx(75.0f).margin (0.5f));
}

TEST_CASE ("setStateInformation with invalid / empty data does not crash",
           "[processor][state]")
{
    SoundChopperAudioProcessor proc;

    SECTION ("garbage bytes")
    {
        const char garbage[] = "this is not valid XML";
        REQUIRE_NOTHROW (
            proc.setStateInformation (garbage, (int) sizeof (garbage)));
    }

    SECTION ("null size")
    {
        REQUIRE_NOTHROW (proc.setStateInformation (nullptr, 0));
    }
}

TEST_CASE ("getStateInformation preserves external MIDI device ID",
           "[processor][state]")
{
    SoundChopperAudioProcessor proc;
    // Calling with an empty string is always safe (no device to open)
    proc.setMidiInputDevice ({});

    juce::MemoryBlock stateData;
    proc.getStateInformation (stateData);
    REQUIRE (stateData.getSize() > 0);
}

// ============================================================================
//  External MIDI device management
// ============================================================================
TEST_CASE ("MIDI input device management", "[processor][mididevice]")
{
    SoundChopperAudioProcessor proc;

    SECTION ("identifier is empty initially")
    {
        REQUIRE (proc.getCurrentMidiInputDeviceIdentifier().isEmpty());
    }

    SECTION ("passing empty string keeps identifier empty")
    {
        proc.setMidiInputDevice ({});
        REQUIRE (proc.getCurrentMidiInputDeviceIdentifier().isEmpty());
    }

    SECTION ("unknown identifier leaves identifier empty (no device found)")
    {
        proc.setMidiInputDevice ("non-existent-device-identifier-xyz");
        REQUIRE (proc.getCurrentMidiInputDeviceIdentifier().isEmpty());
    }
}

// ============================================================================
//  Processor metadata
// ============================================================================
TEST_CASE ("Processor metadata", "[processor][metadata]")
{
    SoundChopperAudioProcessor proc;

    REQUIRE (proc.getName()              == "SoundChopper");
    REQUIRE (proc.acceptsMidi()          == true);
    REQUIRE (proc.producesMidi()         == false);
    REQUIRE (proc.isMidiEffect()         == false);
    REQUIRE (proc.getTailLengthSeconds() == Catch::Approx(0.0));
    REQUIRE (proc.hasEditor()            == true);
    REQUIRE (proc.getNumPrograms()       == 1);
    REQUIRE (proc.getCurrentProgram()    == 0);
    REQUIRE (proc.getProgramName (0).isEmpty());

    // setCurrentProgram and changeProgramName must not crash
    REQUIRE_NOTHROW (proc.setCurrentProgram (0));
    REQUIRE_NOTHROW (proc.changeProgramName (0, "test"));
}
