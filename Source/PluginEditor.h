#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
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
        const juce::Colour bg       { 0xff0a0c10 };
        const juce::Colour panel    { 0xff13171d };
        const juce::Colour panelHi  { 0xff1b2129 };
        const juce::Colour line     { 0xff2a323d };
        const juce::Colour text     { 0xffd9e1ea };
        const juce::Colour dim      { 0xff7d8896 };
        const juce::Colour cyan     { 0xff2de2e6 };   // brand cyan
        const juce::Colour pink     { 0xffff2d8a };   // brand pink
    }

    class LnF : public juce::LookAndFeel_V4
    {
    public:
        LnF();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (14.0f)); }
        juce::Label* createSliderTextBox (juce::Slider&) override;
    };

    // Input (grey) and output (cyan) waveforms overlaid on one dark panel.
    class Scope : public juce::Component
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
