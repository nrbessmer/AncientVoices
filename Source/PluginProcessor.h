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
//  MUST:   mix=1.0 → zero dry in output
//  MUST:   mix=0.0 → output identical to input
//  MUST:   wet level within 6 dB of dry at depth=1.0
//  MUST:   no heap allocation in processBlock
//  NEVER:  add x (dry) inside FormantFilter::tick()
//  NEVER:  run shimmer/reverb before wet path is isolated
//
//  Signal chain — dry enters output ONLY at step 11:
//    1  Save dry          7  Saturation (inline tanh)
//    2  Mono sum          8  Shimmer (wet only)
//    3  Breathe noise     9  Cave reverb
//    4  N×FormantFilter  10  Limiter
//    5  Wet boost        11  Crossfade: wet×mix + dry×(1-mix)
//    6  Chorus           12  Scope feed → AbstractFifo (lock-free)
// ═══════════════════════════════════════════════════════════════════

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

    // ── Lock-free scope consumer (UI thread only) ──────────────────
    //  CONTRACT: single consumer. Audio thread writes, UI thread reads.
    //  No lock. No allocation on the audio thread.
    //  Returns false when FIFO is empty (no new data this timer tick).
    bool consumeScope(juce::AudioBuffer<float>& dest)
    {
        const int available = scopeFifo_.getNumReady();
        if (available <= 0) return false;

        dest.setSize(1, available, false, false, true);   // alloc on UI thread only
        float* d = dest.getWritePointer(0);

        int start1, size1, start2, size2;
        scopeFifo_.prepareToRead(available, start1, size1, start2, size2);
        juce::FloatVectorOperations::copy(d,         scopeBuf_ + start1, size1);
        juce::FloatVectorOperations::copy(d + size1, scopeBuf_ + start2, size2);
        scopeFifo_.finishedRead(size1 + size2);
        return true;
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    // ── DSP objects ────────────────────────────────────────────────
    FormantFilter  copies_[kMaxCopies];
    float          copyPan_[kMaxCopies]    = {};
    float          copyDetune_[kMaxCopies] = {};
    HumanizeEngine humanizer_;
    CaveReverb     reverb_;
    juce::dsp::Chorus<float>  chorus_;
    juce::dsp::Limiter<float> limiter_;
    juce::dsp::ProcessSpec    spec_;

    // ── Shimmer ────────────────────────────────────────────────────
    std::unique_ptr<RubberBand::RubberBandStretcher> shimmerRB_;
    double shimmerLastSR_ = 0.0;
    int    shimmerLastCh_ = 0;

    // ── Work buffers (prepareToPlay only — never in processBlock) ──
    juce::AudioBuffer<float> dryBuf_, workBuf_, monoInBuf_, shimmerBuf_;
    int maxBlock_ = 0;

    // ── Param cache ────────────────────────────────────────────────
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

    // ── Breath noise ───────────────────────────────────────────────
    std::mt19937  rng_{ 42 };
    std::uniform_real_distribution<float> dist_{ -1.f, 1.f };
    float breatheState_ = 0.f;

    // ── Param throttle ─────────────────────────────────────────────
    int  paramCounter_ = 0;
    std::atomic<bool> stateRestorePending_{ false };

    // ── Lock-free scope FIFO ───────────────────────────────────────
    //  Pattern mirrors MisterMultiVox's harmonizerFifo — brace-init
    //  with size, separate plain-float backing array alongside.
    //  Audio thread writes via prepareToWrite/finishedWrite.
    //  UI thread reads via prepareToRead/finishedRead.
    //  No mutex. No atomic flag. No allocation on audio thread.
    static constexpr int kScopeFifoSize = 8192;
    juce::AbstractFifo   scopeFifo_{ kScopeFifoSize };
    float                scopeBuf_[kScopeFifoSize] = {};

    AVPresetManager presets_;
    double sr_ = 44100.0;

    void updateCache();
    void updateModules();
    void initShimmer(double sr, int ch);
    void applyShimmer(juce::AudioBuffer<float>&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesProcessor)
};
