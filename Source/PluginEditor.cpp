#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  CONSTRUCTOR
// ─────────────────────────────────────────────────────────────────────────────
AncientVoicesEditor::AncientVoicesEditor(AncientVoicesProcessor& p)
    : AudioProcessorEditor(&p),
      proc_(p),
      voicePanel_  (p.apvts),
      ancientPanel_(p.apvts),
      fxPanel_     (p.apvts)
{
    setLookAndFeel(&laf_);
    setSize(kW, kH);

    // ── Header labels ─────────────────────────────────────────────────────
    titleLabel_.setText("ANCIENT VOICES", juce::dontSendNotification);
    titleLabel_.setFont(Fonts::display(20.f));
    titleLabel_.setColour(juce::Label::textColourId, Pal::Gold);
    addAndMakeVisible(titleLabel_);

    presetNameLabel_.setFont(Fonts::body(13.f));
    presetNameLabel_.setColour(juce::Label::textColourId, Pal::Text);
    addAndMakeVisible(presetNameLabel_);

    presetCatLabel_.setFont(Fonts::label(10.f));
    presetCatLabel_.setColour(juce::Label::textColourId, Pal::TextFaint);
    addAndMakeVisible(presetCatLabel_);

    // ── Prev / Next buttons ───────────────────────────────────────────────
    prevBtn_.addListener(this); nextBtn_.addListener(this);
    addAndMakeVisible(prevBtn_); addAndMakeVisible(nextBtn_);

    // ── Preset browser ────────────────────────────────────────────────────
    static const char* kCats[] = {"All","Chant","Drone","Choir","Ritual","Breath"};
    for (const char* cat : kCats) {
        auto* btn = catButtons_.add(new juce::TextButton(cat));
        btn->addListener(this);
        addAndMakeVisible(btn);
    }

    browserList_.setModel(this);
    browserList_.setRowHeight(22);
    browserList_.setColour(juce::ListBox::backgroundColourId, Pal::Surface);
    browserList_.setColour(juce::ListBox::outlineColourId,    Pal::Border);
    browserList_.setOutlineThickness(1);
    addAndMakeVisible(browserList_);
    rebuildBrowser();

    // ── Tabs ──────────────────────────────────────────────────────────────
    tabs_.addTab("VOICE",   Pal::Surface, &voicePanel_,   false);
    tabs_.addTab("ANCIENT", Pal::Surface, &ancientPanel_, false);
    tabs_.addTab("FX",      Pal::Surface, &fxPanel_,      false);
    addAndMakeVisible(tabs_);

    // ── Formant Scope ─────────────────────────────────────────────────────
    addAndMakeVisible(formantScope_);

    // ── Load default preset ───────────────────────────────────────────────
    loadPreset(0);

    startTimerHz(30);
}

AncientVoicesEditor::~AncientVoicesEditor()
{
    stopTimer();
    browserList_.setModel(nullptr);
    setLookAndFeel(nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
//  PAINT
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesEditor::paint(juce::Graphics& g)
{
    AncientLAF::bg(g, getLocalBounds());

    // Header panel
    auto headerArea = getLocalBounds().removeFromTop(52).toFloat();
    g.setColour(Pal::Raised);
    g.fillRect(headerArea);
    g.setColour(Pal::Border);
    g.drawHorizontalLine(52, 0.f, float(getWidth()));

    // Browser panel background
    auto browserArea = getLocalBounds().removeFromLeft(190).withTrimmedTop(52).toFloat();
    g.setColour(Pal::Surface);
    g.fillRect(browserArea);
    g.setColour(Pal::Border);
    g.drawVerticalLine(190, 52.f, float(getHeight()));

    // Subtle title glyph watermark
    g.setFont(juce::Font(80.f));
    g.setColour(Pal::Gold.withAlpha(0.03f));
    g.drawText("𒀭", getLocalBounds().removeFromRight(200).removeFromTop(200),
               juce::Justification::centred);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RESIZED
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesEditor::resized()
{
    auto full = getLocalBounds();

    // ── Header ────────────────────────────────────────────────────────────
    auto header = full.removeFromTop(52);
    header.reduce(12, 0);
    titleLabel_.setBounds(header.removeFromLeft(180).withSizeKeepingCentre(180, 28));
    header.removeFromLeft(12);
    presetCatLabel_.setBounds  (header.removeFromLeft(70).withSizeKeepingCentre(70, 14));
    presetNameLabel_.setBounds (header.removeFromLeft(200).withSizeKeepingCentre(200, 20));
    auto navArea = header.removeFromRight(72).withSizeKeepingCentre(72, 26);
    prevBtn_.setBounds(navArea.removeFromLeft(34));
    navArea.removeFromLeft(4);
    nextBtn_.setBounds(navArea.removeFromLeft(34));

    // ── Left: browser ─────────────────────────────────────────────────────
    auto left = full.removeFromLeft(190);

    // Category buttons row
    auto catRow = left.removeFromTop(28).reduced(4, 4);
    int bw = catRow.getWidth() / catButtons_.size();
    for (auto* btn : catButtons_)
        btn->setBounds(catRow.removeFromLeft(bw).reduced(1, 0));

    // List box fills rest
    browserList_.setBounds(left.reduced(4, 4));

    // ── Right: tabs + scope ───────────────────────────────────────────────
    auto right = full;

    // Formant scope — fixed height at bottom
    auto scopeArea = right.removeFromBottom(110);
    formantScope_.setBounds(scopeArea.reduced(8, 6));

    // Tabs fill remaining space
    tabs_.setBounds(right);
}

// ─────────────────────────────────────────────────────────────────────────────
//  TIMER — feed scope + update formant display
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesEditor::timerCallback()
{
    // Use member buffer to avoid heap allocation on every timer tick
    if (proc_.consumeScope(scopeSnap_) && scopeSnap_.getNumSamples() > 0)
        formantScope_.pushSamples(scopeSnap_.getReadPointer(0), scopeSnap_.getNumSamples());

    float vowelPos = proc_.apvts.getRawParameterValue("vowel_pos")->load();
    int   eraIdx   = int(proc_.apvts.getRawParameterValue("era")->load());
    formantScope_.setFormantState(vowelPos, AncientEra(eraIdx));
}

// ─────────────────────────────────────────────────────────────────────────────
//  BUTTON LISTENER
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesEditor::buttonClicked(juce::Button* btn)
{
    if (btn == &prevBtn_) { presetMgr_.prev(proc_.apvts); updatePresetLabel(); return; }
    if (btn == &nextBtn_) { presetMgr_.next(proc_.apvts); updatePresetLabel(); return; }

    // Category filter buttons
    for (auto* cb : catButtons_) {
        if (btn == cb) {
            selectedCat_ = cb->getButtonText();
            for (auto* b : catButtons_)
                b->setToggleState(b == cb, juce::dontSendNotification);
            rebuildBrowser();
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PRESET BROWSER
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesEditor::rebuildBrowser()
{
    browserRows_.clear();
    for (int i = 0; i < presetMgr_.count(); ++i) {
        const auto& pr = presetMgr_.get(i);
        if (selectedCat_ == "All" || selectedCat_ == pr.category)
            browserRows_.push_back({ i, pr.name, pr.category });
    }
    browserList_.updateContent();
    browserList_.repaint();
}

int AncientVoicesEditor::getNumRows()
{
    return int(browserRows_.size());
}

void AncientVoicesEditor::paintListBoxItem(int row, juce::Graphics& g,
                                            int w, int h, bool selected)
{
    if (row >= int(browserRows_.size())) return;
    const auto& br = browserRows_[size_t(row)];

    if (selected) {
        g.setColour(Pal::Amber.withAlpha(0.15f));
        g.fillRoundedRectangle(2, 1, w - 4, h - 2, 3.f);
        g.setColour(Pal::Amber.withAlpha(0.4f));
        g.drawRoundedRectangle(2, 1, w - 4, h - 2, 3.f, 1.f);
    }

    // Category colour dot
    static const struct { const char* cat; juce::Colour col; } kCatCols[] = {
        {"Chant",  Pal::Gold},  {"Drone",  Pal::Amber}, {"Choir",  Pal::Sage},
        {"Ritual", Pal::Crimson},{"Breath", Pal::Purple}
    };
    juce::Colour dot = Pal::TextFaint;
    for (auto& cc : kCatCols)
        if (br.cat == cc.cat) { dot = cc.col; break; }

    g.setColour(dot);
    g.fillEllipse(10, h / 2 - 3, 6, 6);

    g.setFont(Fonts::label(11.f));
    g.setColour(selected ? Pal::Amber : Pal::Text);
    g.drawText(br.label, 24, 0, w - 26, h, juce::Justification::centredLeft);
}

void AncientVoicesEditor::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row >= int(browserRows_.size())) return;
    loadPreset(browserRows_[size_t(row)].presetIdx);
}

// ─────────────────────────────────────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesEditor::loadPreset(int idx)
{
    presetMgr_.apply(idx, proc_.apvts);
    updatePresetLabel();

    // Highlight in browser
    for (int i = 0; i < int(browserRows_.size()); ++i)
        if (browserRows_[size_t(i)].presetIdx == idx)
            { browserList_.selectRow(i); break; }
}

void AncientVoicesEditor::updatePresetLabel()
{
    const auto& pr = presetMgr_.get(presetMgr_.current());
    presetNameLabel_.setText(pr.name,     juce::dontSendNotification);
    presetCatLabel_.setText (pr.category, juce::dontSendNotification);
}
