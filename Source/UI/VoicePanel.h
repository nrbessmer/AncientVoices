#pragma once
#include <JuceHeader.h>
#include "UI.h"

// ─────────────────────────────────────────────────────────────────────────────
//  VOICE PANEL
//  Contains:
//    - XY Pad: X = Vowel Position (A→U), Y = Openness
//    - BREATHE, ROUGHNESS, GENDER knobs
//    - VOICES, SPREAD, DRIFT knobs
//    - INTERVAL combo
//    - Amplitude ADSR + EnvDisplay
// ─────────────────────────────────────────────────────────────────────────────

class VoicePanel : public juce::Component, private juce::Slider::Listener {
public:
    explicit VoicePanel(juce::AudioProcessorValueTreeState& apvts)
        : breatheK_("BREATHE", Pal::Amber),
          roughK_  ("ROUGH",   Pal::Amber),
          genderK_ ("GENDER",  Pal::Amber),
          spreadK_ ("SPREAD",  Pal::Sage),
          driftK_  ("DRIFT",   Pal::Sage),
          atkK_    ("ATTACK"),
          decK_    ("DECAY"),
          susK_    ("SUSTAIN"),
          relK_    ("RELEASE")
    {
        // Voices combo
        for (int i = 1; i <= 8; ++i)
            voicesCb_.addItem(juce::String(i) + (i==1?" voice":" voices"), i);

        // Interval combo — matches IntervalTables kIntervalNames
        for (const char* n : {"Unison","Fifth","Octave","Sacred","Just Triad","Ancient","Drone"})
            intervalCb_.addItem(n, intervalCb_.getNumItems() + 1);

        addAndMakeVisible(voicesCb_);
        addAndMakeVisible(intervalCb_);

        for (auto* k : { &breatheK_, &roughK_, &genderK_, &spreadK_, &driftK_,
                         &atkK_, &decK_, &susK_, &relK_ })
            addAndMakeVisible(k);

        addAndMakeVisible(envDisplay_);
        addAndMakeVisible(xyPad_);

        // XY pad — vowel_pos (0–4) on X, vowel_open (0.5–1.5) on Y
        xyPad_.bind(apvts, "vowel_pos", "vowel_open",
                    0.f, 4.f,    // vowel_pos range
                    0.5f, 1.5f); // vowel_open range
        xyPad_.setLabels("VOWEL", "OPEN");

        // Attachments
        voicesCbA_   = std::make_unique<CA>(apvts, "num_voices",  voicesCb_);
        intervalCbA_ = std::make_unique<CA>(apvts, "interval",    intervalCb_);

        breatheA_ = mkSA(apvts, "breathe",      breatheK_.slider);
        roughA_   = mkSA(apvts, "roughness",    roughK_.slider);
        genderA_  = mkSA(apvts, "gender",       genderK_.slider);
        spreadA_  = mkSA(apvts, "spread",       spreadK_.slider);
        driftA_   = mkSA(apvts, "drift",        driftK_.slider);
        atkA_     = mkSA(apvts, "env_attack",   atkK_.slider);
        decA_     = mkSA(apvts, "env_decay",    decK_.slider);
        susA_     = mkSA(apvts, "env_sustain",  susK_.slider);
        relA_     = mkSA(apvts, "env_release",  relK_.slider);

        for (auto* k : { &atkK_, &decK_, &susK_, &relK_ })
            k->slider.addListener(this);
        updateEnv();
    }

    ~VoicePanel() override {
        for (auto* k : { &atkK_, &decK_, &susK_, &relK_ })
            k->slider.removeListener(this);
    }

    void sliderValueChanged(juce::Slider*) override { updateEnv(); }

    void paint(juce::Graphics& g) override {
        AncientLAF::bg(g, getLocalBounds());
        AncientLAF::sectionLabel(g, {12, 8, 120, 14},  "Voice Character");
        AncientLAF::sectionLabel(g, {12, 148, 80, 14}, "Choir");
        AncientLAF::sectionLabel(g, {12, 232, 100, 14},"Amplitude Envelope");
    }

    void resized() override {
        auto b  = getLocalBounds().reduced(12, 8);
        int  kw = 54;

        // ── Voice Character section ───────────────────────────────────────
        b.removeFromTop(16);

        // XY Pad (left, 140x140) + 3 knobs (right, each 54w x 70h = 210px total)
        // Must allocate at least 210px height to fit all 3 knobs
        auto r1 = b.removeFromTop(220);
        xyPad_.setBounds(r1.removeFromLeft(140).removeFromTop(140));
        r1.removeFromLeft(8);
        // Knob column: height 220 is enough for 3 * 70 = 210px
        breatheK_.setBounds(r1.removeFromTop(kw + 16).removeFromLeft(kw));
        roughK_.setBounds  (r1.removeFromTop(kw + 16).removeFromLeft(kw));
        genderK_.setBounds (r1.removeFromTop(kw + 16).removeFromLeft(kw));

        // ── Choir section ─────────────────────────────────────────────────
        b.removeFromTop(10);
        b.removeFromTop(14);

        auto r2 = b.removeFromTop(26);
        voicesCb_.setBounds  (r2.removeFromLeft(110).reduced(0, 3));
        r2.removeFromLeft(8);
        intervalCb_.setBounds(r2.removeFromLeft(120).reduced(0, 3));

        b.removeFromTop(6);
        auto r3 = b.removeFromTop(70);
        spreadK_.setBounds(r3.removeFromLeft(kw));
        r3.removeFromLeft(4);
        driftK_.setBounds (r3.removeFromLeft(kw));

        // ── Envelope section ──────────────────────────────────────────────
        b.removeFromTop(10);
        b.removeFromTop(14);

        auto r4 = b.removeFromTop(82);
        auto envRight = r4.removeFromRight(160);
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

    juce::ComboBox voicesCb_, intervalCb_;
    Knob breatheK_, roughK_, genderK_, spreadK_, driftK_;
    Knob atkK_, decK_, susK_, relK_;
    EnvDisplay envDisplay_;
    XYPad      xyPad_;

    std::unique_ptr<CA> voicesCbA_, intervalCbA_;
    std::unique_ptr<SA> breatheA_, roughA_, genderA_, spreadA_, driftA_;
    std::unique_ptr<SA> atkA_, decA_, susA_, relA_;

    void updateEnv() {
        envDisplay_.set(float(atkK_.slider.getValue()), float(decK_.slider.getValue()),
                        float(susK_.slider.getValue()), float(relK_.slider.getValue()));
    }
};
