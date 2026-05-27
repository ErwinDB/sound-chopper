#pragma once

#include "PluginProcessor.h"
#include "EditorLayout.h"

// ============================================================================
//  AdvancedPanel – groups the Gate Length and Fade Time sliders that are only
//  visible when the user has enabled "Advanced" mode.
// ============================================================================
class AdvancedPanel : public juce::Component
{
public:
    // Number of rows this panel occupies (Gate Length + Fade Time).
    static constexpr int kNumRows = 2;

    explicit AdvancedPanel (juce::AudioProcessorValueTreeState& apvts)
    {
        gateLengthLabel.setText ("Gate Length", juce::dontSendNotification);
        fadeTimeLabel  .setText ("Fade Time",   juce::dontSendNotification);

        EditorLayout::initSlider (gateLengthSlider);
        EditorLayout::initSlider (fadeTimeSlider);

        gateLengthAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, ParamID::gateLength, gateLengthSlider);
        fadeTimeAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, ParamID::fadeTime, fadeTimeSlider);

        addAndMakeVisible (gateLengthLabel);
        addAndMakeVisible (gateLengthSlider);
        addAndMakeVisible (fadeTimeLabel);
        addAndMakeVisible (fadeTimeSlider);

        for (auto* child : getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
                label->setColour (juce::Label::textColourId, juce::Colours::white);
    }

    void resized() override
    {
        using namespace EditorLayout;

        const int controlX     = kMargin + kLabelWidth + 4;
        const int controlWidth = getWidth() - controlX - kMargin;

        auto rowY = [](int r) { return r * (kRowHeight + kRowGap); };

        gateLengthLabel .setBounds (kMargin,  rowY (0), kLabelWidth,  kRowHeight);
        gateLengthSlider.setBounds (controlX, rowY (0), controlWidth, kRowHeight);

        fadeTimeLabel   .setBounds (kMargin,  rowY (1), kLabelWidth,  kRowHeight);
        fadeTimeSlider  .setBounds (controlX, rowY (1), controlWidth, kRowHeight);
    }

private:
    juce::Label  gateLengthLabel;
    juce::Slider gateLengthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        gateLengthAttachment;

    juce::Label  fadeTimeLabel;
    juce::Slider fadeTimeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        fadeTimeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdvancedPanel)
};
