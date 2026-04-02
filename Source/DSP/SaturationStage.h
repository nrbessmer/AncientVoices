#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
//  SATURATION STAGE
//  Tube-style soft saturation applied to the formant-filtered signal.
//  Adds harmonics, warmth, and presence — makes the vocal transformation
//  sound more organic and less like a digital filter.
//
//  Uses a soft-clip waveshaper: y = x / (1 + |x|) — asymmetric to simulate
//  tube push. Followed by a simple 2-pole resonant high-pass to restore
//  upper harmonics that saturation can dull.
// ─────────────────────────────────────────────────────────────────────────────

class SaturationStage {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        // HPF to restore air after saturation
        float g  = std::tan(juce::MathConstants<float>::pi * 80.f / float(sr_));
        float k  = 0.7f;
        hpA1_ = 1.f / (1.f + g * (g + k));
        hpA2_ = g * hpA1_;
        hpA3_ = g * hpA2_;
        hpS1_ = hpS2_ = 0.f;
    }

    // drive: 0 = clean, 1 = fully saturated
    void setDrive(float d) { drive_ = juce::jlimit(0.f, 1.f, d); }

    float tick(float x) {
        if (drive_ < 0.001f) return x;

        // Pre-gain: boost signal into saturation curve
        float preGain = 1.f + drive_ * 4.f;
        float boosted = x * preGain;

        // Asymmetric soft clip: positive uses tanh, negative uses x/(1+|x|)
        float saturated;
        if (boosted >= 0.f)
            saturated = std::tanh(boosted);
        else
            saturated = boosted / (1.f + std::abs(boosted));

        // Compensate gain — soft clip reduces level
        float compensated = saturated / preGain;

        // HPF to remove mud added by saturation
        float v3 = compensated - hpS2_;
        float v1 = hpA1_ * hpS1_ + hpA2_ * v3;
        float v2 = hpS2_ + hpA2_ * hpS1_ + hpA3_ * v3;
        hpS1_ = 2.f * v1 - hpS1_;
        hpS2_ = 2.f * v2 - hpS2_;
        float hp = compensated - v2;  // high-pass = input - low-pass

        // Blend dry with saturated+air
        return x * (1.f - drive_ * 0.7f) + (hp * 0.4f + compensated * 0.6f) * drive_ * 0.7f + x * (1.f - drive_ * 0.3f);
    }

private:
    double sr_    = 44100.0;
    float  drive_ = 0.f;

    // HPF state
    float hpA1_ = 0.f, hpA2_ = 0.f, hpA3_ = 0.f;
    float hpS1_ = 0.f, hpS2_ = 0.f;
};
