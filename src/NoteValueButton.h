#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
//  NoteValueButton – a toggle button that draws a music note symbol
//  (optionally with flags for 8th/16th/32nd notes and a triplet bracket).
// ============================================================================
class NoteValueButton final : public juce::Button
{
public:
    NoteValueButton (int flagsToDraw, bool shouldDrawTriplet)
        : juce::Button ({}),
          flagCount (flagsToDraw),
          isTriplet (shouldDrawTriplet)
    {
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        auto fillColour = getToggleState() ? juce::Colours::white
                                           : juce::Colour (0xff888888);
        g.setColour (fillColour);
        g.fillRoundedRectangle (bounds, kCornerSize);

        // Border: black and bold when selected, grey otherwise.
        g.setColour (getToggleState() ? juce::Colours::black
                                      : juce::Colour (0xff888888));
        g.drawRoundedRectangle (bounds, kCornerSize, getToggleState() ? 2.2f : 1.4f);

        drawNotation (g, bounds.reduced (bounds.getWidth() * 0.08f,
                                         bounds.getHeight() * 0.16f));
    }

private:
    static constexpr float kCornerSize = 12.0f;

    const int  flagCount;
    const bool isTriplet;

    void drawNotation (juce::Graphics& g, juce::Rectangle<float> bounds) const
    {
        const int noteCount = isTriplet ? 3 : 1;
        const float noteSpacing = noteCount > 1 ? bounds.getWidth() / (noteCount + 1.0f) : 0.0f;
        const float noteHeadW   = juce::jmin (bounds.getHeight() * 0.34f,
                                              bounds.getWidth() / (noteCount * 2.5f));
        const float noteHeadH   = noteHeadW * 0.72f;
        const float stemHeight  = bounds.getHeight() * 0.54f;
        const float headY       = bounds.getBottom() - noteHeadH - bounds.getHeight() * 0.10f;
        const float stemTopY    = juce::jmax (bounds.getY() + bounds.getHeight() * 0.18f,
                                              headY - stemHeight);

        g.setColour (juce::Colours::black);

        for (int i = 0; i < noteCount; ++i)
        {
            const float centreX = noteCount > 1
                ? bounds.getX() + noteSpacing * (i + 1.0f)
                : bounds.getCentreX();
            const float headX = centreX - noteHeadW * 0.5f;

            g.fillEllipse (headX, headY, noteHeadW, noteHeadH);

            const float stemX = headX + noteHeadW - 1.5f;
            g.drawLine (stemX, headY + noteHeadH * 0.5f, stemX, stemTopY, 2.2f);

            for (int flag = 0; flag < flagCount; ++flag)
            {
                const float flagY = stemTopY + flag * noteHeadH * 0.55f;
                juce::Path flagPath;
                flagPath.startNewSubPath (stemX, flagY);
                flagPath.quadraticTo (stemX + noteHeadW * 0.70f,
                                      flagY + noteHeadH * 0.05f,
                                      stemX + noteHeadW * 0.30f,
                                      flagY + noteHeadH * 0.95f);
                g.strokePath (flagPath, juce::PathStrokeType (2.0f));
            }
        }

        if (isTriplet)
        {
            const float tripletY = bounds.getY();
            const float leftX    = bounds.getX() + noteHeadW * 0.5f;
            const float rightX   = bounds.getRight() - noteHeadW * 0.5f;
            const float numberW  = noteHeadW * 0.95f;
            const float midX     = bounds.getCentreX();

            g.drawLine (leftX, tripletY + 6.0f, midX - numberW * 0.75f, tripletY + 6.0f, 1.6f);
            g.drawLine (midX + numberW * 0.75f, tripletY + 6.0f, rightX, tripletY + 6.0f, 1.6f);
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (bounds.getHeight() * 0.22f)
                                                       .withStyle ("Bold")));
            g.drawText ("3",
                        juce::Rectangle<float> (midX - numberW * 0.5f, tripletY,
                                                numberW, bounds.getHeight() * 0.28f).toNearestInt(),
                        juce::Justification::centred);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteValueButton)
};
