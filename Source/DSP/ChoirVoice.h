#pragma once
#include <JuceHeader.h>
#include "GlottalSource.h"
#include "FormantFilter.h"
#include "HumanizeEngine.h"
#include "../Data/IntervalTables.h"

// ─────────────────────────────────────────────────────────────────────────────
//  CHOIR VOICE
//  One complete voice: glottal oscillator → formant filter bank.
//  Includes its own amplitude envelope (ADSR).
//  The ChoirEngine manages up to 8 of these.
// ─────────────────────────────────────────────────────────────────────────────

class ChoirVoice {
public:
    // ── Lifecycle ────────────────────────────────────────────────────────────
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        glottal_.prepare(sampleRate);
        formant_.prepare(sampleRate);
        env_.setSampleRate(sampleRate);
        env_.setParameters({ 0.01f, 0.1f, 0.9f, 0.5f });
    }

    // Called by ChoirEngine when a note is triggered for this voice slot
    void noteOn(int midiNote, float velocity,
                float intervalRatio,
                int voiceIndex,
                HumanizeEngine& humanizer)
    {
        midiNote_      = midiNote;
        velocity_      = velocity;
        intervalRatio_ = intervalRatio;
        voiceIndex_    = voiceIndex;
        active_        = true;
        releaseFlag_   = false;

        baseFreqHz_    = midiToHz(midiNote) * intervalRatio; // cache — avoid exp2 per sample

        glottal_.setFrequency(baseFreqHz_);
        glottal_.reset();
        formant_.reset();
        env_.reset();
        env_.noteOn();
        humanizer.triggerVoice(voiceIndex);
    }

    void noteOff(HumanizeEngine& humanizer) {
        env_.noteOff();
        humanizer.releaseVoice(voiceIndex_);
        releaseFlag_ = true;
    }

    bool isActive()  const { return active_; }
    bool isPlaying() const { return active_ && env_.isActive(); }
    int  getMidiNote() const { return midiNote_; }

    // ── Parameter setters (forwarded from ChoirEngine) ────────────────────
    void setBreathe(float b)   { glottal_.setBreathe(b); }
    void setRoughness(float r) { glottal_.setRoughness(r); }
    void setOpenness(float o)  { glottal_.setOpenness(o); formant_.setOpenness(o); }
    void setGender(float g)    { formant_.setGender(g); }
    void setVowelPos(float v)  { formant_.setVowelPos(v); }
    void setEra(AncientEra e)  { formant_.setEra(e); }

    void setEnvelope(float a, float d, float s, float r) {
        env_.setParameters({a, d, s, r});
    }

    // ── Per-sample render ─────────────────────────────────────────────────
    // humanization is passed in from ChoirEngine which owns the HumanizeEngine
    float tick(const VoiceHumanization& hum) {
        if (!active_) return 0.f;

        float envVal = env_.getNextSample();
        if (!env_.isActive() && releaseFlag_) {
            active_ = false;
            return 0.f;
        }

        // Apply pitch offset from humanizer (cents → ratio)
        float pitchRatio = std::exp2(hum.pitchOffsetCents / 1200.f);
        glottal_.setFrequency(baseFreqHz_ * pitchRatio);

        float glottalOut = glottal_.tick();
        float formantOut = formant_.tick(glottalOut);

        float amp = envVal * velocity_ * hum.ampOffset * 0.25f;
        return formantOut * amp;
    }

    // Stereo pan position (-1.0 left → +1.0 right)
    float getPan() const { return pan_; }
    void  setPan(float p) { pan_ = juce::jlimit(-1.f,1.f,p); }

private:
    double sr_           = 44100.0;
    GlottalSource glottal_;
    FormantFilter formant_;
    juce::ADSR    env_;

    int   midiNote_      = 60;
    float velocity_      = 1.f;
    float intervalRatio_ = 1.f;
    float baseFreqHz_    = 440.f;  // cached at noteOn, avoids exp2 per sample
    int   voiceIndex_    = 0;
    float pan_           = 0.f;
    bool  active_        = false;
    bool  releaseFlag_   = false;

    static float midiToHz(int note) {
        return 440.f * std::exp2(float(note - 69) / 12.f);
    }
};
