#include "PluginEditor.h"

AncientVoicesEditor::AncientVoicesEditor(AncientVoicesProcessor& p)
    : AudioProcessorEditor(&p),
      proc_(p),
      voicePanel_  (p.apvts),
      ancientPanel_(p.apvts),
      fxPanel_     (p.apvts)
{
    // Exact Ancient Synth constructor order — proven to work in Logic
    setLookAndFeel(&laf_);
    setSize(kW, kH);
    setResizable(false, false);
    setOpaque(true);

    titleLabel_.setText("ANCIENT VOICES", juce::dontSendNotification);
    titleLabel_.setFont(Fonts::display(17.f));
    titleLabel_.setColour(juce::Label::textColourId, Pal::High);
    addAndMakeVisible(titleLabel_);

    presetNameLabel_.setFont(Fonts::body(13.f));
    presetNameLabel_.setColour(juce::Label::textColourId, Pal::High);
    addAndMakeVisible(presetNameLabel_);

    presetCatLabel_.setFont(Fonts::label(9.5f));
    presetCatLabel_.setColour(juce::Label::textColourId, Pal::Primary);
    addAndMakeVisible(presetCatLabel_);

    prevBtn_.addListener(this);
    nextBtn_.addListener(this);
    addAndMakeVisible(prevBtn_);
    addAndMakeVisible(nextBtn_);

    presetTree_.setRootItemVisible(false);
    presetTree_.setColour(juce::TreeView::backgroundColourId, Pal::Surface);
    presetTree_.setColour(juce::TreeView::linesColourId,      Pal::Border);
    presetTree_.setIndentSize(14);
    buildTree();
    addAndMakeVisible(presetTree_);

    tabs_.addTab("VOICE",   Pal::Surface, &voicePanel_,   false);
    tabs_.addTab("ANCIENT", Pal::Surface, &ancientPanel_, false);
    tabs_.addTab("FX",      Pal::Surface, &fxPanel_,      false);
    addAndMakeVisible(tabs_);

    addAndMakeVisible(formantScope_);

    updatePresetLabel();
    startTimerHz(30);
}

AncientVoicesEditor::~AncientVoicesEditor()
{
    stopTimer();
    presetTree_.setRootItem(nullptr);
    setLookAndFeel(nullptr);
}

void AncientVoicesEditor::buildTree()
{
    treeRoot_.clearSubItems();
    juce::StringArray cats;
    for (int i = 0; i < presetMgr_.count(); ++i) {
        juce::String cat = presetMgr_.get(i).category;
        if (!cats.contains(cat)) cats.add(cat);
    }
    for (const auto& cat : cats) {
        auto* catItem = new PresetTreeItem(cat);
        catItem->setOpen(cat == "Chant");
        for (int i = 0; i < presetMgr_.count(); ++i)
            if (juce::String(presetMgr_.get(i).category) == cat)
                catItem->addSubItem(new PresetTreeItem(presetMgr_.get(i).name, i));
        treeRoot_.addSubItem(catItem);
    }
    presetTree_.setRootItem(&treeRoot_);
}

void AncientVoicesEditor::paint(juce::Graphics& g)
{
    // Match Ancient Synth pattern exactly
    g.fillAll(Pal::Base);
    AncientLAF::bg(g, getLocalBounds());

    // Header
    g.setColour(Pal::Surface);
    g.fillRect(getLocalBounds().removeFromTop(52));

    // Borders — same style as Ancient Synth
    g.setColour(Pal::Border);
    g.fillRect(0,   52, getWidth(), 1);              // header bottom
    g.fillRect(190, 53, 1, getHeight() - 74);        // sidebar right
    g.fillRect(0, getHeight() - 21, getWidth(), 1);  // footer top

    // Gold header accent line
    g.setColour(Pal::Primary);
    g.fillRect(0, 50, getWidth(), 2);

    // Footer background
    g.setColour(Pal::Surface);
    g.fillRect(0, getHeight() - 20, getWidth(), 20);

    // Copyright left
    g.setFont(Fonts::mono(9.f));
    g.setColour(Pal::Low);
    g.drawText("(c)(p) 2026 Tully EDM Vibe  |  info@tullyedmvibe.com",
               8, getHeight() - 19, getWidth() - 16, 16,
               juce::Justification::centredLeft);

    // Version right
    g.setColour(Pal::Low);
    g.drawText("Ancient Voices v1.0",
               8, getHeight() - 19, getWidth() - 16, 16,
               juce::Justification::centredRight);
}

void AncientVoicesEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;

    auto full = getLocalBounds();

    // Strip footer from layout so components don't overlap it
    full.removeFromBottom(21);

    auto header = full.removeFromTop(52);
    header.reduce(12, 0);
    titleLabel_.setBounds    (header.removeFromLeft(180).withSizeKeepingCentre(180, 28));
    header.removeFromLeft(12);
    presetCatLabel_.setBounds (header.removeFromLeft(70).withSizeKeepingCentre(70, 14));
    presetNameLabel_.setBounds(header.removeFromLeft(200).withSizeKeepingCentre(200, 20));
    auto nav = header.removeFromRight(72).withSizeKeepingCentre(72, 26);
    prevBtn_.setBounds(nav.removeFromLeft(34));
    nav.removeFromLeft(4);
    nextBtn_.setBounds(nav);

    presetTree_.setBounds(full.removeFromLeft(190).reduced(0, 2));

    auto scopeArea = full.removeFromBottom(110);
    formantScope_.setBounds(scopeArea.reduced(8, 6));

    tabs_.setBounds(full);
}

void AncientVoicesEditor::timerCallback()
{
    if (proc_.consumeScope(scopeSnap_) && scopeSnap_.getNumSamples() > 0)
        formantScope_.pushSamples(scopeSnap_.getReadPointer(0), scopeSnap_.getNumSamples());

    float vowelPos = proc_.apvts.getRawParameterValue("vowel_pos")->load();
    int   eraIdx   = int(proc_.apvts.getRawParameterValue("era")->load());
    formantScope_.setFormantState(vowelPos, AncientEra(eraIdx));

    if (auto* sel = presetTree_.getSelectedItem(0)) {
        auto* item = dynamic_cast<PresetTreeItem*>(sel);
        if (item && !item->isCategoryHeader()) {
            int idx = item->presetIndex();
            if (idx != presetMgr_.current()) {
                presetMgr_.apply(idx, proc_.apvts);
                updatePresetLabel();
            }
        }
    }
}

void AncientVoicesEditor::buttonClicked(juce::Button* btn)
{
    if (btn == &prevBtn_) { presetMgr_.prev(proc_.apvts); updatePresetLabel(); selectCurrentInTree(); }
    if (btn == &nextBtn_) { presetMgr_.next(proc_.apvts); updatePresetLabel(); selectCurrentInTree(); }
}

void AncientVoicesEditor::loadPreset(int idx)
{
    presetMgr_.apply(idx, proc_.apvts);
    updatePresetLabel();
    selectCurrentInTree();
}

void AncientVoicesEditor::updatePresetLabel()
{
    const auto& pr = presetMgr_.get(presetMgr_.current());
    presetNameLabel_.setText(pr.name,     juce::dontSendNotification);
    presetCatLabel_.setText (pr.category, juce::dontSendNotification);
}

void AncientVoicesEditor::selectCurrentInTree()
{
    int target = presetMgr_.current();
    for (int c = 0; c < treeRoot_.getNumSubItems(); ++c) {
        auto* catItem = treeRoot_.getSubItem(c);
        for (int p = 0; p < catItem->getNumSubItems(); ++p) {
            auto* leaf = dynamic_cast<PresetTreeItem*>(catItem->getSubItem(p));
            if (leaf && leaf->presetIndex() == target) {
                catItem->setOpen(true);
                leaf->setSelected(true, true);
                presetTree_.scrollToKeepItemVisible(leaf);
                return;
            }
        }
    }
}
