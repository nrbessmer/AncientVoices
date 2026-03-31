#pragma once
#include <JuceHeader.h>
#include "ChoirVoice.h"
#include "HumanizeEngine.h"
#include "../Data/IntervalTables.h"

// ─────────────────────────────────────────────────────────────────────────────
//  CHOIR ENGINE
//  Manages up to 8 polyphonic ChoirVoice instances.
//  Each MIDI note triggers numVoices_ simultaneous voice slots,
//  each assigned a different interval ratio from IntervalTables.
//  Voices are spread across the stereo field.
// ─────────────────────────────────────────────────────────────────────────────

class ChoirEngine {
public:
    static constexpr int kMaxVoices = 8;

    void prepare(double sampleRate, int blockSize) {
        sr_ = sampleRate;
        for (auto& v : voices_) v.prepare(sampleRate);
        humanizer_.prepare(sampleRate);
        (void)blockSize;
    }

    // ── Global parameter setters ──────────────────────────────────────────
    void setNumVoices(int n)          { numVoices_  = juce::jlimit(1,8,n); }
    void setInterval(ChoirInterval i) { interval_   = i; updatePans(); }
    void setSpread(float s)           { spread_     = juce::jlimit(0.f,1.f,s); updatePans(); }
    void setDrift(float d)            { drift_      = d; humanizer_.setDrift(d); }
    void setChant(float c)            { humanizer_.setChant(c); }
    void setBpm(float bpm)            { humanizer_.setBpm(bpm); }

    void setBreathe(float b)          { for (auto& v:voices_) v.setBreathe(b); }
    void setRoughness(float r)        { for (auto& v:voices_) v.setRoughness(r); }
    void setOpenness(float o)         { for (auto& v:voices_) v.setOpenness(o); }
    void setGender(float g)           { for (auto& v:voices_) v.setGender(g); }
    void setVowelPos(float vp)        { for (auto& v:voices_) v.setVowelPos(vp); }
    void setEra(AncientEra e)         { for (auto& v:voices_) v.setEra(e); }
    void setEnvelope(float a,float d,float s,float r) {
        for (auto& v:voices_) v.setEnvelope(a,d,s,r);
    }

    // ── MIDI handling ─────────────────────────────────────────────────────
    void noteOn(int midiNote, float velocity) {
        // Assign numVoices_ slots, each with a different interval ratio
        for (int i = 0; i < numVoices_; ++i) {
            int slot = findFreeSlot();
            if (slot < 0) slot = stealSlot();
            float ratio = getVoiceRatio(interval_, i);
            voices_[slot].noteOn(midiNote, velocity, ratio, slot, humanizer_);
            voices_[slot].setPan(pans_[i]);
        }
    }

    void noteOff(int midiNote) {
        for (auto& v : voices_)
            if (v.isPlaying() && v.getMidiNote() == midiNote)
                v.noteOff(humanizer_);
    }

    void allNotesOff() {
        for (auto& v : voices_) v.noteOff(humanizer_);
    }

    // ── Process block ─────────────────────────────────────────────────────
    void process(juce::AudioBuffer<float>& buffer) {
        int numSamples = buffer.getNumSamples();
        buffer.clear();

        for (int s = 0; s < numSamples; ++s) {
            float L = 0.f, R = 0.f;

            for (int i = 0; i < kMaxVoices; ++i) {
                if (!voices_[i].isPlaying()) continue;
                auto hum = humanizer_.tick(i);
                float sample = voices_[i].tick(hum);
                float pan = voices_[i].getPan();
                // Equal-power pan
                float panRad = (pan + 1.f) * 0.5f * juce::MathConstants<float>::halfPi;
                L += sample * std::cos(panRad);
                R += sample * std::sin(panRad);
            }

            if (buffer.getNumChannels() >= 2) {
                buffer.addSample(0, s, L);
                buffer.addSample(1, s, R);
            } else {
                buffer.addSample(0, s, (L + R) * 0.5f);
            }
        }
    }

    // Feed MIDI messages before process()
    void processMidi(const juce::MidiBuffer& midi) {
        for (const auto meta : midi) {
            auto msg = meta.getMessage();
            if (msg.isNoteOn())
                noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())
                noteOff(msg.getNoteNumber());
            else if (msg.isAllNotesOff())
                allNotesOff();
        }
    }

private:
    ChoirVoice     voices_[kMaxVoices];
    HumanizeEngine humanizer_;
    double         sr_         = 44100.0;
    int            numVoices_  = 4;
    ChoirInterval  interval_   = ChoirInterval::Unison;
    float          spread_     = 0.6f;
    float          drift_      = 0.3f;
    float          pans_[kMaxVoices] = {};

    int findFreeSlot() const {
        for (int i = 0; i < kMaxVoices; ++i)
            if (!voices_[i].isPlaying()) return i;
        return -1;
    }

    int stealSlot() const {
        // Steal oldest released voice (simplistic: first inactive)
        for (int i = 0; i < kMaxVoices; ++i)
            if (!voices_[i].isActive()) return i;
        return 0; // steal voice 0 if all active
    }

    void updatePans() {
        // Spread voices evenly across stereo field
        for (int i = 0; i < kMaxVoices; ++i) {
            float t = (numVoices_ > 1)
                ? float(i) / float(numVoices_ - 1)  // 0.0 → 1.0
                : 0.5f;
            pans_[i] = (t * 2.f - 1.f) * spread_;   // -spread → +spread
        }
    }
};
