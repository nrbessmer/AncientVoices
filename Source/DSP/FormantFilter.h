#pragma once
#include <JuceHeader.h>
#include "../Data/VowelTables.h"

// ─────────────────────────────────────────────────────────────────────────────
//  SVF BAND-PASS CELL
//  Second-order state variable filter, band-pass output.
//  Used as individual formant resonator.
// ─────────────────────────────────────────────────────────────────────────────
class SVFBand {
public:
    void prepare(double sr) { sr_ = sr; reset(); }
    void reset() { s1_ = s2_ = 0.f; }

    void setParams(float freqHz, float bwHz) {
        float f  = juce::jlimit(20.f, float(sr_) * 0.48f, freqHz);
        float bw = juce::jmax(10.f, bwHz);
        // Bilinear SVF: g = tan(pi*f/sr), k = 1/Q = f/bw (bandwidth in Hz)
        float g  = std::tan(juce::MathConstants<float>::pi * f / float(sr_));
        float k  = f / bw;   // 1/Q
        a1_ = 1.f / (1.f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    float tick(float x) {
        float v3 = x - s2_;
        float v1 = a1_ * s1_ + a2_ * v3;
        float v2 = s2_ + a2_ * s1_ + a3_ * v3;
        s1_ = 2.f * v1 - s1_;
        s2_ = 2.f * v2 - s2_;
        return v1;  // band-pass output
    }

private:
    double sr_ = 44100.0;
    float  s1_ = 0.f, s2_ = 0.f;
    float  a1_ = 0.f, a2_ = 0.f, a3_ = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  FORMANT FILTER
//  4 parallel SVF band-pass filters (F1–F4).
//  Smoothly interpolates between vowel positions and eras.
// ─────────────────────────────────────────────────────────────────────────────
class FormantFilter {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        for (auto& f : filters_) f.prepare(sampleRate);
        // Gain smoothers
        for (auto& s : gainSmooth_) s.reset(sampleRate, 0.02);
        updateCoeffs();
    }

    void setVowelPos(float pos)    { vowelPos_ = pos;  dirty_ = true; }
    void setEra(AncientEra era)    { era_      = era;  dirty_ = true; }
    void setOpenness(float o)      { openness_ = juce::jlimit(0.5f,1.5f,o); dirty_ = true; }
    void setGender(float g)        { gender_   = juce::jlimit(0.7f,1.4f,g); dirty_ = true; }

    float tick(float x) {
        if (dirty_) { updateCoeffs(); dirty_ = false; }
        // Sum 4 formant resonators with perceptual gains
        // F1 loudest, F2 next, F3/F4 softer for naturalness
        float out = filters_[0].tick(x) * gainSmooth_[0].getNextValue()
                  + filters_[1].tick(x) * gainSmooth_[1].getNextValue()
                  + filters_[2].tick(x) * gainSmooth_[2].getNextValue()
                  + filters_[3].tick(x) * gainSmooth_[3].getNextValue();
        return out * 0.5f;  // normalize sum
    }

    void reset() { for (auto& f : filters_) f.reset(); }

private:
    double sr_ = 44100.0;
    SVFBand filters_[4];
    juce::SmoothedValue<float> gainSmooth_[4];
    AncientEra era_    = AncientEra::Sumerian;
    float vowelPos_    = 0.f;
    float openness_    = 1.f;
    float gender_      = 1.f;
    bool  dirty_       = true;  // recalculate coeffs on next tick()

    // Perceptual gains per formant (F1 dominant)
    static constexpr float kGains[4] = { 1.0f, 0.7f, 0.4f, 0.2f };

    void updateCoeffs() {
        FormantPoint fp = interpolateFormant(era_, vowelPos_);

        float freqs[4] = { fp.f1 * openness_ * gender_,
                           fp.f2 * gender_,
                           fp.f3 * gender_,
                           fp.f4 * gender_ };
        float bws[4]   = { fp.b1, fp.b2, fp.b3, fp.b4 };

        for (int i = 0; i < 4; ++i) {
            filters_[i].setParams(freqs[i], bws[i]);
            gainSmooth_[i].setTargetValue(kGains[i]);
        }
    }
};
