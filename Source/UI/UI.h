#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
//  PALETTE  (same dark amber palette as Ancient Synth for family resemblance)
// ─────────────────────────────────────────────────────────────────────────────
namespace Pal {
    const juce::Colour
        Void     {0xFF070810}, Surface  {0xFF0B0D15},
        Raised   {0xFF111320}, Border   {0xFF222535},
        Amber    {0xFFE8A850}, AmberDim {0xFFB07830},
        Sage     {0xFF60C090}, Purple   {0xFFB898F0},
        Text     {0xFFF0E8DC}, TextDim  {0xFFB0A898}, TextFaint{0xFF706860},
        Gold     {0xFFD4AA40}, Teal     {0xFF40B0A0}, Crimson  {0xFFD07070},
        // Ancient Voices accent — warm terracotta for vocal identity
        Vocal    {0xFFD4785A}, VocalDim {0xFF904030};
}

namespace Fonts {
    inline juce::Font body   (float s=13.f){return {"Futura",s,juce::Font::plain};}
    inline juce::Font label  (float s=11.f){auto f=body(s);f.setExtraKerningFactor(.04f);return f;}
    inline juce::Font display(float s=18.f){return {"Futura",s,juce::Font::bold};}
    inline juce::Font mono   (float s=11.f){return {juce::Font::getDefaultMonospacedFontName(),s,juce::Font::plain};}
}

// ─────────────────────────────────────────────────────────────────────────────
//  LOOK AND FEEL
// ─────────────────────────────────────────────────────────────────────────────
class AncientLAF : public juce::LookAndFeel_V4 {
public:
    AncientLAF() {
        setColour(juce::ResizableWindow::backgroundColourId,      Pal::Void);
        setColour(juce::ComboBox::backgroundColourId,             Pal::Raised);
        setColour(juce::ComboBox::outlineColourId,                Pal::Border);
        setColour(juce::ComboBox::textColourId,                   Pal::Text);
        setColour(juce::ComboBox::arrowColourId,                  Pal::TextDim);
        setColour(juce::PopupMenu::backgroundColourId,            Pal::Surface);
        setColour(juce::PopupMenu::textColourId,                  Pal::Text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Pal::VocalDim);
        setColour(juce::TextButton::buttonColourId,               Pal::Raised);
        setColour(juce::TextButton::buttonOnColourId,             Pal::VocalDim);
        setColour(juce::TextButton::textColourOffId,              Pal::TextDim);
        setColour(juce::TextButton::textColourOnId,               Pal::Vocal);
        setColour(juce::TabbedButtonBar::tabTextColourId,         Pal::TextDim);
        setColour(juce::TabbedButtonBar::frontTextColourId,       Pal::Vocal);
        setColour(juce::Label::textColourId,                      Pal::Text);
        setColour(juce::ListBox::backgroundColourId,              Pal::Surface);
        setColour(juce::ListBox::outlineColourId,                 Pal::Border);
        setColour(juce::ToggleButton::tickColourId,               Pal::Vocal);
        setColour(juce::ToggleButton::textColourId,               Pal::TextDim);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float pos, float startA, float endA, juce::Slider& s) override {
        float cx=x+w*.5f, cy=y+h*.5f, r=std::min(w,h)*.38f;
        auto pi = juce::MathConstants<float>::pi;
        juce::Path track;
        track.addCentredArc(cx,cy,r,r,0,startA,endA,true);
        g.setColour(Pal::Border);
        g.strokePath(track,juce::PathStrokeType(2.5f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        if (pos > 0.001f) {
            juce::Path arc;
            arc.addCentredArc(cx,cy,r,r,0,startA,startA+pos*(endA-startA),true);
            auto ac = s.findColour(juce::Slider::rotarySliderFillColourId);
            g.setColour(ac.isOpaque() ? ac : Pal::Vocal);
            g.strokePath(arc,juce::PathStrokeType(2.5f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
        g.setColour(Pal::Raised); g.fillEllipse(cx-r*.72f,cy-r*.72f,r*1.44f,r*1.44f);
        g.setColour(Pal::Border); g.drawEllipse(cx-r*.72f,cy-r*.72f,r*1.44f,r*1.44f,1.f);
        float va=startA+pos*(endA-startA);
        float ix=cx+(r*.52f)*std::cos(va-pi*.5f), iy=cy+(r*.52f)*std::sin(va-pi*.5f);
        g.setColour(Pal::Vocal); g.fillEllipse(ix-3.f,iy-3.f,6.f,6.f);
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool, int,int,int,int, juce::ComboBox&) override {
        g.setColour(Pal::Raised); g.fillRoundedRectangle(0,0,w,h,3.f);
        g.setColour(Pal::Border); g.drawRoundedRectangle(.5f,.5f,w-1,h-1,3.f,1.f);
        juce::Path arr; float ax=w-12.f, ay=h*.5f;
        arr.addTriangle(ax,ay-3,ax+7,ay-3,ax+3.5f,ay+3);
        g.setColour(Pal::TextDim); g.fillPath(arr);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return Fonts::label(11.f); }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool hov, bool dn) override {
        auto r=b.getLocalBounds().toFloat();
        juce::Colour f=b.getToggleState()?Pal::VocalDim:hov?Pal::Raised.brighter(.05f):Pal::Raised;
        if(dn) f=f.darker(.1f);
        g.setColour(f); g.fillRoundedRectangle(r,3.f);
        g.setColour(b.getToggleState()?Pal::Vocal:Pal::Border);
        g.drawRoundedRectangle(r.reduced(.5f),3.f,1.f);
    }
    juce::Font getTextButtonFont(juce::TextButton&,int) override { return Fonts::label(11.f); }

    void drawTabButton(juce::TabBarButton& btn, juce::Graphics& g, bool active, bool hov) override {
        auto r=btn.getActiveArea().toFloat();
        g.setColour(active?Pal::Surface:Pal::Void); g.fillRect(r);
        g.setColour(active?Pal::Vocal:hov?Pal::TextDim:Pal::TextFaint);
        g.setFont(Fonts::label(11.f)); g.drawText(btn.getButtonText(),r.toNearestInt(),juce::Justification::centred);
        if(active){g.setColour(Pal::Vocal);g.fillRect(r.removeFromBottom(2.f));}
        g.setColour(Pal::Border); g.drawLine(r.getRight(),r.getY(),r.getRight(),r.getBottom(),1.f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn, bool, bool) override {
        auto r=btn.getLocalBounds().toFloat();
        bool on=btn.getToggleState();
        float bw=28,bh=14,bx=4,by=r.getCentreY()-bh*.5f;
        g.setColour(on?Pal::VocalDim:Pal::Raised); g.fillRoundedRectangle(bx,by,bw,bh,bh*.5f);
        g.setColour(on?Pal::Vocal:Pal::Border);    g.drawRoundedRectangle(bx,by,bw,bh,bh*.5f,1.f);
        float dx=on?bx+bw-bh+2:bx+2;
        g.setColour(on?Pal::Vocal:Pal::TextDim); g.fillEllipse(dx,by+2,bh-4,bh-4);
        g.setFont(Fonts::label(10.5f)); g.setColour(on?Pal::Text:Pal::TextDim);
        g.drawText(btn.getButtonText(),int(bx+bw+6),0,r.getWidth()-int(bw+10),r.getHeight(),
                   juce::Justification::centredLeft);
    }

    static void bg(juce::Graphics& g, juce::Rectangle<int> b) {
        g.setColour(Pal::Void); g.fillRect(b);
        g.setColour(Pal::Border.withAlpha(.05f));
        for(int x=b.getX();x<b.getRight();x+=32) g.drawVerticalLine(x,float(b.getY()),float(b.getBottom()));
        for(int y=b.getY();y<b.getBottom();y+=32) g.drawHorizontalLine(y,float(b.getX()),float(b.getRight()));
    }
    static void panel(juce::Graphics& g, juce::Rectangle<float> b) {
        g.setColour(Pal::Raised); g.fillRoundedRectangle(b,4.f);
        g.setColour(Pal::Border); g.drawRoundedRectangle(b.reduced(.5f),4.f,1.f);
    }
    static void sectionLabel(juce::Graphics& g, juce::Rectangle<int> b, const juce::String& t) {
        g.setFont(Fonts::label(10.5f)); g.setColour(Pal::TextDim);
        g.drawText(t.toUpperCase(),b,juce::Justification::centredLeft);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  KNOB
// ─────────────────────────────────────────────────────────────────────────────
class Knob : public juce::Component {
public:
    juce::Slider slider;
    Knob(const juce::String& lbl, juce::Colour ac=Pal::Vocal) : lbl_(lbl) {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        slider.setColour(juce::Slider::rotarySliderFillColourId,ac);
        slider.setPopupDisplayEnabled(true,true,nullptr);
        addAndMakeVisible(slider);
    }
    void resized() override { slider.setBounds(getLocalBounds().withTrimmedBottom(16).reduced(2)); }
    void paint(juce::Graphics& g) override {
        g.setFont(Fonts::label(10.f)); g.setColour(Pal::TextDim);
        g.drawText(lbl_,getLocalBounds().removeFromBottom(16),juce::Justification::centred);
    }
private: juce::String lbl_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  XY PAD  (Vowel Position × Openness)
//  Stores actual parameter min/max so paint and mouse handler normalise correctly.
//  SliderAttachment handles the APVTS binding; we don't override the slider range.
// ─────────────────────────────────────────────────────────────────────────────
class XYPad : public juce::Component {
public:
    // xMin/xMax and yMin/yMax must match the actual APVTS parameter ranges
    void bind(juce::AudioProcessorValueTreeState& apvts,
              const char* xi, const char* yi,
              float xMin=0.f, float xMax=1.f,
              float yMin=0.f, float yMax=1.f)
    {
        xMin_ = xMin; xMax_ = xMax;
        yMin_ = yMin; yMax_ = yMax;
        // Do NOT call setRange — SliderAttachment will configure from the parameter
        xa_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,xi,xs_);
        ya_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,yi,ys_);
    }
    void setLabels(const juce::String& x, const juce::String& y) { xl_=x; yl_=y; }

    void paint(juce::Graphics& g) override {
        auto b=getLocalBounds().toFloat();
        AncientLAF::panel(g,b); b=b.reduced(6);
        g.setColour(Pal::Border.withAlpha(.5f));
        g.drawHorizontalLine(int(b.getCentreY()),b.getX(),b.getRight());
        g.drawVerticalLine(int(b.getCentreX()),b.getY(),b.getBottom());

        // Normalise current values to 0–1 for pixel positioning
        float nx = (xMax_ > xMin_) ? float((xs_.getValue()-xMin_)/(xMax_-xMin_)) : 0.f;
        float ny = (yMax_ > yMin_) ? float((ys_.getValue()-yMin_)/(yMax_-yMin_)) : 0.f;
        nx = juce::jlimit(0.f,1.f,nx);
        ny = juce::jlimit(0.f,1.f,ny);

        float px=b.getX()+nx*b.getWidth();
        float py=b.getBottom()-ny*b.getHeight();
        g.setColour(Pal::Vocal.withAlpha(.15f));
        g.drawHorizontalLine(int(py),b.getX(),b.getRight());
        g.drawVerticalLine(int(px),b.getY(),b.getBottom());
        g.setColour(Pal::Vocal); g.fillEllipse(px-6,py-6,12,12);
        g.setColour(Pal::Void);  g.fillEllipse(px-2.5f,py-2.5f,5,5);
        g.setFont(Fonts::label(10.f)); g.setColour(Pal::TextFaint);
        g.drawText(xl_,b.getRight()-60,b.getBottom()-14,60,14,juce::Justification::centredRight);
        g.drawText(yl_,b.getX(),b.getY(),60,14,juce::Justification::centredLeft);

        // Vowel labels at 1/5 positions along X axis
        static const char* vowels[] = {"A","E","I","O","U"};
        g.setColour(Pal::TextFaint);
        for (int i=0;i<5;++i) {
            float vx = b.getX() + (float(i)/4.f) * b.getWidth();
            g.drawText(vowels[i], int(vx)-6, int(b.getBottom())-12, 12, 12,
                       juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { update(e); }
    void mouseDrag(const juce::MouseEvent& e) override { update(e); }

private:
    juce::Slider xs_, ys_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xa_, ya_;
    juce::String xl_{"VOWEL"}, yl_{"OPEN"};
    float xMin_=0.f, xMax_=1.f, yMin_=0.f, yMax_=1.f;

    void update(const juce::MouseEvent& e) {
        auto b=getLocalBounds().toFloat().reduced(6);
        // Map pixel to 0–1, then scale to actual parameter range
        float nx = juce::jlimit(0.f,1.f,(e.position.x-b.getX())/b.getWidth());
        float ny = juce::jlimit(0.f,1.f,1.f-(e.position.y-b.getY())/b.getHeight());
        xs_.setValue(xMin_ + nx*(xMax_-xMin_), juce::sendNotificationAsync);
        ys_.setValue(yMin_ + ny*(yMax_-yMin_), juce::sendNotificationAsync);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ENVELOPE DISPLAY
// ─────────────────────────────────────────────────────────────────────────────
class EnvDisplay : public juce::Component {
public:
    void set(float a, float d, float s, float r) { a_=a; d_=d; s_=s; r_=r; repaint(); }
    void paint(juce::Graphics& g) override {
        auto b=getLocalBounds().toFloat().reduced(3);
        AncientLAF::panel(g,b); b=b.reduced(8,6);
        float tot=a_+d_+.2f+r_, W=b.getWidth(), top=b.getY(), bot=b.getBottom();
        float x1=b.getX()+a_/tot*W, x2=x1+d_/tot*W, x3=x2+.2f/tot*W, x4=x3+r_/tot*W;
        float sy=top+(1.f-s_)*b.getHeight();
        juce::Path p; p.startNewSubPath(b.getX(),bot);
        p.lineTo(x1,top);p.lineTo(x2,sy);p.lineTo(x3,sy);p.lineTo(x4,bot);
        juce::Path fill=p; fill.lineTo(b.getX(),bot); fill.closeSubPath();
        g.setColour(Pal::Vocal.withAlpha(.07f)); g.fillPath(fill);
        g.setColour(Pal::Vocal.withAlpha(.75f)); g.strokePath(p,juce::PathStrokeType(1.5f));
        g.setColour(Pal::Vocal);
        for (auto[px,py]:std::initializer_list<std::pair<float,float>>{{x1,top},{x2,sy},{x3,sy}})
            g.fillEllipse(px-2.5f,py-2.5f,5,5);
        g.setFont(Fonts::mono(9.f)); g.setColour(Pal::TextFaint);
        g.drawText("ENV", b.reduced(2).toNearestInt(), juce::Justification::topLeft);
    }
private: float a_=.05f,d_=.2f,s_=.85f,r_=1.2f;
};
