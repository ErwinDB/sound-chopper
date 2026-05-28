// LedIndicatorTest.cpp – tests for LedIndicator.h
//
// LedIndicator is a small JUCE Component that draws a coloured circle.
// We test:
//   • Construction with various colours
//   • setOn() state transitions (including the no-repaint case when the
//     state does not change)
//   • paint() via an off-screen juce::Image so we can exercise the drawing
//     code without a display

#include <catch2/catch_all.hpp>
#include "LedIndicator.h"

// ============================================================================
//  Construction
// ============================================================================
TEST_CASE ("LedIndicator constructs without crashing", "[led]")
{
    LedIndicator led (juce::Colours::green);
    (void) led;
}

TEST_CASE ("LedIndicator constructs with various colours", "[led]")
{
    { LedIndicator led (juce::Colours::white);          (void) led; }
    { LedIndicator led (juce::Colours::red);            (void) led; }
    { LedIndicator led (juce::Colours::limegreen);      (void) led; }
    { LedIndicator led (juce::Colours::transparentBlack); (void) led; }
}

// ============================================================================
//  setOn / state transitions
// ============================================================================
TEST_CASE ("LedIndicator setOn changes state", "[led]")
{
    LedIndicator led (juce::Colours::green);
    led.setSize (20, 20);

    SECTION ("setOn(true) does not crash")
    {
        REQUIRE_NOTHROW (led.setOn (true));
    }

    SECTION ("setOn(false) does not crash")
    {
        REQUIRE_NOTHROW (led.setOn (false));
    }

    SECTION ("repeated setOn with the same value does not crash")
    {
        led.setOn (true);
        REQUIRE_NOTHROW (led.setOn (true));   // no state change → no repaint

        led.setOn (false);
        REQUIRE_NOTHROW (led.setOn (false));  // no state change → no repaint
    }

    SECTION ("toggling repeatedly does not crash")
    {
        for (int i = 0; i < 10; ++i)
        {
            led.setOn (i % 2 == 0);
        }
    }
}

// ============================================================================
//  paint – exercised via an off-screen image
// ============================================================================
static void paintLed (LedIndicator& led)
{
    juce::Image img (juce::Image::ARGB, led.getWidth(), led.getHeight(), true);
    juce::Graphics g (img);
    led.paint (g);
}

TEST_CASE ("LedIndicator paint – LED off", "[led][paint]")
{
    LedIndicator led (juce::Colours::green);
    led.setSize (20, 20);
    led.setOn (false);
    REQUIRE_NOTHROW (paintLed (led));
}

TEST_CASE ("LedIndicator paint – LED on", "[led][paint]")
{
    LedIndicator led (juce::Colours::limegreen);
    led.setSize (20, 20);
    led.setOn (true);
    REQUIRE_NOTHROW (paintLed (led));
}

TEST_CASE ("LedIndicator paint – minimal size (1x1)", "[led][paint]")
{
    LedIndicator led (juce::Colours::red);
    led.setSize (1, 1);
    led.setOn (true);
    REQUIRE_NOTHROW (paintLed (led));
}
