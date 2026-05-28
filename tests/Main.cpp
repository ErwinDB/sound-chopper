// Main.cpp – custom Catch2 entry point that initialises JUCE before running tests.
//
// Using Catch2::Catch2 (without WithMain) so that we can set up the JUCE
// MessageManager and make the current thread the message thread.  This
// allows JUCE Components to be constructed and manipulated from the test
// thread without triggering assertions.

#include <catch2/catch_session.hpp>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    // Initialise JUCE (creates the MessageManager, initialises logging, etc.)
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Make the main thread the message thread so that JUCE Components can be
    // created and destroyed from within test cases.
    juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();

    return Catch::Session().run (argc, argv);
}
