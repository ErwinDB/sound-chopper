#pragma once

#include "PluginProcessor.h"
#include "MidiModePanel.h"
#include "FilterPanel.h"
#include "AdvancedPanel.h"
#include <array>

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

    int  computeNumRows() const noexcept;
    int  computeMaxRows() const noexcept;

private:
    void updateRhythmSourceVisibility (bool usingMidi);
    void updateAdvancedVisibility (bool advanced);
    void updateFilterVisibility (bool filterOn);
    void updateWindowSize();
    void syncRhythmSourceButtons();
    void syncRhythmPatternButtons();

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

    // ---- Dry / Wet ----------------------------------------------------
    juce::Label  dryWetLabel;
    juce::Slider dryWetSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        dryWetAttachment;

    // ---- Sub-panels ---------------------------------------------------
    MidiModePanel midiPanel;
    FilterPanel   filterPanel;
    AdvancedPanel advancedPanel;

    // ---- Advanced toggle button ---------------------------------------
    juce::ToggleButton advancedButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundChopperAudioProcessorEditor)
};
