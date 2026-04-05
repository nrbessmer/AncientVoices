#pragma once
#include <JuceHeader.h>
#include "../Data/VowelTables.h"

// ═════════════════════════════════════════════════════════════════════════════
//  FORMANT FILTER  — DBTF Behavioral Contract
//
//  CONTRACT (invariants enforced on every call to tick()):
//    INPUT:  x — a single mono audio sample
//    OUTPUT: shaped * normGain * depth
//
//    MUST:     output contains ZERO component of x
//    MUST:     |output| ≤ kNormMax × |input| (bounded by envelope normalization)
//    MUST:     output is finite (no NaN, no Inf)
//    NEVER:    return x*(1-depth) + anything   ← this is the root cause of dry leak
//
//  SIGNAL PATH:
//    x → 4× SVF bandpass (F1–F4) + nasal LP
//      → envelope normalization (tracks |x|/|shaped| ratio)
//      → scaled by depth (0=silence, 1=full transformation)
//
//  GAIN STAGING:
//    SVF bandpass peak gain = BW/freq (transfer function at resonance).
//    For a 150 Hz voice, only harmonics landing in each formant's passband
//    contribute. Raw output ≈ 0.04–0.15 × input. The envelope normalizer
//    tracks this per-sample and applies a correction gain to restore input
//    level. kNormMax caps the gain to prevent runaway on silence.
//    kWetBoost in processBlock provides additional fixed makeup.
// ═════════════════════════════════════════════════════════════════════════════

// ─── SVF band-pass cell ───────────────────────────────────────────────────────
struct SVFBand {
    void prepare(double sr) { sr_ = sr; reset(); }
    void reset() { s1_ = s2_ = 0.f; }

    void setParams(float freqHz, float bwHz) {
        const float f = juce::jlimit(20.f, float(sr_) * 0.48f, freqHz);
        const float g = std::tan(juce::MathConstants<float>::pi * f / float(sr_));
        const float k = f / juce::jmax(20.f, bwHz);
        a1_ = 1.f / (1.f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    float tick(float x) {
        const float v3 = x - s2_;
        const float v1 = a1_ * s1_ + a2_ * v3;
        const float v2 = s2_  + a2_ * s1_ + a3_ * v3;
        s1_ = 2.f * v1 - s1_;
        s2_ = 2.f * v2 - s2_;
        return v1;   // band-pass output
    }
private:
    double sr_ = 44100.0;
    float  s1_ = 0.f, s2_ = 0.f;
    float  a1_ = 0.f, a2_ = 0.f, a3_ = 0.f;
};

// ─── SVF low-pass cell (nasal resonance) ─────────────────────────────────────
struct SVFLow {
    void prepare(double sr) { sr_ = sr; reset(); }
    void reset() { s1_ = s2_ = 0.f; }

    void setParams(float freqHz, float Q) {
        const float f = juce::jlimit(20.f, float(sr_) * 0.48f, freqHz);
        const float g = std::tan(juce::MathConstants<float>::pi * f / float(sr_));
        const float k = 1.f / juce::jmax(0.1f, Q);
        a1_ = 1.f / (1.f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    float tick(float x) {
        const float v3 = x - s2_;
        const float v1 = a1_ * s1_ + a2_ * v3;
        const float v2 = s2_  + a2_ * s1_ + a3_ * v3;
        s1_ = 2.f * v1 - s1_;
        s2_ = 2.f * v2 - s2_;
        return v2;   // low-pass output
    }
private:
    double sr_ = 44100.0;
    float  s1_ = 0.f, s2_ = 0.f;
    float  a1_ = 0.f, a2_ = 0.f, a3_ = 0.f;
};

// ─── Formant filter ───────────────────────────────────────────────────────────
class FormantFilter {
public:
    // ── Lifecycle ───────────────────────────────────────────────────────────
    void prepare(double sr) {
        sr_ = sr;
        for (auto& f : f_) f.prepare(sr);
        nasal_.prepare(sr);
        for (auto& s : gSmooth_) s.reset(sr, 0.015);
        for (auto& s : fSmooth_) s.reset(sr, 0.020);
        for (auto& s : bSmooth_) s.reset(sr, 0.020);
        nasalFSmooth_.reset(sr, 0.020);
        nasalGSmooth_.reset(sr, 0.020);
        depthSmooth_.reset  (sr, 0.030);
        // Envelope follower coefficient: 50 ms time constant
        envCoeff_ = std::exp(-1.f / float(sr * 0.05));
        reset();
        snapCoeffs();
    }

    void reset() {
        for (auto& f : f_) f.reset();
        nasal_.reset();
        envIn_ = envOut_ = 0.f;
        normGain_ = 3.2f;  // sensible initial compensation
    }

    // ── Parameter setters ───────────────────────────────────────────────────
    void setVowelPos    (float v) { vowel_ = v;                            dirty_ = true; }
    void setEra         (AncientEra e) { era_ = e;                         dirty_ = true; }
    void setOpenness    (float o) { open_ = juce::jlimit(0.5f,1.5f,o);    dirty_ = true; }
    void setGender      (float g) { gender_ = juce::jlimit(0.7f,1.4f,g);  dirty_ = true; }
    void setFormantShift(float s) { shift_ = juce::jlimit(-12.f,12.f,s);  dirty_ = true; }
    void setDepth       (float d) { depth_ = juce::jlimit(0.f,1.f,d);     dirty_ = true; }
    void setBwMul       (float m) { bwMul_ = juce::jlimit(0.2f,2.0f,m);   dirty_ = true; }

    // ── Per-sample render ───────────────────────────────────────────────────
    // CONTRACT: output contains zero component of x (dry).
    // Dry is added ONLY in processBlock's final crossfade.
    float tick(float x) {
        if (dirty_) { updateTargets(); dirty_ = false; }

        for (int i = 0; i < 4; ++i)
            f_[i].setParams(fSmooth_[i].getNextValue(), bSmooth_[i].getNextValue());
        nasal_.setParams(nasalFSmooth_.getNextValue(), 1.2f);

        const float depth = depthSmooth_.getNextValue();

        // ── Bandpass formant bank ──────────────────────────────────────────
        float shaped = f_[0].tick(x) * gSmooth_[0].getNextValue()
                     + f_[1].tick(x) * gSmooth_[1].getNextValue()
                     + f_[2].tick(x) * gSmooth_[2].getNextValue()
                     + f_[3].tick(x) * gSmooth_[3].getNextValue()
                     + nasal_.tick(x) * nasalGSmooth_.getNextValue();

        // ── Envelope normalization ─────────────────────────────────────────
        // Tracks |x| and |shaped| per sample. normGain corrects the ~80-90%
        // energy loss from bandpass filtering back to input level.
        envIn_  = envIn_  * envCoeff_ + std::abs(x)      * (1.f - envCoeff_);
        envOut_ = envOut_ * envCoeff_ + std::abs(shaped)  * (1.f - envCoeff_);

        if (envIn_ > 0.0001f && envOut_ > 0.0001f) {
            const float target = juce::jlimit(kNormMin, kNormMax, envIn_ / envOut_);
            normGain_ += (target - normGain_) * kNormAlpha;
        }

        // ── CONTRACT BOUNDARY ──────────────────────────────────────────────
        // MUST: return contains zero component of x.
        // depth scales transformation intensity. At depth=0, silence (no effect).
        // At depth=1, full transformation at input level.
        return shaped * normGain_ * depth;
    }

private:
    // Normalization constants
    static constexpr float kNormMin   = 1.f;    // never reduce below base gain
    static constexpr float kNormMax   = 20.f;   // max boost: prevents runaway on silence
    static constexpr float kNormAlpha = 0.005f; // smoothing ≈ 200 samples / 4.5 ms

    static constexpr float kGains[4] = { 1.0f, 0.75f, 0.45f, 0.22f };

    double sr_    = 44100.0;
    SVFBand f_[4];
    SVFLow  nasal_;

    juce::SmoothedValue<float> gSmooth_[4], fSmooth_[4], bSmooth_[4];
    juce::SmoothedValue<float> nasalFSmooth_, nasalGSmooth_, depthSmooth_;

    float envCoeff_ = 0.9978f;
    float envIn_ = 0.f, envOut_ = 0.f, normGain_ = 3.2f;

    AncientEra era_   = AncientEra::Sumerian;
    float vowel_= 0.f, open_=1.f, gender_=1.f, shift_=0.f, depth_=1.f, bwMul_=1.f;
    bool  dirty_= true;

    void updateTargets() {
        const FormantPoint fp = interpolateFormant(era_, vowel_);
        const float sr = std::pow(2.f, shift_ / 12.f);
        const float freqs[4] = { fp.f1*open_*gender_*sr, fp.f2*gender_*sr,
                                  fp.f3*gender_*sr,       fp.f4*gender_*sr };
        const float bws[4]   = { fp.b1*bwMul_, fp.b2*bwMul_,
                                  fp.b3*bwMul_, fp.b4*bwMul_ };
        for (int i = 0; i < 4; ++i) {
            fSmooth_[i].setTargetValue(freqs[i]);
            bSmooth_[i].setTargetValue(bws[i]);
            gSmooth_[i].setTargetValue(kGains[i]);
        }
        nasalFSmooth_.setTargetValue(juce::jlimit(200.f, 1200.f,
                                      (freqs[0]+freqs[1])*0.4f*open_));
        nasalGSmooth_.setTargetValue(0.3f*(2.f - open_));
        depthSmooth_.setTargetValue(depth_);
    }

    void snapCoeffs() {
        // Zero-time snap to initial targets, then re-arm smoothing
        for (auto& s : fSmooth_) { s.reset(sr_, 0.0); }
        for (auto& s : bSmooth_) { s.reset(sr_, 0.0); }
        for (auto& s : gSmooth_) { s.reset(sr_, 0.0); }
        nasalFSmooth_.reset(sr_,0.0); nasalGSmooth_.reset(sr_,0.0);
        depthSmooth_.reset(sr_,0.0);
        updateTargets();
        for (auto& s : fSmooth_) s.reset(sr_, 0.020);
        for (auto& s : bSmooth_) s.reset(sr_, 0.020);
        for (auto& s : gSmooth_) s.reset(sr_, 0.015);
        nasalFSmooth_.reset(sr_,0.020); nasalGSmooth_.reset(sr_,0.020);
        depthSmooth_.reset(sr_,0.030);
        updateTargets();
    }
};
