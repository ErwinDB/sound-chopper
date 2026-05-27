#include "PluginEditor.h"
#include "EditorLayout.h"
#include "NoteValueButton.h"
#include <BinaryData.h>

using namespace EditorLayout;

// ============================================================================
//  Constructor
// ============================================================================
SoundChopperAudioProcessorEditor::SoundChopperAudioProcessorEditor (
    SoundChopperAudioProcessor& p)
    : AudioProcessorEditor (p),
      processorRef (p),
      midiPanel    (p, p.apvts),
      filterPanel  (p.apvts),
      advancedPanel(p.apvts)
{
#if JUCE_STANDALONE_APPLICATION
    juce::Process::makeForegroundProcess();
#endif

    auto& apvts = p.apvts;

    // ---- Rhythm Source ------------------------------------------------
    for (auto& choice : SoundChopperAudioProcessor::rhythmSourceChoices)
        rhythmSourceCombo.addItem (choice, rhythmSourceCombo.getNumItems() + 1);
    rhythmSourceAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, ParamID::rhythmSource, rhythmSourceCombo);
    for (size_t i = 0; i < rhythmSourceButtons.size(); ++i)
    {
        auto& button = rhythmSourceButtons[i];
        button.setButtonText (SoundChopperAudioProcessor::rhythmSourceChoices[(int) i]);
        button.setClickingTogglesState (false);
        button.onClick = [this, index = (int) i]
        {
            rhythmSourceCombo.setSelectedItemIndex (index, juce::sendNotificationSync);
        };
        addAndMakeVisible (button);
    }
    rhythmSourceCombo.onChange = [this]
    {
        syncRhythmSourceButtons();
        updateRhythmSourceVisibility (rhythmSourceCombo.getSelectedItemIndex() == 1);
    };

    // Highlight the selected rhythm source button with a bright background.
    for (auto& button : rhythmSourceButtons)
    {
        button.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2a2a2a));
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff00b4b4));
        button.setColour (juce::TextButton::textColourOffId,  juce::Colours::white);
        button.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
    }

    // ---- Rhythm Pattern -----------------------------------------------
    for (auto& choice : SoundChopperAudioProcessor::rhythmChoices)
        rhythmPatternCombo.addItem (choice, rhythmPatternCombo.getNumItems() + 1);
    rhythmPatternAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, ParamID::rhythmPattern, rhythmPatternCombo);
    for (int i = 0; i < SoundChopperAudioProcessor::rhythmChoices.size(); ++i)
    {
        const bool isTriplet = SoundChopperAudioProcessor::rhythmChoices[i].containsChar ('T');
        const int slashIndex = SoundChopperAudioProcessor::rhythmChoices[i].indexOfChar ('/');
        const int denominator = SoundChopperAudioProcessor::rhythmChoices[i]
            .substring (slashIndex + 1)
            .initialSectionContainingOnly ("0123456789")
            .getIntValue();

        int flagCount = 0;
        switch (denominator)
        {
            case 8:  flagCount = 1; break;
            case 16: flagCount = 2; break;
            case 32: flagCount = 3; break;
            default: break;
        }

        auto* button = new NoteValueButton (flagCount, isTriplet);
        rhythmPatternButtons.add (button);
        button->onClick = [this, i]
        {
            rhythmPatternCombo.setSelectedItemIndex (i, juce::sendNotificationSync);
        };
        addAndMakeVisible (button);
    }
    rhythmPatternCombo.onChange = [this]
    {
        syncRhythmPatternButtons();
    };

    // ---- Dry / Wet ----------------------------------------------------
    // The parameter always exists and defaults to 100 %.
    // The slider UI is compiled in only when SHOW_DRY_WET is set to 1.
    dryWetAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, ParamID::dryWet, dryWetSlider);
#if SHOW_DRY_WET
    dryWetLabel.setText ("Dry/Wet", juce::dontSendNotification);
    initSlider (dryWetSlider);
    dryWetLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (dryWetLabel);
    addAndMakeVisible (dryWetSlider);
#endif

    // ---- MIDI panel ---------------------------------------------------
    addAndMakeVisible (midiPanel);

    // ---- Filter panel -------------------------------------------------
    // The filter parameters always exist; the panel manages its own UI guards.
    addAndMakeVisible (filterPanel);
#if SHOW_FILTER_PARAMS
    apvts.addParameterListener (ParamID::filterEnabled, this);
#endif

    // ---- Advanced panel -----------------------------------------------
    addAndMakeVisible (advancedPanel);
    advancedPanel.setVisible (false);

    // ---- Advanced toggle button ---------------------------------------
    advancedButton.setButtonText ("Advanced");
    advancedButton.onClick = [this]
    {
        advancedMode = advancedButton.getToggleState();
        updateAdvancedVisibility (advancedMode);
    };
    advancedButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (advancedButton);

    // ---- Listen for rhythm parameter changes ---------------------------
    apvts.addParameterListener (ParamID::rhythmSource, this);
    apvts.addParameterListener (ParamID::rhythmPattern, this);

    // Apply initial visibility
    const bool usingMidi =
        static_cast<int> (apvts.getRawParameterValue (ParamID::rhythmSource)->load()) == 1;
    updateRhythmSourceVisibility (usingMidi);

    // Poll LED state at ~20 Hz
    startTimerHz (20);

    syncRhythmSourceButtons();
    syncRhythmPatternButtons();
}

// ============================================================================
//  Destructor
// ============================================================================
SoundChopperAudioProcessorEditor::~SoundChopperAudioProcessorEditor()
{
    stopTimer();
    processorRef.apvts.removeParameterListener (ParamID::rhythmSource, this);
    processorRef.apvts.removeParameterListener (ParamID::rhythmPattern, this);
#if SHOW_FILTER_PARAMS
    processorRef.apvts.removeParameterListener (ParamID::filterEnabled, this);
#endif
}

// ============================================================================
//  parameterChanged  (called on the message thread)
// ============================================================================
void SoundChopperAudioProcessorEditor::parameterChanged (
    const juce::String& paramID, float newValue)
{
    if (paramID == ParamID::rhythmSource)
    {
        const bool usingMidi = (static_cast<int> (newValue) == 1);
        juce::MessageManager::callAsync ([this, usingMidi]
        {
            updateRhythmSourceVisibility (usingMidi);
        });
    }
    else if (paramID == ParamID::rhythmPattern)
    {
        juce::MessageManager::callAsync ([this]
        {
            syncRhythmPatternButtons();
        });
    }
#if SHOW_FILTER_PARAMS
    else if (paramID == ParamID::filterEnabled)
    {
        const bool filterOn = (newValue > 0.5f);
        juce::MessageManager::callAsync ([this, filterOn]
        {
            updateFilterVisibility (filterOn);
        });
    }
#endif
}

// ============================================================================
//  timerCallback  (message thread, ~20 Hz)
// ============================================================================
void SoundChopperAudioProcessorEditor::timerCallback()
{
    midiPanel.updateLeds();
}

// ============================================================================
//  updateRhythmSourceVisibility
// ============================================================================
void SoundChopperAudioProcessorEditor::updateRhythmSourceVisibility (bool usingMidi)
{
    midiModeActive = usingMidi;

    for (auto* button : rhythmPatternButtons)
        button->setVisible (!usingMidi);
    midiPanel.setVisible (usingMidi);
    syncRhythmSourceButtons();

    updateWindowSize();   // triggers resized()
}

// ============================================================================
//  updateAdvancedVisibility
// ============================================================================
void SoundChopperAudioProcessorEditor::updateAdvancedVisibility (bool advanced)
{
    advancedMode = advanced;

    advancedPanel.setVisible (advanced);

    updateWindowSize();   // triggers resized()
}

// ============================================================================
//  updateFilterVisibility
// ============================================================================
void SoundChopperAudioProcessorEditor::updateFilterVisibility (bool filterOn)
{
    filterPanel.setFilterOn (filterOn);

    updateWindowSize();   // triggers resized()
}

// ============================================================================
//  computeNumRows
// ============================================================================
int SoundChopperAudioProcessorEditor::computeNumRows() const noexcept
{
    int rows = 1;   // Rhythm Source

    if (midiModeActive)
        rows += MidiModePanel::kNumRows;    // info, source, gate note, bypass note, LEDs
    else
        rows += 1;  // Rhythm Pattern

#if SHOW_DRY_WET
    rows += 1;      // Dry/Wet
#endif

    rows += filterPanel.numRows();

    rows += 1;      // Advanced button
    if (advancedMode)
        rows += AdvancedPanel::kNumRows;    // Gate Length, Fade Time

    return rows;
}

void SoundChopperAudioProcessorEditor::syncRhythmSourceButtons()
{
    const int selectedIndex = rhythmSourceCombo.getSelectedItemIndex();
    for (size_t i = 0; i < rhythmSourceButtons.size(); ++i)
        rhythmSourceButtons[i].setToggleState (selectedIndex == (int) i,
                                               juce::dontSendNotification);
}

void SoundChopperAudioProcessorEditor::syncRhythmPatternButtons()
{
    const int selectedIndex = rhythmPatternCombo.getSelectedItemIndex();
    for (int i = 0; i < rhythmPatternButtons.size(); ++i)
        rhythmPatternButtons[i]->setToggleState (selectedIndex == i,
                                                 juce::dontSendNotification);
}

// ============================================================================
//  computeMaxRows  –  largest row count achievable with any combination of
//                     runtime state and compile-time feature flags
// ============================================================================
int SoundChopperAudioProcessorEditor::computeMaxRows() const noexcept
{
    int rows = 1;   // Rhythm Source

    // MIDI mode has more rows than Fixed Pattern mode
    rows += MidiModePanel::kNumRows;    // info, source, gate note, bypass note, LEDs

#if SHOW_DRY_WET
    rows += 1;      // Dry/Wet
#endif

#if SHOW_FILTER_PARAMS
    rows += FilterPanel::kMaxNumRows;   // Filter On + Type + Freq + Q
#endif

    rows += 1;                          // Advanced button
    rows += AdvancedPanel::kNumRows;    // Gate Length, Fade Time

    return rows;
}

// ============================================================================
//  updateWindowSize  –  resizes the window to exactly fit the currently
//                       visible controls.  Using computeNumRows() (not the
//                       constant computeMaxRows()) ensures that setSize()
//                       always produces a new size when controls are
//                       shown/hidden, which guarantees resized() is called
//                       and the newly-visible controls are laid out.
// ============================================================================
void SoundChopperAudioProcessorEditor::updateWindowSize()
{
    setSize (kWindowWidth, windowHeight (computeNumRows()));
}

// ============================================================================
//  paint
// ============================================================================
void SoundChopperAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fill the entire window with the image's edge colour so areas not covered
    // by the image blend seamlessly.
    g.fillAll (juce::Colour (0xff0a0e17));

    const auto bgImage = juce::ImageCache::getFromMemory (
        BinaryData::background_jpg, BinaryData::background_jpgSize);

    if (bgImage.isValid())
    {
        // Scale the image to fill the window width while maintaining the
        // aspect ratio.  This keeps the logo near the top of the window at a
        // consistent, predictable size regardless of the current window height.
        const float scale   = (float) getWidth() / (float) bgImage.getWidth();
        const int   imgH    = juce::roundToInt ((float) bgImage.getHeight() * scale);
        g.drawImage (bgImage, 0, 0, getWidth(), imgH,
                     0, 0, bgImage.getWidth(), bgImage.getHeight());
    }

    // Black panel behind the controls area (below the logo at the top).
    const int controlsPanelHeight = controlsHeight (computeNumRows());
    g.setColour (juce::Colours::black);
    g.fillRect (0, kLogoHeight, getWidth(), controlsPanelHeight);
}

// ============================================================================
//  resized
// ============================================================================
void SoundChopperAudioProcessorEditor::resized()
{
    const int usableWidth = getWidth() - kMargin * 2;

    auto rowY = [this](int r) { return kLogoHeight + kMargin + r * (kRowHeight + kRowGap); };

    int row = 0;

    // Rhythm Source buttons (full usable width, evenly split)
    {
        const int buttonWidth = (usableWidth - kButtonGap) / (int) rhythmSourceButtons.size();
        for (size_t i = 0; i < rhythmSourceButtons.size(); ++i)
        {
            rhythmSourceButtons[i].setBounds (kMargin + (int) i * (buttonWidth + kButtonGap),
                                              rowY (row), buttonWidth, kRowHeight);
        }
        ++row;
    }

    if (midiModeActive)
    {
        midiPanel.setBounds (0, rowY (row), getWidth(),
                             MidiModePanel::kNumRows * (kRowHeight + kRowGap));
        row += MidiModePanel::kNumRows;
    }
    else
    {
        const int buttonWidth = (usableWidth - (rhythmPatternButtons.size() - 1) * kButtonGap)
                                / rhythmPatternButtons.size();
        for (int i = 0; i < rhythmPatternButtons.size(); ++i)
            rhythmPatternButtons[i]->setBounds (kMargin + i * (buttonWidth + kButtonGap),
                                                rowY (row), buttonWidth, kRowHeight);
        ++row;
    }

#if SHOW_DRY_WET
    {
        const int controlX     = kMargin + kLabelWidth + 4;
        const int controlWidth = getWidth() - controlX - kMargin;
        dryWetLabel .setBounds (kMargin,  rowY (row), kLabelWidth,  kRowHeight);
        dryWetSlider.setBounds (controlX, rowY (row), controlWidth, kRowHeight);
        ++row;
    }
#endif

#if SHOW_FILTER_PARAMS
    {
        const int filterRows = filterPanel.numRows();
        filterPanel.setBounds (0, rowY (row), getWidth(),
                               filterRows * (kRowHeight + kRowGap));
        row += filterRows;
    }
#endif

    // Advanced toggle button (full usable width)
    advancedButton.setBounds (kMargin, rowY (row), usableWidth, kRowHeight);
    ++row;

    if (advancedMode)
    {
        advancedPanel.setBounds (0, rowY (row), getWidth(),
                                 AdvancedPanel::kNumRows * (kRowHeight + kRowGap));
        row += AdvancedPanel::kNumRows;
    }
}
