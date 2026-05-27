#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
