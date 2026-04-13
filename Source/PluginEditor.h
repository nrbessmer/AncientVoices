#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/err.h>

class AncientVoicesAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    AncientVoicesAudioProcessorEditor (AncientVoicesAudioProcessor&);
    ~AncientVoicesAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AncientVoicesAudioProcessor& audioProcessor;
    juce::TooltipWindow tooltipWindow;

    juce::Label    titleLabel, subtitleLabel, presetLabel, profileDisplayLabel, footerLabel;
    juce::ComboBox presetCombo;

    juce::Slider mixSlider, reverbSlider, delaySlider, driveSlider, outputSlider;
    juce::Label  mixLabel,  reverbLabel,  delayLabel,  driveLabel,  outputLabel;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> mixAtt, reverbAtt, delayAtt, driveAtt, outputAtt;

    void populatePresetCombo();
    void onPresetComboChanged();
    void syncComboFromParameter();

    class CustomAudioVisualiserComponent : public juce::AudioVisualiserComponent
    {
    public:
        CustomAudioVisualiserComponent(int nc) : juce::AudioVisualiserComponent(nc)
        { setBufferSize(2048); setSamplesPerBlock(512); setOpaque(false); }
        void paint(juce::Graphics& g) override { juce::AudioVisualiserComponent::paint(g); }
    };

    std::unique_ptr<CustomAudioVisualiserComponent> waveformDisplay;
    void updateWaveforms();
    void timerCallback() override;

    bool isLicenseValid();
    void getSerialKeyFromUser(std::function<void(juce::String, juce::String)> cb, bool isRetry);
    bool validateLicenseKey(const juce::String& userID, const juce::String& key);
    bool hasValidSerialStored();
    void storeValidSerial();
    void refreshUIAfterLicenseValidation();
    std::vector<unsigned char> decodeBase64(const juce::String& s);

    void configureKnob(juce::Slider& s, juce::Label& lbl, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AncientVoicesAudioProcessorEditor)
};
