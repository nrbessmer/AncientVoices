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
//  ANCIENT VOICES EDITOR
//  Layout:
//    Left column  : Preset browser (collapsible)
//    Center/Right : Tab strip [VOICE | ANCIENT | FX]
//                   FormantScope (always visible, bottom of tabs area)
//    Header       : Title, preset name, prev/next buttons
// ─────────────────────────────────────────────────────────────────────────────

class AncientVoicesEditor : public juce::AudioProcessorEditor,
                             public juce::ListBoxModel,
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

    // ── Preset browser ────────────────────────────────────────────────────
    juce::ListBox    browserList_;
    juce::OwnedArray<juce::TextButton> catButtons_;
    juce::String     selectedCat_{"All"};
    struct BrowserRow { int presetIdx; juce::String label; juce::String cat; };
    std::vector<BrowserRow> browserRows_;
    void rebuildBrowser();

    // ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool sel) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

    // ── Tab panels ────────────────────────────────────────────────────────
    juce::TabbedComponent tabs_{ juce::TabbedButtonBar::TabsAtTop };
    VoicePanel   voicePanel_;
    AncientPanel ancientPanel_;
    AVFXPanel    fxPanel_;

    // ── Formant scope (always visible) ───────────────────────────────────
    FormantScope formantScope_;
    juce::AudioBuffer<float> scopeSnap_;  // reused buffer, avoids heap alloc in timer

    // ── Timer: push scope data and refresh ───────────────────────────────
    void timerCallback() override;

    // ── Button listener ───────────────────────────────────────────────────
    void buttonClicked(juce::Button*) override;

    // ── Helpers ───────────────────────────────────────────────────────────
    void loadPreset(int idx);
    void updatePresetLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesEditor)
};
