#pragma once
#include <JuceHeader.h>
#include <random>

// ─────────────────────────────────────────────────────────────────────────────
//  HUMANIZE ENGINE  v3
//
//  Per-voice amplitude, pitch, and micro-timing modulation making the choir
//  sound like real singers rather than a machine.
//
//  FIX v3:  Added pitchOffsetCents to VoiceHumanization so ChoirVoice can
//           apply per-voice pitch drift (was referenced but never declared).
//
//  Per voice:
//    - Dual LFO amplitude vibrato (primary fast + secondary swell)
//    - Pitch vibrato in cents (+/- 12 cents at full drift)
//    - Breath envelope (slow inhale/exhale cycle unique per voice)
//    - Onset ramp (voice fades in over ~300ms, staggered per voice)
// ─────────────────────────────────────────────────────────────────────────────

struct VoiceHumanization {
    float ampOffset        = 1.f;   // amplitude multiplier
    float pitchOffsetCents = 0.f;   // pitch shift in cents (+/- 12 at full drift)
    float timingDelaySec   = 0.f;   // reserved for future micro-timing
    bool  active           = false;
};

class HumanizeEngine {
public:
    static constexpr int kMaxVoices = 8;

    void prepare(double sampleRate) {
        sr_ = sampleRate;
        for (int i = 0; i < kMaxVoices; ++i)
            initVoice(i);
    }

    void setDrift(float d) { drift_ = juce::jlimit(0.f, 1.f, d); }
    void setChant(float c) { chant_ = juce::jlimit(0.f, 1.f, c); }
    void setBpm(float bpm) { bpm_   = juce::jmax(60.f, bpm); }

    void triggerVoice(int idx) {
        if (idx < 0 || idx >= kMaxVoices) return;
        auto& v = voices_[idx];
        v.lfoPhaseA   = dist01_(rng_) * twoPi;
        v.lfoPhaseB   = dist01_(rng_) * twoPi;
        v.pitchPhase  = dist01_(rng_) * twoPi;
        v.lfoRateA    = 4.2f + dist01_(rng_) * 1.6f;    // 4.2–5.8 Hz vibrato
        v.lfoRateB    = 0.8f + dist01_(rng_) * 1.2f;    // 0.8–2.0 Hz swell
        v.pitchRate   = 4.5f + dist01_(rng_) * 1.0f;    // 4.5–5.5 Hz pitch vibrato
        v.ampBase     = 0.78f + dist01_(rng_) * 0.22f;  // 0.78–1.0
        v.breathPhase = dist01_(rng_) * twoPi;
        v.breathRate  = 0.15f + dist01_(rng_) * 0.25f;
        v.onsetRamp   = 0.f;
        v.onsetDelay  = float(idx) * 0.025f;             // stagger entries 25ms apart
        v.onsetTimer  = 0.f;
        v.active      = true;
    }

    void releaseVoice(int idx) {
        if (idx >= 0 && idx < kMaxVoices) voices_[idx].active = false;
    }

    VoiceHumanization tick(int idx) {
        VoiceHumanization out;
        if (idx < 0 || idx >= kMaxVoices || !voices_[idx].active)
            return out;

        auto& v  = voices_[idx];
        float dt = 1.f / float(sr_);

        // Onset delay stagger
        v.onsetTimer += dt;
        if (v.onsetTimer < v.onsetDelay) {
            out.ampOffset = 0.f;
            out.active    = true;
            return out;
        }

        // Onset ramp — fade in over 300ms
        v.onsetRamp = juce::jmin(1.f, v.onsetRamp + dt / 0.3f);

        // Amplitude LFOs
        v.lfoPhaseA += twoPi * v.lfoRateA * dt;
        float vibratoAmp = drift_ * 0.08f * std::sin(v.lfoPhaseA);

        v.lfoPhaseB += twoPi * v.lfoRateB * dt;
        float swellAmp = drift_ * 0.12f * std::sin(v.lfoPhaseB);

        // Breath cycle
        v.breathPhase += twoPi * v.breathRate * dt;
        float breathMod = 1.f - chant_ * 0.15f * (0.5f + 0.5f * std::sin(v.breathPhase));

        // Pitch vibrato (cents)
        v.pitchPhase += twoPi * v.pitchRate * dt;
        out.pitchOffsetCents = drift_ * 12.f * std::sin(v.pitchPhase);  // +/- 12 cents max

        float ampMod = v.ampBase * (1.f + vibratoAmp + swellAmp) * breathMod * v.onsetRamp;
        out.ampOffset = juce::jlimit(0.f, 1.4f, ampMod);
        out.active    = true;
        return out;
    }

private:
    static constexpr float twoPi = juce::MathConstants<float>::twoPi;

    struct PerVoice {
        float lfoPhaseA  = 0.f, lfoPhaseB = 0.f;
        float lfoRateA   = 4.5f, lfoRateB = 1.0f;
        float pitchPhase = 0.f,  pitchRate = 4.8f;
        float ampBase    = 1.f;
        float breathPhase= 0.f, breathRate = 0.2f;
        float onsetRamp  = 0.f;
        float onsetDelay = 0.f;
        float onsetTimer = 0.f;
        bool  active     = false;
    };

    PerVoice voices_[kMaxVoices];
    double   sr_    = 44100.0;
    float    drift_ = 0.3f;
    float    chant_ = 0.f;
    float    bpm_   = 120.f;

    std::mt19937 rng_{ std::random_device{}() };
    std::uniform_real_distribution<float> dist01_{ 0.f, 1.f };

    void initVoice(int i) {
        voices_[i] = PerVoice{};
        voices_[i].lfoPhaseA  = dist01_(rng_) * twoPi;
        voices_[i].lfoPhaseB  = dist01_(rng_) * twoPi;
        voices_[i].onsetDelay = float(i) * 0.025f;
        voices_[i].active     = true;  // always active as an audio effect
    }
};
