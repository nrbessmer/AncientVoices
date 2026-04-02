#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
//  PALETTE  — White on Deep Blue theme
//
//  Background hierarchy:
//    Base     #1A2744  — deep navy blue (main background)
//    Surface  #1E3054  — slightly lighter navy (panels)
//    Raised   #253B66  — control backgrounds
//    Lift     #2E4878  — hover state
//
//  Accent: Gold / Amber — warm contrast against blue
//    Primary  #E8B84B  — main accent (knob arcs, selection, active)
//    Bright   #F5D07A  — highlights
//    Dim      #8A6820  — inactive arc tracks
//
//  Text: White for maximum legibility on dark blue
//    High     #FFFFFF  — primary text, full white
//    Mid      #B8C8E8  — labels, secondary (light blue-white)
//    Low      #6A82A8  — hints, disabled
//
//  Secondary accents:
//    Cyan     #4EC9E8  — scope, live data
//    Coral    #FF7B6B  — warnings
//    Gold     #E8B84B  — shimmer, special
//    Mint     #5DC8A0  — choir section
// ─────────────────────────────────────────────────────────────────────────────
namespace Pal {
    inline constexpr juce::uint32
        cBase    = 0xFF1A2744,   // deep navy
        cSurface = 0xFF1E3054,   // lighter navy
        cRaised  = 0xFF253B66,   // control bg
        cLift    = 0xFF2E4878,   // hover
        cBorder  = 0xFF3A5488,   // borders
        cBorderHi= 0xFF4E6EA8,   // active border
        // Accent — gold on blue
        cPrimary = 0xFFE8B84B,   // gold
        cBright  = 0xFFF5D07A,   // bright gold
        cDim     = 0xFF8A6820,   // dim gold
        cTrack   = 0xFF2A3E6A,   // knob track
        // Text — white on blue
        cHigh    = 0xFFFFFFFF,   // pure white
        cMid     = 0xFFB8C8E8,   // light blue-white
        cLow     = 0xFF6A82A8,   // muted blue
        // Secondary
        cCyan    = 0xFF4EC9E8,   // cyan
        cCoral   = 0xFFFF7B6B,   // coral
        cGold    = 0xFFE8B84B,   // gold
        cMint    = 0xFF5DC8A0;   // mint

    const juce::Colour
        Base    {cBase},    Surface  {cSurface}, Raised   {cRaised},
        Lift    {cLift},    Border   {cBorder},  BorderHi {cBorderHi},
        Primary {cPrimary}, Bright   {cBright},  Dim      {cDim},
        Track   {cTrack},
        High    {cHigh},    Mid      {cMid},      Low      {cLow},
        Cyan    {cCyan},    Coral    {cCoral},    Gold     {cGold},
        Mint    {cMint},
        // Aliases
        Void      = Base,
        Text      = High,
        TextDim   = Mid,
        TextFaint = Low,
        Amber     = Gold,
        AmberDim  {0xFF8A6820},
        Sage      = Mint,
        Purple    {0xFFAA88EE},
        Teal      = Cyan,
        Crimson   = Coral,
        Vocal     = Primary,
        VocalDim  = Dim;
}

// ─────────────────────────────────────────────────────────────────────────────
//  TYPOGRAPHY
// ─────────────────────────────────────────────────────────────────────────────
namespace Fonts {
    inline juce::Font sysFont(float size, bool bold = false) {
        auto style = bold ? juce::String("Bold") : juce::String("Regular");
        auto f1 = juce::Font(juce::FontOptions()
                      .withName("SF Pro Display").withStyle(style).withHeight(size));
        if (f1.getTypefaceName() == "SF Pro Display") return f1;
        auto f2 = juce::Font(juce::FontOptions()
                      .withName("Helvetica Neue").withStyle(style).withHeight(size));
        if (f2.getTypefaceName() == "Helvetica Neue") return f2;
        return juce::Font(juce::FontOptions()
                   .withName(juce::Font::getDefaultSansSerifFontName())
                   .withStyle(style).withHeight(size));
    }
    inline juce::Font body   (float s = 13.f)  { return sysFont(s, false); }
    inline juce::Font label  (float s = 10.5f) { auto f = sysFont(s, false); f.setExtraKerningFactor(0.06f); return f; }
    inline juce::Font display(float s = 16.f)  { return sysFont(s, true); }
    inline juce::Font mono   (float s = 11.f)  {
        return juce::Font(juce::FontOptions()
                   .withName(juce::Font::getDefaultMonospacedFontName()).withHeight(s));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  LOOK AND FEEL  — White on Blue
// ─────────────────────────────────────────────────────────────────────────────
class AncientLAF : public juce::LookAndFeel_V4 {
public:
    AncientLAF() {
        setColour(juce::ResizableWindow::backgroundColourId, Pal::Base);

        // ComboBox
        setColour(juce::ComboBox::backgroundColourId, Pal::Raised);
        setColour(juce::ComboBox::outlineColourId,    Pal::Border);
        setColour(juce::ComboBox::textColourId,       Pal::High);
        setColour(juce::ComboBox::arrowColourId,      Pal::Mid);

        // PopupMenu
        setColour(juce::PopupMenu::backgroundColourId,            Pal::Raised);
        setColour(juce::PopupMenu::textColourId,                  Pal::High);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Pal::Primary.withAlpha(0.3f));
        setColour(juce::PopupMenu::highlightedTextColourId,       Pal::High);

        // Buttons
        setColour(juce::TextButton::buttonColourId,   Pal::Raised);
        setColour(juce::TextButton::buttonOnColourId, Pal::Primary.withAlpha(0.3f));
        setColour(juce::TextButton::textColourOffId,  Pal::Mid);
        setColour(juce::TextButton::textColourOnId,   Pal::High);

        // Tabs
        setColour(juce::TabbedButtonBar::tabTextColourId,   Pal::Mid);
        setColour(juce::TabbedButtonBar::frontTextColourId, Pal::High);

        // Labels
        setColour(juce::Label::textColourId, Pal::High);

        // TreeView
        setColour(juce::ListBox::backgroundColourId,  Pal::Surface);
        setColour(juce::ListBox::outlineColourId,     Pal::Border);
        setColour(juce::TreeView::backgroundColourId, Pal::Surface);
        setColour(juce::TreeView::linesColourId,      Pal::Border);

        // Scrollbar
        setColour(juce::ScrollBar::thumbColourId, Pal::Border);

        // Toggle
        setColour(juce::ToggleButton::tickColourId, Pal::Primary);
        setColour(juce::ToggleButton::textColourId, Pal::Mid);
    }

    // ── Rotary knob — gold arc on blue ────────────────────────────────────
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float pos, float startA, float endA, juce::Slider& s) override
    {
        float cx = x + w * .5f, cy = y + h * .5f;
        float r  = std::min(w, h) * .40f;
        float ri = r * .68f;
        auto  pi = juce::MathConstants<float>::pi;

        // Background disc — slightly lighter blue
        g.setColour(Pal::Raised);
        g.fillEllipse(cx - ri, cy - ri, ri * 2.f, ri * 2.f);

        // Track arc (full range, dim)
        {
            juce::Path track;
            track.addCentredArc(cx, cy, r, r, 0, startA, endA, true);
            g.setColour(Pal::Track);
            g.strokePath(track, juce::PathStrokeType(3.f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }

        // Value arc — gold
        if (pos > 0.001f) {
            juce::Path arc;
            arc.addCentredArc(cx, cy, r, r, 0, startA, startA + pos * (endA - startA), true);
            auto ac = s.findColour(juce::Slider::rotarySliderFillColourId);
            g.setColour(ac.isOpaque() ? ac : Pal::Primary);
            g.strokePath(arc, juce::PathStrokeType(3.f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }

        // Disc border — subtle blue-white
        g.setColour(Pal::Border);
        g.drawEllipse(cx - ri, cy - ri, ri * 2.f, ri * 2.f, 1.f);

        // Pointer line — gold
        float va  = startA + pos * (endA - startA);
        float px  = cx + (ri * .58f) * std::cos(va - pi * .5f);
        float py  = cy + (ri * .58f) * std::sin(va - pi * .5f);
        float px0 = cx + (ri * .15f) * std::cos(va - pi * .5f);
        float py0 = cy + (ri * .15f) * std::sin(va - pi * .5f);
        auto ac = s.findColour(juce::Slider::rotarySliderFillColourId);
        g.setColour(ac.isOpaque() ? ac : Pal::Primary);
        g.drawLine(px0, py0, px, py, 2.f);
        g.fillEllipse(px - 2.5f, py - 2.5f, 5.f, 5.f);
    }

    // ── ComboBox ──────────────────────────────────────────────────────────
    void drawComboBox(juce::Graphics& g, int w, int h, bool,
                      int, int, int, int, juce::ComboBox& cb) override
    {
        bool hov = cb.isMouseOver();
        g.setColour(hov ? Pal::Lift : Pal::Raised);
        g.fillRoundedRectangle(0, 0, w, h, 5.f);
        g.setColour(hov ? Pal::Primary : Pal::Border);
        g.drawRoundedRectangle(.5f, .5f, w - 1.f, h - 1.f, 5.f, 1.f);

        float ax = w - 14.f, ay = h * .5f;
        juce::Path arr;
        arr.addTriangle(ax, ay - 2.5f, ax + 7.f, ay - 2.5f, ax + 3.5f, ay + 2.5f);
        g.setColour(Pal::Primary);
        g.fillPath(arr);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return Fonts::body(12.f); }

    // ── TextButton ────────────────────────────────────────────────────────
    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool hov, bool dn) override
    {
        auto r = b.getLocalBounds().toFloat();
        juce::Colour fill;
        if      (b.getToggleState()) fill = Pal::Primary.withAlpha(0.25f);
        else if (dn)                 fill = Pal::Raised.darker(.15f);
        else if (hov)                fill = Pal::Lift;
        else                         fill = Pal::Raised;
        g.setColour(fill);
        g.fillRoundedRectangle(r, 5.f);
        g.setColour(b.getToggleState() ? Pal::Primary : hov ? Pal::Primary.withAlpha(0.5f) : Pal::Border);
        g.drawRoundedRectangle(r.reduced(.5f), 5.f, 1.f);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return Fonts::label(11.f); }

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        auto r      = btn.getLocalBounds().toFloat();
        bool active = btn.getToggleState();
        bool hov    = btn.isMouseOver();
        g.setColour(active ? Pal::Primary : hov ? Pal::High : Pal::Mid);
        g.setFont(Fonts::label(11.f));
        g.drawText(btn.getButtonText(), r.toNearestInt(), juce::Justification::centred);
        if (active) {
            g.setColour(Pal::Primary);
            g.fillRect(r.removeFromBottom(2.f));
        }
        g.setColour(Pal::Border);
        g.drawLine(r.getRight(), r.getY() + 4.f, r.getRight(), r.getBottom() - 4.f, 1.f);
    }

    // ── Tab bar ───────────────────────────────────────────────────────────
    void drawTabButton(juce::TabBarButton& btn, juce::Graphics& g, bool active, bool hov) override
    {
        auto r = btn.getActiveArea().toFloat();
        g.setColour(active ? Pal::Surface : hov ? Pal::Lift : Pal::Base);
        g.fillRect(r);
        g.setColour(active ? Pal::High : hov ? Pal::Mid.brighter(.1f) : Pal::Low);
        g.setFont(Fonts::label(11.f));
        g.drawText(btn.getButtonText(), r.toNearestInt(), juce::Justification::centred);
        if (active) {
            g.setColour(Pal::Primary);
            g.fillRect(r.removeFromBottom(2.f));
        }
        g.setColour(Pal::Border);
        g.drawLine(r.getRight(), r.getY() + 4.f, r.getRight(), r.getBottom() - 4.f, 1.f);
    }

    // ── Scrollbar ─────────────────────────────────────────────────────────
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*bar*/, int x, int y,
                       int w, int h, bool vert, int tp, int ts, bool hov, bool dn) override
    {
        g.setColour(Pal::Base);
        g.fillRect(x, y, w, h);
        if (ts <= 0) return;
        auto thumb = juce::Rectangle<float>(
            vert ? float(x + 2) : float(x + tp),
            vert ? float(y + tp) : float(y + 2),
            vert ? float(w - 4) : float(ts),
            vert ? float(ts)    : float(h - 4));
        g.setColour((hov || dn) ? Pal::Primary : Pal::Border);
        g.fillRoundedRectangle(thumb, 3.f);
    }

    // ── Static panel helpers ──────────────────────────────────────────────
    static void bg(juce::Graphics& g, juce::Rectangle<int> b) {
        g.setColour(Pal::Base);
        g.fillRect(b);
    }

    static void panel(juce::Graphics& g, juce::Rectangle<float> b) {
        g.setColour(Pal::Surface);
        g.fillRoundedRectangle(b, 6.f);
        g.setColour(Pal::Border);
        g.drawRoundedRectangle(b.reduced(.5f), 6.f, 1.f);
    }

    static void sectionLabel(juce::Graphics& g, juce::Rectangle<int> b, const juce::String& t) {
        g.setFont(Fonts::label(9.5f));
        g.setColour(Pal::Primary);   // gold section labels on blue background
        g.drawText(t.toUpperCase(), b, juce::Justification::centredLeft);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  KNOB  — compact, gold arc on blue, white label
// ─────────────────────────────────────────────────────────────────────────────
class Knob : public juce::Component {
public:
    juce::Slider slider;

    Knob(const juce::String& lbl, juce::Colour ac = Pal::Primary) : lbl_(lbl) {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, ac);
        slider.setPopupDisplayEnabled(true, true, nullptr);
        addAndMakeVisible(slider);
    }

    void resized() override {
        slider.setBounds(getLocalBounds().withTrimmedBottom(18).reduced(2));
    }

    void paint(juce::Graphics& g) override {
        auto labelArea = getLocalBounds().removeFromBottom(18);
        g.setFont(Fonts::label(9.5f));
        g.setColour(Pal::Mid);
        g.drawText(lbl_.toUpperCase(), labelArea, juce::Justification::centred);
    }

private:
    juce::String lbl_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  XY PAD  — Vowel × Openness, blue background with gold cursor
// ─────────────────────────────────────────────────────────────────────────────
class XYPad : public juce::Component {
public:
    void bind(juce::AudioProcessorValueTreeState& apvts,
              const char* xi, const char* yi,
              float xMin = 0.f, float xMax = 1.f,
              float yMin = 0.f, float yMax = 1.f)
    {
        xMin_ = xMin; xMax_ = xMax;
        yMin_ = yMin; yMax_ = yMax;
        xa_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, xi, xs_);
        ya_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, yi, ys_);
    }

    void setLabels(const juce::String& x, const juce::String& y) { xl_ = x; yl_ = y; }

    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat();

        g.setColour(Pal::Raised);
        g.fillRoundedRectangle(b, 6.f);
        g.setColour(Pal::Border);
        g.drawRoundedRectangle(b.reduced(.5f), 6.f, 1.f);

        auto inner = b.reduced(8.f);

        // Grid
        g.setColour(Pal::Border.withAlpha(.6f));
        g.drawHorizontalLine(int(inner.getCentreY()), inner.getX(), inner.getRight());
        g.drawVerticalLine  (int(inner.getCentreX()), inner.getY(), inner.getBottom());

        // Position
        float nx = (xMax_ > xMin_) ? float((xs_.getValue() - xMin_) / (xMax_ - xMin_)) : 0.f;
        float ny = (yMax_ > yMin_) ? float((ys_.getValue() - yMin_) / (yMax_ - yMin_)) : 0.f;
        nx = juce::jlimit(0.f, 1.f, nx);
        ny = juce::jlimit(0.f, 1.f, ny);

        float px = inner.getX() + nx * inner.getWidth();
        float py = inner.getBottom() - ny * inner.getHeight();

        // Crosshair
        g.setColour(Pal::Primary.withAlpha(.15f));
        g.drawHorizontalLine(int(py), inner.getX(), inner.getRight());
        g.drawVerticalLine  (int(px), inner.getY(), inner.getBottom());

        // Gold cursor
        g.setColour(Pal::Primary.withAlpha(.3f));
        g.fillEllipse(px - 10.f, py - 10.f, 20.f, 20.f);
        g.setColour(Pal::Primary);
        g.fillEllipse(px - 5.f, py - 5.f, 10.f, 10.f);
        g.setColour(Pal::Base);
        g.fillEllipse(px - 2.f, py - 2.f, 4.f, 4.f);

        // Axis labels — white
        g.setFont(Fonts::label(9.f));
        g.setColour(Pal::Mid);
        g.drawText(xl_, int(inner.getRight()) - 40, int(inner.getBottom()) + 1, 40, 12,
                   juce::Justification::centredRight);
        g.drawText(yl_, int(inner.getX()), int(inner.getY()) - 12, 40, 12,
                   juce::Justification::centredLeft);

        // Vowel markers — white, gold highlight when near
        static const char* kVowels[] = {"A", "E", "I", "O", "U"};
        g.setFont(Fonts::label(9.5f));
        for (int i = 0; i < 5; ++i) {
            float vx   = inner.getX() + (float(i) / 4.f) * inner.getWidth();
            bool  near = std::abs(nx - float(i) / 4.f) < 0.08f;
            g.setColour(near ? Pal::Primary : Pal::Mid);
            g.drawText(kVowels[i], int(vx) - 8, int(b.getBottom()) - 14, 16, 12,
                       juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { update(e); repaint(); }
    void mouseDrag(const juce::MouseEvent& e) override { update(e); repaint(); }

private:
    juce::Slider xs_, ys_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xa_, ya_;
    juce::String xl_{"VOWEL"}, yl_{"OPEN"};
    float xMin_ = 0.f, xMax_ = 1.f, yMin_ = 0.f, yMax_ = 1.f;

    void update(const juce::MouseEvent& e) {
        auto b  = getLocalBounds().toFloat().reduced(8.f);
        float nx = juce::jlimit(0.f, 1.f, (e.position.x - b.getX()) / b.getWidth());
        float ny = juce::jlimit(0.f, 1.f, 1.f - (e.position.y - b.getY()) / b.getHeight());
        xs_.setValue(xMin_ + nx * (xMax_ - xMin_), juce::sendNotificationAsync);
        ys_.setValue(yMin_ + ny * (yMax_ - yMin_), juce::sendNotificationAsync);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ENVELOPE DISPLAY
// ─────────────────────────────────────────────────────────────────────────────
class EnvDisplay : public juce::Component {
public:
    void set(float a, float d, float s, float r) { a_=a; d_=d; s_=s; r_=r; repaint(); }
    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(3);
        AncientLAF::panel(g, b); b = b.reduced(8, 6);
        float tot = a_ + d_ + .2f + r_, W = b.getWidth(), top = b.getY(), bot = b.getBottom();
        float x1 = b.getX() + a_/tot*W, x2 = x1 + d_/tot*W;
        float x3 = x2 + .2f/tot*W;
        float sy = top + (1.f - s_) * b.getHeight();
        juce::Path p;
        p.startNewSubPath(b.getX(), bot);
        p.lineTo(x1, top); p.lineTo(x2, sy); p.lineTo(x3, sy); p.lineTo(x3 + r_/tot*W, bot);
        juce::Path fill = p; fill.lineTo(b.getX(), bot); fill.closeSubPath();
        g.setColour(Pal::Primary.withAlpha(.12f)); g.fillPath(fill);
        g.setColour(Pal::Primary.withAlpha(.85f));
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
private: float a_=.05f, d_=.2f, s_=.85f, r_=1.2f;
};
