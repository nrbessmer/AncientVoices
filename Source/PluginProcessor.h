#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <rubberband/RubberBandStretcher.h>
#include "DSP/FormantFilter.h"
#include "DSP/HumanizeEngine.h"
#include "DSP/PitchTracker.h"
#include "DSP/PitchQuantizer.h"
#include "Data/Presets.h"

// =============================================================================
//  ANCIENT VOICES  v3  —  Stereo vocal transformation effect
//
//  WHAT CHANGED FROM v2 (and why v2 had distortion + broken rendering):
//
//  1. CaveReverb (custom Schroeder) -> juce::dsp::Reverb
//     Reason: feedback 0.98 at size=1.0 caused near-instability; JUCE reverb
//     is tuned and battle-tested.
//
//  2. SaturationStage (custom) -> juce::dsp::WaveShaper<float> (tanh)
//     Reason: SaturationStage::tick() added the dry signal TWICE — the last
//     line was: return saturated + x*(1-drive*0.7f) + x*(1-drive*0.3f)
//     Output exceeded 1.0 even with drive=0. This was the primary distortion.
//
//  3. VocalChorus (custom) -> juce::dsp::Chorus<float>
//     Reason: custom chorus set filter coefficients every sample inside
//     the per-copy loop — O(N*blockSize) coefficient recalculation.
//
//  4. FormantShimmer (custom granular) -> RubberBand pitch shift
//     Reason: shimmer feedback buffer had no gain ceiling — compounded
//     every sample, adding to clipping. RubberBand is used exactly as
//     MisterMultiVox uses it for harmonization: proven, stable, real-time.
//
//  5. FormantFilter gain: *1.5f -> *2.2f (calibrated)
//     Reason: 1.5f * 8 copies * sqrt(8) normalisation still exceeded 1.0.
//     2.2f compensates for the actual ~0.45 output ratio of 4 SVF bandpass
//     filters summed with {1.0, 0.75, 0.45, 0.22} gains.
//
//  6. HumanizeEngine::triggerVoice() now called in prepareToPlay()
//     Reason: was never called — all voices had active=false, output was
//     silence multiplied by zero ampOffset.
//
//  7. VoiceHumanization::pitchOffsetCents added
//     Reason: ChoirVoice referenced hum.pitchOffsetCents but the field
//     did not exist — compile error in the synthesiser path.
//
//  8. Stack VLA float outL[2048] -> heap-allocated workBuf_
//     Reason: Logic Pro can use blocks > 2048 samples. Stack VLA is
//     undefined behavior and causes crashes with large buffer sizes.
//
//  SIGNAL PATH:
//    Input (stereo/mono)
//      -> mono sum
//      -> N copies (1-8), each:
//           juce::dsp::Chorus    (per-copy LFO thickness)
//           FormantFilter SVF    (F1-F4 vowel shaping — the core effect)
//           HumanizeEngine       (amp drift + pitch vibrato per copy)
//      -> equal-power stereo pan sum / sqrt(N)
//      -> juce::dsp::WaveShaper tanh  (soft harmonic warmth)
//      -> RubberBand shimmer    (octave-up pitch shift, wet/dry blended)
//      -> juce::dsp::Reverb     (cave/room space)
//      -> juce::dsp::Limiter    (safety ceiling -1 dBFS)
//      -> wet/dry blend with original input
//      -> master vol * gain dB
//      -> scope capture (lock-free)
// =============================================================================

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
    bool hasEditor() const override { return true; }

    const juce::String getName()  const override;
    bool   acceptsMidi()  const override;
    bool   producesMidi() const override;
    bool   isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms()    override { return presets_.count(); }
    int  getCurrentProgram() override { return presets_.current(); }
    void setCurrentProgram(int i) override { presets_.apply(i, apvts); }
    const juce::String getProgramName(int i) override { return presets_.get(i).name; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int size) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState apvts;

    bool consumeScope(juce::AudioBuffer<float>& dest) {
        const juce::ScopedLock sl(scopeLock_);
        if (!scopeReady_.load(std::memory_order_relaxed)) return false;
        scopeReady_.store(false, std::memory_order_relaxed);
        dest.makeCopyOf(scopeBuf_);
        return true;
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    // ── FormantFilter (SVF, fixed gain) ───────────────────────────────────
    FormantFilter copies_[kMaxCopies];
    float         copyPan_[kMaxCopies] = {};

    // ── JUCE DSP modules (proven: same pattern as MisterMultiVox) ─────────
    juce::dsp::Chorus<float>     chorus_;
    juce::dsp::WaveShaper<float> waveshaper_;
    juce::dsp::Reverb            reverb_;
    juce::dsp::Limiter<float>    limiter_;
    juce::dsp::ProcessSpec       spec_;

    // ── RubberBand shimmer (octave up, same usage as MMV harmonizer) ──────
    std::unique_ptr<RubberBand::RubberBandStretcher> shimmerRB_;
    double shimmerLastSR_      = 0.0;
    int    shimmerLastChannels_ = 0;

    // ── HumanizeEngine (fixed: voices now triggered in prepareToPlay) ──────
    HumanizeEngine humanizer_;

    // ── Pitch tracker ─────────────────────────────────────────────────────
    PitchTracker   pitchTracker_;
    PitchQuantizer pitchQuantizer_;
    int            pitchBlockCounter_ = 0;
    juce::SmoothedValue<float> trackedPitchSmooth_;

    // ── Heap work buffers (fixes VLA stack crash with large Logic buffers) ─
    juce::AudioBuffer<float> workBuf_;    // stereo accumulation for N copies
    juce::AudioBuffer<float> shimmerBuf_; // RubberBand temp buffer
    juce::AudioBuffer<float> dryBuf_;     // dry input saved for wet/dry blend
    int maxBlockSize_ = 0;

    // ── Parameter cache ───────────────────────────────────────────────────
    struct ParamCache {
        float vowelPos     = 0.f;
        float openness     = 1.f;
        float gender       = 1.f;
        float formantShift = 0.f;
        int   numCopies    = 4;
        float spread       = 0.6f;
        float drift        = 0.3f;
        float chant        = 0.f;
        float mix          = 0.8f;
        int   eraIdx       = 0;
        float snapAmount   = 0.f;
        int   snapScale    = 0;
        // Reverb
        float reverbRoom   = 0.6f;
        float reverbDamp   = 0.4f;
        float reverbWet    = 0.3f;
        float reverbWidth  = 1.0f;
        // Shimmer (RubberBand)
        float shimmerMix   = 0.15f;
        float shimmerAmt   = 0.5f;
        // Saturation
        float satDrive     = 0.15f;
        // Master
        float masterVol    = 1.0f;
        float masterGain   = 0.0f;
    };
    ParamCache cache_;

    void updateCacheFromApvts();
    void updateModules();
    void initShimmerRB(double sampleRate, int numChannels);
    void applyShimmer(juce::AudioBuffer<float>& buffer);

    double sr_ = 44100.0;

    AVPresetManager presets_;

    juce::AudioBuffer<float> scopeBuf_;
    juce::CriticalSection    scopeLock_;
    std::atomic<bool>        scopeReady_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AncientVoicesProcessor)
};
