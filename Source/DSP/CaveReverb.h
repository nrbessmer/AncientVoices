#pragma once
#include <JuceHeader.h>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  CAVE REVERB  — True-stereo Schroeder reverb for stone/cave acoustics.
//  L and R channels have separate comb filter banks (slightly different delay
//  lengths) for natural stereo decorrelation. A shared all-pass diffuser
//  section adds density. DC-blocking highpass prevents low-frequency build-up.
// ─────────────────────────────────────────────────────────────────────────────

class CaveReverb {
public:
    static constexpr int kCombs  = 8;
    static constexpr int kAllPas = 4;

    void prepare(double sampleRate) {
        sr_ = sampleRate;
        // L comb delays (ms) — prime-ish lengths for dense diffusion
        static const float kCombMsL[kCombs] = { 29.7f,37.1f,41.1f,43.7f,47.0f,50.0f,53.3f,56.1f };
        // R comb delays slightly offset for stereo decorrelation
        static const float kCombMsR[kCombs] = { 30.1f,37.8f,41.7f,44.5f,47.8f,50.9f,54.1f,56.9f };
        for (int i = 0; i < kCombs; ++i) {
            int lenL = int(kCombMsL[i] * 0.001 * sampleRate) + 1;
            int lenR = int(kCombMsR[i] * 0.001 * sampleRate) + 1;
            combL_[i].assign(size_t(lenL), 0.f); combIdxL_[i] = 0;
            combR_[i].assign(size_t(lenR), 0.f); combIdxR_[i] = 0;
        }
        static const float kApMs[kAllPas] = { 5.0f, 1.7f, 0.77f, 0.35f };
        for (int i = 0; i < kAllPas; ++i) {
            int len = int(kApMs[i] * 0.001 * sampleRate) + 1;
            apL_[i].assign(size_t(len), 0.f); apIdxL_[i] = 0;
            apR_[i].assign(size_t(len), 0.f); apIdxR_[i] = 0;
        }
        int pdLen = int(0.1 * sampleRate) + 1;
        preDelayL_.assign(size_t(pdLen), 0.f); pdIdxL_ = 0;
        preDelayR_.assign(size_t(pdLen), 0.f); pdIdxR_ = 0;
        // DC-blocker state
        dcX_[0] = dcX_[1] = dcY_[0] = dcY_[1] = 0.f;
        updateParams();
    }

    void setMix(float m)       { mix_        = juce::jlimit(0.f, 1.f, m); }
    void setSize(float s)      { size_       = juce::jlimit(0.f, 1.f, s); updateParams(); }
    void setDamp(float d)      { damp_       = juce::jlimit(0.f, 1.f, d); updateParams(); }
    void setPreDelay(float ms) { preDelayMs_ = juce::jlimit(0.f, 100.f, ms); updateParams(); }

    void process(float inL, float inR, float& outL, float& outR) {
        // Pre-delay
        auto applyPreDelay = [&](std::vector<float>& buf, int& idx, float in) -> float {
            buf[size_t(idx)] = in;
            int r = (idx - preDelaySamples_ + int(buf.size())) % int(buf.size());
            idx = (idx + 1) % int(buf.size());
            return buf[size_t(r)];
        };
        float dL = applyPreDelay(preDelayL_, pdIdxL_, inL);
        float dR = applyPreDelay(preDelayR_, pdIdxR_, inR);

        // Parallel combs — L
        float wetL = 0.f;
        for (int i = 0; i < kCombs; ++i) {
            auto& buf = combL_[i]; int idx = combIdxL_[i];
            float rd = buf[size_t(idx)];
            combFiltL_[i] = rd * (1.f - dampCoeff_) + combFiltL_[i] * dampCoeff_;
            buf[size_t(idx)] = dL + combFiltL_[i] * feedback_;
            combIdxL_[i] = (idx + 1) % int(buf.size());
            wetL += rd;
        }
        wetL *= (1.f / kCombs);

        // Parallel combs — R
        float wetR = 0.f;
        for (int i = 0; i < kCombs; ++i) {
            auto& buf = combR_[i]; int idx = combIdxR_[i];
            float rd = buf[size_t(idx)];
            combFiltR_[i] = rd * (1.f - dampCoeff_) + combFiltR_[i] * dampCoeff_;
            buf[size_t(idx)] = dR + combFiltR_[i] * feedback_;
            combIdxR_[i] = (idx + 1) % int(buf.size());
            wetR += rd;
        }
        wetR *= (1.f / kCombs);

        // All-pass diffusers — L
        for (int i = 0; i < kAllPas; ++i) {
            auto& buf = apL_[i]; int idx = apIdxL_[i];
            float rd = buf[size_t(idx)];
            buf[size_t(idx)] = wetL + rd * 0.5f;
            apIdxL_[i] = (idx + 1) % int(buf.size());
            wetL = -wetL + rd;
        }
        // All-pass diffusers — R
        for (int i = 0; i < kAllPas; ++i) {
            auto& buf = apR_[i]; int idx = apIdxR_[i];
            float rd = buf[size_t(idx)];
            buf[size_t(idx)] = wetR + rd * 0.5f;
            apIdxR_[i] = (idx + 1) % int(buf.size());
            wetR = -wetR + rd;
        }

        // DC blocking (one-pole highpass, R=0.995)
        auto dcBlock = [&](float x, float& xm1, float& ym1) -> float {
            float y = x - xm1 + 0.995f * ym1;
            xm1 = x; ym1 = y; return y;
        };
        wetL = dcBlock(wetL, dcX_[0], dcY_[0]);
        wetR = dcBlock(wetR, dcX_[1], dcY_[1]);

        outL = inL * (1.f - mix_) + wetL * mix_;
        outR = inR * (1.f - mix_) + wetR * mix_;
    }

private:
    double sr_ = 44100.0;
    float  mix_ = 0.3f, size_ = 0.6f, damp_ = 0.4f, preDelayMs_ = 20.f;
    float  feedback_ = 0.8f, dampCoeff_ = 0.3f;
    int    preDelaySamples_ = 0;

    std::vector<float> combL_[kCombs], combR_[kCombs];
    int   combIdxL_[kCombs] = {}, combIdxR_[kCombs] = {};
    float combFiltL_[kCombs] = {}, combFiltR_[kCombs] = {};

    std::vector<float> apL_[kAllPas], apR_[kAllPas];
    int apIdxL_[kAllPas] = {}, apIdxR_[kAllPas] = {};

    std::vector<float> preDelayL_, preDelayR_;
    int pdIdxL_ = 0, pdIdxR_ = 0;

    float dcX_[2] = {}, dcY_[2] = {};   // DC blocker state L, R

    void updateParams() {
        feedback_        = 0.70f + size_ * 0.25f;
        dampCoeff_       = 0.15f + damp_ * 0.70f;
        preDelaySamples_ = int(preDelayMs_ * 0.001 * sr_);
    }
};
