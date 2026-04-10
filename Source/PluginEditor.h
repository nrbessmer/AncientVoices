#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class AncientVoicesAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    AncientVoicesAudioProcessorEditor (AncientVoicesAudioProcessor&);
    ~AncientVoicesAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Reference to the audio processor
    AncientVoicesAudioProcessor& processor;

    // Timer callback for UI refresh
    void timerCallback() override;

    //==============================================================================
    // Header bar components
    juce::Label titleLabel;
    juce::ComboBox presetCombo;
    juce::TextButton prevButton;
    juce::TextButton nextButton;

    //==============================================================================
    // Row 1: Era, Voice, Voices, Spread
    juce::ComboBox eraCombo;
    juce::ComboBox voiceCombo;
    juce::Slider voicesDial;
    juce::Slider spreadDial;

    // Row 2: Drift, Chant, Formant, Mix
    juce::Slider driftDial;
    juce::Slider chantDial;
    juce::Slider formantDial;
    juce::Slider mixDial;

    // Row 3: Cave, Size, Shimmer, Interval
    juce::Slider caveDial;
    juce::Slider sizeDial;
    juce::Slider shimmerDial;
    juce::Slider intervalDial;

    // Row 4: Breath, Rough, Output
    juce::Slider breathDial;
    juce::Slider roughDial;
    juce::Slider outputDial;

    //==============================================================================
    // Labels for each control
    juce::Label eraLabel;
    juce::Label voiceLabel;
    juce::Label voicesLabel;
    juce::Label spreadLabel;
    juce::Label driftLabel;
    juce::Label chantLabel;
    juce::Label formantLabel;
    juce::Label mixLabel;
    juce::Label caveLabel;
    juce::Label sizeLabel;
    juce::Label shimmerLabel;
    juce::Label intervalLabel;
    juce::Label breathLabel;
    juce::Label roughLabel;
    juce::Label outputLabel;

    //==============================================================================
    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> eraAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> voicesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driftAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chantAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> caveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shimmerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intervalAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> breathAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> roughAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    //==============================================================================
    // Helper to configure a rotary dial
    void setupRotaryDial(juce::Slider& dial);
    void setupLabel(juce::Label& label, const juce::String& text);

    // Populate preset combo with categories and names
    void populatePresetCombo();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AncientVoicesAudioProcessorEditor)
};
