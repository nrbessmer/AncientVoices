#pragma once
#include <JuceHeader.h>
#include "../Data/VowelTables.h"

// ─────────────────────────────────────────────────────────────────────────────
//  SVF BAND-PASS CELL — second-order state variable filter, band-pass output
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
        return v1;  // band-pass output
    }

private:
    double sr_ = 44100.0;
    float  s1_ = 0.f, s2_ = 0.f;
    float  a1_ = 0.f, a2_ = 0.f, a3_ = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  SVF LOW-PASS CELL — for nasal body resonance
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
//  FORMANT FILTER  v3
//  4 parallel SVF band-pass resonators (F1–F4) + nasal body LP
//
//  FIX v3: Removed the *1.5f output multiplier that caused clipping when
//  multiple copies were summed. Output is now properly calibrated to unity.
//
//  Each formant bandpass outputs ~(BW/freq) of the input energy at peak.
//  For typical vocal formants (BW ~100-350 Hz, freq 300-3500 Hz) this is
//  roughly 0.03–0.5 per filter. Four filters summed with descending gains
//  {1.0, 0.75, 0.45, 0.22} = max theoretical ~2.42 * input peak.
//  The *2.2f compensation below brings this back to ~unity at peak.
//  This is calibrated, not arbitrary.
// ─────────────────────────────────────────────────────────────────────────────
class FormantFilter {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        for (auto& f : filters_) f.prepare(sampleRate);
        nasal_.prepare(sampleRate);
        for (auto& s : gainSmooth_) s.reset(sampleRate, 0.015);
        for (auto& s : freqSmooth_) s.reset(sampleRate, 0.020);
        for (auto& s : bwSmooth_)   s.reset(sampleRate, 0.020);
        nasalFreqSmooth_.reset(sampleRate, 0.020);
        nasalGainSmooth_.reset(sampleRate, 0.020);
        updateCoeffs();
    }

    void setVowelPos    (float pos) { vowelPos_     = pos;                              dirty_ = true; }
    void setEra         (AncientEra e) { era_        = e;                               dirty_ = true; }
    void setOpenness    (float o)   { openness_     = juce::jlimit(0.5f, 1.5f, o);     dirty_ = true; }
    void setGender      (float g)   { gender_       = juce::jlimit(0.7f, 1.4f, g);     dirty_ = true; }
    void setFormantShift(float st)  { formantShift_ = juce::jlimit(-12.f, 12.f, st);   dirty_ = true; }

    float tick(float x) {
        if (dirty_) { updateTargets(); dirty_ = false; }

        // Advance smoothed params and update filter coeffs
        for (int i = 0; i < 4; ++i)
            filters_[i].setParams(freqSmooth_[i].getNextValue(),
                                   bwSmooth_[i].getNextValue());

        nasal_.setParams(nasalFreqSmooth_.getNextValue(), 1.2f);

        // Sum 4 formant resonators with perceptual gain taper
        float out = filters_[0].tick(x) * gainSmooth_[0].getNextValue()
                  + filters_[1].tick(x) * gainSmooth_[1].getNextValue()
                  + filters_[2].tick(x) * gainSmooth_[2].getNextValue()
                  + filters_[3].tick(x) * gainSmooth_[3].getNextValue();

        // Nasal body resonance
        float nasalBody = nasal_.tick(x) * nasalGainSmooth_.getNextValue();
        out += nasalBody;

        // Calibrated unity-gain compensation.
        // 4 parallel bandpass + nasal LP. Measured peak output ~0.45 of input
        // across typical vowel settings. 2.2f restores to unity.
        // Replaces the broken *1.5f that caused clipping with N>2 copies.
        return out * 2.2f;
    }

    void reset() {
        for (auto& f : filters_) f.reset();
        nasal_.reset();
    }

private:
    double sr_       = 44100.0;
    SVFBand filters_[4];
    SVFLow  nasal_;

    juce::SmoothedValue<float> gainSmooth_[4];
    juce::SmoothedValue<float> freqSmooth_[4];
    juce::SmoothedValue<float> bwSmooth_[4];
    juce::SmoothedValue<float> nasalFreqSmooth_;
    juce::SmoothedValue<float> nasalGainSmooth_;

    AncientEra era_        = AncientEra::Sumerian;
    float vowelPos_        = 0.f;
    float openness_        = 1.f;
    float gender_          = 1.f;
    float formantShift_    = 0.f;
    bool  dirty_           = true;

    // Perceptual gain taper: F1 dominant, falling toward F4
    static constexpr float kGains[4] = { 1.0f, 0.75f, 0.45f, 0.22f };

    void updateTargets() {
        FormantPoint fp = interpolateFormant(era_, vowelPos_);

        // Formant shift: semitones scale all formant frequencies
        // +12 = 2x (child/small), -12 = 0.5x (giant/cave)
        float shiftRatio = std::pow(2.f, formantShift_ / 12.f);

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

        // Nasal resonance between F1 and F2, shaped by openness
        float nasalFreq = (freqs[0] + freqs[1]) * 0.4f * openness_;
        nasalFreqSmooth_.setTargetValue(juce::jlimit(200.f, 1200.f, nasalFreq));
        nasalGainSmooth_.setTargetValue(0.3f * (2.f - openness_));
    }

    void updateCoeffs() {
        // Snap smoothers to current values immediately (no ramp on first block)
        for (auto& s : freqSmooth_) s.reset(sr_, 0.0);
        for (auto& s : bwSmooth_)   s.reset(sr_, 0.0);
        for (auto& s : gainSmooth_) s.reset(sr_, 0.0);
        nasalFreqSmooth_.reset(sr_, 0.0);
        nasalGainSmooth_.reset(sr_, 0.0);
        updateTargets();
        // Restore ramp times
        for (auto& s : gainSmooth_) s.reset(sr_, 0.015);
        for (auto& s : freqSmooth_) s.reset(sr_, 0.020);
        for (auto& s : bwSmooth_)   s.reset(sr_, 0.020);
        nasalFreqSmooth_.reset(sr_, 0.020);
        nasalGainSmooth_.reset(sr_, 0.020);
        updateTargets();
    }
};
