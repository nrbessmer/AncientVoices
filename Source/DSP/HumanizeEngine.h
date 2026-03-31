#pragma once
#include <JuceHeader.h>
#include <random>

// ─────────────────────────────────────────────────────────────────────────────
//  HUMANIZE ENGINE
//  Generates per-voice randomized offsets that make the choir sound like
//  real human singers rather than a synth stack.
//
//  Per voice:
//    - Slow pitch drift LFO (unique rate/phase per voice)
//    - Timing offset (voice enters slightly late)
//    - Breath envelope micro-variation (each voice breathes at own pace)
//    - Vibrato onset (vibrato depth increases after note attack settles)
// ─────────────────────────────────────────────────────────────────────────────

struct VoiceHumanization {
    float pitchOffsetCents = 0.f;   // instantaneous pitch offset
    float ampOffset        = 1.f;   // amplitude scalar 0.8–1.0
    float timingDelaySec   = 0.f;   // initial attack delay (samples)
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

    // Called when drift/jitter amount changes (0.0 = robot, 1.0 = very human)
    void setDrift(float d)  { drift_  = juce::jlimit(0.f,1.f,d); }
    void setChant(float c)  { chant_  = juce::jlimit(0.f,1.f,c); }
    void setBpm(float bpm)  { bpm_    = juce::jmax(60.f,bpm); }

    // Randomize parameters for a newly triggered voice
    void triggerVoice(int voiceIdx) {
        if (voiceIdx < 0 || voiceIdx >= kMaxVoices) return;
        auto& v = voices_[voiceIdx];
        v.driftPhase    = dist01_(rng_) * juce::MathConstants<float>::twoPi;
        v.driftRate     = 3.5f + dist01_(rng_) * 2.5f;    // 3.5–6 Hz vibrato
        v.driftRandScale= dist01_(rng_);                    // unique randomness per voice
        v.ampBase       = 0.82f + dist01_(rng_) * 0.18f;
        v.breathPhase   = dist01_(rng_) * juce::MathConstants<float>::twoPi;
        v.breathRate    = 0.2f + dist01_(rng_) * 0.3f;
        v.vibratoOnset  = 0.f;
        v.active        = true;
    }

    void releaseVoice(int voiceIdx) {
        if (voiceIdx >= 0 && voiceIdx < kMaxVoices)
            voices_[voiceIdx].active = false;
    }

    // Returns instantaneous humanization for a voice (call once per sample)
    VoiceHumanization tick(int voiceIdx) {
        VoiceHumanization out;
        if (voiceIdx < 0 || voiceIdx >= kMaxVoices || !voices_[voiceIdx].active)
            return out;

        auto& v = voices_[voiceIdx];
        float dt = 1.f / float(sr_);

        // Vibrato onset — ramp up over ~600ms after note trigger
        v.vibratoOnset = juce::jmin(1.f, v.vibratoOnset + dt / 0.6f);

        // Pitch drift: depth computed from current drift_ so knob affects held notes
        v.driftPhase += juce::MathConstants<float>::twoPi * v.driftRate * dt;
        float liveDepth  = drift_ * (8.f + v.driftRandScale * 12.f);  // cents
        float pitchCents = std::sin(v.driftPhase) * liveDepth * v.vibratoOnset;

        // Chant pulse — rhythmic amplitude swell tied to BPM
        float chantPhaseInc = (bpm_ / 60.f) * juce::MathConstants<float>::twoPi * dt;
        v.breathPhase += chantPhaseInc * v.breathRate;
        float breathAmp = 1.f - chant_ * 0.12f * (0.5f + 0.5f * std::sin(v.breathPhase));

        out.pitchOffsetCents = pitchCents;
        out.ampOffset        = v.ampBase * breathAmp;
        out.active           = true;
        return out;
    }

private:
    struct PerVoice {
        float driftPhase    = 0.f;
        float driftRate     = 4.f;
        float driftRandScale= 0.5f;  // random 0-1, depth scaled by live drift_ each tick
        float ampBase       = 1.f;
        float breathPhase   = 0.f;
        float breathRate    = 0.25f;
        float vibratoOnset  = 0.f;
        bool  active        = false;
    };

    PerVoice voices_[kMaxVoices];
    double sr_    = 44100.0;
    float  drift_ = 0.3f;
    float  chant_ = 0.f;
    float  bpm_   = 120.f;

    std::mt19937 rng_{ std::random_device{}() };
    std::uniform_real_distribution<float> dist01_{ 0.f, 1.f };

    void initVoice(int i) {
        voices_[i] = PerVoice{};
        voices_[i].driftPhase = dist01_(rng_) * juce::MathConstants<float>::twoPi;
    }
};
