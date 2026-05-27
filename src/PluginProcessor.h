#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ============================================================================
//  Parameter IDs – kept as constants to avoid typos
// ============================================================================
namespace ParamID
{
    inline constexpr const char* rhythmPattern  = "rhythmPattern";
    inline constexpr const char* gateLength     = "gateLength";
    inline constexpr const char* fadeTime       = "fadeTime";
    inline constexpr const char* dryWet         = "dryWet";
    inline constexpr const char* rhythmSource   = "rhythmSource";
    inline constexpr const char* gateNote       = "gateNote";      // MIDI note that opens the gate
    inline constexpr const char* bypassNote     = "bypassNote";    // MIDI note that bypasses the plugin
    inline constexpr const char* filterEnabled  = "filterEnabled"; // future: filtering
    inline constexpr const char* filterType     = "filterType";    // future: filter type
    inline constexpr const char* filterFreq     = "filterFreq";    // future: filter freq
    inline constexpr const char* filterQ        = "filterQ";       // future: filter Q
}

// ============================================================================
//  MIDI gate state machine – used when rhythmSource == "MIDI Input"
// ============================================================================
enum class MidiGateState { Closed, FadingIn, Open, FadingOut };

// ============================================================================
//  SoundChopperAudioProcessor
// ============================================================================
class SoundChopperAudioProcessor : public juce::AudioProcessor,
                                    private juce::MidiInputCallback
{
public:
    SoundChopperAudioProcessor();
    ~SoundChopperAudioProcessor() override;

    // ---- AudioProcessor interface ------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool   acceptsMidi()  const override { return true;  }  // MIDI rhythm input (future)
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int          getNumPrograms()                                       override { return 1; }
    int          getCurrentProgram()                                    override { return 0; }
    void         setCurrentProgram (int)                                override {}
    const juce::String getProgramName (int)                             override { return {}; }
    void         changeProgramName (int, const juce::String&)           override {}

    void getStateInformation (juce::MemoryBlock& destData)              override;
    void setStateInformation (const void* data, int sizeInBytes)        override;

    // ---- Public state ------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    // ---- LED indicator state (written on audio thread, read on message thread) ----
    std::atomic<bool> midiNoteLedActive { false };  // any MIDI note held
    std::atomic<bool> gateLedActive     { false };  // gate note held
    std::atomic<bool> bypassLedActive   { false };  // bypass note held

    // Available rhythm choices (also used by the editor)
    static const juce::StringArray rhythmChoices;

    // Available rhythm source choices
    static const juce::StringArray rhythmSourceChoices;

    // Available filter type choices (future)
    static const juce::StringArray filterTypeChoices;

    // MIDI note names for all 128 notes (C-1 … G9, Ableton-style octave numbering)
    static const juce::StringArray midiNoteNames;

    // Default MIDI note numbers for gate and bypass (Ableton convention: C1 = 36)
    static constexpr int kDefaultGateNote   = 36;  // C1 – Kick drum
    static constexpr int kDefaultBypassNote = 38;  // D1 – Snare drum

    // ---- External MIDI device (Logic IAC Driver workaround) ---------------
    // Call from the message thread only (e.g. from the editor).
    // Pass an empty string to close any currently-open device.
    void         setMidiInputDevice (const juce::String& deviceIdentifier);
    juce::String getCurrentMidiInputDeviceIdentifier() const;

private:
    // ---- Parameter layout helper ------------------------------------------
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- Per-sample gate calculation (fixed-pattern mode) -----------------
    // Returns a gain value in [0, 1] for the given playback phase within a
    // chop cycle.
    //   phase      – normalised position within the cycle [0, 1)
    //   gateLength – fraction of cycle that is "open" [0, 1]
    //   fadeFrac   – fraction of gate duration used for each fade ramp [0, 0.5]
    static float calculateGate (double phase, float gateLength, float fadeFrac) noexcept;

    // ---- Audio-rate parameter smoothers ------------------------------------
    juce::LinearSmoothedValue<float> smoothedDryWet;

    // ---- DSP objects (future filter feature) ------------------------------
    // One filter instance per channel; resized in prepareToPlay.
    juce::OwnedArray<juce::dsp::StateVariableTPTFilter<float>> filters;

    double currentSampleRate = 44100.0;

    // ---- MIDI gate state (used when rhythmSource == "MIDI Input") ---------
    MidiGateState midiGateState        = MidiGateState::Closed;
    int           midiGateStateSamples = 0;   // samples elapsed in current state
    bool          bypassActive         = false; // bypass latched on; cleared by next Gate Note On
    int           midiLedPulseSamplesRemaining = 0; // white LED flash countdown

    // ---- External MIDI device (Logic IAC Driver workaround) ---------------
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    std::unique_ptr<juce::MidiInput> externalMidiInput;
    juce::String                     externalMidiDeviceId;
    juce::MidiMessageCollector       midiCollector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundChopperAudioProcessor)
};
