// PluginEditor.cpp

#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
AncientVoicesAudioProcessorEditor::AncientVoicesAudioProcessorEditor(AncientVoicesAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // ---- Header bar ----
    titleLabel.setText("ANCIENT VOICES", juce::dontSendNotification);
    titleLabel.setFont(juce::Font("Arial", 22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFFFD700)); // Gold
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    // Preset combo
    populatePresetCombo();
    presetCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2A3A56));
    presetCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    presetCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF4A5A7A));
    presetCombo.onChange = [this]
    {
        int selectedId = presetCombo.getSelectedId();
        if (selectedId > 0)
            processor.applyFactoryPreset(selectedId - 1);
    };
    addAndMakeVisible(presetCombo);

    // Prev / Next buttons
    prevButton.setButtonText("<");
    prevButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A3A56));
    prevButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    prevButton.onClick = [this]
    {
        int current = presetCombo.getSelectedId();
        if (current <= 1)
            presetCombo.setSelectedId(AncientVoices::kNumPresets, juce::sendNotification);
        else
            presetCombo.setSelectedId(current - 1, juce::sendNotification);
    };
    addAndMakeVisible(prevButton);

    nextButton.setButtonText(">");
    nextButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A3A56));
    nextButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    nextButton.onClick = [this]
    {
        int current = presetCombo.getSelectedId();
        if (current >= AncientVoices::kNumPresets)
            presetCombo.setSelectedId(1, juce::sendNotification);
        else
            presetCombo.setSelectedId(current + 1, juce::sendNotification);
    };
    addAndMakeVisible(nextButton);

    // ---- Row 1: Era, Voice, Voices, Spread ----
    eraCombo.addItem("Sumerian", 1);
    eraCombo.addItem("Egyptian", 2);
    eraCombo.addItem("Byzantine", 3);
    eraCombo.addItem("Vedic", 4);
    eraCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2A3A56));
    eraCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    eraCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF4A5A7A));
    addAndMakeVisible(eraCombo);
    eraAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "ERA", eraCombo);
    setupLabel(eraLabel, "Era");

    voiceCombo.addItem("Ancient", 1);
    voiceCombo.addItem("Human", 2);
    voiceCombo.addItem("Robot", 3);
    voiceCombo.addItem("Ethereal", 4);
    voiceCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2A3A56));
    voiceCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    voiceCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF4A5A7A));
    addAndMakeVisible(voiceCombo);
    voiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "VOICE_MODE", voiceCombo);
    setupLabel(voiceLabel, "Voice");

    setupRotaryDial(voicesDial);
    addAndMakeVisible(voicesDial);
    voicesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "NUM_VOICES", voicesDial);
    setupLabel(voicesLabel, "Voices");

    setupRotaryDial(spreadDial);
    addAndMakeVisible(spreadDial);
    spreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "SPREAD", spreadDial);
    setupLabel(spreadLabel, "Spread");

    // ---- Row 2: Drift, Chant, Formant, Mix ----
    setupRotaryDial(driftDial);
    addAndMakeVisible(driftDial);
    driftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "DRIFT", driftDial);
    setupLabel(driftLabel, "Drift");

    setupRotaryDial(chantDial);
    addAndMakeVisible(chantDial);
    chantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "CHANT", chantDial);
    setupLabel(chantLabel, "Chant");

    setupRotaryDial(formantDial);
    addAndMakeVisible(formantDial);
    formantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "FORMANT_DEPTH", formantDial);
    setupLabel(formantLabel, "Formant");

    setupRotaryDial(mixDial);
    addAndMakeVisible(mixDial);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "MIX", mixDial);
    setupLabel(mixLabel, "Mix");

    // ---- Row 3: Cave, Size, Shimmer, Interval ----
    setupRotaryDial(caveDial);
    addAndMakeVisible(caveDial);
    caveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "CAVE_MIX", caveDial);
    setupLabel(caveLabel, "Cave");

    setupRotaryDial(sizeDial);
    addAndMakeVisible(sizeDial);
    sizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "CAVE_SIZE", sizeDial);
    setupLabel(sizeLabel, "Size");

    setupRotaryDial(shimmerDial);
    addAndMakeVisible(shimmerDial);
    shimmerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "SHIMMER_MIX", shimmerDial);
    setupLabel(shimmerLabel, "Shimmer");

    setupRotaryDial(intervalDial);
    addAndMakeVisible(intervalDial);
    intervalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "INTERVAL", intervalDial);
    setupLabel(intervalLabel, "Interval");

    // ---- Row 4: Breath, Rough, Output ----
    setupRotaryDial(breathDial);
    addAndMakeVisible(breathDial);
    breathAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "BREATHE", breathDial);
    setupLabel(breathLabel, "Breath");

    setupRotaryDial(roughDial);
    addAndMakeVisible(roughDial);
    roughAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "ROUGHNESS", roughDial);
    setupLabel(roughLabel, "Rough");

    setupRotaryDial(outputDial);
    outputDial.setTextValueSuffix(" dB");
    addAndMakeVisible(outputDial);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "OUTPUT_GAIN", outputDial);
    setupLabel(outputLabel, "Output");

    // ---- Timer for UI updates ----
    startTimerHz(20);

    // ---- Window size LAST (AU stability rule) ----
    setSize(900, 620);
}

AncientVoicesAudioProcessorEditor::~AncientVoicesAudioProcessorEditor()
{
    stopTimer();

    // Reset all attachments before components are destroyed
    eraAttachment.reset();
    voiceAttachment.reset();
    voicesAttachment.reset();
    spreadAttachment.reset();
    driftAttachment.reset();
    chantAttachment.reset();
    formantAttachment.reset();
    mixAttachment.reset();
    caveAttachment.reset();
    sizeAttachment.reset();
    shimmerAttachment.reset();
    intervalAttachment.reset();
    breathAttachment.reset();
    roughAttachment.reset();
    outputAttachment.reset();
}

//==============================================================================
void AncientVoicesAudioProcessorEditor::setupRotaryDial(juce::Slider& dial)
{
    dial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    dial.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    dial.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFFD700));   // YELLOW fill
    dial.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF4A5A7A)); // Border
    dial.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFFD700));               // YELLOW thumb
    dial.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    dial.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    dial.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    dial.setSliderSnapsToMousePosition(false);
}

void AncientVoicesAudioProcessorEditor::setupLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font("Arial", 12.0f, juce::Font::plain));
    label.setColour(juce::Label::textColourId, juce::Colour(0xFFCFD8E6)); // Dim text
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void AncientVoicesAudioProcessorEditor::populatePresetCombo()
{
    // Category boundaries: 0-6 CHANT, 7-13 DRONE, 14-21 CHOIR, 22-29 RITUAL,
    //                       30-37 BREATH, 38-43 SHIMMER, 44-49 DARK
    const int catStarts[] = { 0, 7, 14, 22, 30, 38, 44 };
    const int numCats = 7;
    int catIdx = 0;

    for (int i = 0; i < AncientVoices::kNumPresets; ++i)
    {
        // Add section heading when entering a new category
        if (catIdx < numCats && i == catStarts[catIdx])
        {
            presetCombo.addSectionHeading(AncientVoices::kCategoryNames[catIdx]);
            ++catIdx;
        }

        presetCombo.addItem(AncientVoices::kFactoryPresets[i].name, i + 1);
    }

    presetCombo.setSelectedId(1, juce::dontSendNotification);
}

//==============================================================================
void AncientVoicesAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Vertical gradient background: #3A4A6B (top) to #1A2236 (bottom)
    juce::ColourGradient gradient(juce::Colour(0xFF3A4A6B), 0.0f, 0.0f,
                                   juce::Colour(0xFF1A2236), 0.0f, (float)getHeight(), false);
    g.setGradientFill(gradient);
    g.fillRect(getLocalBounds());

    // Header bar background
    g.setColour(juce::Colour(0xFF2A3A56));
    g.fillRect(0, 0, getWidth(), 54);

    // Header bar bottom border
    g.setColour(juce::Colour(0xFF4A5A7A));
    g.drawLine(0.0f, 54.0f, (float)getWidth(), 54.0f, 1.0f);

    // Copyright text at bottom
    g.setColour(juce::Colour(0xFF6A7A9A));
    g.setFont(juce::Font("Arial", 10.0f, juce::Font::plain));
    g.drawText("(c)(p) 2026 Tully EDM Vibe — Ancient Voices",
               0, getHeight() - 20, getWidth(), 20,
               juce::Justification::centred);
}

//==============================================================================
void AncientVoicesAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // ---- Header bar (54px) ----
    auto headerArea = area.removeFromTop(54);
    titleLabel.setBounds(headerArea.removeFromLeft(180).reduced(10, 12));

    auto navRight = headerArea.removeFromRight(80);
    nextButton.setBounds(navRight.removeFromRight(36).reduced(2, 14));
    prevButton.setBounds(navRight.removeFromRight(36).reduced(2, 14));

    presetCombo.setBounds(headerArea.reduced(10, 12));

    // ---- Main controls area ----
    area.removeFromTop(10);  // top padding
    area.removeFromBottom(24); // copyright space

    const int numCols = 4;
    const int numRows = 4;
    const int cellW = area.getWidth() / numCols;
    const int cellH = area.getHeight() / numRows;
    const int dialSize = juce::jmin(cellW - 20, cellH - 30);
    const int labelH = 16;

    auto getCell = [&](int row, int col) -> juce::Rectangle<int>
    {
        return juce::Rectangle<int>(area.getX() + col * cellW,
                                     area.getY() + row * cellH,
                                     cellW, cellH);
    };

    auto placeDial = [&](juce::Slider& dial, juce::Label& label, int row, int col)
    {
        auto cell = getCell(row, col);
        auto dialBounds = cell.withSizeKeepingCentre(dialSize, dialSize).translated(0, -8);
        dial.setBounds(dialBounds);
        label.setBounds(cell.getX(), dialBounds.getBottom() + 2, cellW, labelH);
    };

    auto placeCombo = [&](juce::ComboBox& combo, juce::Label& label, int row, int col)
    {
        auto cell = getCell(row, col);
        auto comboBounds = cell.withSizeKeepingCentre(cellW - 30, 28).translated(0, -4);
        combo.setBounds(comboBounds);
        label.setBounds(cell.getX(), comboBounds.getBottom() + 4, cellW, labelH);
    };

    // Row 1
    placeCombo(eraCombo, eraLabel, 0, 0);
    placeCombo(voiceCombo, voiceLabel, 0, 1);
    placeDial(voicesDial, voicesLabel, 0, 2);
    placeDial(spreadDial, spreadLabel, 0, 3);

    // Row 2
    placeDial(driftDial, driftLabel, 1, 0);
    placeDial(chantDial, chantLabel, 1, 1);
    placeDial(formantDial, formantLabel, 1, 2);
    placeDial(mixDial, mixLabel, 1, 3);

    // Row 3
    placeDial(caveDial, caveLabel, 2, 0);
    placeDial(sizeDial, sizeLabel, 2, 1);
    placeDial(shimmerDial, shimmerLabel, 2, 2);
    placeDial(intervalDial, intervalLabel, 2, 3);

    // Row 4
    placeDial(breathDial, breathLabel, 3, 0);
    placeDial(roughDial, roughLabel, 3, 1);
    placeDial(outputDial, outputLabel, 3, 2);
    // Cell (3, 3) intentionally empty
}

//==============================================================================
void AncientVoicesAudioProcessorEditor::timerCallback()
{
    // Future: update any animated UI elements here
    // Currently no waveform display — keep lightweight
}
