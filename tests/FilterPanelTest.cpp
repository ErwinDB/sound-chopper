// FilterPanelTest.cpp – tests for FilterPanel.h
//
// FilterPanel wraps four filter-related parameters in a JUCE Component.
// We test construction with a live APVTS, numRows() in both states, and
// setFilterOn() toggling.  resized() is exercised by giving the panel a
// non-zero size.

#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "FilterPanel.h"

// ============================================================================
//  Construction
// ============================================================================
TEST_CASE ("FilterPanel constructs without crashing", "[filterpanel]")
{
    SoundChopperAudioProcessor proc;
    FilterPanel fp (proc.apvts);
    (void) fp;
}

// ============================================================================
//  numRows
// ============================================================================
TEST_CASE ("FilterPanel numRows() when filter is off", "[filterpanel]")
{
    SoundChopperAudioProcessor proc;
    FilterPanel fp (proc.apvts);

    // SHOW_FILTER_PARAMS=1, filter off → 1 row (the toggle only)
    REQUIRE (fp.numRows() == 1);
}

TEST_CASE ("FilterPanel numRows() when filter is on", "[filterpanel]")
{
    SoundChopperAudioProcessor proc;
    FilterPanel fp (proc.apvts);

    fp.setFilterOn (true);

    // SHOW_FILTER_PARAMS=1, filter on → 1 + 3 = 4 rows
    REQUIRE (fp.numRows() == 4);
}

// ============================================================================
//  setFilterOn
// ============================================================================
TEST_CASE ("FilterPanel setFilterOn toggles row count", "[filterpanel]")
{
    SoundChopperAudioProcessor proc;
    FilterPanel fp (proc.apvts);

    fp.setFilterOn (false);
    const int offRows = fp.numRows();

    fp.setFilterOn (true);
    const int onRows = fp.numRows();

    REQUIRE (onRows > offRows);
}

TEST_CASE ("FilterPanel setFilterOn can be toggled repeatedly", "[filterpanel]")
{
    SoundChopperAudioProcessor proc;
    FilterPanel fp (proc.apvts);

    for (int i = 0; i < 5; ++i)
    {
        fp.setFilterOn (i % 2 == 0);
    }
}

// ============================================================================
//  resized
// ============================================================================
TEST_CASE ("FilterPanel resized with filter off does not crash", "[filterpanel]")
{
    SoundChopperAudioProcessor proc;
    FilterPanel fp (proc.apvts);
    fp.setSize (500, 200);
    REQUIRE_NOTHROW (fp.resized());
}

TEST_CASE ("FilterPanel resized with filter on does not crash", "[filterpanel]")
{
    SoundChopperAudioProcessor proc;
    FilterPanel fp (proc.apvts);
    fp.setFilterOn (true);
    fp.setSize (500, 200);
    REQUIRE_NOTHROW (fp.resized());
}

// ============================================================================
//  kMaxNumRows constant
// ============================================================================
TEST_CASE ("FilterPanel kMaxNumRows is 4", "[filterpanel]")
{
    REQUIRE (FilterPanel::kMaxNumRows == 4);
}
