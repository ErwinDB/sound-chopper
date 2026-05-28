// NoteValueButtonTest.cpp – tests for NoteValueButton.h
//
// NoteValueButton draws a musical note symbol inside a rounded rectangle.
// The drawing logic varies with flagCount (0–3) and isTriplet (true/false).
// We exercise all combinations via off-screen painting.

#include <catch2/catch_all.hpp>
#include "NoteValueButton.h"

// Helper: paint a button into an in-memory image so that drawing code is
// exercised without a display.
static void paintButton (NoteValueButton& btn, bool isHighlighted, bool isDown)
{
    juce::Image img (juce::Image::ARGB, btn.getWidth(), btn.getHeight(), true);
    juce::Graphics g (img);
    btn.paintButton (g, isHighlighted, isDown);
}

// ============================================================================
//  Construction
// ============================================================================
TEST_CASE ("NoteValueButton constructs for all flag counts", "[notebutton]")
{
    for (int flags = 0; flags <= 3; ++flags)
    {
        { NoteValueButton btn (flags, false); (void) btn; }
        { NoteValueButton btn (flags, true);  (void) btn; }
    }
}

// ============================================================================
//  paintButton – normal note (not triplet)
// ============================================================================
TEST_CASE ("NoteValueButton paints non-triplet notes", "[notebutton][paint]")
{
    for (int flags = 0; flags <= 3; ++flags)
    {
        NoteValueButton btn (flags, false);
        btn.setSize (60, 60);

        SECTION ("un-toggled (flags=" + std::to_string (flags) + ")")
        {
            btn.setToggleState (false, juce::dontSendNotification);
            REQUIRE_NOTHROW (paintButton (btn, false, false));
        }

        SECTION ("toggled (flags=" + std::to_string (flags) + ")")
        {
            btn.setToggleState (true, juce::dontSendNotification);
            REQUIRE_NOTHROW (paintButton (btn, false, false));
        }

        SECTION ("highlighted (flags=" + std::to_string (flags) + ")")
        {
            REQUIRE_NOTHROW (paintButton (btn, true, false));
        }

        SECTION ("pressed (flags=" + std::to_string (flags) + ")")
        {
            REQUIRE_NOTHROW (paintButton (btn, false, true));
        }
    }
}

// ============================================================================
//  paintButton – triplet notes
// ============================================================================
TEST_CASE ("NoteValueButton paints triplet notes", "[notebutton][paint]")
{
    for (int flags = 0; flags <= 3; ++flags)
    {
        NoteValueButton btn (flags, true);
        btn.setSize (60, 60);

        SECTION ("un-toggled triplet (flags=" + std::to_string (flags) + ")")
        {
            btn.setToggleState (false, juce::dontSendNotification);
            REQUIRE_NOTHROW (paintButton (btn, false, false));
        }

        SECTION ("toggled triplet (flags=" + std::to_string (flags) + ")")
        {
            btn.setToggleState (true, juce::dontSendNotification);
            REQUIRE_NOTHROW (paintButton (btn, false, false));
        }
    }
}

// ============================================================================
//  paintButton – edge-case sizes
// ============================================================================
TEST_CASE ("NoteValueButton paints at minimal size", "[notebutton][paint]")
{
    NoteValueButton btn (2, true);
    btn.setSize (10, 10);
    REQUIRE_NOTHROW (paintButton (btn, false, false));
}

TEST_CASE ("NoteValueButton paints at large size", "[notebutton][paint]")
{
    NoteValueButton btn (3, false);
    btn.setSize (200, 200);
    REQUIRE_NOTHROW (paintButton (btn, true, true));
}
