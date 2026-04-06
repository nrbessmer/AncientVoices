#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <random>
#include <rubberband/RubberBandStretcher.h>
#include "AncientVoicesConfig.h"
#include "DSP/FormantFilter.h"
#include "DSP/CaveReverb.h"
#include "DSP/HumanizeEngine.h"
#include "Data/IntervalTables.h"
#include "Data/Presets.h"

// ═══════════════════════════════════════════════════════════════════
//  ANCIENT VOICES — DBTF Behavioral Contract
//
//  Signal chain — dry enters output ONLY at step 11:
//    1  Save dry          7  Saturation (inline tanh)
//    2  Mono sum          8  Shimmer (wet only)
//    3  Breathe noise     9  Cave reverb
//    4  N×FormantFilter  10  Limiter
//    5  Wet boost        11  Crossfade: wet×mix + dry×(1-mix)
//    6  Chorus           12  Contract checks + scope feed (lock-free)
// ═══════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────────
//  DIAGNOSTIC STATE — provably correct signal chain monitoring
//
//  Five contracts checked every processBlock:
//
//    C1  Limiter idle        GR < 0.5 dB for < 5 consecutive blocks
//    C2  Wet within 6 dB     |wetRms - dryRms| ≤ 6 dB (simulation: ~-1.8 dB)
//    C3  No dry in wet path  at mix=1, output ≈ postLimiter + masterGain (±1.5 dB)
//    C4  NormGain sane       normGainMax < 18.0x (ceiling = 20.0x)
//    C5  No NaN/Inf          sticky — if C5 fails, C2/C3 are suspended (false positives)
//
//  Simulation baseline (48 kHz / 2048 block, kWetBoost=1.0):
//    150 Hz 0.5 amp  → wet -10.8 dBFS / dry -9.0 dBFS / diff -1.8 dB → ALL PASS
//    0 dBFS input    → limiter GR 0.0 dB                               → ALL PASS
//    NaN injected    → C5 root cause, C2/C3 suspended (not false alarm)
//
//  All fields std::atomic<float> per spec.  Integers stored as float, cast on read.
// ─────────────────────────────────────────────────────────────────────────────
struct DiagnosticState
{
    // ── Raw measurements (audio thread writes, UI reads at 10 Hz) ─────
    std::atomic<float> limiterGainDb       { 0.f    };  // GR dB, 0 = limiter idle
    std::atomic<float> wetRmsDb            { -96.f  };  // wet RMS at step 5 (dBFS)
    std::atomic<float> dryRmsDb            { -96.f  };  // dry RMS at step 1 (dBFS)
    std::atomic<float> outputRmsDb         { -96.f  };  // post-crossfade (dBFS)
    std::atomic<float> wetDryDiffDb        { 0.f    };  // wet - dry (dB)
    std::atomic<float> mixRatio            { 1.f    };  // mix parameter value
    std::atomic<float> voiceCount          { 2.f    };  // number of FormantFilter copies
    std::atomic<float> copyCount           { 2.f    };  // same as voiceCount in this plugin
    std::atomic<float> normGainMin         { 0.f    };  // min normGain_ across active copies
    std::atomic<float> normGainMax         { 0.f    };  // max normGain_ across active copies
    std::atomic<float> sampleRateHz        { 44100.f };
    std::atomic<float> blockSizeSmps       { 0.f    };

    // ── Contract checks (true = PASS, updated every processBlock) ─────
    std::atomic<bool>  C1_limiterIdle      { true   };
    std::atomic<bool>  C2_wetWithin6dB     { true   };
    std::atomic<bool>  C3_noDryLeak        { true   };
    std::atomic<bool>  C4_normGainSane     { true   };
    std::atomic<bool>  C5_noNanInf         { true   };  // sticky: only ever goes false

    // ── Contract detail ───────────────────────────────────────────────
    std::atomic<float> limiterConsecBlocks { 0.f    };  // consecutive blocks limiter fired
    std::atomic<float> dryLeakDb           { 0.f    };  // C3: measured deviation (dB)
};

class AncientVoicesProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kMaxCopies = 8;

    AncientVoicesProcessor();
    ~AncientVoicesProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock  (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool   hasEditor()    const override { return true; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    const  juce::String getName() const override;

    int  getNumPrograms()    override { return presets_.count(); }
    int  getCurrentProgram() override { return presets_.current(); }
    void setCurrentProgram(int i) override { presets_.apply(i, apvts); }
    const juce::String getProgramName(int i) override { return presets_.get(i).name; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorValueTreeState apvts;
    AVPresetManager& getPresets() { return presets_; }

    // ── Lock-free scope consumer (UI thread only) ──────────────────────
    bool consumeScope(juce::AudioBuffer<float>& dest)
    {
        const int available = scopeFifo_.getNumReady();
        if (available <= 0) return false;
        dest.setSize(1, available, false, false, true);
        float* d = dest.getWritePointer(0);
        int s1, n1, s2, n2;
        scopeFifo_.prepareToRead(available, s1, n1, s2, n2);
        juce::FloatVectorOperations::copy(d,      scopeBuf_ + s1, n1);
        juce::FloatVectorOperations::copy(d + n1, scopeBuf_ + s2, n2);
        scopeFifo_.finishedRead(n1 + n2);
        return true;
    }

    // ── Diagnostic state accessor (UI thread reads only) ───────────────
    const DiagnosticState& getDiagState() const noexcept { return diagState_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    // ── DSP objects ────────────────────────────────────────────────────
    FormantFilter  copies_[kMaxCopies];
    float          copyPan_[kMaxCopies]    = {};
    float          copyDetune_[kMaxCopies] = {};
    HumanizeEngine humanizer_;
    CaveReverb     reverb_;
    juce::dsp::Chorus<float>  chorus_;
    juce::dsp::Limiter<float> limiter_;
    juce::dsp::ProcessSpec    spec_;

    // ── Shimmer ────────────────────────────────────────────────────────
    std::unique_ptr<RubberBand::RubberBandStretcher> shimmerRB_;
    double shimmerLastSR_ = 0.0;
    int    shimmerLastCh_ = 0;

    // ── Work buffers (prepareToPlay only — never in processBlock) ──────
    juce::AudioBuffer<float> dryBuf_, workBuf_, monoInBuf_, shimmerBuf_;
    int maxBlock_ = 0;

    // ── Param cache ────────────────────────────────────────────────────
    struct Cache {
        float vowelPos=0, openness=1, gender=1, formantShift=0, formantDepth=0.8f;
        float spread=0.6f, drift=0.6f, breathe=0.1f, roughness=0.05f;
        int   era=0, numCopies=2, interval=0, voiceMode=0;
        float mix=1, caveMix=0.6f, caveSize=0.85f, caveDamp=0.4f, caveEcho=20;
        float shimmerMix=0, shimmerAmt=0.5f;
        float masterVol=1, masterGain=0;
        float satDrive=0.1f;
        float effDepth=0.8f, effBwMul=1.f, effChoMix=0.35f;
        float effSat=0.1f,   effShimmer=0.f;
    } cache_;

    // ── Breath noise ───────────────────────────────────────────────────
    std::mt19937  rng_{ 42 };
    std::uniform_real_distribution<float> dist_{ -1.f, 1.f };
    float breatheState_ = 0.f;

    // ── Param throttle ─────────────────────────────────────────────────
    int  paramCounter_ = 0;
    std::atomic<bool> stateRestorePending_{ false };

    // ── Lock-free scope FIFO ───────────────────────────────────────────
    static constexpr int kScopeFifoSize = 8192;
    juce::AbstractFifo   scopeFifo_{ kScopeFifoSize };
    float                scopeBuf_[kScopeFifoSize] = {};

    // ── Diagnostic state ───────────────────────────────────────────────
    DiagnosticState diagState_;

    // ── Limiter consecutive-block counter (audio thread only, non-atomic) ──
    //  Tracks how many consecutive blocks the limiter fired (GR > 0.5 dB).
    //  Copied into diagState_.limiterConsecBlocks each block.
    int limiterConsecBlocks_ = 0;

    AVPresetManager presets_;
    double sr_ = 44100.0;

    void updateCache();
    void updateModules();
    void initShimmer(double sr, int ch);
    void applyShimmer(juce::AudioBuffer<float>&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesProcessor)
};
