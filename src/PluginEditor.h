#pragma once

#include "PluginProcessor.h"
#include <array>

// ============================================================================
//  LedIndicator – a small coloured circle that turns on/off
// ============================================================================
class LedIndicator : public juce::Component
{
public:
    explicit LedIndicator (juce::Colour onColour) : colour (onColour) {}

    void setOn (bool shouldBeOn)
    {
        if (on != shouldBeOn)
        {
            on = shouldBeOn;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (on ? colour : colour.withAlpha (0.15f));
        g.fillEllipse (bounds);
        g.setColour (juce::Colours::grey);
        g.drawEllipse (bounds, 1.0f);
    }

private:
    juce::Colour colour;
    bool on = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LedIndicator)
};

// ============================================================================
//  SoundChopperAudioProcessorEditor
//
//  Custom editor that shows / hides controls depending on the selected
//  Rhythm Source:
//
//    Fixed Pattern  →  Rhythm Pattern combo box is visible
//    MIDI Input     →  Rhythm Pattern is hidden; an info label is shown
//                      instructing the user to route a MIDI track to this
//                      plugin's MIDI input in their DAW.
// ============================================================================
class SoundChopperAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      public juce::AudioProcessorValueTreeState::Listener,
      public juce::Timer
{
public:
    explicit SoundChopperAudioProcessorEditor (SoundChopperAudioProcessor& p);
    ~SoundChopperAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

    // Called on the message thread whenever rhythmSource changes
    void parameterChanged (const juce::String& paramID, float newValue) override;

private:
    void updateRhythmSourceVisibility (bool usingMidi);
    void updateAdvancedVisibility (bool advanced);
    void updateWindowSize();
    int  computeNumRows() const noexcept;
    int  computeMaxRows() const noexcept;
    void syncRhythmSourceButtons();
    void syncRhythmPatternButtons();
    void setMidiLedVisibility (bool shouldBeVisible);

    SoundChopperAudioProcessor& processorRef;
    bool midiModeActive = false;
    bool advancedMode   = false;

    // ---- Rhythm Source ------------------------------------------------
    juce::ComboBox rhythmSourceCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        rhythmSourceAttachment;
    std::array<juce::TextButton, 2> rhythmSourceButtons;

    // ---- Rhythm Pattern (Fixed Pattern mode only) ---------------------
    juce::ComboBox rhythmPatternCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        rhythmPatternAttachment;
    juce::OwnedArray<juce::Button> rhythmPatternButtons;

    // ---- MIDI info label (MIDI Input mode only) -----------------------
    juce::Label midiInfoLabel;

    // ---- MIDI Source (MIDI Input mode only – Logic IAC Driver workaround) --
    juce::Label    midiSourceLabel;
    juce::ComboBox midiSourceCombo;
    juce::Array<juce::MidiDeviceInfo> midiDevices;  // list populated in constructor

    // ---- Gate Note (MIDI Input mode only) ----------------------------
    juce::Label    gateNoteLabel;
    juce::ComboBox gateNoteCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        gateNoteAttachment;

    // ---- Bypass Note (MIDI Input mode only) --------------------------
    juce::Label    bypassNoteLabel;
    juce::ComboBox bypassNoteCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        bypassNoteAttachment;

    // ---- Gate Length --------------------------------------------------
    juce::Label  gateLengthLabel;
    juce::Slider gateLengthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        gateLengthAttachment;

    // ---- Fade Time ----------------------------------------------------
    juce::Label  fadeTimeLabel;
    juce::Slider fadeTimeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        fadeTimeAttachment;

    // ---- Dry / Wet ----------------------------------------------------
    juce::Label  dryWetLabel;
    juce::Slider dryWetSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        dryWetAttachment;

    // ---- Filter On ----------------------------------------------------
    juce::Label        filterEnabledLabel;
    juce::ToggleButton filterEnabledButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        filterEnabledAttachment;

    // ---- Filter Type --------------------------------------------------
    juce::Label    filterTypeLabel;
    juce::ComboBox filterTypeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        filterTypeAttachment;

    // ---- Filter Freq --------------------------------------------------
    juce::Label  filterFreqLabel;
    juce::Slider filterFreqSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        filterFreqAttachment;

    // ---- Filter Q -----------------------------------------------------
    juce::Label  filterQLabel;
    juce::Slider filterQSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        filterQAttachment;

    // ---- LED indicators -----------------------------------------------
    juce::Label  midiLedLabel;
    LedIndicator midiLed    { juce::Colours::white };

    juce::Label  gateLedLabel;
    LedIndicator gateLed    { juce::Colours::limegreen };

    juce::Label  bypassLedLabel;
    LedIndicator bypassLed  { juce::Colours::red };

    // ---- Advanced toggle button ---------------------------------------
    juce::ToggleButton advancedButton;

    bool filterOnActive = false;
    void updateFilterVisibility (bool filterOn);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundChopperAudioProcessorEditor)
};
