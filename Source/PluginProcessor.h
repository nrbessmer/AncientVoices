#pragma once

#include <JuceHeader.h>
#include <vector>
#include <complex>
#include <map>
#include <cmath>  // For mathematical functions and constants
#include <juce_dsp/juce_dsp.h>
#include <rubberband/RubberBandStretcher.h>
#include "DSPModules.h"
class AncientVoicesAudioProcessor  : public juce::AudioProcessor
{
public:
    AncientVoicesAudioProcessor();
    ~AncientVoicesAudioProcessor() override;
    juce::ApplicationProperties& getApplicationProperties() { return appProperties; }
    // Accessor
    void applyDynamicEQ(juce::AudioBuffer<float>& buffer, int profileIndex);
    void applyFormantShifting(juce::AudioBuffer<float>& buffer, int profileIndex);
    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void setLicenseStatus(bool status);
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
    std::vector<float> getLastInputSamples();
    std::vector<float> getLastOutputSamples();
    // Declare maxAllowedDelay as a member variable
       float maxAllowedDelay;private:
    // Buffers to store recent audio data
    std::vector<float> lastInputSamples;
    std::vector<float> lastOutputSamples;
    std::mutex inputMutex;
    std::mutex outputMutex;
    juce::ApplicationProperties appProperties;
 
    // Buffer size for waveform visualization
    int waveformBufferSize = 2048;
    
    // Adjust //==============================================================================
    // Sound Profiles
    //
    // MMV's original 10 profiles (Alpha..Kappa) are preserved byte-for-byte.
    // Ancient Voices appends 50 new profiles AV_00..AV_49 organized into
    // 7 categories rooted in Sumerian/Egyptian/Mesopotamian antiquity:
    //   CHANT    : AV_00..AV_06   (7 presets)
    //   DRONE    : AV_07..AV_13   (7 presets)
    //   CHOIR    : AV_14..AV_20   (7 presets)
    //   RITUAL   : AV_21..AV_28   (8 presets)
    //   BREATH   : AV_29..AV_35   (7 presets)
    //   SHIMMER  : AV_36..AV_42   (7 presets)
    //   DARK     : AV_43..AV_49   (7 presets)
    // Total: 60 profiles (NumProfiles == 60)
    enum class SoundProfile
    {
        Alpha,
        Beta,
        Gamma,
        Delta,
        Epsilon,
        Zeta,
        Eta,
        Theta,
        Iota,
        Kappa,
        // --- Ancient Voices extensions ---
        AV_00, AV_01, AV_02, AV_03, AV_04, AV_05, AV_06,   // CHANT
        AV_07, AV_08, AV_09, AV_10, AV_11, AV_12, AV_13,   // DRONE
        AV_14, AV_15, AV_16, AV_17, AV_18, AV_19, AV_20,   // CHOIR
        AV_21, AV_22, AV_23, AV_24, AV_25, AV_26, AV_27, AV_28,  // RITUAL
        AV_29, AV_30, AV_31, AV_32, AV_33, AV_34, AV_35,   // BREATH
        AV_36, AV_37, AV_38, AV_39, AV_40, AV_41, AV_42,   // SHIMMER
        AV_43, AV_44, AV_45, AV_46, AV_47, AV_48, AV_49,   // DARK
        NumProfiles // Make sure to update the total count
    };

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

        // ------------------------------------------------------------
        // Session 1 extensions — all default 0 = module disabled.
        // MMV's original 10 profiles leave these zero, preserving
        // their original character exactly.
        // ------------------------------------------------------------
        float formantShiftSemitones = 0.0f;   // ±N semitones, independent of pitch (0 = bypass)

        float   lfo1RateHz    = 0.0f;         // 0 = LFO disabled
        float   lfo1Depth     = 0.0f;         // unit depends on destination
        int     lfo1Shape     = 0;            // 0=sine 1=tri 2=saw 3=random-stepped
        AV::ModDest lfo1Dest  = AV::ModDest::None;

        float   envFollowAttackMs  = 5.0f;
        float   envFollowReleaseMs = 150.0f;
        float   envFollowDepth     = 0.0f;    // 0 = disabled
        AV::ModDest envFollowDest  = AV::ModDest::None;

        // ------------------------------------------------------------
        // Session 2 extensions — multi-tap delay, reverse reverb,
        // comb filter, enhanced reverb. All default 0/off.
        // ------------------------------------------------------------
        float multiTapMix          = 0.0f;    // 0 = bypass multi-tap
        float multiTapTimesMs[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
        float multiTapGains[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
        float multiTapPans[4]      = { 0.0f, 0.0f, 0.0f, 0.0f };

        float reverseReverbMix     = 0.0f;    // 0 = bypass
        float reverseReverbWindowSec = 2.0f;

        float combFreqHz           = 0.0f;    // 0 = bypass comb
        float combFeedback         = 0.0f;
        float combMix              = 0.0f;

        bool  useEnhancedReverb    = false;   // if true, routes through EnhancedReverb instead
        float reverbPreDelayMs     = 0.0f;    // only used when useEnhancedReverb
        float reverbHfDampingDb    = 0.0f;    // negative dB = cut highs (e.g. -6.0)

        // ------------------------------------------------------------
        // Session 3 extensions — phase vocoder, granular, bitcrusher,
        // dynamic EQ, noise generator, transient shaper. All default 0/off.
        // ------------------------------------------------------------
        float pvFreezeAmount       = 0.0f;    // 0..1 — hold magnitudes
        float pvSmearAmount        = 0.0f;    // 0..1 — spectral average
        float pvMix                = 0.0f;    // 0 = bypass

        float granularMix          = 0.0f;    // 0 = bypass
        float granularGrainSizeMs  = 50.0f;
        float granularDensity      = 0.0f;    // grains/sec
        float granularPitchSpreadSemi = 0.0f;
        float granularPitchBaseSemi   = 0.0f;
        float granularReverseProb  = 0.0f;
        float granularPositionJitter = 0.5f;

        float bitcrushMix          = 0.0f;    // 0 = bypass
        float bitcrushBitDepth     = 12.0f;
        float bitcrushSampleRateHz = 22050.0f;

        float dynEqMix             = 0.0f;    // 0 = bypass
        float dynEqFreqHz          = 2000.0f;
        float dynEqQ               = 1.2f;
        float dynEqTargetGainDb    = 0.0f;    // + boost, - cut
        float dynEqThreshold       = 0.1f;

        float noiseLevel           = 0.0f;    // 0 = bypass
        int   noiseColor           = 0;       // 0=white, 1=pink
        float noiseHighPassHz      = 0.0f;
        bool  noiseGated           = false;

        float transientAttackDb    = 0.0f;    // 0 = bypass
        float transientMix         = 0.0f;
    };


    std::map<SoundProfile, ProfileSettings> profiles;

    // Current profile settings
    ProfileSettings currentProfile;
    int profileIndex;

    // Effect Toggles
    bool stutterEnabled;
    bool harmonyEnabled;
    bool noiseReductionEnabled;
    bool repeaterEnabled;

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
    std::vector<juce::dsp::DelayLine<float>> delayLines; // Vector to hold delay lines for each channel
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> delayLine;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Chorus<float> flanger;
    juce::dsp::Limiter<float> limiter;


    juce::SmoothedValue<float> pitchShiftSmoothed;

    // JUCE DSP Delay Lines for Repeater and Stutter
    juce::dsp::DelayLine<float> repeaterDelayLine;
    juce::dsp::DelayLine<float> stutterDelayLine;
    juce::dsp::ProcessSpec spec; // To define the processing specifications
    void saveLicenseStatus(bool isValid);
    bool isLicenseValid();
    bool isLicensed = false;    // Initialization and Effect Application
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
    void adjustForSampleRate(float currentSampleRate);     std::unique_ptr<RubberBand::RubberBandStretcher> rubberBandPitchShifter;
    void initializeDSPModules(const juce::dsp::ProcessSpec& spec);
    void resetDSPModules();
    // New effect methods declarations
    void applyVocalDoublerEffect(juce::AudioBuffer<float>& buffer, float detuneAmount, float delayMs);
    void applySpacedOutDelay(juce::AudioBuffer<float>& buffer, float delayTimeMs, float panAmount);
    void applyInstabilityEffect(juce::AudioBuffer<float>& buffer, float instabilityDepth);
    void applyGlitchEffect(juce::AudioBuffer<float>& buffer, float glitchAmount);
    void applyRubberBandPitchShift(juce::AudioBuffer<float>& buffer, float pitchShiftAmount);
    void applyAdvancedSaturation(juce::AudioBuffer<float>& buffer, float drive, float mix, float tone);
    // Declare the function here
    juce::AudioProcessorValueTreeState apvts;
    void applyVocalTransitEffect(juce::AudioBuffer<float>& buffer);
    // New getTempo method declaration
    double getTempo() const;  // Retrieves the current host tempo
    // DSP Member coefficients
    juce::dsp::Convolution convolutionReverb;   // Convolution reverb
    juce::dsp::Chorus<float> modulation;         // Modulation effect (e.g., Chorus)
    std::vector<juce::dsp::IIR::Filter<float>> antiAliasFilters;
    // Member variables
    juce::AudioBuffer<float> inputBuffer;
    int bufferWritePosition = 0;
    int bufferReadPosition = 0;
    const int bufferSize = 16384; // Adjust the size as needed
    float sampleRate = 0.0f;  // Stores the current sample rate
    // Add the following member variables for harmonizer buffer management
     juce::AbstractFifo harmonizerFifo { 16384 }; // Buffer size can be adjusted as needed
     juce::AudioBuffer<float> harmonizerBuffer;
    std::atomic<float>* reverbWet = nullptr;
      std::atomic<float>* delayMix = nullptr;
      std::atomic<float>* duckingSensitivity = nullptr;
      std::atomic<float>* grainSize = nullptr;
      std::atomic<float>* glitchIntensity = nullptr;
      std::atomic<float>* mixAmount = nullptr;
      std::atomic<float>* userReverb = nullptr;
      std::atomic<float>* userDelay = nullptr;
      std::atomic<float>* userDrive = nullptr;
      std::atomic<float>* userOutput = nullptr;
      std::atomic<float>* userPitch = nullptr;   // user pitch knob, semitones, added to preset pitch
      juce::SmoothedValue<float> mixSmoothed;
      juce::dsp::DelayLine<float> delay{ 44100 }; // Approx. 1-second buffer
      juce::dsp::Gain<float> duckingGain;
    juce::dsp::WaveShaper<float> glitchEffect;

    // ------------------------------------------------------------
    // Session 1 DSP modules — instantiated once, prepared in
    // prepareToPlay(), driven per-preset from applyProfileEffects().
    // ------------------------------------------------------------
    AV::LFO              lfo1;
    AV::EnvelopeFollower envFollower;
    AV::FormantShifter   formantShifter;

    // ------------------------------------------------------------
    // Session 2 DSP modules
    // ------------------------------------------------------------
    AV::MultiTapDelay    multiTap;
    AV::ReverseReverb    reverseReverb;
    AV::CombFilter       combFilter;
    AV::EnhancedReverb   enhancedReverb;

    // ------------------------------------------------------------
    // Session 3 DSP modules
    // ------------------------------------------------------------
    AV::PhaseVocoderFreeze phaseVocoder;
    AV::GranularEngine     granular;
    AV::Bitcrusher         bitcrusher;
    AV::DynamicEqBand      dynamicEq;
    AV::NoiseGenerator     noiseGen;
    AV::TransientShaper    transientShaper;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AncientVoicesAudioProcessor)
};
