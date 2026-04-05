#pragma once
#include <JuceHeader.h>
#include "UI.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FX PANEL
//  Mix:         WET/DRY master blend
//  Cave Reverb: MIX, SIZE, DAMP, ECHO (pre-delay)
//  Shimmer:     MIX, AMOUNT
//  Master:      VOLUME, GAIN
// ─────────────────────────────────────────────────────────────────────────────

class AVFXPanel : public juce::Component {
public:
    explicit AVFXPanel(juce::AudioProcessorValueTreeState& apvts)
        : mixK_     ("MIX",    Pal::Primary),
          caveMixK_ ("MIX",    Pal::Purple),
          caveSzK_  ("SIZE",   Pal::Purple),
          caveDmK_  ("DAMP",   Pal::Purple),
          caveEchoK_("ECHO",   Pal::Purple),
          shimMixK_ ("MIX",    Pal::Teal),
          shimAmtK_ ("SHIMMER",Pal::Teal),
          volK_     ("VOLUME", Pal::Amber),
          gainK_    ("GAIN",   Pal::Amber)
    {
        for (auto* k : { &mixK_,
                         &caveMixK_, &caveSzK_, &caveDmK_, &caveEchoK_,
                         &shimMixK_, &shimAmtK_, &volK_, &gainK_ })
            addAndMakeVisible(k);

        mixA_      = mkSA(apvts, "mix",         mixK_.slider);
        caveMixA_  = mkSA(apvts, "cave_mix",    caveMixK_.slider);
        caveSzA_   = mkSA(apvts, "cave_size",   caveSzK_.slider);
        caveDmA_   = mkSA(apvts, "cave_damp",   caveDmK_.slider);
        caveEchoA_ = mkSA(apvts, "cave_echo",   caveEchoK_.slider);
        shimMixA_  = mkSA(apvts, "shimmer_mix", shimMixK_.slider);
        shimAmtA_  = mkSA(apvts, "shimmer_amt", shimAmtK_.slider);
        volA_      = mkSA(apvts, "master_vol",  volK_.slider);
        gainA_     = mkSA(apvts, "master_gain", gainK_.slider);
    }

    void paint(juce::Graphics& g) override {
        AncientLAF::bg(g, getLocalBounds());
        AncientLAF::sectionLabel(g, {12, 8,   100, 14}, "Wet / Dry");
        AncientLAF::sectionLabel(g, {12, 102, 100, 14}, "Cave Reverb");
        AncientLAF::sectionLabel(g, {12, 196, 100, 14}, "Shimmer");
        AncientLAF::sectionLabel(g, {12, 290, 80,  14}, "Master");
    }

    void resized() override {
        auto b  = getLocalBounds().reduced(12, 8);
        int  kw = 56;

        // ── Wet/Dry ───────────────────────────────────────────────────────
        b.removeFromTop(16);
        auto r0 = b.removeFromTop(70);
        mixK_.setBounds(r0.removeFromLeft(kw));

        // ── Cave Reverb ───────────────────────────────────────────────────
        b.removeFromTop(12);
        b.removeFromTop(14);
        auto r1 = b.removeFromTop(70);
        caveMixK_.setBounds  (r1.removeFromLeft(kw));
        caveSzK_.setBounds   (r1.removeFromLeft(kw));
        caveDmK_.setBounds   (r1.removeFromLeft(kw));
        caveEchoK_.setBounds (r1.removeFromLeft(kw));

        // ── Shimmer ───────────────────────────────────────────────────────
        b.removeFromTop(12);
        b.removeFromTop(14);
        auto r2 = b.removeFromTop(70);
        shimMixK_.setBounds(r2.removeFromLeft(kw));
        shimAmtK_.setBounds(r2.removeFromLeft(kw));

        // ── Master ────────────────────────────────────────────────────────
        b.removeFromTop(12);
        b.removeFromTop(14);
        auto r3 = b.removeFromTop(70);
        volK_.setBounds (r3.removeFromLeft(kw));
        gainK_.setBounds(r3.removeFromLeft(kw));
    }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    static std::unique_ptr<SA> mkSA(juce::AudioProcessorValueTreeState& a,
                                    const char* id, juce::Slider& s) {
        return std::make_unique<SA>(a, id, s);
    }

    Knob mixK_;
    Knob caveMixK_, caveSzK_, caveDmK_, caveEchoK_;
    Knob shimMixK_, shimAmtK_;
    Knob volK_, gainK_;

    std::unique_ptr<SA> mixA_;
    std::unique_ptr<SA> caveMixA_, caveSzA_, caveDmA_, caveEchoA_;
    std::unique_ptr<SA> shimMixA_, shimAmtA_;
    std::unique_ptr<SA> volA_, gainA_;
};
