#pragma once

#include <JuceHeader.h>
#include <vector>
#include <complex>
#include <array>
#include <atomic>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <rubberband/RubberBandStretcher.h>
#include "Data/Presets.h"

class AncientVoicesAudioProcessor  : public juce::AudioProcessor
{
public:
    AncientVoicesAudioProcessor();
    ~AncientVoicesAudioProcessor() override;

    // Accessor
    void applyDynamicEQ(juce::AudioBuffer<float>& buffer, int profileIndex);
    void applyFormantShifting(juce::AudioBuffer<float>& buffer, int profileIndex);

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifdef JucePlugin_Enable_ARA
    bool isARACompatible() const { return true; }
   #endif

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    // Declare maxAllowedDelay as a member variable
    float maxAllowedDelay;

    // Ancient Voices preset application
    void applyFactoryPreset(int presetIndex);

    // Rebuild currentProfile from live APVTS parameter values
    void loadCurrentProfileFromParams();

private:
    // Buffer size for waveform visualization
    int waveformBufferSize = 2048;

    //==============================================================================
    // ProfileSettings — 23 fields, IDENTICAL to MMV, in original declaration order
    struct ProfileSettings
    {
        float pitchShiftAmount;             // Pitch shift amount for vocal pitch adjustment
        float harmonizerAmount;             // Amount of harmonization effect applied
        float reverbAmount;                 // Amount of reverb for spatial depth
        float distortionAmount;             // Amount of distortion to add grit
        float delayAmount;                  // Delay amount for echo effects
        float chorusDepth;                  // Depth of the chorus effect for richness
        float flangerRate;                  // Rate of flanger effect for modulation
        float filterCutoff;                 // Cutoff frequency for tone shaping with filter
        float compressionRatio;             // Ratio for compression to control dynamics
        float instabilityDepth;             // Depth of instability effect for slight pitch modulation
        float vocalDoublerDetuneAmount;     // Detune amount for vocal doubler effect
        float vocalDoublerDelayMs;          // Delay time in milliseconds for vocal doubler
        float spacedOutDelayTimeMs;         // Delay time in milliseconds for spaced-out ambiance
        float spacedOutPanAmount;           // Panning amount for spaced-out effect
        float glitchAmount;                 // Amount of glitch effect for chopped texture
        float outputGain;                   // Final gain adjustment for volume control

        // Additional attributes for specialized effects
        int stopAndGoInterval;              // Interval in samples for stop-and-go effect
        float stopAndGoSilenceRatio;        // Silence ratio for stop-and-go effect
        int stutterRepeatLength;            // Length of repeated stutter segment in samples
        int reverseInterval;                // Interval in samples for periodic audio reversal

        // Saturation effect parameters
        float saturationDrive;              // Drive level for saturation
        float saturationMix;                // Mix level for saturation blending
        float saturationTone;               // Tone setting for saturation to shape warmth or brightness
    };

    // Profile storage — 50 presets (was std::map<SoundProfile, ProfileSettings> in MMV)
    std::array<ProfileSettings, 50> profiles;

    // Current profile settings
    ProfileSettings currentProfile;
    int profileIndex;

    // DSP Modules
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> windowFunction;
    juce::AudioBuffer<float> fftBuffer;
    juce::AudioBuffer<float> overlapBuffer;
    juce::AudioBuffer<float> accumulatedBuffer;
    int fftSize;
    int hopSize;
    int overlapPosition;

    std::unique_ptr<RubberBand::RubberBandStretcher> rubberBandStretcher;
    juce::dsp::Reverb reverb;
    juce::dsp::WaveShaper<float> distortion;
    juce::dsp::Chorus<float> chorus;
    std::vector<juce::dsp::DelayLine<float>> delayLines;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> delayLine;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Chorus<float> flanger;
    juce::dsp::Limiter<float> limiter;

    juce::SmoothedValue<float> pitchShiftSmoothed;

    // JUCE DSP Delay Lines for Repeater and Stutter
    juce::dsp::DelayLine<float> repeaterDelayLine;
    juce::dsp::DelayLine<float> stutterDelayLine;
    juce::dsp::ProcessSpec spec;

    // Initialization and Effect Application
    void initializeProfiles();
    void applyProfileEffects(juce::AudioBuffer<float>& buffer);
    void performPitchShifting(juce::AudioBuffer<float>& buffer, float shiftAmount);
    void applyHarmonization(juce::AudioBuffer<float>& buffer, float harmonyAmount);
    void applyReverbEffect(juce::AudioBuffer<float>& buffer, float reverbAmount);
    void applyDistortionEffect(juce::AudioBuffer<float>& buffer, float distortionAmount);
    void applyDelayEffect(juce::AudioBuffer<float>& buffer, float delayAmount);
    void applyChorusEffect(juce::AudioBuffer<float>& buffer, float depth);
    void applyFlangerEffect(juce::AudioBuffer<float>& buffer, float rate);
    void applyCompressionEffect(juce::AudioBuffer<float>& buffer, float ratio);
    void applyStutterEffect(juce::AudioBuffer<float>& buffer, float stutterAmount);
    void applyNoiseReduction(juce::AudioBuffer<float>& buffer, float reductionAmount);
    void applyRepeaterEffect(juce::AudioBuffer<float>& buffer, float repeaterAmount);
    void applyLimiter(juce::AudioBuffer<float>& buffer);
    void applyReverserEffect(juce::AudioBuffer<float>& buffer, int reverseInterval);
    void applyStopAndGoEffect(juce::AudioBuffer<float>& buffer, int interval, float silenceRatio);
    void adjustForSampleRate(float currentSampleRate);

    std::unique_ptr<RubberBand::RubberBandStretcher> rubberBandPitchShifter;
    void initializeDSPModules(const juce::dsp::ProcessSpec& spec);
    void resetDSPModules();

    // New effect methods declarations
    void applyVocalDoublerEffect(juce::AudioBuffer<float>& buffer, float detuneAmount, float delayMs);
    void applySpacedOutDelay(juce::AudioBuffer<float>& buffer, float delayTimeMs, float panAmount);
    void applyInstabilityEffect(juce::AudioBuffer<float>& buffer, float instabilityDepth);
    void applyGlitchEffect(juce::AudioBuffer<float>& buffer, float glitchAmount);
    void applyRubberBandPitchShift(juce::AudioBuffer<float>& buffer, float pitchShiftAmount);
    void applyAdvancedSaturation(juce::AudioBuffer<float>& buffer, float drive, float mix, float tone);

    juce::AudioProcessorValueTreeState apvts;

    void applyVocalTransitEffect(juce::AudioBuffer<float>& buffer);

    // getTempo method
    double getTempo() const;

    // DSP Member coefficients
    juce::dsp::Convolution convolutionReverb;
    juce::dsp::Chorus<float> modulation;
    std::vector<juce::dsp::IIR::Filter<float>> antiAliasFilters;

    // Member variables
    juce::AudioBuffer<float> inputBuffer;
    int bufferWritePosition = 0;
    int bufferReadPosition = 0;
    const int bufferSize = 16384;
    float sampleRate = 0.0f;

    // Harmonizer buffer management
    juce::AbstractFifo harmonizerFifo { 16384 };
    juce::AudioBuffer<float> harmonizerBuffer;

    std::atomic<float>* reverbWet = nullptr;
    std::atomic<float>* delayMix = nullptr;
    std::atomic<float>* grainSize = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AncientVoicesAudioProcessor)
};
