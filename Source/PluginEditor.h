#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

#define ANCIENT_VOICES_MINIMAL_EDITOR 0

#if ANCIENT_VOICES_MINIMAL_EDITOR

class AncientVoicesEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int kW = 400, kH = 200;
    explicit AncientVoicesEditor(AncientVoicesProcessor& p) : AudioProcessorEditor(&p)
    { setSize(kW, kH); setOpaque(true); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xFFE8B84B));
        g.setColour(juce::Colours::black); g.setFont(18.f);
        g.drawText("Ancient Voices — MINIMAL TEST EDITOR", getLocalBounds(), juce::Justification::centred);
    }
    void resized() override {}
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesEditor)
};

#else

#include "UI/UI.h"
#include "UI/FormantScope.h"
#include "UI/VoicePanel.h"
#include "UI/AncientPanel.h"
#include "UI/FXPanel.h"
#include "Data/Presets.h"

// ─────────────────────────────────────────────────────────────────────────────
//  DIAGNOSTIC OVERLAY
//
//  CONTRACT:
//   - Collapsed: 24×24 "D" button (gold on navy). Positioned by editor's
//     resized() which allocates 30px from the right of the header bar before
//     placing the nav buttons — no overlap.
//   - Expanded:  280×320 floating panel, dark surface, monospaced text.
//     Self-repositions via setBounds() on toggle.
//   - Timer: 10 Hz, self-managed. Separate from editor's 30 Hz scope timer.
//   - No allocation in timerCallback or paint.
//
//  PANEL LAYOUT (expanded, 280×344 total):
//    Row 0  "ANCIENT VOICES DIAGNOSTICS"     header
//    ──────────────────────────────────────  separator
//    Row 1  Limiter GR          [value] [C1 PASS/FAIL]
//    Row 2  Wet RMS (step 5)    [value]
//    Row 3  Dry RMS (step 1)    [value]
//    Row 4  Wet-dry diff        [value] [C2 PASS/FAIL]
//    Row 5  Output RMS          [value]
//    Row 6  Dry leak (C3)       [value] [C3 PASS/FAIL]
//    ──────────────────────────────────────  separator
//    Row 7  Mix param           [value]
//    Row 8  Voice count         [value]
//    Row 9  Active copies       [value]
//    Row 10 NormGain min        [value] [C4 PASS/FAIL]
//    Row 11 NormGain max        [value]
//    Row 12 Block / SR          [value]
//    ──────────────────────────────────────  separator
//    Row 13 NaN/Inf detected    [status][C5 PASS/FAIL]
//    Row 14 Note: C2/C3 suspended if C5 fails
// ─────────────────────────────────────────────────────────────────────────────
class DiagnosticOverlay : public juce::Component, private juce::Timer
{
public:
    static constexpr int kPanelW   = 280;
    static constexpr int kPanelH   = 344;
    static constexpr int kBtnSz    = 24;
    static constexpr int kSliceW   = 30;   // header space reserved by editor's resized()

    explicit DiagnosticOverlay(AncientVoicesProcessor& p) : proc_(p)
    {
        toggleBtn_.setButtonText("D");
        toggleBtn_.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF0A1628));
        toggleBtn_.setColour(juce::TextButton::textColourOffId, Pal::Gold);
        toggleBtn_.setColour(juce::TextButton::textColourOnId,  Pal::Gold);
        toggleBtn_.onClick = [this] { toggle(); };
        addAndMakeVisible(toggleBtn_);
        startTimerHz(10);
    }
    ~DiagnosticOverlay() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        if (!expanded_) return;

        // Panel background
        g.setColour(juce::Colour(0xF20D1117));
        g.fillRoundedRectangle(0.f, float(kBtnSz), float(kPanelW), float(kPanelH), 6.f);
        g.setColour(Pal::Primary.withAlpha(0.45f));
        g.drawRoundedRectangle(0.5f, float(kBtnSz) + 0.5f,
                               float(kPanelW) - 1.f, float(kPanelH) - 1.f, 6.f, 1.f);

        const auto& ds = proc_.getDiagState();

        // Cache all atomic reads up front (one load each, UI thread)
        const float limGain    = ds.limiterGainDb.load();
        const float wetRms     = ds.wetRmsDb.load();
        const float dryRms     = ds.dryRmsDb.load();
        const float outRms     = ds.outputRmsDb.load();
        const float wdDiff     = ds.wetDryDiffDb.load();
        const float mix        = ds.mixRatio.load();
        const int   voices     = int(ds.voiceCount.load());
        const int   copies     = int(ds.copyCount.load());
        const float ngMin      = ds.normGainMin.load();
        const float ngMax      = ds.normGainMax.load();
        const float sr         = ds.sampleRateHz.load();
        const int   bs         = int(ds.blockSizeSmps.load());
        const float leakDb     = ds.dryLeakDb.load();
        const float limBlocks  = ds.limiterConsecBlocks.load();

        const bool C1 = ds.C1_limiterIdle.load();
        const bool C2 = ds.C2_wetWithin6dB.load();
        const bool C3 = ds.C3_noDryLeak.load();
        const bool C4 = ds.C4_normGainSane.load();
        const bool C5 = ds.C5_noNanInf.load();

        // Colours
        const juce::Colour kPass = Pal::Mint;
        const juce::Colour kFail = Pal::Coral;
        const juce::Colour kDim  = Pal::Low;
        const juce::Colour kVal  = Pal::Amber;
        const juce::Colour kHdr  = Pal::Primary;

        const int kTop  = kBtnSz + 8;
        const int kRow  = 22;
        int y = kTop;

        // Title
        g.setFont(Fonts::label(10.f));
        g.setColour(kHdr);
        g.drawText("ANCIENT VOICES DIAGNOSTICS", 8, y, kPanelW - 16, 16,
                   juce::Justification::centredLeft);
        y += 20;
        sep(g, y); y += 6;

        // Helper lambdas
        auto val = [&](const juce::String& label, const juce::String& value,
                       bool showContract, bool contractPass)
        {
            g.setFont(Fonts::mono(10.f));
            g.setColour(kDim);
            g.drawText(label, 8, y, 136, 18, juce::Justification::centredLeft);
            g.setColour(kVal);
            g.drawText(value, 144, y, 80, 18, juce::Justification::centredLeft);
            if (showContract) {
                g.setColour(contractPass ? kPass : kFail);
                g.drawText(contractPass ? "PASS" : "FAIL",
                           228, y, 44, 18, juce::Justification::centredLeft);
            }
            y += kRow;
        };

        // ── Signal level section ──────────────────────────────────────────
        // C1: Limiter idle — non-zero GR means gain staging problem
        val("Limiter GR",
            juce::String(limGain, 1) + " dB  (" + juce::String(int(limBlocks)) + " blk)",
            true, C1);

        val("Wet RMS (step 5)", juce::String(wetRms, 1) + " dBFS", false, false);
        val("Dry RMS (step 1)", juce::String(dryRms, 1) + " dBFS", false, false);

        // C2: Wet within 6 dB — simulation baseline -1.8 dB
        val("Wet-dry diff",
            juce::String(wdDiff, 1) + " dB",
            true, C2);

        val("Output RMS",  juce::String(outRms, 1) + " dBFS", false, false);

        // C3: No dry in wet path at mix=1
        val("Dry leak (mix=1)",
            juce::String(leakDb, 2) + " dB",
            mix > 0.99f, C3);

        sep(g, y); y += 6;

        // ── Parameter / voice section ─────────────────────────────────────
        val("Mix param",       juce::String(mix, 2),     false, false);
        val("Voice count",     juce::String(voices),     false, false);
        val("Active copies",   juce::String(copies),     false, false);

        // C4: NormGain sane — simulation settles to 2.7–4.0x on vocal content
        val("NormGain min",    juce::String(ngMin, 2) + "x", false, false);
        val("NormGain max",    juce::String(ngMax, 2) + "x", true,  C4);

        val("Block / SR",
            juce::String(bs) + " / " + juce::String(int(sr)) + " Hz",
            false, false);

        sep(g, y); y += 6;

        // ── C5: NaN/Inf sticky ────────────────────────────────────────────
        val("NaN/Inf detected",
            C5 ? "clean" : "YES (sticky)",
            true, C5);

        if (!C5) {
            g.setFont(Fonts::mono(9.f));
            g.setColour(kFail.withAlpha(0.8f));
            g.drawText("C2 + C3 suspended (NaN in dry path)",
                       8, y, kPanelW - 16, 14, juce::Justification::centredLeft);
            y += 16;
        }

        // Footer
        g.setFont(Fonts::mono(9.f));
        g.setColour(kDim.withAlpha(0.6f));
        g.drawText("Reload plugin to clear NaN flag.",
                   8, y, kPanelW - 16, 14, juce::Justification::centredLeft);
    }

    void resized() override
    {
        toggleBtn_.setBounds(expanded_ ? kPanelW - kBtnSz : 0, 0, kBtnSz, kBtnSz);
    }

private:
    AncientVoicesProcessor& proc_;
    juce::TextButton        toggleBtn_;
    bool                    expanded_ = false;

    void sep(juce::Graphics& g, int y) const {
        g.setColour(Pal::Border.withAlpha(0.4f));
        g.fillRect(8, y, kPanelW - 16, 1);
    }

    void timerCallback() override { if (expanded_) repaint(); }

    void toggle()
    {
        expanded_ = !expanded_;
        if (auto* p = getParentComponent()) {
            if (expanded_)
                // Expand leftward from the button position — stays within editor
                setBounds(p->getWidth() - kPanelW - 4,
                          14,
                          kPanelW,
                          kBtnSz + kPanelH);
            else
                // Collapse back to the slot the editor allocated in resized()
                setBounds(p->getWidth() - kSliceW + (kSliceW - kBtnSz) / 2,
                          14,
                          kBtnSz,
                          kBtnSz);
        }
        resized();
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiagnosticOverlay)
};

// ─────────────────────────────────────────────────────────────────────────────
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
                {"Chant",Pal::Primary},{"Drone",Pal::Cyan},{"Choir",Pal::Mint},
                {"Ritual",Pal::Coral},{"Breath",Pal::Mid},{"Shimmer",Pal::Gold},
                {"Dark",juce::Colour(0xFF8855BBu)},
            };
            juce::Colour accent = Pal::Mid;
            for (auto& cc : kCatCols) if (cc.cat==catName_) { accent=cc.col; break; }
            g.setColour(isOpen()?Pal::Raised:Pal::Surface); g.fillRect(0,0,w,h);
            g.setColour(accent); g.fillRect(0,0,3,h);
            float tx=10.f,ty=h*.5f; juce::Path tri;
            if (isOpen()) tri.addTriangle(tx,ty-3.5f,tx+7.f,ty-3.5f,tx+3.5f,ty+3.f);
            else          tri.addTriangle(tx,ty-4.5f,tx+5.f,ty,tx,ty+4.5f);
            g.setColour(accent); g.fillPath(tri);
            g.setFont(Fonts::label(10.5f));
            g.setColour(isOpen()?Pal::High:Pal::Mid);
            g.drawText(catName_.toUpperCase(),24,0,w-26,h,juce::Justification::centredLeft);
        } else {
            bool sel=isSelected();
            if (sel) {
                g.setColour(Pal::Primary.withAlpha(.15f));
                g.fillRoundedRectangle(4.f,1.f,float(w)-8.f,float(h)-2.f,4.f);
                g.setColour(Pal::Primary.withAlpha(.4f));
                g.drawRoundedRectangle(4.5f,1.5f,float(w)-9.f,float(h)-3.f,4.f,1.f);
            }
            g.setFont(Fonts::body(12.f));
            g.setColour(sel?Pal::Bright:Pal::Mid);
            g.drawText(catName_,22,0,w-26,h,juce::Justification::centredLeft);
        }
    }
    void itemClicked(const juce::MouseEvent&) override { if (isCat_) setOpen(!isOpen()); }
private:
    bool isCat_; juce::String catName_; int presetIdx_;
};

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
    AVPresetManager&        presetMgr_;

    juce::Label      titleLabel_, presetNameLabel_, presetCatLabel_;
    juce::TextButton prevBtn_{"<"}, nextBtn_{">"};

    juce::TreeView   presetTree_;
    PresetTreeItem   treeRoot_{ juce::String("Root") };
    void buildTree();

    juce::TabbedComponent tabs_{ juce::TabbedButtonBar::TabsAtTop };
    VoicePanel   voicePanel_;
    AncientPanel ancientPanel_;
    AVFXPanel    fxPanel_;

    FormantScope formantScope_;
    juce::AudioBuffer<float> scopeSnap_;

    // Diagnostic overlay — self-manages 10 Hz timer, z-ordered on top
    DiagnosticOverlay diagOverlay_;

    void timerCallback() override;
    void buttonClicked(juce::Button*) override;
    void loadPreset(int idx);
    void updatePresetLabel();
    void selectCurrentInTree();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesEditor)
};

#endif
