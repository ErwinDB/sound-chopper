#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
//  EditorLayout – shared layout constants and helpers used by the plugin
//  editor and its sub-panels.
// ============================================================================
namespace EditorLayout
{
    constexpr int kScale          = 2;
    constexpr int kMargin         = 10 * kScale;
    constexpr int kRowHeight      = 30 * kScale;
    constexpr int kRowGap         = 6  * kScale;
    constexpr int kButtonGap      = 6  * kScale;
    constexpr int kLabelWidth     = 140 * kScale;
    constexpr int kWindowWidth    = 440 * kScale;
    constexpr int kArtworkPadding = 160;
    // Height of the logo area at the top (the black panel starts below this).
    constexpr int kLogoHeight     = 80 * kScale;

    // Total height of the controls panel for a given number of rows.
    constexpr int controlsHeight (int numRows) noexcept
    {
        return kMargin * 2 + numRows * (kRowHeight + kRowGap) - kRowGap;
    }

    // Total window height for a given number of visible rows.
    constexpr int windowHeight (int numRows) noexcept
    {
        int minHeight = kLogoHeight + controlsHeight (numRows);
        if (numRows < 5)
            minHeight += kArtworkPadding;
        return minHeight;
    }

    // Configure a juce::Slider as a standard horizontal slider.
    inline void initSlider (juce::Slider& s)
    {
        s.setSliderStyle  (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70 * kScale, 20 * kScale);
    }
}
