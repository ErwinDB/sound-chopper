// EditorLayoutTest.cpp – tests for EditorLayout.h
//
// EditorLayout.h contains only compile-time constants and three small
// inline helpers (controlsHeight, windowHeight, initSlider).  We test
// the numerical contracts of each helper and verify the key constants.

#include <catch2/catch_all.hpp>
#include "EditorLayout.h"

using namespace EditorLayout;

// ============================================================================
//  Constants
// ============================================================================
TEST_CASE ("EditorLayout constants are positive", "[layout][constants]")
{
    REQUIRE (kScale          > 0);
    REQUIRE (kMargin         > 0);
    REQUIRE (kRowHeight      > 0);
    REQUIRE (kRowGap         >= 0);
    REQUIRE (kButtonGap      >= 0);
    REQUIRE (kLabelWidth     > 0);
    REQUIRE (kWindowWidth    > 0);
    REQUIRE (kArtworkPadding > 0);
    REQUIRE (kLogoHeight     > 0);
}

TEST_CASE ("EditorLayout kScale is 2", "[layout][constants]")
{
    REQUIRE (kScale == 2);
}

TEST_CASE ("EditorLayout kWindowWidth equals 440 * kScale", "[layout][constants]")
{
    REQUIRE (kWindowWidth == 440 * kScale);
}

// ============================================================================
//  controlsHeight
// ============================================================================
TEST_CASE ("controlsHeight returns positive value for one row", "[layout][helpers]")
{
    REQUIRE (controlsHeight (1) > 0);
}

TEST_CASE ("controlsHeight increases with more rows", "[layout][helpers]")
{
    REQUIRE (controlsHeight (2) > controlsHeight (1));
    REQUIRE (controlsHeight (5) > controlsHeight (3));
}

TEST_CASE ("controlsHeight formula: kMargin*2 + n*(kRowHeight+kRowGap) - kRowGap",
           "[layout][helpers]")
{
    for (int n = 1; n <= 10; ++n)
    {
        const int expected = kMargin * 2 + n * (kRowHeight + kRowGap) - kRowGap;
        REQUIRE (controlsHeight (n) == expected);
    }
}

// ============================================================================
//  windowHeight
// ============================================================================
TEST_CASE ("windowHeight returns positive value", "[layout][helpers]")
{
    REQUIRE (windowHeight (1) > 0);
    REQUIRE (windowHeight (5) > 0);
}

TEST_CASE ("windowHeight includes logo area", "[layout][helpers]")
{
    REQUIRE (windowHeight (1) >= kLogoHeight);
    REQUIRE (windowHeight (10) >= kLogoHeight);
}

TEST_CASE ("windowHeight adds artwork padding when rows < 5", "[layout][helpers]")
{
    // For < 5 rows, kArtworkPadding is added
    const int h4 = windowHeight (4);
    const int h5 = windowHeight (5);
    REQUIRE (h4 == kLogoHeight + controlsHeight (4) + kArtworkPadding);
    REQUIRE (h5 == kLogoHeight + controlsHeight (5));
}

TEST_CASE ("windowHeight increases with more rows (excluding artwork boundary)",
           "[layout][helpers]")
{
    // windowHeight drops between n=4 and n=5 because kArtworkPadding is removed;
    // check monotonicity only within each side of that boundary.
    for (int n = 1; n < 4; ++n)
        REQUIRE (windowHeight (n + 1) > windowHeight (n));
    for (int n = 5; n < 10; ++n)
        REQUIRE (windowHeight (n + 1) > windowHeight (n));
}

// ============================================================================
//  initSlider
// ============================================================================
TEST_CASE ("initSlider configures a LinearHorizontal slider", "[layout][initSlider]")
{
    juce::Slider slider;
    initSlider (slider);

    REQUIRE (slider.getSliderStyle() == juce::Slider::LinearHorizontal);
    REQUIRE (slider.getTextBoxPosition() == juce::Slider::TextBoxRight);
}
