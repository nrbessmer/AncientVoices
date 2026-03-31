#pragma once
#include <JuceHeader.h>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  FORMANT SHIMMER
//  Granular pitch-shift (+1 octave) applied to reverb tail.
//  Uses two overlapping grain windows (cross-fading) for smooth upward shift.
//  Adds the "divine/ethereal" overtone layer characteristic of ceremonial
//  spaces (stone cathedrals, caves, temples).
// ─────────────────────────────────────────────────────────────────────────────

class FormantShimmer {
public:
    static constexpr int kGrainMs = 80;   // grain size in ms

    void prepare(double sampleRate) {
        sr_      = sampleRate;
        int gsz  = int(kGrainMs * 0.001 * sampleRate);
        grainSize_ = gsz;
        bufSize_ = gsz * 4;
        buf_.assign(size_t(bufSize_), 0.f);
        writePos_ = 0;
        // Always reset read heads on prepare — prevents stale positions after SR change
        readA_ = 0.f;
        readB_ = float(gsz) * 0.5f;
        winPos_ = 0;
    }

    void setMix(float m)    { mix_   = juce::jlimit(0.f,1.f,m); }
    void setAmount(float a) { amount_= juce::jlimit(0.f,1.f,a); }

    float tick(float x) {
        if (mix_ < 0.001f || grainSize_ <= 0 || bufSize_ <= 0) return x;

        // Write input to buffer
        buf_[size_t(writePos_)] = x;
        writePos_ = (writePos_ + 1) % bufSize_;

        // Pitch shift +1 octave: read at 2x speed
        float readSpeedRatio = 2.0f;

        // Read head A
        int riA = int(readA_); float fracA = readA_ - float(riA);
        int i0A = (writePos_ - grainSize_ + riA + bufSize_) % bufSize_;
        int i1A = (i0A + 1) % bufSize_;
        float sampA = buf_[size_t(i0A)] + fracA * (buf_[size_t(i1A)] - buf_[size_t(i0A)]);

        // Read head B (offset by half grain)
        int riB = int(readB_); float fracB = readB_ - float(riB);
        int i0B = (writePos_ - grainSize_ + riB + bufSize_) % bufSize_;
        int i1B = (i0B + 1) % bufSize_;
        float sampB = buf_[size_t(i0B)] + fracB * (buf_[size_t(i1B)] - buf_[size_t(i0B)]);

        // Hann window cross-fade
        float t     = float(winPos_) / float(grainSize_);
        float winA  = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * t));
        float winB  = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * (t + 0.5f)));
        winB = std::min(winB, 1.f);

        float shifted = sampA * winA + sampB * winB;

        // Advance read heads
        readA_ = std::fmod(readA_ + readSpeedRatio, float(grainSize_));
        readB_ = std::fmod(readB_ + readSpeedRatio, float(grainSize_));
        winPos_ = (winPos_ + 1) % grainSize_;

        // Softly attenuate shimmer so it doesn't overpower
        float shimmerLevel = amount_ * 0.35f;
        return x * (1.f - mix_) + shifted * mix_ * shimmerLevel + x * mix_ * (1.f - shimmerLevel);
    }

private:
    double sr_        = 44100.0;
    std::vector<float> buf_;
    int    bufSize_   = 0;
    int    grainSize_ = 0;
    int    writePos_  = 0;
    float  readA_     = 0.f;
    float  readB_     = 0.f;
    int    winPos_    = 0;
    float  mix_       = 0.f;
    float  amount_    = 0.5f;
};
