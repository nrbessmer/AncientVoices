#pragma once
#include <JuceHeader.h>
#include "UI.h"

// ─────────────────────────────────────────────────────────────────────────────
//  VOICE PANEL  v3
//
//  Top:  4-button voice mode bank  [ANCIENT] [HUMAN] [ROBOT] [ETHEREAL]
//  Then: XY Pad + BREATHE ROUGH GENDER
//  Then: VOICES / INTERVAL combos + DEPTH MIX SPREAD DRIFT knobs
//  Then: ADSR + EnvDisplay
//
//  Layout (fits 381px inner height):
//    Mode buttons    26px
//    gap              4px
//    "Voice Char"    14px
//    XY+knobs       118px
//    gap              6px
//    "Choir"         14px
//    Combos          22px
//    gap              4px
//    Knob row        56px
//    gap              6px
//    "Env"           14px
//    ADSR+display    97px
//    Total          381px ✓
// ─────────────────────────────────────────────────────────────────────────────

class VoicePanel : public juce::Component,
                   private juce::Slider::Listener,
                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit VoicePanel(juce::AudioProcessorValueTreeState& apvts)
        : apvts_(apvts),
          breatheK_("BREATHE", Pal::Amber),
          roughK_  ("ROUGH",   Pal::Amber),
          genderK_ ("GENDER",  Pal::Amber),
          depthK_  ("DEPTH",   Pal::Primary),
          mixK_    ("MIX",     Pal::Primary),
          spreadK_ ("SPREAD",  Pal::Sage),
          driftK_  ("DRIFT",   Pal::Sage),
          atkK_    ("ATTACK"),
          decK_    ("DECAY"),
          susK_    ("SUSTAIN"),
          relK_    ("RELEASE")
    {
        // ── Voice mode buttons ─────────────────────────────────────────
        static const char* kModeLabels[] = { "ANCIENT", "HUMAN", "ROBOT", "ETHEREAL" };
        for (int i = 0; i < 4; ++i) {
            modeBtn_[i].setButtonText(kModeLabels[i]);
            modeBtn_[i].setClickingTogglesState(false);
            modeBtn_[i].onClick = [this, i] { setMode(i); };
            addAndMakeVisible(modeBtn_[i]);
        }

        // ── Combos ────────────────────────────────────────────────────
        for (int i = 1; i <= 8; ++i)
            voicesCb_.addItem(juce::String(i) + (i==1?" voice":" voices"), i);
        for (const char* n : {"Unison","Fifth","Octave","Sacred","Just Triad","Ancient","Drone"})
            intervalCb_.addItem(n, intervalCb_.getNumItems() + 1);
        addAndMakeVisible(voicesCb_);
        addAndMakeVisible(intervalCb_);

        // ── Knobs ─────────────────────────────────────────────────────
        for (auto* k : { &breatheK_, &roughK_, &genderK_,
                         &depthK_, &mixK_, &spreadK_, &driftK_,
                         &atkK_, &decK_, &susK_, &relK_ })
            addAndMakeVisible(k);

        addAndMakeVisible(envDisplay_);
        addAndMakeVisible(xyPad_);

        xyPad_.bind(apvts, "vowel_pos", "vowel_open", 0.f, 4.f, 0.5f, 1.5f);
        xyPad_.setLabels("VOWEL", "OPEN");

        // ── Attachments ───────────────────────────────────────────────
        voicesCbA_   = std::make_unique<CA>(apvts, "num_voices",   voicesCb_);
        intervalCbA_ = std::make_unique<CA>(apvts, "interval",     intervalCb_);
        breatheA_ = mkSA(apvts, "breathe",       breatheK_.slider);
        roughA_   = mkSA(apvts, "roughness",     roughK_.slider);
        genderA_  = mkSA(apvts, "gender",        genderK_.slider);
        depthA_   = mkSA(apvts, "formant_depth", depthK_.slider);
        mixA_     = mkSA(apvts, "mix",           mixK_.slider);
        spreadA_  = mkSA(apvts, "spread",        spreadK_.slider);
        driftA_   = mkSA(apvts, "drift",         driftK_.slider);
        atkA_     = mkSA(apvts, "env_attack",    atkK_.slider);
        decA_     = mkSA(apvts, "env_decay",     decK_.slider);
        susA_     = mkSA(apvts, "env_sustain",   susK_.slider);
        relA_     = mkSA(apvts, "env_release",   relK_.slider);

        for (auto* k : { &atkK_, &decK_, &susK_, &relK_ })
            k->slider.addListener(this);

        // Listen for voice_mode changes from other sources (AncientPanel, presets)
        apvts.addParameterListener("voice_mode", this);
        updateModeButtons(int(apvts.getRawParameterValue("voice_mode")->load()));
        updateEnv();
    }

    ~VoicePanel() override {
        apvts_.removeParameterListener("voice_mode", this);
        for (auto* k : { &atkK_, &decK_, &susK_, &relK_ })
            k->slider.removeListener(this);
    }

    void sliderValueChanged(juce::Slider*) override { updateEnv(); }

    void parameterChanged(const juce::String& id, float val) override {
        if (id == "voice_mode")
            juce::MessageManager::callAsync([this, v = int(val)] { updateModeButtons(v); });
    }

    void paint(juce::Graphics& g) override {
        AncientLAF::bg(g, getLocalBounds());
        AncientLAF::sectionLabel(g, {12,  34, 120, 14}, "Voice Character");
        AncientLAF::sectionLabel(g, {12, 172,  80, 14}, "Choir");
        AncientLAF::sectionLabel(g, {12, 276, 120, 14}, "Amplitude Envelope");
    }

    void resized() override {
        auto b  = getLocalBounds().reduced(12, 8);
        int  kw = 54;

        // ── Mode buttons ─────────────────────────────────────────────
        // y=8 (b start). 4 buttons fill the row equally.
        auto r0 = b.removeFromTop(26);  // cumulative=34
        int bw = r0.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            modeBtn_[i].setBounds(r0.removeFromLeft(i < 3 ? bw : r0.getWidth()));

        b.removeFromTop(4);             // gap, cumulative=38

        // ── Voice Character ──────────────────────────────────────────
        b.removeFromTop(14);            // label, cumulative=52
        auto r1 = b.removeFromTop(118); // XY+knobs, cumulative=170
        xyPad_.setBounds(r1.removeFromLeft(118).removeFromTop(118));
        r1.removeFromLeft(8);
        // 3 knobs stacked at 39px each (3×39=117px ≈ 118)
        breatheK_.setBounds(r1.removeFromTop(39).removeFromLeft(kw));
        roughK_.setBounds  (r1.removeFromTop(39).removeFromLeft(kw));
        genderK_.setBounds (r1.removeFromTop(39).removeFromLeft(kw));

        // ── Choir ────────────────────────────────────────────────────
        b.removeFromTop(6);             // gap, cumulative=176
        b.removeFromTop(14);            // label, cumulative=190
        auto r2 = b.removeFromTop(22);  // combos, cumulative=212
        voicesCb_.setBounds  (r2.removeFromLeft(110).reduced(0, 2));
        r2.removeFromLeft(6);
        intervalCb_.setBounds(r2.removeFromLeft(120).reduced(0, 2));

        b.removeFromTop(4);             // gap, cumulative=216
        auto r3 = b.removeFromTop(56);  // knobs, cumulative=272
        depthK_.setBounds  (r3.removeFromLeft(kw));
        r3.removeFromLeft(4);
        mixK_.setBounds    (r3.removeFromLeft(kw));
        r3.removeFromLeft(4);
        spreadK_.setBounds (r3.removeFromLeft(kw));
        r3.removeFromLeft(4);
        driftK_.setBounds  (r3.removeFromLeft(kw));

        // ── Amplitude Envelope ────────────────────────────────────────
        b.removeFromTop(6);             // gap, cumulative=278
        b.removeFromTop(14);            // label, cumulative=292
        auto r4 = b;                    // remaining ≈ 89px
        auto envRight = r4.removeFromRight(148);
        envDisplay_.setBounds(envRight.reduced(0, 8));
        for (auto* k : { &atkK_, &decK_, &susK_, &relK_ })
            k->setBounds(r4.removeFromLeft(kw));
    }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    static std::unique_ptr<SA> mkSA(juce::AudioProcessorValueTreeState& a,
                                    const char* id, juce::Slider& s) {
        return std::make_unique<SA>(a, id, s);
    }

    void setMode(int idx) {
        if (auto* p = apvts_.getParameter("voice_mode"))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(float(idx)));
        updateModeButtons(idx);
    }

    void updateModeButtons(int active) {
        static const juce::Colour kCols[4] = {
            Pal::Gold, Pal::Mint, Pal::Purple, Pal::Teal
        };
        for (int i = 0; i < 4; ++i) {
            bool on = (i == active);
            modeBtn_[i].setColour(juce::TextButton::buttonColourId,
                                  on ? kCols[i].withAlpha(0.25f) : Pal::Raised);
            modeBtn_[i].setColour(juce::TextButton::textColourOffId,
                                  on ? kCols[i] : Pal::Mid);
        }
        repaint();
    }

    juce::AudioProcessorValueTreeState& apvts_;

    juce::TextButton modeBtn_[4];
    juce::ComboBox   voicesCb_, intervalCb_;
    Knob breatheK_, roughK_, genderK_;
    Knob depthK_, mixK_, spreadK_, driftK_;
    Knob atkK_, decK_, susK_, relK_;
    EnvDisplay envDisplay_;
    XYPad      xyPad_;

    std::unique_ptr<CA> voicesCbA_, intervalCbA_;
    std::unique_ptr<SA> breatheA_, roughA_, genderA_;
    std::unique_ptr<SA> depthA_, mixA_, spreadA_, driftA_;
    std::unique_ptr<SA> atkA_, decA_, susA_, relA_;

    void updateEnv() {
        envDisplay_.set(float(atkK_.slider.getValue()), float(decK_.slider.getValue()),
                        float(susK_.slider.getValue()), float(relK_.slider.getValue()));
    }
};
