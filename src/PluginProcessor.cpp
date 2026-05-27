#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
//  Static data
// ============================================================================

// Fixed rhythm choices – labelled with standard note-value notation.
// The matching cycle lengths (in quarter notes) are defined inside processBlock.
// Listed from longest to shortest.
const juce::StringArray SoundChopperAudioProcessor::rhythmChoices
{
    "1/4  - Quarter note",
    "1/4T - Quarter-note triplet",
    "1/8  - Eighth note",
    "1/8T - Eighth-note triplet",
    "1/16 - Sixteenth note",
    "1/16T - Sixteenth-note triplet",
    "1/32 - Thirty-second note",
    "1/32T - Thirty-second-note triplet"
};

// Rhythm source choices
const juce::StringArray SoundChopperAudioProcessor::rhythmSourceChoices
{
    "Fixed Pattern",
    "MIDI Input"   // gate is driven by incoming MIDI note on/off
};

// Filter type choices (future feature)
const juce::StringArray SoundChopperAudioProcessor::filterTypeChoices
{
    "Low Pass",
    "High Pass",
    "Band Pass",
    "Notch"
};

// MIDI note names: C1 = 36 (Ableton-style octaveNumForMiddleC = 3)
const juce::StringArray SoundChopperAudioProcessor::midiNoteNames = []
{
    juce::StringArray names;
    for (int i = 0; i < 128; ++i)
        names.add (juce::MidiMessage::getMidiNoteName (i, true, true, 3));
    return names;
}();

// Cycle lengths in quarter notes for each rhythm choice (same order as
// rhythmChoices above, longest to shortest).
static constexpr double kRhythmDivisions[]
{
    1.0,           // 1/4
    2.0 / 3.0,     // 1/4T  (quarter-note triplet: 3 fit in 2 beats)
    0.5,           // 1/8
    1.0 / 3.0,     // 1/8T  (eighth-note triplet:  3 fit in 1 beat)
    0.25,          // 1/16
    1.0 / 6.0,     // 1/16T (sixteenth-note triplet: 3 fit in 1 eighth)
    0.125,         // 1/32
    1.0 / 12.0     // 1/32T (thirty-second-note triplet: 3 fit in 1 sixteenth)
};

static_assert (std::size (kRhythmDivisions) == 8,
               "kRhythmDivisions must match rhythmChoices");

// ============================================================================
//  Parameter layout
// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
SoundChopperAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ------------------------------------------------------------------
    //  Rhythm source  (current: fixed pattern; future: MIDI)
    // ------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::rhythmSource, 1 },
        "Rhythm Source",
        rhythmSourceChoices,
        0));   // default: "Fixed Pattern"

    // ------------------------------------------------------------------
    //  Fixed-pattern rhythm selection
    // ------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::rhythmPattern, 1 },
        "Rhythm Pattern",
        rhythmChoices,
        2));   // default: 1/8

    // ------------------------------------------------------------------
    //  MIDI gate note – the note that opens the gate (MIDI Input mode)
    // ------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::gateNote, 1 },
        "Gate Note",
        midiNoteNames,
        kDefaultGateNote));   // default: C1 (36) – Kick drum

    // ------------------------------------------------------------------
    //  MIDI bypass note – the note that bypasses the plugin (MIDI Input mode)
    // ------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::bypassNote, 1 },
        "Bypass Note",
        midiNoteNames,
        kDefaultBypassNote)); // default: D1 (38) – Snare drum

    // ------------------------------------------------------------------
    //  Gate length – percentage of the cycle that is "open"
    // ------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::gateLength, 1 },
        "Gate Length",
        juce::NormalisableRange<float> (1.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes{}.withLabel ("%")));

    // ------------------------------------------------------------------
    //  Fade time – % of gate duration used for each fade ramp (in + out)
    // ------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::fadeTime, 1 },
        "Fade Time",
        juce::NormalisableRange<float> (0.0f, 50.0f, 0.1f),
        10.0f,
        juce::AudioParameterFloatAttributes{}.withLabel ("%")));

    // ------------------------------------------------------------------
    //  Dry / Wet mix
    // ------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::dryWet, 1 },
        "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes{}.withLabel ("%")));

    // ------------------------------------------------------------------
    //  Future: filter
    // ------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::filterEnabled, 1 },
        "Filter On",
        false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::filterType, 1 },
        "Filter Type",
        filterTypeChoices,
        0));   // default: Low Pass

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::filterFreq, 1 },
        "Filter Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f),
        1000.0f,
        juce::AudioParameterFloatAttributes{}.withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::filterQ, 1 },
        "Filter Q",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f),
        0.707f));

    return { params.begin(), params.end() };
}

// ============================================================================
//  Constructor / Destructor
// ============================================================================
SoundChopperAudioProcessor::SoundChopperAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SoundChopper", createParameterLayout())
{
}

SoundChopperAudioProcessor::~SoundChopperAudioProcessor()
{
    // Close any open external MIDI device before destruction.
    externalMidiInput.reset();
}

// ============================================================================
//  Prepare / Release
// ============================================================================
void SoundChopperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    smoothedDryWet.reset (sampleRate, 0.02);  // 20 ms smoothing
    smoothedDryWet.setCurrentAndTargetValue (
        apvts.getRawParameterValue (ParamID::dryWet)->load() / 100.0f);

    // Prepare the MIDI message collector (must be reset whenever sample rate changes)
    midiCollector.reset (sampleRate);

    // Reset MIDI gate state
    midiGateState        = MidiGateState::Closed;
    midiGateStateSamples = 0;
    bypassActive         = false;
    midiLedPulseSamplesRemaining = 0;
    midiNoteLedActive.store (false, std::memory_order_relaxed);
    gateLedActive    .store (false, std::memory_order_relaxed);
    bypassLedActive  .store (false, std::memory_order_relaxed);

    // Prepare DSP filters – one per channel
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 1;  // each filter processes one channel

    filters.clear();
    for (int ch = 0; ch < getTotalNumInputChannels(); ++ch)
    {
        auto* f = filters.add (new juce::dsp::StateVariableTPTFilter<float>());
        f->prepare (spec);
        f->setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        f->setCutoffFrequency (
            apvts.getRawParameterValue (ParamID::filterFreq)->load());
        f->setResonance (
            apvts.getRawParameterValue (ParamID::filterQ)->load());
    }
}

void SoundChopperAudioProcessor::releaseResources()
{
    filters.clear();
}

// ============================================================================
//  Bus layout
// ============================================================================
bool SoundChopperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Only mono or stereo are supported
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input and output layouts must match (this is an audio effect, not an instrument)
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

// ============================================================================
//  Gate calculation (static helper)
// ============================================================================
float SoundChopperAudioProcessor::calculateGate (double   phase,
                                                  float    gateLength,
                                                  float    fadeFrac) noexcept
{
    // phase is in [0, 1).  gateLength is in (0, 1].  fadeFrac is in [0, 0.5].
    if (phase >= static_cast<double> (gateLength))
        return 0.0f;   // gate closed

    // Duration of one fade ramp, expressed as a fraction of the cycle
    const double fadeDuration = static_cast<double> (gateLength * fadeFrac);

    if (fadeDuration <= 0.0)
        return 1.0f;   // no fading, gate is fully open

    if (phase < fadeDuration)
        return static_cast<float> (phase / fadeDuration);         // fade in

    if (phase >= static_cast<double> (gateLength) - fadeDuration)
        return static_cast<float>
            ((static_cast<double> (gateLength) - phase) / fadeDuration);  // fade out

    return 1.0f;  // fully open
}

// ============================================================================
//  processBlock
// ============================================================================
void SoundChopperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalInputChannels  = getTotalNumInputChannels();
    const int totalOutputChannels = getTotalNumOutputChannels();
    const int numSamples          = buffer.getNumSamples();

    // Clear any extra output channels that have no corresponding input
    for (int ch = totalInputChannels; ch < totalOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // ---- Read parameters -----------------------------------------------
    const int   rhythmSourceIndex = static_cast<int> (
        apvts.getRawParameterValue (ParamID::rhythmSource)->load());
    const bool  usingMidiInput   = (rhythmSourceIndex == 1);
    const int   rhythmIndex      = static_cast<int> (
        apvts.getRawParameterValue (ParamID::rhythmPattern)->load());
    const float gateLength       =
        apvts.getRawParameterValue (ParamID::gateLength)->load() / 100.0f;
    const float fadeFrac         =
        apvts.getRawParameterValue (ParamID::fadeTime)->load() / 100.0f;
    const float targetDryWet     =
        apvts.getRawParameterValue (ParamID::dryWet)->load() / 100.0f;
    const bool  filterOn         =
        apvts.getRawParameterValue (ParamID::filterEnabled)->load() > 0.5f;
    const int   gateNoteVal      = static_cast<int> (
        apvts.getRawParameterValue (ParamID::gateNote)->load());
    const int   bypassNoteVal    = static_cast<int> (
        apvts.getRawParameterValue (ParamID::bypassNote)->load());

    smoothedDryWet.setTargetValue (targetDryWet);

    // ---- Merge external MIDI device messages (Logic IAC Driver workaround) --
    // When an external MIDI source is open, drain the collector and merge its
    // timestamped messages into the host-provided MidiBuffer.
    if (usingMidiInput && externalMidiInput != nullptr)
    {
        juce::MidiBuffer externalMidi;
        midiCollector.removeNextBlockOfMessages (externalMidi, numSamples);
        for (const auto metadata : externalMidi)
            midiMessages.addEvent (metadata.getMessage(), metadata.samplePosition);
    }

    // ---- Get playhead position -----------------------------------------
    bool   isPlaying    = false;
    double ppqPosition  = 0.0;
    double bpm          = 120.0;

    if (auto* ph = getPlayHead())
    {
        if (auto posInfo = ph->getPosition())
        {
            isPlaying   = posInfo->getIsPlaying();
            bpm         = posInfo->getBpm()         .orFallback (120.0);
            ppqPosition = posInfo->getPpqPosition().orFallback (0.0);
        }
    }

    // ---- Update white LED: short pulse for each received Note On ----------
    constexpr double kMidiLedPulseSeconds = 0.08;
    int noteOnCountThisBlock = 0;
    for (const auto& metaMsg : midiMessages)
    {
        const auto msg = metaMsg.getMessage();
        if (msg.isNoteOn())
            ++noteOnCountThisBlock;
    }

    if (noteOnCountThisBlock > 0)
    {
        const int pulseSamples = std::max (1, static_cast<int> (kMidiLedPulseSeconds * currentSampleRate));
        midiLedPulseSamplesRemaining = std::max (midiLedPulseSamplesRemaining, pulseSamples);
    }

    const bool midiPulseActive = (midiLedPulseSamplesRemaining > 0);
    if (midiLedPulseSamplesRemaining > 0)
        midiLedPulseSamplesRemaining = std::max (0, midiLedPulseSamplesRemaining - numSamples);
    midiNoteLedActive.store (midiPulseActive, std::memory_order_relaxed);

    // ---- Chopping ---------------------------------------------------------
    if (usingMidiInput)
    {
        // ------------------------------------------------------------------
        //  MIDI-driven gate mode
        //
        //  Notes determine when the gate is open / closed.
        //  Note kBypassNote (127) is special: while it is held the plugin is
        //  fully bypassed (audio passes through at unity gain).
        //
        //  On each Note On the gate opens and runs a timed fade-in → open →
        //  fade-out envelope (gateLength + fadeTime), matching the fixed-pattern
        //  behaviour.  Note Off events are ignored.
        // ------------------------------------------------------------------

        const double cycleLengthPPQ    = kRhythmDivisions[rhythmIndex];
        const double samplesPerQtrNote = currentSampleRate * 60.0 / bpm;
        const int gateDurationSamples  = static_cast<int> (gateLength * cycleLengthPPQ * samplesPerQtrNote);
        const int fadeDurationSamples  = static_cast<int> (fadeFrac * static_cast<float> (gateDurationSamples));
        const int openDurationSamples  = std::max (0, gateDurationSamples - 2 * fadeDurationSamples);

        // Helper: transition gate state and reset the per-state sample counter.
        // If there is no fade (fadeDurationSamples == 0), skip straight to the
        // fully Open / Closed state.
        auto transitionMidiState = [&] (MidiGateState newState)
        {
            if (fadeDurationSamples == 0)
            {
                midiGateState        =
                    (newState == MidiGateState::FadingIn)  ? MidiGateState::Open  :
                    (newState == MidiGateState::FadingOut) ? MidiGateState::Closed : newState;
            }
            else
            {
                midiGateState = newState;
            }
            midiGateStateSamples = 0;
        };

        auto midiIter = midiMessages.cbegin();

        for (int s = 0; s < numSamples; ++s)
        {
            // ---- Process MIDI events that fire at or before this sample ----
            while (midiIter != midiMessages.cend()
                   && (*midiIter).samplePosition <= s)
            {
                const auto msg = (*midiIter).getMessage();

                if (msg.isNoteOn())
                {
                    if (msg.getNoteNumber() == bypassNoteVal)
                    {
                        // Latch bypass on; audio passes at full gain until the
                        // next Gate Note On clears it.
                        bypassActive = true;
                    }
                    else if (msg.getNoteNumber() == gateNoteVal)
                    {
                        // Clear any active bypass latch first.
                        bypassActive = false;
                        // Only (re-)trigger when the gate is fully closed; the timed
                        // envelope will open and then close it automatically.
                        if (midiGateState == MidiGateState::Closed)
                            transitionMidiState (MidiGateState::FadingIn);
                    }
                }
                // Note-off events are intentionally ignored; the bypass latch is
                // cleared by the next Gate Note On, and the gate closes via its
                // timed envelope (gateLength + fadeTime).

                ++midiIter;
            }

            // ---- Compute per-sample wet gain from gate state ---------------
            float wetGain;

            if (bypassActive)
            {
                wetGain = 1.0f;
                // Advance smoother without applying attenuation
                smoothedDryWet.getNextValue();
            }
            else
            {
                switch (midiGateState)
                {
                    case MidiGateState::Closed:
                        wetGain = 0.0f;
                        break;

                    case MidiGateState::FadingIn:
                        wetGain = (fadeDurationSamples > 0)
                            ? std::min (1.0f, static_cast<float> (midiGateStateSamples)
                                              / static_cast<float> (fadeDurationSamples))
                            : 1.0f;
                        ++midiGateStateSamples;
                        if (midiGateStateSamples >= fadeDurationSamples)
                            midiGateState = MidiGateState::Open;
                        break;

                    case MidiGateState::Open:
                        wetGain = 1.0f;
                        ++midiGateStateSamples;
                        if (midiGateStateSamples >= openDurationSamples)
                            transitionMidiState (MidiGateState::FadingOut);
                        break;

                    case MidiGateState::FadingOut:
                        wetGain = (fadeDurationSamples > 0)
                            ? std::max (0.0f, 1.0f - static_cast<float> (midiGateStateSamples)
                                                     / static_cast<float> (fadeDurationSamples))
                            : 0.0f;
                        ++midiGateStateSamples;
                        if (midiGateStateSamples >= fadeDurationSamples)
                            midiGateState = MidiGateState::Closed;
                        break;

                    default:
                        wetGain = 0.0f;
                        break;
                }

                const float dryWetNow = smoothedDryWet.getNextValue();
                const float totalGain = 1.0f - dryWetNow + dryWetNow * wetGain;

                for (int ch = 0; ch < totalInputChannels; ++ch)
                    buffer.getWritePointer (ch)[s] *= totalGain;

                // Skip the unity-gain apply below (already done above)
                continue;
            }

            // Bypass-note path: apply dry/wet at unity wet gain
            {
                const float dryWetNow = smoothedDryWet.getCurrentValue();
                (void) dryWetNow;   // unity wetGain → totalGain always 1.0
                // No scaling needed; audio passes through at full amplitude.
            }
        }

        // Update gate and bypass LEDs; gate LED mirrors the gate state so it
        // turns off when the timed envelope has fully closed.
        gateLedActive  .store (midiGateState != MidiGateState::Closed, std::memory_order_relaxed);
        bypassLedActive.store (bypassActive,                          std::memory_order_relaxed);
    }
    else
    {
        // ------------------------------------------------------------------
        //  Fixed-pattern mode (PPQ-based chopping) – original behaviour
        // ------------------------------------------------------------------

        // Clear gate/bypass LEDs (not applicable in fixed-pattern mode)
        gateLedActive  .store (false, std::memory_order_relaxed);
        bypassLedActive.store (false, std::memory_order_relaxed);

        // Pass audio through unchanged when the DAW transport is stopped
        if (!isPlaying)
            return;

        const double cycleLengthPPQ    = kRhythmDivisions[rhythmIndex];
        const double samplesPerQtrNote = currentSampleRate * 60.0 / bpm;

        for (int s = 0; s < numSamples; ++s)
        {
            const double samplePPQ = ppqPosition + static_cast<double> (s) / samplesPerQtrNote;

            double phase = std::fmod (samplePPQ, cycleLengthPPQ) / cycleLengthPPQ;
            if (phase < 0.0)
                phase += 1.0;

            const float wetGain   = calculateGate (phase, gateLength, fadeFrac);
            const float dryWetNow = smoothedDryWet.getNextValue();
            const float totalGain = 1.0f - dryWetNow + dryWetNow * wetGain;

            for (int ch = 0; ch < totalInputChannels; ++ch)
                buffer.getWritePointer (ch)[s] *= totalGain;
        }
    }

    // ---- Optional filter (future feature) ---------------------------------
    if (filterOn && filters.size() == totalInputChannels)
    {
        const float freq = apvts.getRawParameterValue (ParamID::filterFreq)->load();
        const float q    = apvts.getRawParameterValue (ParamID::filterQ)->load();
        const int   type = static_cast<int> (
            apvts.getRawParameterValue (ParamID::filterType)->load());

        constexpr juce::dsp::StateVariableTPTFilterType filterTypes[]
        {
            juce::dsp::StateVariableTPTFilterType::lowpass,
            juce::dsp::StateVariableTPTFilterType::highpass,
            juce::dsp::StateVariableTPTFilterType::bandpass,
            juce::dsp::StateVariableTPTFilterType::lowpass  // "Notch" – to be added later
        };

        for (int ch = 0; ch < totalInputChannels; ++ch)
        {
            auto* f = filters[ch];
            f->setCutoffFrequency (freq);
            f->setResonance       (q);
            f->setType            (filterTypes[type]);

            auto channelBlock = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (
                static_cast<size_t> (ch));
            juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
            f->process (ctx);
        }
    }
}

// ============================================================================
//  Editor
// ============================================================================
juce::AudioProcessorEditor* SoundChopperAudioProcessor::createEditor()
{
    return new SoundChopperAudioProcessorEditor (*this);
}

// ============================================================================
//  State persistence
// ============================================================================
void SoundChopperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (externalMidiDeviceId.isNotEmpty())
        xml->setAttribute ("externalMidiDeviceId", externalMidiDeviceId);
    copyXmlToBinary (*xml, destData);
}

void SoundChopperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

        const auto deviceId = xmlState->getStringAttribute ("externalMidiDeviceId", {});
        if (deviceId.isNotEmpty())
            setMidiInputDevice (deviceId);
    }
}

// ============================================================================
//  External MIDI device management  (call from message thread)
// ============================================================================
void SoundChopperAudioProcessor::setMidiInputDevice (const juce::String& deviceIdentifier)
{
    // Close any currently-open device first
    externalMidiInput.reset();
    externalMidiDeviceId = {};

    if (deviceIdentifier.isEmpty())
        return;

    // Find the device with this identifier and open it
    for (const auto& dev : juce::MidiInput::getAvailableDevices())
    {
        if (dev.identifier == deviceIdentifier)
        {
            externalMidiInput = juce::MidiInput::openDevice (dev.identifier, this);
            if (externalMidiInput != nullptr)
            {
                externalMidiDeviceId = deviceIdentifier;
                externalMidiInput->start();
            }
            break;
        }
    }
}

juce::String SoundChopperAudioProcessor::getCurrentMidiInputDeviceIdentifier() const
{
    return externalMidiDeviceId;
}

// ============================================================================
//  MidiInputCallback  (called on the MIDI thread)
// ============================================================================
void SoundChopperAudioProcessor::handleIncomingMidiMessage (juce::MidiInput*,
                                                             const juce::MidiMessage& message)
{
    midiCollector.addMessageToQueue (message);
}

// ============================================================================
//  Plugin entry point
// ============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoundChopperAudioProcessor();
}
