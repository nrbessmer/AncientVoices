#pragma once
#include <JuceHeader.h>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  CAVE REVERB  v2 — True-stereo Schroeder reverb with early reflections
//
//  Signal path:
//    Input → Pre-delay → Early Reflections → Parallel Combs → Allpass chain
//    → DC block → Mix
//
//  Early reflections model the discrete echoes off cave walls before the
//  diffuse tail builds up. Makes it sound like a real stone space rather
//  than a generic reverb.
// ─────────────────────────────────────────────────────────────────────────────

class CaveReverb {
public:
    static constexpr int kCombs  = 8;
    static constexpr int kAllPas = 4;
    static constexpr int kER     = 6;   // early reflection taps

    void prepare(double sampleRate) {
        sr_ = sampleRate;

        // Comb filter delay lengths — prime-ish for dense diffusion
        static const float kCombMsL[kCombs] = { 29.7f,37.1f,41.1f,43.7f,47.0f,50.0f,53.3f,56.1f };
        static const float kCombMsR[kCombs] = { 30.1f,37.8f,41.7f,44.5f,47.8f,50.9f,54.1f,56.9f };
        for (int i = 0; i < kCombs; ++i) {
            int lenL = int(kCombMsL[i] * 0.001 * sampleRate) + 1;
            int lenR = int(kCombMsR[i] * 0.001 * sampleRate) + 1;
            combL_[i].assign(size_t(lenL), 0.f); combIdxL_[i] = 0;
            combR_[i].assign(size_t(lenR), 0.f); combIdxR_[i] = 0;
        }

        // Allpass delay lengths
        static const float kApMs[kAllPas] = { 5.0f, 1.7f, 0.77f, 0.35f };
        for (int i = 0; i < kAllPas; ++i) {
            int len = int(kApMs[i] * 0.001 * sampleRate) + 1;
            apL_[i].assign(size_t(len), 0.f); apIdxL_[i] = 0;
            apR_[i].assign(size_t(len), 0.f); apIdxR_[i] = 0;
        }

        // Pre-delay buffer (max 100ms)
        int pdLen = int(0.1 * sampleRate) + 1;
        preDelayL_.assign(size_t(pdLen), 0.f); pdIdxL_ = 0;
        preDelayR_.assign(size_t(pdLen), 0.f); pdIdxR_ = 0;

        // Early reflections — 6 taps per side, different times for L and R
        static const float kERmsL[kER] = { 5.1f, 9.3f, 14.2f, 19.8f, 27.4f, 35.6f };
        static const float kERmsR[kER] = { 6.2f, 10.8f, 16.1f, 22.3f, 29.7f, 38.1f };
        static const float kERgain[kER] = { 0.80f, 0.65f, 0.52f, 0.40f, 0.30f, 0.22f };
        int erBufSize = int(0.05 * sampleRate) + 1;
        erBufL_.assign(size_t(erBufSize), 0.f);
        erBufR_.assign(size_t(erBufSize), 0.f);
        erBufIdx_ = 0;
        for (int i = 0; i < kER; ++i) {
            erTapL_[i]  = int(kERmsL[i] * 0.001 * sampleRate);
            erTapR_[i]  = int(kERmsR[i] * 0.001 * sampleRate);
            erGain_[i]  = kERgain[i];
        }

        dcX_[0] = dcX_[1] = dcY_[0] = dcY_[1] = 0.f;
        updateParams();
    }

    void setMix(float m)       { mix_        = juce::jlimit(0.f, 1.f, m); }
    void setSize(float s)      { size_       = juce::jlimit(0.f, 1.f, s); updateParams(); }
    void setDamp(float d)      { damp_       = juce::jlimit(0.f, 1.f, d); updateParams(); }
    void setPreDelay(float ms) { preDelayMs_ = juce::jlimit(0.f, 100.f, ms); updateParams(); }

    void process(float inL, float inR, float& outL, float& outR) {
        // Write to early reflection buffer
        erBufL_[size_t(erBufIdx_)] = inL;
        erBufR_[size_t(erBufIdx_)] = inR;
        int erSize = int(erBufL_.size());

        // Sum early reflections
        float erL = inL * 0.1f, erR = inR * 0.1f;
        for (int i = 0; i < kER; ++i) {
            int tapL = (erBufIdx_ - erTapL_[i] + erSize) % erSize;
            int tapR = (erBufIdx_ - erTapR_[i] + erSize) % erSize;
            erL += erBufL_[size_t(tapL)] * erGain_[i] * erMix_;
            erR += erBufR_[size_t(tapR)] * erGain_[i] * erMix_;
        }
        erBufIdx_ = (erBufIdx_ + 1) % erSize;

        // Pre-delay
        auto pd = [&](std::vector<float>& buf, int& idx, float in) -> float {
            buf[size_t(idx)] = in;
            int r = (idx - preDelaySamples_ + int(buf.size())) % int(buf.size());
            idx = (idx + 1) % int(buf.size());
            return buf[size_t(r)];
        };
        float dL = pd(preDelayL_, pdIdxL_, erL);
        float dR = pd(preDelayR_, pdIdxR_, erR);

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

        // Allpass diffusers
        for (int i = 0; i < kAllPas; ++i) {
            auto& buf = apL_[i]; int idx = apIdxL_[i];
            float rd = buf[size_t(idx)];
            buf[size_t(idx)] = wetL + rd * 0.5f;
            apIdxL_[i] = (idx + 1) % int(buf.size());
            wetL = -wetL + rd;
        }
        for (int i = 0; i < kAllPas; ++i) {
            auto& buf = apR_[i]; int idx = apIdxR_[i];
            float rd = buf[size_t(idx)];
            buf[size_t(idx)] = wetR + rd * 0.5f;
            apIdxR_[i] = (idx + 1) % int(buf.size());
            wetR = -wetR + rd;
        }

        // DC blocking
        auto dcBlock = [](float x, float& xm1, float& ym1) -> float {
            float y = x - xm1 + 0.995f * ym1;
            xm1 = x; ym1 = y; return y;
        };
        wetL = dcBlock(wetL, dcX_[0], dcY_[0]);
        wetR = dcBlock(wetR, dcX_[1], dcY_[1]);

        outL = inL * (1.f - mix_) + wetL * mix_;
        outR = inR * (1.f - mix_) + wetR * mix_;
    }

private:
    double sr_   = 44100.0;
    float  mix_  = 0.3f, size_ = 0.6f, damp_ = 0.4f, preDelayMs_ = 20.f;
    float  feedback_ = 0.84f, dampCoeff_ = 0.3f;
    float  erMix_    = 0.6f;   // early reflection level
    int    preDelaySamples_ = 0;

    std::vector<float> combL_[kCombs], combR_[kCombs];
    int   combIdxL_[kCombs] = {}, combIdxR_[kCombs] = {};
    float combFiltL_[kCombs] = {}, combFiltR_[kCombs] = {};

    std::vector<float> apL_[kAllPas], apR_[kAllPas];
    int apIdxL_[kAllPas] = {}, apIdxR_[kAllPas] = {};

    std::vector<float> preDelayL_, preDelayR_;
    int pdIdxL_ = 0, pdIdxR_ = 0;

    // Early reflections
    std::vector<float> erBufL_, erBufR_;
    int   erBufIdx_   = 0;
    int   erTapL_[kER] = {}, erTapR_[kER] = {};
    float erGain_[kER] = {};

    float dcX_[2] = {}, dcY_[2] = {};

    void updateParams() {
        feedback_        = 0.78f + size_ * 0.20f;   // 0.78–0.98
        dampCoeff_       = 0.08f + damp_ * 0.65f;   // 0.08–0.73
        erMix_           = 0.4f + size_ * 0.4f;     // more reflections in bigger spaces
        preDelaySamples_ = int(preDelayMs_ * 0.001 * sr_);
    }
};
