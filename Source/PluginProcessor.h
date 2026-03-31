#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "DSP/ChoirEngine.h"
#include "DSP/CaveReverb.h"
#include "DSP/FormantShimmer.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ANCIENT VOICES — AudioProcessor
// ─────────────────────────────────────────────────────────────────────────────

class AncientVoicesProcessor : public juce::AudioProcessor {
public:
    AncientVoicesProcessor();
    ~AncientVoicesProcessor() override = default;

    // ── AudioProcessor interface ──────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ancient Voices"; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms()    override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // ── Public members ────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // Thread-safe scope: audio thread fills scopeBuf_ then sets scopeReady_ under scopeLock_.
    // Message thread grabs a copy under the same lock.
    bool consumeScope(juce::AudioBuffer<float>& dest) {
        const juce::ScopedLock sl(scopeLock_);
        if (!scopeReady_.load(std::memory_order_relaxed))
            return false;
        scopeReady_.store(false, std::memory_order_relaxed);
        dest.makeCopyOf(scopeBuf_);
        return true;
    }

private:
    // ── APVTS parameter creation ──────────────────────────────────────────
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    // ── DSP modules ───────────────────────────────────────────────────────
    ChoirEngine    choir_;
    CaveReverb     reverb_;          // single true-stereo reverb instance
    FormantShimmer shimmerL_, shimmerR_;

    // Parameter cache (read once per block for efficiency)
    struct ParamCache {
        // Voice
        float vowelPos   = 0.f;
        float openness   = 1.f;
        float breathe    = 0.1f;
        float roughness  = 0.05f;
        float gender     = 1.f;
        // Choir
        int   numVoices  = 4;
        int   intervalIdx= 0;
        float spread     = 0.6f;
        float drift      = 0.3f;
        float ritual     = 0.f;
        float chant      = 0.f;
        // Era
        int   eraIdx     = 0;
        // Envelope
        float attack     = 0.05f;
        float decay      = 0.1f;
        float sustain    = 0.9f;
        float release    = 0.8f;
        // FX
        float caveSize   = 0.6f;
        float caveDamp   = 0.4f;
        float caveEcho   = 20.f;
        float caveMix    = 0.3f;
        float shimmerMix = 0.15f;
        float shimmerAmt = 0.5f;
        // Master
        float masterVol  = 1.0f;
        float masterGain = 0.0f;  // dB
    };
    ParamCache cache_;
    void updateCacheFromApvts();

    juce::AudioBuffer<float> scopeBuf_;
    juce::AudioBuffer<float> choirBuf_;
    juce::CriticalSection    scopeLock_;
    std::atomic<bool>        scopeReady_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesProcessor)
};
