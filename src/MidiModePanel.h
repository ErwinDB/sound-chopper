#pragma once

#include "PluginProcessor.h"
#include "LedIndicator.h"
#include "EditorLayout.h"

// ============================================================================
//  MidiModePanel – groups all MIDI-input-mode controls (info label, source
//  combo, gate/bypass note combos) and the three LED indicators into a single
//  child component.  The parent editor calls setVisible() on this panel to
//  show or hide everything at once.
// ============================================================================
class MidiModePanel : public juce::Component
{
public:
    // Number of rows this panel occupies (info, source, gate note, bypass note, LEDs).
    static constexpr int kNumRows = 5;

    MidiModePanel (SoundChopperAudioProcessor& proc,
                   juce::AudioProcessorValueTreeState& apvts)
        : processorRef (proc)
    {
        using namespace EditorLayout;

        // ---- MIDI info label --------------------------------------------
        midiInfoLabel.setText (
            "Route a MIDI track to this plugin's MIDI input, or select an IAC Driver Bus below (Logic workaround).",
            juce::dontSendNotification);
        midiInfoLabel.setJustificationType (juce::Justification::centredLeft);
        midiInfoLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f * (float) kScale)
                                                              .withStyle  ("Italic")));
        addAndMakeVisible (midiInfoLabel);

        // ---- MIDI Source (Logic IAC Driver workaround) ------------------
        midiSourceLabel.setText ("MIDI Source", juce::dontSendNotification);
        midiSourceCombo.addItem ("None (use host MIDI routing)", 1);
        midiDevices = juce::MidiInput::getAvailableDevices();
        for (int i = 0; i < midiDevices.size(); ++i)
            midiSourceCombo.addItem (midiDevices[i].name, i + 2);

        // Restore the previously-selected device (if any)
        {
            const auto currentId = proc.getCurrentMidiInputDeviceIdentifier();
            int selectedComboId  = 1;
            for (int i = 0; i < midiDevices.size(); ++i)
            {
                if (midiDevices[i].identifier == currentId)
                {
                    selectedComboId = i + 2;
                    break;
                }
            }
            midiSourceCombo.setSelectedId (selectedComboId, juce::dontSendNotification);
        }

        midiSourceCombo.onChange = [this]
        {
            const int selectedId = midiSourceCombo.getSelectedId();
            if (selectedId <= 1)
            {
                processorRef.setMidiInputDevice ({});
            }
            else
            {
                const int devIndex = selectedId - 2;
                if (devIndex >= 0 && devIndex < midiDevices.size())
                    processorRef.setMidiInputDevice (midiDevices[devIndex].identifier);
            }
        };

        addAndMakeVisible (midiSourceLabel);
        addAndMakeVisible (midiSourceCombo);

        // ---- Gate Note --------------------------------------------------
        gateNoteLabel.setText ("Gate Note", juce::dontSendNotification);
        for (auto& name : SoundChopperAudioProcessor::midiNoteNames)
            gateNoteCombo.addItem (name, gateNoteCombo.getNumItems() + 1);
        gateNoteAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                apvts, ParamID::gateNote, gateNoteCombo);
        addAndMakeVisible (gateNoteLabel);
        addAndMakeVisible (gateNoteCombo);

        // ---- Bypass Note ------------------------------------------------
        bypassNoteLabel.setText ("Bypass Note", juce::dontSendNotification);
        for (auto& name : SoundChopperAudioProcessor::midiNoteNames)
            bypassNoteCombo.addItem (name, bypassNoteCombo.getNumItems() + 1);
        bypassNoteAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                apvts, ParamID::bypassNote, bypassNoteCombo);
        addAndMakeVisible (bypassNoteLabel);
        addAndMakeVisible (bypassNoteCombo);

        // ---- LED indicators ---------------------------------------------
        midiLedLabel  .setText ("MIDI",   juce::dontSendNotification);
        gateLedLabel  .setText ("Gate",   juce::dontSendNotification);
        bypassLedLabel.setText ("Bypass", juce::dontSendNotification);
        addAndMakeVisible (midiLedLabel);
        addAndMakeVisible (midiLed);
        addAndMakeVisible (gateLedLabel);
        addAndMakeVisible (gateLed);
        addAndMakeVisible (bypassLedLabel);
        addAndMakeVisible (bypassLed);

        // Set white text on all labels
        for (auto* child : getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
                label->setColour (juce::Label::textColourId, juce::Colours::white);
    }

    // Poll the processor's LED atomics and update the indicators.
    void updateLeds()
    {
        midiLed  .setOn (processorRef.midiNoteLedActive.load (std::memory_order_relaxed));
        gateLed  .setOn (processorRef.gateLedActive    .load (std::memory_order_relaxed));
        bypassLed.setOn (processorRef.bypassLedActive  .load (std::memory_order_relaxed));
    }

    void resized() override
    {
        using namespace EditorLayout;

        const int controlX     = kMargin + kLabelWidth + 4;
        const int controlWidth = getWidth() - controlX - kMargin;
        const int usableWidth  = getWidth() - kMargin * 2;

        auto rowY = [](int r) { return r * (kRowHeight + kRowGap); };

        auto placeRow = [&](int r, juce::Label& label, juce::Component& control)
        {
            label  .setBounds (kMargin,  rowY (r), kLabelWidth,  kRowHeight);
            control.setBounds (controlX, rowY (r), controlWidth, kRowHeight);
        };

        // Row 0: info label (full usable width)
        midiInfoLabel.setBounds (kMargin, rowY (0), usableWidth, kRowHeight);

        // Rows 1–3: source / gate note / bypass note
        placeRow (1, midiSourceLabel, midiSourceCombo);
        placeRow (2, gateNoteLabel,   gateNoteCombo);
        placeRow (3, bypassNoteLabel, bypassNoteCombo);

        // Row 4: LED indicators
        {
            const int ledY     = rowY (4);
            const int colWidth = usableWidth / 3;
            const int ledSize  = kRowHeight - 16;

            auto placeLedCol = [&](int colIdx, juce::Label& label, LedIndicator& led)
            {
                const int x = kMargin + colIdx * colWidth;
                label.setBounds (x, ledY, colWidth - ledSize - 8, kRowHeight);
                led  .setBounds (x + colWidth - ledSize - 4,
                                 ledY + (kRowHeight - ledSize) / 2,
                                 ledSize, ledSize);
            };

            placeLedCol (0, midiLedLabel,   midiLed);
            placeLedCol (1, gateLedLabel,   gateLed);
            placeLedCol (2, bypassLedLabel, bypassLed);
        }
    }

private:
    SoundChopperAudioProcessor& processorRef;

    juce::Label    midiInfoLabel;

    juce::Label    midiSourceLabel;
    juce::ComboBox midiSourceCombo;
    juce::Array<juce::MidiDeviceInfo> midiDevices;

    juce::Label    gateNoteLabel;
    juce::ComboBox gateNoteCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gateNoteAttachment;

    juce::Label    bypassNoteLabel;
    juce::ComboBox bypassNoteCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bypassNoteAttachment;

    juce::Label  midiLedLabel;
    LedIndicator midiLed    { juce::Colours::white };

    juce::Label  gateLedLabel;
    LedIndicator gateLed    { juce::Colours::limegreen };

    juce::Label  bypassLedLabel;
    LedIndicator bypassLed  { juce::Colours::red };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiModePanel)
};
