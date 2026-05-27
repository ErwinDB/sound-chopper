#pragma once

#include "PluginProcessor.h"
#include "EditorLayout.h"

// ============================================================================
//  FilterPanel – groups the four filter controls (enabled toggle, type,
//  frequency, Q) into a single child component.
//
//  The panel is compiled in regardless of the SHOW_FILTER_PARAMS flag so that
//  APVTS attachments (which must always be created) can live here.  When
//  SHOW_FILTER_PARAMS is 0 the controls are never made visible and
//  numRows() returns 0.
// ============================================================================
class FilterPanel : public juce::Component
{
public:
    // Maximum number of rows this panel can occupy (enabled + type + freq + Q).
    static constexpr int kMaxNumRows = 4;

    explicit FilterPanel (juce::AudioProcessorValueTreeState& apvts)
    {
        // APVTS attachments are always created so the parameters are always
        // connected to their UI controls (even when the controls are hidden).
        filterEnabledAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, ParamID::filterEnabled, filterEnabledButton);
        filterTypeAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                apvts, ParamID::filterType, filterTypeCombo);
        filterFreqAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, ParamID::filterFreq, filterFreqSlider);
        filterQAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, ParamID::filterQ, filterQSlider);

#if SHOW_FILTER_PARAMS
        filterEnabledLabel.setText ("Filter On",   juce::dontSendNotification);
        filterTypeLabel   .setText ("Filter Type", juce::dontSendNotification);
        filterFreqLabel   .setText ("Filter Freq", juce::dontSendNotification);
        filterQLabel      .setText ("Filter Q",    juce::dontSendNotification);

        for (auto& choice : SoundChopperAudioProcessor::filterTypeChoices)
            filterTypeCombo.addItem (choice, filterTypeCombo.getNumItems() + 1);

        EditorLayout::initSlider (filterFreqSlider);
        EditorLayout::initSlider (filterQSlider);

        addAndMakeVisible (filterEnabledLabel);
        addAndMakeVisible (filterEnabledButton);
        addAndMakeVisible (filterTypeLabel);
        addAndMakeVisible (filterTypeCombo);
        addAndMakeVisible (filterFreqLabel);
        addAndMakeVisible (filterFreqSlider);
        addAndMakeVisible (filterQLabel);
        addAndMakeVisible (filterQSlider);

        // Set white text on all labels / toggle button
        for (auto* child : getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
                label->setColour (juce::Label::textColourId, juce::Colours::white);
        filterEnabledButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);

        // Initialise sub-control visibility from the current parameter value.
        filterOnActive = apvts.getRawParameterValue (ParamID::filterEnabled)->load() > 0.5f;
        filterTypeLabel .setVisible (filterOnActive);
        filterTypeCombo .setVisible (filterOnActive);
        filterFreqLabel .setVisible (filterOnActive);
        filterFreqSlider.setVisible (filterOnActive);
        filterQLabel    .setVisible (filterOnActive);
        filterQSlider   .setVisible (filterOnActive);
#endif
    }

    // Show or hide the filter sub-controls.  Call updateWindowSize() on the
    // parent editor after this to trigger a layout refresh.
    void setFilterOn (bool filterOn)
    {
        filterOnActive = filterOn;
        filterTypeLabel .setVisible (filterOn);
        filterTypeCombo .setVisible (filterOn);
        filterFreqLabel .setVisible (filterOn);
        filterFreqSlider.setVisible (filterOn);
        filterQLabel    .setVisible (filterOn);
        filterQSlider   .setVisible (filterOn);
    }

    // Number of rows currently occupied by this panel.
    int numRows() const noexcept
    {
#if SHOW_FILTER_PARAMS
        return 1 + (filterOnActive ? 3 : 0);
#else
        return 0;
#endif
    }

    void resized() override
    {
#if SHOW_FILTER_PARAMS
        using namespace EditorLayout;

        const int controlX     = kMargin + kLabelWidth + 4;
        const int controlWidth = getWidth() - controlX - kMargin;

        auto rowY = [](int r) { return r * (kRowHeight + kRowGap); };

        auto placeRow = [&](int r, juce::Label& label, juce::Component& control)
        {
            label  .setBounds (kMargin,  rowY (r), kLabelWidth,  kRowHeight);
            control.setBounds (controlX, rowY (r), controlWidth, kRowHeight);
        };

        placeRow (0, filterEnabledLabel, filterEnabledButton);
        if (filterOnActive)
        {
            placeRow (1, filterTypeLabel, filterTypeCombo);
            placeRow (2, filterFreqLabel, filterFreqSlider);
            placeRow (3, filterQLabel,    filterQSlider);
        }
#endif
    }

private:
    bool filterOnActive = false;

    juce::Label        filterEnabledLabel;
    juce::ToggleButton filterEnabledButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        filterEnabledAttachment;

    juce::Label    filterTypeLabel;
    juce::ComboBox filterTypeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        filterTypeAttachment;

    juce::Label  filterFreqLabel;
    juce::Slider filterFreqSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        filterFreqAttachment;

    juce::Label  filterQLabel;
    juce::Slider filterQSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        filterQAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPanel)
};
