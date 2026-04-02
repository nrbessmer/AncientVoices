#pragma once
#include <JuceHeader.h>
#include "UI.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ANCIENT PANEL
//  ERA selector with descriptive text and glyph.
//  No ritual parameter — removed in effect conversion.
// ─────────────────────────────────────────────────────────────────────────────

class AncientPanel : public juce::Component, private juce::ComboBox::Listener {
public:
    explicit AncientPanel(juce::AudioProcessorValueTreeState& apvts) {
        for (const char* e : {"Sumerian","Egyptian","Byzantine","Vedic"})
            eraCb_.addItem(e, eraCb_.getNumItems() + 1);
        eraCb_.addListener(this);
        addAndMakeVisible(eraCb_);
        addAndMakeVisible(eraDesc_);
        addAndMakeVisible(eraGlyph_);

        eraDesc_.setFont(Fonts::body(12.f));
        eraDesc_.setColour(juce::Label::textColourId, Pal::TextDim);
        eraDesc_.setJustificationType(juce::Justification::topLeft);
        eraDesc_.setMinimumHorizontalScale(0.8f);

        eraGlyph_.setFont(juce::Font(juce::FontOptions().withHeight(36.f)));
        eraGlyph_.setColour(juce::Label::textColourId, Pal::Gold.withAlpha(0.5f));
        eraGlyph_.setJustificationType(juce::Justification::centred);

        eraA_ = std::make_unique<CA>(apvts, "era", eraCb_);
        updateEraText(0);
    }

    ~AncientPanel() override {
        eraCb_.removeListener(this);
    }

    void paint(juce::Graphics& g) override {
        AncientLAF::bg(g, getLocalBounds());
        AncientLAF::sectionLabel(g, {12, 8, 60, 14}, "Era");

        auto b = getLocalBounds().toFloat();
        g.setColour(Pal::Border);
        g.drawHorizontalLine(200, b.getX() + 12, b.getRight() - 12);
    }

    void resized() override {
        auto b = getLocalBounds().reduced(12, 8);
        b.removeFromTop(16);

        auto r1 = b.removeFromTop(26);
        eraCb_.setBounds(r1.removeFromLeft(140).reduced(0, 3));

        b.removeFromTop(8);

        auto descRow = b.removeFromTop(160);
        eraGlyph_.setBounds(descRow.removeFromLeft(70));
        eraDesc_.setBounds(descRow.reduced(4, 0));
    }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    juce::ComboBox eraCb_;
    juce::Label    eraDesc_, eraGlyph_;
    std::unique_ptr<CA> eraA_;

    void comboBoxChanged(juce::ComboBox* cb) override {
        if (cb == &eraCb_) updateEraText(eraCb_.getSelectedItemIndex());
    }

    void updateEraText(int idx) {
        static const char* kGlyphs[] = { "𒀭", "𓂀", "☧", "ॐ" };
        static const char* kDescs[]  = {
            "Sumerian vocal tradition\n(c. 3000 BCE). Open chest\nresonance, sacred ritual\n"
            "chant of Mesopotamia.\nWide vowels, drone bass.",

            "Egyptian ceremonial voice\n(c. 2500 BCE). Forward nasal\nplacement, bright upper\n"
            "harmonics. Temple liturgy\nand hymns to the gods.",

            "Byzantine choral tradition\n(c. 400 CE). Warm chest-mix,\nliturgical resonance.\n"
            "Multi-voice heterophony,\nmodal harmony.",

            "Vedic chant (c. 1500 BCE).\nThroat resonance, Sanskrit\nvowel articulation.\n"
            "Overtone emphasis,\ntamboura drone style.",
        };
        eraGlyph_.setText(kGlyphs[juce::jlimit(0,3,idx)], juce::dontSendNotification);
        eraDesc_.setText (kDescs [juce::jlimit(0,3,idx)], juce::dontSendNotification);
        repaint();
    }
};
