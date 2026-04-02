#pragma once
#include <JuceHeader.h>
#include <vector>
#include <random>

// ─────────────────────────────────────────────────────────────────────────────
//  VOCAL CHORUS
//  Per-copy LFO-modulated delay line (0.5–25ms) giving natural choir
//  thickness. Each copy has its own LFO rate, phase, and depth so no two
//  copies sound alike. Much cleaner than pitch-shifting — no buffer
//  discontinuities or glitches.
//
//  Used in PluginProcessor to pre-process each copy's input before
//  it enters the formant filter.
// ─────────────────────────────────────────────────────────────────────────────

class VocalChorus {
public:
    static constexpr int kMaxCopies = 8;
    static constexpr int kBufSize   = 4096;  // ~93ms at 44100

    void prepare(double sampleRate) {
        sr_ = sampleRate;
        for (int i = 0; i < kMaxCopies; ++i) {
            buf_[i].assign(kBufSize, 0.f);
            writePos_[i] = 0;
            // Give each copy unique LFO parameters
            lfoPhase_[i] = dist01_(rng_) * juce::MathConstants<float>::twoPi;
            lfoRate_[i]  = 0.4f + dist01_(rng_) * 0.8f;   // 0.4–1.2 Hz
            lfoDepth_[i] = 0.4f + dist01_(rng_) * 0.6f;   // relative depth 0.4–1.0
        }
    }

    // depth 0–1: controls modulation depth (0 = dry, 1 = full chorus)
    void setDepth(float d) { depth_ = juce::jlimit(0.f, 1.f, d); }

    // Process one sample for copy i
    // Returns the chorused sample
    float tick(int copyIdx, float x) {
        if (copyIdx < 0 || copyIdx >= kMaxCopies) return x;
        if (depth_ < 0.001f) return x;

        auto& b     = buf_[copyIdx];
        int&  wpos  = writePos_[copyIdx];

        // Write to buffer
        b[size_t(wpos)] = x;

        // LFO modulates delay time between 2ms and 20ms
        float dt    = 1.f / float(sr_);
        lfoPhase_[copyIdx] += juce::MathConstants<float>::twoPi * lfoRate_[copyIdx] * dt;

        float baseDelayMs  = 8.f;
        float modDepthMs   = depth_ * 10.f * lfoDepth_[copyIdx];
        float delayMs      = baseDelayMs + modDepthMs * std::sin(lfoPhase_[copyIdx]);
        float delaySamples = juce::jlimit(1.f, float(kBufSize - 2), float(delayMs * 0.001 * sr_));

        // Interpolated read
        int   d0   = int(delaySamples);
        float frac = delaySamples - float(d0);
        int   r0   = (wpos - d0 + kBufSize) % kBufSize;
        int   r1   = (r0 - 1 + kBufSize) % kBufSize;
        float wet  = b[size_t(r0)] + frac * (b[size_t(r1)] - b[size_t(r0)]);

        // Advance write position
        wpos = (wpos + 1) % kBufSize;

        // Blend: mix dry with chorused
        return x * (1.f - depth_ * 0.5f) + wet * depth_ * 0.5f;
    }

private:
    double sr_ = 44100.0;
    float  depth_ = 0.f;

    std::vector<float> buf_[kMaxCopies];
    int   writePos_[kMaxCopies] = {};
    float lfoPhase_[kMaxCopies] = {};
    float lfoRate_[kMaxCopies]  = {};
    float lfoDepth_[kMaxCopies] = {};

    std::mt19937 rng_{ 42 };  // fixed seed for reproducible voice characters
    std::uniform_real_distribution<float> dist01_{ 0.f, 1.f };
};
