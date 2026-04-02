#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/UI.h"
#include "UI/FormantScope.h"
#include "UI/VoicePanel.h"
#include "UI/AncientPanel.h"
#include "UI/FXPanel.h"
#include "Data/Presets.h"

// ─────────────────────────────────────────────────────────────────────────────
//  PRESET TREE ITEM  — either a category header or a leaf preset
// ─────────────────────────────────────────────────────────────────────────────
class PresetTreeItem : public juce::TreeViewItem {
public:
    // Category header
    explicit PresetTreeItem(const juce::String& catName)
        : isCat_(true), catName_(catName), presetIdx_(-1) {}

    // Leaf preset
    PresetTreeItem(const juce::String& name, int idx)
        : isCat_(false), catName_(name), presetIdx_(idx) {}

    bool  mightContainSubItems() override { return isCat_; }
    int   getItemHeight() const override  { return isCat_ ? 22 : 20; }
    bool  isCategoryHeader() const        { return isCat_; }
    int   presetIndex()      const        { return presetIdx_; }
    const juce::String& label() const     { return catName_; }

    void paintItem(juce::Graphics& g, int w, int h) override {
        if (isCat_) {
            // Per-category accent colour
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

            // Left accent bar
            g.setColour(accent);
            g.fillRect(0, 0, 3, h);

            // Triangle
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
    bool        isCat_;
    juce::String catName_;
    int         presetIdx_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  EDITOR
// ─────────────────────────────────────────────────────────────────────────────
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
    AVPresetManager         presetMgr_;

    // ── Header ────────────────────────────────────────────────────────────
    juce::Label      titleLabel_, presetNameLabel_, presetCatLabel_;
    juce::TextButton prevBtn_{"<"}, nextBtn_{">"};

    // ── Preset tree browser ───────────────────────────────────────────────
    juce::TreeView   presetTree_;
    PresetTreeItem   treeRoot_ { juce::String("Root") };
    void buildTree();

    // ── Tab panels ────────────────────────────────────────────────────────
    juce::TabbedComponent tabs_{ juce::TabbedButtonBar::TabsAtTop };
    VoicePanel   voicePanel_;
    AncientPanel ancientPanel_;
    AVFXPanel    fxPanel_;

    // ── Formant scope ─────────────────────────────────────────────────────
    FormantScope formantScope_;
    juce::AudioBuffer<float> scopeSnap_;

    // ── Timer ─────────────────────────────────────────────────────────────
    void timerCallback() override;

    // ── Button listener ───────────────────────────────────────────────────
    void buttonClicked(juce::Button*) override;

    // ── Helpers ───────────────────────────────────────────────────────────
    void loadPreset(int idx);
    void updatePresetLabel();
    void selectCurrentInTree();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesEditor)
};
