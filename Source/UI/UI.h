#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
//  PALETTE  — Near-black warm background, amber accent
//
//  Background hierarchy (warm near-black — Baby Audio / Valhalla register):
//    Base     #0C0E12  — near-black, slight cool depth
//    Surface  #131720  — panel interiors
//    Raised   #1C2230  — control backgrounds
//    Lift     #242D3E  — hover state
//
//  Accent: warm amber (less yellow, more burnt-orange than v2 gold)
//    Primary  #D4883A  — main accent (knob arcs, active state)
//    Bright   #ECA855  — highlights
//    Dim      #7A4E20  — inactive arc tracks
//    Track    #1C2230  — knob track groove
//
//  Text: warm off-white (not pure #FFFFFF — sits easier on dark bg)
//    High     #EAE6E0  — primary text
//    Mid      #8892A4  — labels, secondary
//    Low      #4A5264  — hints, disabled
//
//  Secondary accents (same roles, warmer tones):
//    Cyan     #3EC4D8
//    Coral    #E86A5A
//    Gold     #D4883A  (aliases Primary)
//    Mint     #4DC49A
//    Purple   #9B7FD4
// ─────────────────────────────────────────────────────────────────────────────
namespace Pal {
    inline constexpr juce::uint32
        cBase    = 0xFF0C0E12,
        cSurface = 0xFF131720,
        cRaised  = 0xFF1C2230,
        cLift    = 0xFF242D3E,
        cBorder  = 0xFF2A3448,
        cBorderHi= 0xFF3E5070,
        cPrimary = 0xFFD4883A,
        cBright  = 0xFFECA855,
        cDim     = 0xFF7A4E20,
        cTrack   = 0xFF1C2230,
        cHigh    = 0xFFEAE6E0,
        cMid     = 0xFF8892A4,
        cLow     = 0xFF4A5264,
        cCyan    = 0xFF3EC4D8,
        cCoral   = 0xFFE86A5A,
        cGold    = 0xFFD4883A,
        cMint    = 0xFF4DC49A;

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
        AmberDim  {cDim},
        Sage      = Mint,
        Purple    {0xFF9B7FD4u},
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
    inline juce::Font label  (float s = 10.5f) {
        auto f = sysFont(s, false);
        f.setExtraKerningFactor(0.07f);   // slightly tighter than v2 (0.06)
        return f;
    }
    inline juce::Font display(float s = 16.f)  { return sysFont(s, true); }
    inline juce::Font mono   (float s = 11.f)  {
        return juce::Font(juce::FontOptions()
                   .withName(juce::Font::getDefaultMonospacedFontName()).withHeight(s));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  LOOK AND FEEL
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
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Pal::Primary.withAlpha(0.25f));
        setColour(juce::PopupMenu::highlightedTextColourId,       Pal::High);

        // Buttons
        setColour(juce::TextButton::buttonColourId,   Pal::Raised);
        setColour(juce::TextButton::buttonOnColourId, Pal::Primary.withAlpha(0.25f));
        setColour(juce::TextButton::textColourOffId,  Pal::Mid);
        setColour(juce::TextButton::textColourOnId,   Pal::High);

        // Tabs
        setColour(juce::TabbedButtonBar::tabTextColourId,   Pal::Mid);
        setColour(juce::TabbedButtonBar::frontTextColourId, Pal::High);

        // Labels
        setColour(juce::Label::textColourId, Pal::High);

        // TreeView
        setColour(juce::ListBox::backgroundColourId,  Pal::Surface);
        setColour(juce::TreeView::backgroundColourId, Pal::Surface);
        setColour(juce::TreeView::linesColourId,      Pal::Border);

        // Scrollbar
        setColour(juce::ScrollBar::thumbColourId, Pal::Border);

        // Toggle
        setColour(juce::ToggleButton::tickColourId, Pal::Primary);
        setColour(juce::ToggleButton::textColourId, Pal::Mid);
    }

    // ── Rotary knob ───────────────────────────────────────────────────────
    //  Baby Audio character: thick arc, clear pointer dot, matte disc.
    //  Arc stroke 4.5 px (was 3 px in v2) — legible at small sizes.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float pos, float startA, float endA, juce::Slider& s) override
    {
        const float cx  = x + w * 0.5f;
        const float cy  = y + h * 0.5f;
        const float r   = std::min(w, h) * 0.42f;   // slightly larger than v2 (0.40)
        const float ri  = r * 0.66f;
        const float pi  = juce::MathConstants<float>::pi;

        // Matte disc — one-tone fill, no gradient (Baby Audio is flat, not shiny)
        g.setColour(Pal::Raised);
        g.fillEllipse(cx - ri, cy - ri, ri * 2.f, ri * 2.f);

        // Very subtle inner shadow ring
        g.setColour(Pal::Base.withAlpha(0.6f));
        g.drawEllipse(cx - ri, cy - ri, ri * 2.f, ri * 2.f, 1.5f);

        // Track arc (full range, very dim — just a groove)
        {
            juce::Path track;
            track.addCentredArc(cx, cy, r, r, 0, startA, endA, true);
            g.setColour(Pal::Track.brighter(0.15f));
            g.strokePath(track, juce::PathStrokeType(4.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Value arc — warm amber, thick and clear
        if (pos > 0.001f) {
            juce::Path arc;
            arc.addCentredArc(cx, cy, r, r, 0, startA,
                              startA + pos * (endA - startA), true);
            auto ac = s.findColour(juce::Slider::rotarySliderFillColourId);
            g.setColour(ac.isOpaque() ? ac : Pal::Primary);
            g.strokePath(arc, juce::PathStrokeType(4.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Pointer — short line + filled dot, accent colour
        {
            const float va  = startA + pos * (endA - startA);
            const float cos_ = std::cos(va - pi * 0.5f);
            const float sin_ = std::sin(va - pi * 0.5f);
            const float px  = cx + ri * 0.60f * cos_;
            const float py  = cy + ri * 0.60f * sin_;
            const float px0 = cx + ri * 0.18f * cos_;
            const float py0 = cy + ri * 0.18f * sin_;
            auto ac = s.findColour(juce::Slider::rotarySliderFillColourId);
            g.setColour(ac.isOpaque() ? ac : Pal::Primary);
            g.drawLine(px0, py0, px, py, 2.2f);
            g.fillEllipse(px - 3.f, py - 3.f, 6.f, 6.f);
        }
    }

    // ── ComboBox ──────────────────────────────────────────────────────────
    void drawComboBox(juce::Graphics& g, int w, int h, bool,
                      int, int, int, int, juce::ComboBox& cb) override
    {
        const bool hov = cb.isMouseOver();
        g.setColour(hov ? Pal::Lift : Pal::Raised);
        g.fillRoundedRectangle(0, 0, w, h, 4.f);
        g.setColour(hov ? Pal::Primary.withAlpha(0.7f) : Pal::Border);
        g.drawRoundedRectangle(0.5f, 0.5f, w - 1.f, h - 1.f, 4.f, 1.f);

        float ax = w - 14.f, ay = h * 0.5f;
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
        if      (b.getToggleState()) fill = Pal::Primary.withAlpha(0.22f);
        else if (dn)                 fill = Pal::Raised.darker(0.1f);
        else if (hov)                fill = Pal::Lift;
        else                         fill = Pal::Raised;
        g.setColour(fill);
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(b.getToggleState() ? Pal::Primary
                    : hov ? Pal::Primary.withAlpha(0.45f) : Pal::Border);
        g.drawRoundedRectangle(r.reduced(0.5f), 4.f, 1.f);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return Fonts::label(11.f); }

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        const auto  r      = btn.getLocalBounds().toFloat();
        const bool  active = btn.getToggleState();
        const bool  hov    = btn.isMouseOver();
        g.setColour(active ? Pal::Primary : hov ? Pal::High : Pal::Mid);
        g.setFont(Fonts::label(11.f));
        g.drawText(btn.getButtonText(), r.toNearestInt(), juce::Justification::centred);
        if (active) {
            g.setColour(Pal::Primary);
            // Compute bottom strip without mutating r (r is const)
            g.fillRect(juce::Rectangle<float>(r.getX(), r.getBottom() - 2.f, r.getWidth(), 2.f));
        }
        // Vertical separator between adjacent buttons
        g.setColour(Pal::Border);
        g.drawLine(r.getRight(), r.getY() + 4.f, r.getRight(), r.getBottom() - 4.f, 1.f);
    }

    // ── Tab bar ───────────────────────────────────────────────────────────
    void drawTabButton(juce::TabBarButton& btn, juce::Graphics& g, bool active, bool hov) override
    {
        const auto r = btn.getActiveArea().toFloat();
        g.setColour(active ? Pal::Surface : hov ? Pal::Lift : Pal::Base);
        g.fillRect(r);
        g.setColour(active ? Pal::High : hov ? Pal::Mid.brighter(0.1f) : Pal::Low);
        g.setFont(Fonts::label(11.f));
        g.drawText(btn.getButtonText(), r.toNearestInt(), juce::Justification::centred);
        if (active) {
            g.setColour(Pal::Primary);
            g.fillRect(juce::Rectangle<float>(r.getX(), r.getBottom() - 2.f, r.getWidth(), 2.f));
        }
        g.setColour(Pal::Border);
        g.drawLine(r.getRight(), r.getY() + 4.f, r.getRight(), r.getBottom() - 4.f, 1.f);
    }

    // ── Scrollbar ─────────────────────────────────────────────────────────
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y,
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

    // ── Panel helpers ─────────────────────────────────────────────────────
    static void bg(juce::Graphics& g, juce::Rectangle<int> b) {
        g.setColour(Pal::Base);
        g.fillRect(b);
    }

    static void panel(juce::Graphics& g, juce::Rectangle<float> b) {
        g.setColour(Pal::Surface);
        g.fillRoundedRectangle(b, 5.f);
        g.setColour(Pal::Border);
        g.drawRoundedRectangle(b.reduced(0.5f), 5.f, 1.f);
    }

    // Section label: amber text + 1 px separator line beneath
    static void sectionLabel(juce::Graphics& g, juce::Rectangle<int> b, const juce::String& t) {
        g.setFont(Fonts::label(9.5f));
        g.setColour(Pal::Primary);
        g.drawText(t.toUpperCase(), b, juce::Justification::centredLeft);
        // 1 px rule under the label — cleaner than v2 (no rule)
        g.setColour(Pal::Border);
        g.fillRect(b.getX(), b.getBottom() + 1, b.getWidth(), 1);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  KNOB  — compact rotary with accent arc and label below
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
        slider.setBounds(getLocalBounds().withTrimmedBottom(16).reduced(2));
    }

    void paint(juce::Graphics& g) override {
        g.setFont(Fonts::label(9.5f));
        g.setColour(Pal::Mid);
        g.drawText(lbl_.toUpperCase(),
                   getLocalBounds().removeFromBottom(16),
                   juce::Justification::centred);
    }

private:
    juce::String lbl_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  XY PAD  — Vowel × Openness
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
        g.fillRoundedRectangle(b, 5.f);
        g.setColour(Pal::Border);
        g.drawRoundedRectangle(b.reduced(0.5f), 5.f, 1.f);

        auto inner = b.reduced(8.f);

        // Grid
        g.setColour(Pal::Border.withAlpha(0.5f));
        g.drawHorizontalLine(int(inner.getCentreY()), inner.getX(), inner.getRight());
        g.drawVerticalLine  (int(inner.getCentreX()), inner.getY(), inner.getBottom());

        const float nx = (xMax_ > xMin_)
            ? juce::jlimit(0.f, 1.f, float((xs_.getValue() - xMin_) / (xMax_ - xMin_)))
            : 0.f;
        const float ny = (yMax_ > yMin_)
            ? juce::jlimit(0.f, 1.f, float((ys_.getValue() - yMin_) / (yMax_ - yMin_)))
            : 0.f;

        const float px = inner.getX() + nx * inner.getWidth();
        const float py = inner.getBottom() - ny * inner.getHeight();

        // Crosshair glow
        g.setColour(Pal::Primary.withAlpha(0.1f));
        g.drawHorizontalLine(int(py), inner.getX(), inner.getRight());
        g.drawVerticalLine  (int(px), inner.getY(), inner.getBottom());

        // Cursor — outer halo + solid amber dot + dark centre
        g.setColour(Pal::Primary.withAlpha(0.2f));
        g.fillEllipse(px - 11.f, py - 11.f, 22.f, 22.f);
        g.setColour(Pal::Primary.withAlpha(0.5f));
        g.fillEllipse(px - 7.f, py - 7.f, 14.f, 14.f);
        g.setColour(Pal::Primary);
        g.fillEllipse(px - 4.f, py - 4.f, 8.f, 8.f);
        g.setColour(Pal::Base);
        g.fillEllipse(px - 1.5f, py - 1.5f, 3.f, 3.f);

        // Axis labels
        g.setFont(Fonts::label(9.f));
        g.setColour(Pal::Mid);
        g.drawText(xl_, int(inner.getRight()) - 40, int(inner.getBottom()) + 1, 40, 11,
                   juce::Justification::centredRight);
        g.drawText(yl_, int(inner.getX()), int(inner.getY()) - 11, 40, 11,
                   juce::Justification::centredLeft);

        // Vowel position markers
        static const char* kVowels[] = {"A", "E", "I", "O", "U"};
        g.setFont(Fonts::label(9.5f));
        for (int i = 0; i < 5; ++i) {
            const float vx  = inner.getX() + (float(i) / 4.f) * inner.getWidth();
            const bool  near = std::abs(nx - float(i) / 4.f) < 0.08f;
            g.setColour(near ? Pal::Primary : Pal::Mid.withAlpha(0.7f));
            g.drawText(kVowels[i], int(vx) - 8, int(b.getBottom()) - 13, 16, 11,
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
        auto   b  = getLocalBounds().toFloat().reduced(8.f);
        float  nx = juce::jlimit(0.f, 1.f, (e.position.x - b.getX()) / b.getWidth());
        float  ny = juce::jlimit(0.f, 1.f, 1.f - (e.position.y - b.getY()) / b.getHeight());
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
        AncientLAF::panel(g, b);
        b = b.reduced(8, 6);
        const float tot = a_ + d_ + 0.2f + r_;
        const float W   = b.getWidth();
        const float top = b.getY(), bot = b.getBottom();
        const float x1  = b.getX() + a_ / tot * W;
        const float x2  = x1 + d_ / tot * W;
        const float x3  = x2 + 0.2f / tot * W;
        const float sy  = top + (1.f - s_) * b.getHeight();
        juce::Path p;
        p.startNewSubPath(b.getX(), bot);
        p.lineTo(x1, top); p.lineTo(x2, sy); p.lineTo(x3, sy);
        p.lineTo(x3 + r_ / tot * W, bot);
        juce::Path fill = p;
        fill.lineTo(b.getX(), bot);
        fill.closeSubPath();
        g.setColour(Pal::Primary.withAlpha(0.10f));
        g.fillPath(fill);
        g.setColour(Pal::Primary.withAlpha(0.80f));
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
private:
    float a_ = 0.05f, d_ = 0.2f, s_ = 0.85f, r_ = 1.2f;
};
