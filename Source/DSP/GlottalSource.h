#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <random>

// ─────────────────────────────────────────────────────────────────────────────
//  GLOTTAL SOURCE — Liljencrants-Fant (LF) vocal fold model
//
//  Generates a single-period glottal pulse waveform approximating the
//  airflow derivative produced by human vocal folds. Blends with breath
//  noise for breathy/airy textures.
//
//  Parameters:
//    freq      — fundamental frequency (Hz)
//    breathe   — 0.0 pure tone → 1.0 pure breath noise
//    roughness — glottal irregularity (vocal fry, creak) 0.0–1.0
//    openness  — open quotient (controls pulse width) 0.3–0.8
// ─────────────────────────────────────────────────────────────────────────────

class GlottalSource {
public:
    void prepare(double sampleRate) {
        sr_    = sampleRate;
        phase_ = 0.f;
        noiseState_ = 0.f;
        jitterPhase_ = 0.f;
        shimmer_ = 0.f;
    }

    void setFrequency(float hz)   { freq_     = hz; }
    void setBreathe(float b)      { breathe_  = juce::jlimit(0.f,1.f,b); }
    void setRoughness(float r)    { roughness_= juce::jlimit(0.f,1.f,r); }
    void setOpenness(float o)     { openness_ = juce::jlimit(0.3f,0.8f,o); }

    // Call once per sample — returns glottal waveform sample
    float tick() {
        // ── pitch with jitter (roughness modulates frequency) ─────────────
        float jitterHz = 0.f;
        if (roughness_ > 0.001f) {
            jitterPhase_ += (2.f * juce::MathConstants<float>::pi * 7.5f) / float(sr_);
            jitterHz = roughness_ * freq_ * 0.015f * std::sin(jitterPhase_);
            // shimmer: slower amplitude modulation
            shimmer_ = roughness_ * 0.08f * std::sin(jitterPhase_ * 0.37f);
        }

        float instantFreq = juce::jmax(20.f, freq_ + jitterHz);
        float phaseInc = instantFreq / float(sr_);
        phase_ += phaseInc;
        if (phase_ >= 1.f) phase_ -= 1.f;

        // ── LF glottal pulse ──────────────────────────────────────────────
        float glottal = lfPulse(phase_, openness_);
        glottal *= (1.f - shimmer_);

        // ── breath noise (pink-ish filtered white noise) ──────────────────
        float noise = pinkNoise();
        noise *= breathe_;

        // ── mix ───────────────────────────────────────────────────────────
        float toneLevel = 1.f - breathe_ * 0.85f;
        return glottal * toneLevel + noise;
    }

    void reset() { phase_ = 0.f; jitterPhase_ = 0.f; shimmer_ = 0.f; }

private:
    double sr_       = 44100.0;
    float  freq_     = 220.f;
    float  breathe_  = 0.1f;
    float  roughness_= 0.05f;
    float  openness_ = 0.6f;
    float  phase_    = 0.f;
    float  jitterPhase_ = 0.f;
    float  shimmer_  = 0.f;
    float  noiseState_  = 0.f;

    // Simplified LF pulse — piecewise sinusoidal approximation
    // Produces the glottal airflow derivative shape
    static float lfPulse(float phase, float Oq) {
        // Open phase: 0 → Oq (sinusoidal rise)
        // Closed phase: Oq → 1.0 (exponential return)
        if (phase < Oq) {
            // sinusoidal open phase
            float t = phase / Oq;
            return std::sin(juce::MathConstants<float>::pi * t)
                 * std::sin(juce::MathConstants<float>::pi * t * 0.5f);
        } else {
            // exponential return (closed phase)
            float t = (phase - Oq) / (1.f - Oq);
            return -0.12f * std::exp(-8.f * t) * (1.f - t);
        }
    }

    // Simple 1-pole pink noise approximation
    float pinkNoise() {
        float white = dist_(rng_);
        noiseState_ = noiseState_ * 0.99f + white * 0.01f;
        return (white + noiseState_ * 3.f) * 0.25f;
    }

    std::mt19937 rng_{ std::random_device{}() };
    std::uniform_real_distribution<float> dist_{ -1.f, 1.f };
};
