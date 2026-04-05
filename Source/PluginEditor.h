#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  EDITOR MODE TOGGLE
//  1 = minimal gold rectangle  (confirms AU Cocoa view factory works in Logic)
//  0 = full Ancient Voices UI
//  Change the value, rebuild.
// ─────────────────────────────────────────────────────────────────────────────
#define ANCIENT_VOICES_MINIMAL_EDITOR 0

// ─────────────────────────────────────────────────────────────────────────────
#if ANCIENT_VOICES_MINIMAL_EDITOR

class AncientVoicesEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int kW = 400;
    static constexpr int kH = 200;

    explicit AncientVoicesEditor(AncientVoicesProcessor& p)
        : AudioProcessorEditor(&p)
    {
        setSize(kW, kH);
        setOpaque(true);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFFE8B84B));
        g.setColour(juce::Colours::black);
        g.setFont(18.f);
        g.drawText("Ancient Voices — MINIMAL TEST EDITOR",
                   getLocalBounds(), juce::Justification::centred);
    }

    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesEditor)
};

// ─────────────────────────────────────────────────────────────────────────────
#else  // full editor

#include "UI/UI.h"
#include "UI/FormantScope.h"
#include "UI/VoicePanel.h"
#include "UI/AncientPanel.h"
#include "UI/FXPanel.h"
#include "Data/Presets.h"

class PresetTreeItem : public juce::TreeViewItem {
public:
    explicit PresetTreeItem(const juce::String& catName)
        : isCat_(true), catName_(catName), presetIdx_(-1) {}

    PresetTreeItem(const juce::String& name, int idx)
        : isCat_(false), catName_(name), presetIdx_(idx) {}

    bool  mightContainSubItems() override { return isCat_; }
    int   getItemHeight() const override  { return isCat_ ? 22 : 20; }
    bool  isCategoryHeader() const        { return isCat_; }
    int   presetIndex()      const        { return presetIdx_; }
    const juce::String& label() const     { return catName_; }

    void paintItem(juce::Graphics& g, int w, int h) override {
        if (isCat_) {
            struct CatCol { juce::String cat; juce::Colour col; };
            static const CatCol kCatCols[] = {
                { "Chant",   Pal::Primary },
                { "Drone",   Pal::Cyan    },
                { "Choir",   Pal::Mint    },
                { "Ritual",  Pal::Coral   },
                { "Breath",  Pal::Mid     },
                { "Shimmer", Pal::Gold    },
                { "Dark",    juce::Colour(0xFF8855BBu) },
            };
            juce::Colour accent = Pal::Mid;
            for (auto& cc : kCatCols)
                if (cc.cat == catName_) { accent = cc.col; break; }

            g.setColour(isOpen() ? Pal::Raised : Pal::Surface);
            g.fillRect(0, 0, w, h);
            g.setColour(accent);
            g.fillRect(0, 0, 3, h);

            float tx = 10.f, ty = h * .5f;
            juce::Path tri;
            if (isOpen()) tri.addTriangle(tx, ty - 3.5f, tx + 7.f, ty - 3.5f, tx + 3.5f, ty + 3.f);
            else          tri.addTriangle(tx, ty - 4.5f, tx + 5.f, ty, tx, ty + 4.5f);
            g.setColour(accent); g.fillPath(tri);

            g.setFont(Fonts::label(10.5f));
            g.setColour(isOpen() ? Pal::High : Pal::Mid);
            g.drawText(catName_.toUpperCase(), 24, 0, w - 26, h, juce::Justification::centredLeft);
        } else {
            bool sel = isSelected();
            if (sel) {
                g.setColour(Pal::Primary.withAlpha(.15f));
                g.fillRoundedRectangle(4.f, 1.f, float(w) - 8.f, float(h) - 2.f, 4.f);
                g.setColour(Pal::Primary.withAlpha(.4f));
                g.drawRoundedRectangle(4.5f, 1.5f, float(w) - 9.f, float(h) - 3.f, 4.f, 1.f);
            }
            g.setFont(Fonts::body(12.f));
            g.setColour(sel ? Pal::Bright : Pal::Mid);
            g.drawText(catName_, 22, 0, w - 26, h, juce::Justification::centredLeft);
        }
    }

    void itemClicked(const juce::MouseEvent&) override {
        if (isCat_) setOpen(!isOpen());
    }

private:
    bool         isCat_;
    juce::String catName_;
    int          presetIdx_;
};

class AncientVoicesEditor : public juce::AudioProcessorEditor,
                             private juce::Timer,
                             private juce::Button::Listener
{
public:
    static constexpr int kW = 860;
    static constexpr int kH = 580;

    explicit AncientVoicesEditor(AncientVoicesProcessor&);
    ~AncientVoicesEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AncientVoicesProcessor& proc_;
    AncientLAF              laf_;
    AVPresetManager&        presetMgr_;   // reference to processor's instance — stays in sync with DAW

    juce::Label      titleLabel_, presetNameLabel_, presetCatLabel_;
    juce::TextButton prevBtn_{"<"}, nextBtn_{">"};

    juce::TreeView   presetTree_;
    PresetTreeItem   treeRoot_ { juce::String("Root") };
    void buildTree();

    juce::TabbedComponent tabs_{ juce::TabbedButtonBar::TabsAtTop };
    VoicePanel   voicePanel_;
    AncientPanel ancientPanel_;
    AVFXPanel    fxPanel_;

    FormantScope formantScope_;
    juce::AudioBuffer<float> scopeSnap_;

    void timerCallback() override;
    void buttonClicked(juce::Button*) override;
    void loadPreset(int idx);
    void updatePresetLabel();
    void selectCurrentInTree();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesEditor)
};

#endif  // ANCIENT_VOICES_MINIMAL_EDITOR
