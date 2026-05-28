// AdvancedPanelTest.cpp – tests for AdvancedPanel.h
//
// AdvancedPanel hosts the Gate Length and Fade Time sliders.
// We test construction, the kNumRows constant, and resized().

#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "AdvancedPanel.h"

// ============================================================================
//  Construction
// ============================================================================
TEST_CASE ("AdvancedPanel constructs without crashing", "[advancedpanel]")
{
    SoundChopperAudioProcessor proc;
    AdvancedPanel ap (proc.apvts);
    (void) ap;
}

TEST_CASE ("AdvancedPanel kNumRows is 2", "[advancedpanel]")
{
    REQUIRE (AdvancedPanel::kNumRows == 2);
}

// ============================================================================
//  resized
// ============================================================================
TEST_CASE ("AdvancedPanel resized does not crash", "[advancedpanel]")
{
    SoundChopperAudioProcessor proc;
    AdvancedPanel ap (proc.apvts);
    ap.setSize (500, 200);
    REQUIRE_NOTHROW (ap.resized());
}

TEST_CASE ("AdvancedPanel resized with zero-sized bounds does not crash",
           "[advancedpanel]")
{
    SoundChopperAudioProcessor proc;
    AdvancedPanel ap (proc.apvts);
    ap.setSize (0, 0);
    REQUIRE_NOTHROW (ap.resized());
}

// ============================================================================
//  Children are wired to APVTS parameters
// ============================================================================
TEST_CASE ("AdvancedPanel parameter changes are reflected in APVTS",
           "[advancedpanel]")
{
    SoundChopperAudioProcessor proc;
    AdvancedPanel ap (proc.apvts);
    ap.setSize (500, 200);

    // Change Gate Length via the APVTS and verify the raw value updates
    auto* gateParam = dynamic_cast<juce::AudioParameterFloat*> (
        proc.apvts.getParameter (ParamID::gateLength));
    REQUIRE (gateParam != nullptr);
    *gateParam = 80.0f;

    REQUIRE (*proc.apvts.getRawParameterValue (ParamID::gateLength) == Catch::Approx(80.0f).margin (1.0f));
}
