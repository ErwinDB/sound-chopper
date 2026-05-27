#include "PluginEditor.h"
#include <BinaryData.h>

// ============================================================================
//  Layout constants
// ============================================================================
namespace
{
    constexpr int kScale        = 2;
    constexpr int kMargin       = 10 * kScale;
    constexpr int kRowHeight    = 30 * kScale;
    constexpr int kRowGap       = 6  * kScale;
    constexpr int kButtonGap    = 6  * kScale;
    constexpr int kLabelWidth   = 140 * kScale;
    constexpr int kWindowWidth  = 440 * kScale;
    constexpr float kCornerSize = 12.0f;
    // Extra height below the controls panel so the background artwork remains visible.
    constexpr int kArtworkPadding = 160;
    // Height of the logo area at the top (the black panel starts below this).
    constexpr int kLogoHeight = 80 * kScale;

    constexpr int controlsHeight (int numRows)
    {
        return kMargin * 2 + numRows * (kRowHeight + kRowGap) - kRowGap;
    }

    constexpr int windowHeight (int numRows)
    {
        int minHeight = kLogoHeight + controlsHeight (numRows);
        if (numRows < 5)
            minHeight += kArtworkPadding;
        return minHeight;
    }

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
        const int flagCount;
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
}

// ============================================================================
//  Helper – configure a horizontal slider
// ============================================================================
static void initSlider (juce::Slider& s)
{
    s.setSliderStyle   (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle  (juce::Slider::TextBoxRight, false, 70 * kScale, 20 * kScale);
}

// ============================================================================
//  Constructor
// ============================================================================
SoundChopperAudioProcessorEditor::SoundChopperAudioProcessorEditor (
    SoundChopperAudioProcessor& p)
    : AudioProcessorEditor (p),
      processorRef (p)
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

    // ---- MIDI info label ----------------------------------------------
    midiInfoLabel.setText (
        "Route a MIDI track to this plugin's MIDI input, or select an IAC Driver Bus below (Logic workaround).",
        juce::dontSendNotification);
    midiInfoLabel.setJustificationType (juce::Justification::centredLeft);
    midiInfoLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f * (float) kScale)
                                                          .withStyle  ("Italic")));
    addAndMakeVisible (midiInfoLabel);

    // ---- MIDI Source (Logic IAC Driver workaround) --------------------
    midiSourceLabel.setText ("MIDI Source", juce::dontSendNotification);
    midiSourceCombo.addItem ("None (use host MIDI routing)", 1);
    midiDevices = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < midiDevices.size(); ++i)
        midiSourceCombo.addItem (midiDevices[i].name, i + 2);

    // Restore the previously-selected device (if any)
    {
        const auto currentId = p.getCurrentMidiInputDeviceIdentifier();
        int selectedComboId = 1;  // default: "None"
        for (int i = 0; i < midiDevices.size(); ++i)
        {
            if (midiDevices[i].identifier == currentId)
            {
                selectedComboId = i + 2;
                break;
            }
        }
        midiSourceCombo.setSelectedId (selectedComboId, juce::dontSendNotification);
    }

    midiSourceCombo.onChange = [this]
    {
        const int selectedId = midiSourceCombo.getSelectedId();
        if (selectedId <= 1)
        {
            processorRef.setMidiInputDevice ({});
        }
        else
        {
            const int devIndex = selectedId - 2;
            if (devIndex >= 0 && devIndex < midiDevices.size())
                processorRef.setMidiInputDevice (midiDevices[devIndex].identifier);
        }
    };

    addAndMakeVisible (midiSourceLabel);
    addAndMakeVisible (midiSourceCombo);

    // ---- Gate Note (MIDI Input mode only) ----------------------------
    gateNoteLabel.setText ("Gate Note", juce::dontSendNotification);
    for (auto& name : SoundChopperAudioProcessor::midiNoteNames)
        gateNoteCombo.addItem (name, gateNoteCombo.getNumItems() + 1);
    gateNoteAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, ParamID::gateNote, gateNoteCombo);
    addAndMakeVisible (gateNoteLabel);
    addAndMakeVisible (gateNoteCombo);

    // ---- Bypass Note (MIDI Input mode only) --------------------------
    bypassNoteLabel.setText ("Bypass Note", juce::dontSendNotification);
    for (auto& name : SoundChopperAudioProcessor::midiNoteNames)
        bypassNoteCombo.addItem (name, bypassNoteCombo.getNumItems() + 1);
    bypassNoteAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, ParamID::bypassNote, bypassNoteCombo);
    addAndMakeVisible (bypassNoteLabel);
    addAndMakeVisible (bypassNoteCombo);

    // ---- Gate Length --------------------------------------------------
    gateLengthLabel.setText ("Gate Length", juce::dontSendNotification);
    initSlider (gateLengthSlider);
    gateLengthAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, ParamID::gateLength, gateLengthSlider);
    addAndMakeVisible (gateLengthLabel);
    addAndMakeVisible (gateLengthSlider);
    gateLengthLabel .setVisible (false);
    gateLengthSlider.setVisible (false);

    // ---- Fade Time ----------------------------------------------------
    fadeTimeLabel.setText ("Fade Time", juce::dontSendNotification);
    initSlider (fadeTimeSlider);
    fadeTimeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, ParamID::fadeTime, fadeTimeSlider);
    addAndMakeVisible (fadeTimeLabel);
    addAndMakeVisible (fadeTimeSlider);
    fadeTimeLabel .setVisible (false);
    fadeTimeSlider.setVisible (false);

    // ---- Dry / Wet ----------------------------------------------------
    // The parameter always exists and defaults to 100 %.
    // The slider UI is compiled in only when SHOW_DRY_WET is set to 1.
    dryWetAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, ParamID::dryWet, dryWetSlider);
#if SHOW_DRY_WET
    dryWetLabel.setText ("Dry/Wet", juce::dontSendNotification);
    initSlider (dryWetSlider);
    addAndMakeVisible (dryWetLabel);
    addAndMakeVisible (dryWetSlider);
#endif

    // ---- Filter On ----------------------------------------------------
    // The filter parameters always exist.
    // The controls are compiled in only when SHOW_FILTER_PARAMS is set to 1.
    filterEnabledAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, ParamID::filterEnabled, filterEnabledButton);
    filterTypeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, ParamID::filterType, filterTypeCombo);
    filterFreqAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, ParamID::filterFreq, filterFreqSlider);
    filterQAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, ParamID::filterQ, filterQSlider);
#if SHOW_FILTER_PARAMS
    filterEnabledLabel.setText ("Filter On",   juce::dontSendNotification);
    filterTypeLabel   .setText ("Filter Type", juce::dontSendNotification);
    filterFreqLabel   .setText ("Filter Freq", juce::dontSendNotification);
    filterQLabel      .setText ("Filter Q",    juce::dontSendNotification);
    for (auto& choice : SoundChopperAudioProcessor::filterTypeChoices)
        filterTypeCombo.addItem (choice, filterTypeCombo.getNumItems() + 1);
    initSlider (filterFreqSlider);
    initSlider (filterQSlider);
    addAndMakeVisible (filterEnabledLabel);
    addAndMakeVisible (filterEnabledButton);
    addAndMakeVisible (filterTypeLabel);
    addAndMakeVisible (filterTypeCombo);
    addAndMakeVisible (filterFreqLabel);
    addAndMakeVisible (filterFreqSlider);
    addAndMakeVisible (filterQLabel);
    addAndMakeVisible (filterQSlider);

    // Initialise filter sub-control visibility from the current parameter value.
    filterOnActive = apvts.getRawParameterValue (ParamID::filterEnabled)->load() > 0.5f;
    filterTypeLabel .setVisible (filterOnActive);
    filterTypeCombo .setVisible (filterOnActive);
    filterFreqLabel .setVisible (filterOnActive);
    filterFreqSlider.setVisible (filterOnActive);
    filterQLabel    .setVisible (filterOnActive);
    filterQSlider   .setVisible (filterOnActive);
    apvts.addParameterListener (ParamID::filterEnabled, this);
#endif

    // ---- Listen for rhythm parameter changes ---------------------------
    apvts.addParameterListener (ParamID::rhythmSource, this);
    apvts.addParameterListener (ParamID::rhythmPattern, this);

    // Apply initial visibility
    const bool usingMidi =
        static_cast<int> (apvts.getRawParameterValue (ParamID::rhythmSource)->load()) == 1;
    updateRhythmSourceVisibility (usingMidi);

    // ---- LED indicators -----------------------------------------------
    midiLedLabel  .setText ("MIDI",   juce::dontSendNotification);
    gateLedLabel  .setText ("Gate",   juce::dontSendNotification);
    bypassLedLabel.setText ("Bypass", juce::dontSendNotification);
    addAndMakeVisible (midiLedLabel);
    addAndMakeVisible (midiLed);
    addAndMakeVisible (gateLedLabel);
    addAndMakeVisible (gateLed);
    addAndMakeVisible (bypassLedLabel);
    addAndMakeVisible (bypassLed);
    setMidiLedVisibility (usingMidi);

    // Poll LED state at ~20 Hz
    startTimerHz (20);

    // ---- Advanced toggle button ---------------------------------------
    advancedButton.setButtonText ("Advanced");
    advancedButton.onClick = [this]
    {
        advancedMode = advancedButton.getToggleState();
        updateAdvancedVisibility (advancedMode);
    };
    addAndMakeVisible (advancedButton);

    // ---- Window size (set by updateRhythmSourceVisibility above) ------
    // (setSize is called inside updateRhythmSourceVisibility)
    syncRhythmSourceButtons();
    syncRhythmPatternButtons();

    // ---- Set white text on all labels for readability on black background ---
    for (auto* child : getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child))
            label->setColour (juce::Label::textColourId, juce::Colours::white);
    advancedButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
#if SHOW_FILTER_PARAMS
    filterEnabledButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
#endif
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
    midiLed  .setOn (processorRef.midiNoteLedActive.load (std::memory_order_relaxed));
    gateLed  .setOn (processorRef.gateLedActive    .load (std::memory_order_relaxed));
    bypassLed.setOn (processorRef.bypassLedActive  .load (std::memory_order_relaxed));
}

// ============================================================================
//  updateRhythmSourceVisibility
// ============================================================================
void SoundChopperAudioProcessorEditor::updateRhythmSourceVisibility (bool usingMidi)
{
    midiModeActive = usingMidi;

    for (auto* button : rhythmPatternButtons)
        button->setVisible (!usingMidi);
    midiInfoLabel     .setVisible ( usingMidi);
    midiSourceLabel   .setVisible ( usingMidi);
    midiSourceCombo   .setVisible ( usingMidi);
    gateNoteLabel     .setVisible ( usingMidi);
    gateNoteCombo     .setVisible ( usingMidi);
    bypassNoteLabel   .setVisible ( usingMidi);
    bypassNoteCombo   .setVisible ( usingMidi);
    setMidiLedVisibility (usingMidi);
    syncRhythmSourceButtons();

    updateWindowSize();   // triggers resized()
}

// ============================================================================
//  updateAdvancedVisibility
// ============================================================================
void SoundChopperAudioProcessorEditor::updateAdvancedVisibility (bool advanced)
{
    advancedMode = advanced;

    gateLengthLabel .setVisible (advanced);
    gateLengthSlider.setVisible (advanced);
    fadeTimeLabel   .setVisible (advanced);
    fadeTimeSlider  .setVisible (advanced);

    updateWindowSize();   // triggers resized()
}

// ============================================================================
//  updateFilterVisibility
// ============================================================================
void SoundChopperAudioProcessorEditor::updateFilterVisibility (bool filterOn)
{
    filterOnActive = filterOn;

    filterTypeLabel .setVisible (filterOn);
    filterTypeCombo .setVisible (filterOn);
    filterFreqLabel .setVisible (filterOn);
    filterFreqSlider.setVisible (filterOn);
    filterQLabel    .setVisible (filterOn);
    filterQSlider   .setVisible (filterOn);

    updateWindowSize();   // triggers resized()
}

// ============================================================================
//  computeNumRows
// ============================================================================
int SoundChopperAudioProcessorEditor::computeNumRows() const noexcept
{
    int rows = 1;   // Rhythm Source

    if (midiModeActive)
        rows += 4;  // MIDI info, MIDI Source, Gate Note, Bypass Note
    else
        rows += 1;  // Rhythm Pattern

#if SHOW_DRY_WET
    rows += 1;      // Dry/Wet
#endif

#if SHOW_FILTER_PARAMS
    rows += 1;      // Filter On
    if (filterOnActive)
        rows += 3;  // Filter Type, Filter Freq, Filter Q
#endif

    rows += 1;      // Advanced button
    if (advancedMode)
        rows += 2;  // Gate Length, Fade Time
    if (midiModeActive)
        rows += 1;  // LED row

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

void SoundChopperAudioProcessorEditor::setMidiLedVisibility (bool shouldBeVisible)
{
    midiLedLabel  .setVisible (shouldBeVisible);
    midiLed       .setVisible (shouldBeVisible);
    gateLedLabel  .setVisible (shouldBeVisible);
    gateLed       .setVisible (shouldBeVisible);
    bypassLedLabel.setVisible (shouldBeVisible);
    bypassLed     .setVisible (shouldBeVisible);
}

// ============================================================================
//  computeMaxRows  –  largest row count achievable with any combination of
//                     runtime state and compile-time feature flags
// ============================================================================
int SoundChopperAudioProcessorEditor::computeMaxRows() const noexcept
{
    int rows = 1;   // Rhythm Source

    // MIDI mode has more rows (4) than Fixed Pattern mode (1)
    rows += 4;      // MIDI info, MIDI Source, Gate Note, Bypass Note

#if SHOW_DRY_WET
    rows += 1;      // Dry/Wet
#endif

#if SHOW_FILTER_PARAMS
    rows += 1 + 3;  // Filter On + Filter Type, Filter Freq, Filter Q
#endif

    rows += 1;      // Advanced button
    rows += 2;      // Gate Length, Fade Time (Advanced mode)
    rows += 1;      // LED row (MIDI mode)

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
    const int controlWidth = getWidth() - kMargin * 2 - kLabelWidth - 4;
    const int controlX     = kMargin + kLabelWidth + 4;
    const int usableWidth  = getWidth() - kMargin * 2;

    auto placeRow = [&] (int row, juce::Label& label,
                         juce::Component& control) -> juce::Rectangle<int>
    {
        const int y = kLogoHeight + kMargin + row * (kRowHeight + kRowGap);
        label  .setBounds (kMargin, y, kLabelWidth, kRowHeight);
        control.setBounds (controlX, y, controlWidth, kRowHeight);
        return { kMargin, y, getWidth() - kMargin * 2, kRowHeight };
    };

    int row = 0;

    {
        const int y = kLogoHeight + kMargin + row * (kRowHeight + kRowGap);
        const int buttonWidth = (usableWidth - kButtonGap) / (int) rhythmSourceButtons.size();
        for (size_t i = 0; i < rhythmSourceButtons.size(); ++i)
        {
            rhythmSourceButtons[i].setBounds (kMargin + (int) i * (buttonWidth + kButtonGap),
                                              y, buttonWidth, kRowHeight);
        }
        ++row;
    }

    if (midiModeActive)
    {
        // MIDI info label spans the full usable width
        const int y1 = kLogoHeight + kMargin + row * (kRowHeight + kRowGap);
        midiInfoLabel.setBounds (kMargin, y1, getWidth() - kMargin * 2, kRowHeight);
        ++row;

        placeRow (row++, midiSourceLabel, midiSourceCombo);
        placeRow (row++, gateNoteLabel,   gateNoteCombo);
        placeRow (row++, bypassNoteLabel, bypassNoteCombo);
    }
    else
    {
        const int y = kLogoHeight + kMargin + row * (kRowHeight + kRowGap);
        const int buttonWidth = (usableWidth - (rhythmPatternButtons.size() - 1) * kButtonGap)
                                / rhythmPatternButtons.size();
        for (int i = 0; i < rhythmPatternButtons.size(); ++i)
            rhythmPatternButtons[i]->setBounds (kMargin + i * (buttonWidth + kButtonGap),
                                                y, buttonWidth, kRowHeight);
        ++row;
    }

#if SHOW_DRY_WET
    placeRow (row++, dryWetLabel, dryWetSlider);
#endif

#if SHOW_FILTER_PARAMS
    placeRow (row++, filterEnabledLabel, filterEnabledButton);
    if (filterOnActive)
    {
        placeRow (row++, filterTypeLabel,    filterTypeCombo);
        placeRow (row++, filterFreqLabel,    filterFreqSlider);
        placeRow (row++, filterQLabel,       filterQSlider);
    }
#endif

    // Advanced toggle button (full usable width)
    {
        const int y = kLogoHeight + kMargin + row * (kRowHeight + kRowGap);
        advancedButton.setBounds (kMargin, y, getWidth() - kMargin * 2, kRowHeight);
        ++row;
    }

    if (advancedMode)
    {
        placeRow (row++, gateLengthLabel, gateLengthSlider);
        placeRow (row++, fadeTimeLabel,   fadeTimeSlider);
    }

    if (midiModeActive)
    {
        const int ledY     = kLogoHeight + kMargin + row * (kRowHeight + kRowGap);
        const int colWidth = usableWidth / 3;
        const int ledSize  = kRowHeight - 16;

        auto placeLedCol = [&](int colIdx, juce::Label& label, LedIndicator& led)
        {
            const int x = kMargin + colIdx * colWidth;
            label.setBounds (x, ledY, colWidth - ledSize - 8, kRowHeight);
            led.setBounds   (x + colWidth - ledSize - 4,
                             ledY + (kRowHeight - ledSize) / 2,
                             ledSize, ledSize);
        };

        placeLedCol (0, midiLedLabel,   midiLed);
        placeLedCol (1, gateLedLabel,   gateLed);
        placeLedCol (2, bypassLedLabel, bypassLed);
    }
}
