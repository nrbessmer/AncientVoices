#pragma once
#include <JuceHeader.h>
#include "UI.h"
#include "../Data/VowelTables.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FORMANT SCOPE
//  Displays F1–F4 as Gaussian peaks on a frequency axis (0–5 kHz).
//  Peaks animate smoothly as vowel position and era change.
//  Waveform strip at bottom shows live audio.
//
//  Performance notes:
//   - Gaussian values cached in member arrays, recomputed only when formants move
//   - No heap allocation in paint()
//   - waveWp_ is unsigned to prevent integer overflow after long sessions
// ─────────────────────────────────────────────────────────────────────────────

class FormantScope : public juce::Component, private juce::Timer {
public:
    static constexpr int kWaveN    = 1024;
    static constexpr int kMaxWidth = 800;   // maximum plot width we pre-allocate for

    FormantScope() {
        // Pre-allocate gaussian cache arrays — no heap alloc in paint()
        for (int fi = 0; fi < 4; ++fi)
            gaussCache_[fi].resize(kMaxWidth, 0.f);
        startTimerHz(30);
    }
    ~FormantScope() override { stopTimer(); }

    void setFormantState(float vowelPos, AncientEra era) {
        targetVowel_ = vowelPos;
        targetEra_   = era;
    }

    void pushSamples(const float* data, int numSamples) {
        for (int i = 0; i < numSamples; ++i)
            waveBuf_[waveWp_++ % kWaveN] = data[i];
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(Pal::Surface);
        g.fillRoundedRectangle(bounds, 4.f);
        g.setColour(Pal::Border);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.f, 1.f);

        auto plotArea  = bounds.reduced(6.f, 6.f).withTrimmedBottom(22.f);
        int  W         = juce::jmin(int(plotArea.getWidth()), kMaxWidth);

        // Grid lines
        static const float kGridHz[]     = { 500.f, 1000.f, 2000.f, 3000.f, 4000.f };
        static const char* kGridLabels[] = { "500", "1k", "2k", "3k", "4k" };
        g.setFont(Fonts::mono(9.f));
        for (int i = 0; i < 5; ++i) {
            float x = hzToX(kGridHz[i], plotArea);
            g.setColour(Pal::Border.withAlpha(0.5f));
            g.drawVerticalLine(int(x), plotArea.getY(), plotArea.getBottom());
            g.setColour(Pal::TextFaint);
            g.drawText(kGridLabels[i], int(x)-14, int(plotArea.getBottom())+2, 28, 12,
                       juce::Justification::centred);
        }

        // Smooth vowel animation
        smoothVowel_ += (targetVowel_ - smoothVowel_) * 0.12f;

        FormantPoint fp = interpolateFormant(targetEra_, smoothVowel_);
        const float freqs[4] = { fp.f1, fp.f2, fp.f3, fp.f4 };
        const float bws[4]   = { fp.b1, fp.b2, fp.b3, fp.b4 };
        static const float kGainScale[4] = { 1.0f, 0.75f, 0.5f, 0.3f };

        static const juce::Colour kCols[4] = {
            Pal::Amber, Pal::Sage, Pal::Teal, Pal::Purple
        };
        static const char* kLabels[4] = { "F1","F2","F3","F4" };

        // Recompute Gaussian cache (only if width changed since last paint)
        if (W != lastW_) {
            lastW_ = W;
            cacheValid_ = false;
        }
        if (!cacheValid_) {
            for (int fi = 0; fi < 4; ++fi) {
                float cx    = hzToX(freqs[fi], plotArea) - plotArea.getX();
                float sigma = juce::jmax(6.f, (bws[fi] / 5000.f) * float(W) * 2.f);
                for (int x = 0; x < W; ++x) {
                    float dx = float(x) - cx;
                    gaussCache_[fi][size_t(x)] = std::exp(-(dx*dx) / (2.f*sigma*sigma))
                                                 * kGainScale[fi];
                }
            }
            cacheValid_ = true;
        }

        // Draw each formant — one pass per formant (fill + stroke in single path)
        for (int fi = 0; fi < 4; ++fi) {
            const auto& gc = gaussCache_[fi];
            float ph = plotArea.getHeight() * 0.82f;

            juce::Path fillPath;
            fillPath.startNewSubPath(plotArea.getX(), plotArea.getBottom());
            for (int x = 0; x < W; ++x)
                fillPath.lineTo(plotArea.getX() + float(x),
                                plotArea.getBottom() - gc[size_t(x)] * ph);
            fillPath.lineTo(plotArea.getX() + float(W), plotArea.getBottom());
            fillPath.closeSubPath();

            g.setColour(kCols[fi].withAlpha(0.08f));
            g.fillPath(fillPath);

            // Stroke: re-trace the top edge only
            juce::Path strokePath;
            strokePath.startNewSubPath(plotArea.getX(),
                                       plotArea.getBottom() - gc[0] * ph);
            for (int x = 1; x < W; ++x)
                strokePath.lineTo(plotArea.getX() + float(x),
                                  plotArea.getBottom() - gc[size_t(x)] * ph);
            g.setColour(kCols[fi].withAlpha(0.7f));
            g.strokePath(strokePath, juce::PathStrokeType(1.5f));

            // Label at peak frequency
            float labelX = hzToX(freqs[fi], plotArea);
            g.setFont(Fonts::label(9.5f));
            g.setColour(kCols[fi]);
            g.drawText(kLabels[fi], int(labelX)-10,
                       int(plotArea.getBottom() - ph - 14.f), 20, 12,
                       juce::Justification::centred);
        }

        // Waveform strip
        auto waveArea = bounds.reduced(6.f, 4.f).removeFromBottom(16.f);
        int  wn       = int(waveArea.getWidth());
        juce::Path wave;
        unsigned base = waveWp_ - unsigned(wn);
        wave.startNewSubPath(waveArea.getX(),
                             waveArea.getCentreY() - waveBuf_[base % kWaveN] * waveArea.getHeight() * 0.45f);
        for (int i = 1; i < wn; ++i) {
            float s = waveBuf_[(base + unsigned(i)) % kWaveN];
            wave.lineTo(waveArea.getX() + float(i),
                        waveArea.getCentreY() - s * waveArea.getHeight() * 0.45f);
        }
        g.setColour(Pal::Amber.withAlpha(0.45f));
        g.strokePath(wave, juce::PathStrokeType(1.f));

        // Title
        g.setFont(Fonts::mono(9.f));
        g.setColour(Pal::TextFaint);
        g.drawText("FORMANT SCOPE", bounds.reduced(6,4).toNearestInt(),
                   juce::Justification::topLeft);
    }

    void timerCallback() override {
        cacheValid_ = false;  // invalidate so formant animation updates each tick
        repaint();
    }

private:
    float smoothVowel_     = 0.f;
    float targetVowel_     = 0.f;
    AncientEra targetEra_  = AncientEra::Sumerian;

    float    waveBuf_[kWaveN] = {};
    unsigned waveWp_          = 0;   // unsigned prevents overflow crash

    // Gaussian cache — pre-allocated, recomputed when width changes or formants move
    std::vector<float> gaussCache_[4];
    bool cacheValid_ = false;
    int  lastW_      = 0;

    static float hzToX(float hz, const juce::Rectangle<float>& area) {
        return area.getX() + (juce::jlimit(0.f, 5000.f, hz) / 5000.f) * area.getWidth();
    }
};
