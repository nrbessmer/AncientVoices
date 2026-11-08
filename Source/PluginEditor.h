#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TullyNeon.h"
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/err.h>

namespace avui
{
    // Tully EDM Vibe plugin theme (shared with Mister Low End / VocalScribe)
    namespace Col
    {
        const juce::Colour bg       = neon::Pal::night0;
        const juce::Colour panel    = neon::Pal::glass;
        const juce::Colour panelHi  = neon::Pal::glassHi;
        const juce::Colour line     = neon::Pal::edge;
        const juce::Colour text     = neon::Pal::text;
        const juce::Colour dim      = neon::Pal::dim;
        const juce::Colour cyan     { 0xff2de2e6 };   // brand cyan
        const juce::Colour pink     { 0xffff2d8a };   // brand pink
    }

    class LnF : public neon::LookAndFeel
    {
    public:
        LnF();
        juce::Label* createSliderTextBox (juce::Slider&) override;
    };

    // Input (grey) and output (cyan) waveforms overlaid on one dark panel.
    class Scope : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        void setData (std::vector<float> in, std::vector<float> out) { input = std::move (in); output = std::move (out); repaint(); }
        void paint (juce::Graphics&) override;
    private:
        std::vector<float> input, output;
    };
}

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
    avui::LnF lnf;
    neon::BackdropCache backdrop;
    juce::TooltipWindow tooltipWindow;

    juce::Image logo;
    juce::Rectangle<int> headerArea, logoArea, voiceArea, controlsArea, scopeArea;

    juce::Label    profileDisplayLabel, descriptionLabel, footerLabel;
    juce::HyperlinkButton footerEmailLink;
    juce::ComboBox presetCombo;

    juce::Slider mixSlider, reverbSlider, delaySlider, driveSlider, outputSlider, pitchSlider;
    juce::Label  mixLabel,  reverbLabel,  delayLabel,  driveLabel,  outputLabel,  pitchLabel;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> mixAtt, reverbAtt, delayAtt, driveAtt, outputAtt, pitchAtt;

    void populatePresetCombo();
    void onPresetComboChanged();
    void syncComboFromParameter();
    void showProfile (int globalIndex);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, juce::Colour) const;

    avui::Scope scope;
    void updateWaveforms();
    void timerCallback() override;

    bool isLicenseValid();
    void getSerialKeyFromUser(std::function<void(juce::String, juce::String)> cb, bool isRetry);
    bool validateLicenseKey(const juce::String& userID, const juce::String& key);
    bool hasValidSerialStored();
    void storeValidSerial();
    void refreshUIAfterLicenseValidation();
    std::vector<unsigned char> decodeBase64(const juce::String& s);

    void configureKnob(juce::Slider& s, juce::Label& lbl, const juce::String& name, juce::Colour colour,
                       const juce::String& tip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AncientVoicesAudioProcessorEditor)
};
