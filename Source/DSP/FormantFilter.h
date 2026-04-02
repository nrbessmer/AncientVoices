#pragma once
#include <JuceHeader.h>
#include "../Data/VowelTables.h"

// ─────────────────────────────────────────────────────────────────────────────
//  SVF BAND-PASS CELL  — Second-order state variable filter, band-pass output
// ─────────────────────────────────────────────────────────────────────────────
class SVFBand {
public:
    void prepare(double sr) { sr_ = sr; reset(); }
    void reset()            { s1_ = s2_ = 0.f; }

    void setParams(float freqHz, float bwHz) {
        float f  = juce::jlimit(20.f, float(sr_) * 0.48f, freqHz);
        float bw = juce::jmax(20.f, bwHz);
        float g  = std::tan(juce::MathConstants<float>::pi * f / float(sr_));
        float k  = f / bw;
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
        return v1;
    }

private:
    double sr_ = 44100.0;
    float  s1_ = 0.f, s2_ = 0.f;
    float  a1_ = 0.f, a2_ = 0.f, a3_ = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  SVF LOW-PASS CELL  — for nasal resonance shaping
// ─────────────────────────────────────────────────────────────────────────────
class SVFLow {
public:
    void prepare(double sr) { sr_ = sr; reset(); }
    void reset()            { s1_ = s2_ = 0.f; }

    void setParams(float freqHz, float Q) {
        float f = juce::jlimit(20.f, float(sr_) * 0.48f, freqHz);
        float g = std::tan(juce::MathConstants<float>::pi * f / float(sr_));
        float k = 1.f / juce::jmax(0.1f, Q);
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
        return v2;  // low-pass output
    }

private:
    double sr_ = 44100.0;
    float  s1_ = 0.f, s2_ = 0.f;
    float  a1_ = 0.f, a2_ = 0.f, a3_ = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  FORMANT FILTER  — 4 parallel SVF band-pass resonators (F1–F4)
//  + nasal body resonance low-pass
//  + smooth vowel morphing via SmoothedValues
// ─────────────────────────────────────────────────────────────────────────────
class FormantFilter {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        for (auto& f : filters_) f.prepare(sampleRate);
        nasal_.prepare(sampleRate);
        for (auto& s : gainSmooth_)  s.reset(sampleRate, 0.015);
        for (auto& s : freqSmooth_)  s.reset(sampleRate, 0.020);
        for (auto& s : bwSmooth_)    s.reset(sampleRate, 0.020);
        updateCoeffs();
    }

    void setVowelPos(float pos)     { vowelPos_    = pos;  dirty_ = true; }
    void setEra(AncientEra era)     { era_         = era;  dirty_ = true; }
    void setOpenness(float o)       { openness_    = juce::jlimit(0.5f,1.5f,o); dirty_ = true; }
    void setGender(float g)         { gender_      = juce::jlimit(0.7f,1.4f,g); dirty_ = true; }
    // Shift entire formant bank by semitones (-12 to +12)
    // Positive = smaller vocal tract (higher formants = brighter/smaller)
    // Negative = larger vocal tract (lower formants = darker/bigger)
    void setFormantShift(float st)  { formantShift_ = juce::jlimit(-12.f,12.f,st); dirty_ = true; }

    float tick(float x) {
        if (dirty_) { updateTargets(); dirty_ = false; }

        // Update smoothed frequencies and bandwidths
        for (int i = 0; i < 4; ++i)
            filters_[i].setParams(freqSmooth_[i].getNextValue(), bwSmooth_[i].getNextValue());

        // Update nasal filter
        nasal_.setParams(nasalFreqSmooth_.getNextValue(), 1.2f);

        // Sum 4 formant resonators
        float out = filters_[0].tick(x) * gainSmooth_[0].getNextValue()
                  + filters_[1].tick(x) * gainSmooth_[1].getNextValue()
                  + filters_[2].tick(x) * gainSmooth_[2].getNextValue()
                  + filters_[3].tick(x) * gainSmooth_[3].getNextValue();

        // Add nasal body resonance — low-pass shaped body tone
        float nasalBody = nasal_.tick(x) * nasalGainSmooth_.getNextValue();
        out += nasalBody;

        // Calibrated output gain: 4 parallel band-pass filters each output
        // roughly 0.25 of input at peak, so we need ~4x to restore unity.
        // 1.5f gives a small boost above unity for presence without clipping.
        return out * 1.5f;
    }

    void reset() {
        for (auto& f : filters_) f.reset();
        nasal_.reset();
    }

private:
    double sr_      = 44100.0;
    SVFBand filters_[4];
    SVFLow  nasal_;

    juce::SmoothedValue<float> gainSmooth_[4];
    juce::SmoothedValue<float> freqSmooth_[4];
    juce::SmoothedValue<float> bwSmooth_[4];
    juce::SmoothedValue<float> nasalFreqSmooth_;
    juce::SmoothedValue<float> nasalGainSmooth_;

    AncientEra era_  = AncientEra::Sumerian;
    float vowelPos_    = 0.f;
    float openness_    = 1.f;
    float gender_      = 1.f;
    float formantShift_= 0.f;  // semitones, -12 to +12
    bool  dirty_       = true;

    // Perceptual gains: F1 dominant, falling off toward F4
    static constexpr float kGains[4] = { 1.0f, 0.75f, 0.45f, 0.22f };

    void updateTargets() {
        FormantPoint fp = interpolateFormant(era_, vowelPos_);

        // Formant shift: semitone offset scales all formant frequencies
        // +12 semitones = 2x frequencies (small child voice)
        // -12 semitones = 0.5x frequencies (giant cave voice)
        float shiftRatio = std::pow(2.f, formantShift_ / 12.f);

        // Apply gender scaling and formant shift
        float freqs[4] = {
            fp.f1 * openness_ * gender_ * shiftRatio,
            fp.f2 * gender_ * shiftRatio,
            fp.f3 * gender_ * shiftRatio,
            fp.f4 * gender_ * shiftRatio
        };
        float bws[4] = { fp.b1, fp.b2, fp.b3, fp.b4 };

        for (int i = 0; i < 4; ++i) {
            freqSmooth_[i].setTargetValue(freqs[i]);
            bwSmooth_[i].setTargetValue(bws[i]);
            gainSmooth_[i].setTargetValue(kGains[i]);
        }

        // Nasal resonance: sits between F1 and F2, shaped by openness
        float nasalFreq = (freqs[0] + freqs[1]) * 0.4f * openness_;
        nasalFreqSmooth_.reset(sr_, 0.02);
        nasalFreqSmooth_.setTargetValue(juce::jlimit(200.f, 1200.f, nasalFreq));
        nasalGainSmooth_.reset(sr_, 0.02);
        nasalGainSmooth_.setTargetValue(0.3f * (2.f - openness_));  // more nasal when closed
    }

    void updateCoeffs() {
        // Initialize smoothers to current values immediately
        nasalFreqSmooth_.reset(sr_, 0.0);
        nasalGainSmooth_.reset(sr_, 0.0);
        for (auto& s : freqSmooth_) s.reset(sr_, 0.0);
        for (auto& s : bwSmooth_)   s.reset(sr_, 0.0);
        for (auto& s : gainSmooth_) s.reset(sr_, 0.0);
        updateTargets();
        // Restore smooth ramp times
        for (auto& s : gainSmooth_)  s.reset(sr_, 0.015);
        for (auto& s : freqSmooth_)  s.reset(sr_, 0.020);
        for (auto& s : bwSmooth_)    s.reset(sr_, 0.020);
        nasalFreqSmooth_.reset(sr_, 0.020);
        nasalGainSmooth_.reset(sr_, 0.020);
        updateTargets();
    }
};
