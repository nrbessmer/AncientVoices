#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <rubberband/RubberBandStretcher.h>

//==============================================================================
AncientVoicesAudioProcessor::AncientVoicesAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      fft (11),
      windowFunction(512, juce::dsp::WindowingFunction<float>::hann),
      fftSize (2048),
      hopSize (512),
      overlapPosition (0),
      profileIndex (0),
      apvts (*this, nullptr, "Parameters", createParameters())
{
    // Initialize profiles with their settings
    initializeProfiles();
    // Set currentProfile to the initial profile
    currentProfile = profiles[0];
    inputBuffer.setSize(getTotalNumInputChannels(), 0);
}

AncientVoicesAudioProcessor::~AncientVoicesAudioProcessor()
{
    // Clean up RubberBandStretcher if it exists
    if (rubberBandStretcher != nullptr)
    {
        rubberBandStretcher.reset();
    }

    // Clear DSP modules
    resetDSPModules();

    // Clear delay lines
    delayLines.clear();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AncientVoicesAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Profile Parameter
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "PROFILE_INDEX",                // Parameter ID
        "Profile Index",                // Parameter name
        0,                              // Minimum value
        49,                             // Maximum value (50 presets)
        0                               // Default index
    ));

    // Ancient Voices interactive parameters (Option B)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "ERA", "Era", 0, 3, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "VOICE_MODE", "Voice Mode", 0, 3, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "NUM_VOICES", "Num Voices", 1, 8, 4));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("SPREAD", 1), "Spread",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DRIFT", 1), "Drift",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CHANT", 1), "Chant",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FORMANT_DEPTH", 1), "Formant Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.9f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("MIX", 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CAVE_MIX", 1), "Cave Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CAVE_SIZE", 1), "Cave Size",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("SHIMMER_MIX", 1), "Shimmer Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("BREATHE", 1), "Breathe",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ROUGHNESS", 1), "Roughness",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("INTERVAL", 1), "Interval",
        juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("OUTPUT_GAIN", 1), "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 6.0f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
void AncientVoicesAudioProcessor::releaseResources()
{
    // Release any resources if needed (e.g., when playback stops)
}

bool AncientVoicesAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Check for mono and stereo main input and output channels
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();

    // Support identical mono or stereo channel layouts
    if ((mainInput == juce::AudioChannelSet::mono() && mainOutput == juce::AudioChannelSet::mono()) ||
        (mainInput == juce::AudioChannelSet::stereo() && mainOutput == juce::AudioChannelSet::stereo()))
    {
        return true;
    }

    // Handle additional cases to return true for stereo output specifically
    if (mainOutput == juce::AudioChannelSet::stereo() && mainInput == juce::AudioChannelSet::mono())
    {
        return true; // Allow mono input to stereo output as well
    }

    return false; // Return false for any unsupported configurations
}

//==============================================================================
void AncientVoicesAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::Logger::writeToLog("Total Input Channels: " + juce::String(getTotalNumInputChannels()));
    juce::Logger::writeToLog("Total Output Channels: " + juce::String(getTotalNumOutputChannels()));

    if (getTotalNumInputChannels() == 2 && getTotalNumOutputChannels() == 2) {
        juce::Logger::writeToLog("Initializing for Stereo");
    } else if (getTotalNumInputChannels() == 1 && getTotalNumOutputChannels() == 1) {
        juce::Logger::writeToLog("Initializing for Mono");
    } else {
        juce::Logger::writeToLog("Unexpected channel configuration detected");
    }

    this->sampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    try
    {
        if (sampleRate <= 0)
        {
            return;
        }

        // Initialize DSP Modules
        initializeDSPModules(spec);

        // Ensure the output channel count is valid
        int numOutputChannels = getTotalNumOutputChannels();
        if (numOutputChannels <= 0)
        {
            return;
        }

        // Ensure the input channel count is valid
        int numInputChannels = getTotalNumInputChannels();
        if (numInputChannels <= 0)
        {
            return;
        }

        // Resize and initialize delay lines per channel
        delayLines.clear();
        delayLines.resize(numOutputChannels, juce::dsp::DelayLine<float>(2 * sampleRate));

        const float maxAllowedDelayTimeInSeconds = 2.0f;
        const int maxDelaySamples = static_cast<int>(maxAllowedDelayTimeInSeconds * sampleRate);

        for (int i = 0; i < numOutputChannels; ++i)
        {
            delayLines[i].reset();
            delayLines[i].setMaximumDelayInSamples(maxDelaySamples);
        }

        maxAllowedDelay = maxAllowedDelayTimeInSeconds;

        // Initialize waveform buffers ensuring correct size
        waveformBufferSize = std::max(1, samplesPerBlock * 2);

        // Determine if the plugin is running in real-time mode based on typical sample rate values
        bool isRealTime = (sampleRate == 44100 || sampleRate == 48000);
        bool lastIsRealTime = isRealTime;

        // Initialize RubberBandStretcher based on real-time or offline settings
        if (rubberBandStretcher == nullptr || sampleRate != sampleRate || isRealTime != isRealTime)
        {
            sampleRate = sampleRate;
            lastIsRealTime = isRealTime;

            RubberBand::RubberBandStretcher::Options options = RubberBand::RubberBandStretcher::OptionProcessRealTime;
            if (isRealTime) {
                options |= RubberBand::RubberBandStretcher::OptionWindowShort;
            } else {
                options |= RubberBand::RubberBandStretcher::OptionProcessOffline |
                           RubberBand::RubberBandStretcher::OptionWindowLong |
                           RubberBand::RubberBandStretcher::OptionPhaseIndependent;
            }

            rubberBandStretcher = std::make_unique<RubberBand::RubberBandStretcher>(
                static_cast<size_t>(sampleRate),
                numInputChannels,
                options
            );
        }

        // Reset and configure rubberBandStretcher
        rubberBandStretcher->reset();
        rubberBandStretcher->setPitchScale(1.0f);

        // Report latency to the host
        int latencySamples = rubberBandStretcher->getLatency();
        setLatencySamples(latencySamples);

        // Initialize AbstractFifo for harmonizer buffer
        harmonizerFifo.setTotalSize(16384);
        harmonizerBuffer.setSize(numInputChannels, harmonizerFifo.getTotalSize());
        harmonizerBuffer.clear();
    }
    catch (const std::exception& e)
    {
        //DBG("PrepareToPlay exception: " << e.what());
    }
}


void AncientVoicesAudioProcessor::adjustForSampleRate(float currentSampleRate)
{
    float scalingFactor = (currentSampleRate < 16000.0f) ? 1.0f : (16000.0f / currentSampleRate);

    for (auto& settings : profiles)
    {
        settings.reverbAmount *= scalingFactor;
        settings.delayAmount *= scalingFactor;
        settings.pitchShiftAmount *= scalingFactor;
        settings.flangerRate *= scalingFactor;
        settings.instabilityDepth *= scalingFactor;
        settings.saturationDrive *= scalingFactor;
    }
}

void AncientVoicesAudioProcessor::initializeDSPModules(const juce::dsp::ProcessSpec& spec)
{
    try {
        resetDSPModules();

        // Initialize Reverb with fully wet settings
        juce::dsp::Reverb::Parameters reverbParams;
        reverbParams.roomSize = 0.6f;
        reverbParams.damping = 0.5f;
        reverbParams.width = 1.0f;
        reverbParams.wetLevel = 1.0f;   // 100% wet
        reverbParams.dryLevel = 0.0f;   // 0% dry
        reverb.setParameters(reverbParams);
        reverb.prepare(spec);

        // Prepare other DSP modules with fully wet configurations
        chorus.setMix(1.0f);             // 100% wet for chorus
        chorus.prepare(spec);

        flanger.setMix(1.0f);            // 100% wet for flanger
        flanger.prepare(spec);

        // Modules without wet/dry mix: Prepare as usual, assuming full processing
        distortion.prepare(spec);
        compressor.prepare(spec);
        limiter.prepare(spec);

        // Additional modules as needed
    }
    catch (const std::exception& e)
    {
        // Log exceptions if debugging
    }
}


void AncientVoicesAudioProcessor::resetDSPModules()
{
    // Reset all DSP modules to their default state
    reverb.reset();
    distortion.reset();
    chorus.reset();
    flanger.reset();
    compressor.reset();
    limiter.reset();

    // Reset delay lines
    for (auto& delayLine : delayLines)
    {
        delayLine.reset();
    }
}

//==============================================================================
void AncientVoicesAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    try {
        if (numSamples <= 0 || numOutputChannels <= 0)
            return;

        // Scrub NaNs
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (!std::isfinite(channelData[i]))
                    channelData[i] = 0.0f;
            }
        }

        // Clear unused output channels
        for (int i = numInputChannels; i < numOutputChannels; ++i)
            buffer.clear(i, 0, numSamples);

        juce::dsp::AudioBlock<float> audioBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(audioBlock);

        // Read profile index from APVTS — exactly as MMV
        int newProfileIndex = 0;
        if (auto profileParam = apvts.getRawParameterValue("PROFILE_INDEX"))
            newProfileIndex = profileParam->load();

        profileIndex = newProfileIndex;

        if (profileIndex >= 0 && profileIndex < 50)
            currentProfile = profiles[juce::jlimit(0, 49, profileIndex)];
        else
            currentProfile = profiles[0];

        // Apply effects — exactly as MMV
        applyProfileEffects(buffer);
        applyLimiter(buffer);
        applyNoiseReduction(buffer, 0.5);
    }
    catch (const std::exception& e)
    {
        //DBG("ProcessBlock exception: " << e.what());
    }
}

//==============================================================================
void AncientVoicesAudioProcessor::initializeProfiles()
{
    for (int i = 0; i < AncientVoices::kNumPresets; ++i)
    {
        const auto& p = AncientVoices::kFactoryPresets[i];

        profiles[i] = {
            // pitchShiftAmount — full interval when shimmer active
            (p.shimmerMix > 0.05f) ? juce::jlimit(-12.f, 12.f, p.interval) : 0.f,
            // harmonizerAmount — on when 2+ voices (was 3+)
            (p.numVoices >= 2) ? 1.0f : 0.0f,
            // reverbAmount — doubled: caveMix * (0.6 + caveSize*0.4) gives fuller cave
            juce::jlimit(0.f, 1.f, p.caveMix * (0.6f + p.caveSize * 0.4f)),
            // distortionAmount — doubled from 0.3x to 0.6x
            p.roughness * 0.6f,
            // delayAmount — doubled from 0.12 to 0.24
            (std::fabsf(p.interval) > 1.f) ? 0.24f : 0.0f,
            // chorusDepth — doubled from 0.2 to 0.4
            0.4f,
            // flangerRate — doubled from 0.2x to 0.4x
            p.chant * 0.4f,
            // filterCutoff — era-specific, unchanged
            (p.era == 0) ? 4500.f : (p.era == 1) ? 5000.f : 5500.f,
            // compressionRatio — unchanged
            2.0f,
            // instabilityDepth — doubled from 0.2x to 0.4x
            p.drift * 0.4f,
            // vocalDoublerDetuneAmount — doubled from 0.015x to 0.03x
            juce::jlimit(0.f, 0.15f, p.numVoices * 0.03f),
            // vocalDoublerDelayMs — doubled from 12 to 20
            20.0f,
            // spacedOutDelayTimeMs — doubled from 25x to 50x
            p.spread * 50.0f,
            // spacedOutPanAmount — full spread value
            p.spread,
            // glitchAmount — still 0
            0.0f,
            // outputGain — keep at 0.9
            0.9f,
            // stopAndGoInterval
            0,
            // stopAndGoSilenceRatio
            0.0f,
            // stutterRepeatLength
            0,
            // reverseInterval
            0,
            // saturationDrive — raised from 0.7x to 1.0x
            p.breathe * 1.0f,
            // saturationMix — raised from 0.7x to 1.0x
            p.breathe * 1.0f,
            // saturationTone
            0.5f
        };
    }
}

//==============================================================================
void AncientVoicesAudioProcessor::applyFactoryPreset(int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= AncientVoices::kNumPresets)
        return;

    const auto& p = AncientVoices::kFactoryPresets[presetIndex];

    // Update the APVTS parameters to match preset values
    if (auto* param = apvts.getParameter("PROFILE_INDEX"))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(presetIndex)));

    if (auto* param = apvts.getParameter("ERA"))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(p.era)));
    if (auto* param = apvts.getParameter("VOICE_MODE"))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(p.voiceMode)));
    if (auto* param = apvts.getParameter("NUM_VOICES"))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(p.numVoices)));
    if (auto* param = apvts.getParameter("SPREAD"))
        param->setValueNotifyingHost(param->convertTo0to1(p.spread));
    if (auto* param = apvts.getParameter("DRIFT"))
        param->setValueNotifyingHost(param->convertTo0to1(p.drift));
    if (auto* param = apvts.getParameter("CHANT"))
        param->setValueNotifyingHost(param->convertTo0to1(p.chant));
    if (auto* param = apvts.getParameter("FORMANT_DEPTH"))
        param->setValueNotifyingHost(param->convertTo0to1(p.formantDepth));
    if (auto* param = apvts.getParameter("MIX"))
        param->setValueNotifyingHost(param->convertTo0to1(p.mix));
    if (auto* param = apvts.getParameter("CAVE_MIX"))
        param->setValueNotifyingHost(param->convertTo0to1(p.caveMix));
    if (auto* param = apvts.getParameter("CAVE_SIZE"))
        param->setValueNotifyingHost(param->convertTo0to1(p.caveSize));
    if (auto* param = apvts.getParameter("SHIMMER_MIX"))
        param->setValueNotifyingHost(param->convertTo0to1(p.shimmerMix));
    if (auto* param = apvts.getParameter("BREATHE"))
        param->setValueNotifyingHost(param->convertTo0to1(p.breathe));
    if (auto* param = apvts.getParameter("ROUGHNESS"))
        param->setValueNotifyingHost(param->convertTo0to1(p.roughness));
    if (auto* param = apvts.getParameter("INTERVAL"))
        param->setValueNotifyingHost(param->convertTo0to1(p.interval));

    // Also directly set currentProfile from the profiles array
    currentProfile = profiles[presetIndex];
}

//==============================================================================
void AncientVoicesAudioProcessor::loadCurrentProfileFromParams()
{
    // Read live parameter values from APVTS
    int era       = static_cast<int>(apvts.getRawParameterValue("ERA")->load());
    int numVoices = static_cast<int>(apvts.getRawParameterValue("NUM_VOICES")->load());
    float spread     = apvts.getRawParameterValue("SPREAD")->load();
    float drift      = apvts.getRawParameterValue("DRIFT")->load();
    float chant      = apvts.getRawParameterValue("CHANT")->load();
    float caveMix    = apvts.getRawParameterValue("CAVE_MIX")->load();
    float caveSize   = apvts.getRawParameterValue("CAVE_SIZE")->load();
    float shimmerMix = apvts.getRawParameterValue("SHIMMER_MIX")->load();
    float breathe    = apvts.getRawParameterValue("BREATHE")->load();
    float roughness  = apvts.getRawParameterValue("ROUGHNESS")->load();
    float interval   = apvts.getRawParameterValue("INTERVAL")->load();
    float outputGainDb = apvts.getRawParameterValue("OUTPUT_GAIN")->load();

    // Apply the same mapping table as initializeProfiles()
    currentProfile.pitchShiftAmount     = (shimmerMix > 0.05f) ? juce::jlimit(-12.f, 12.f, interval) : 0.f;
    currentProfile.harmonizerAmount     = (numVoices >= 2) ? 1.0f : 0.0f;
    currentProfile.reverbAmount         = juce::jlimit(0.f, 1.f, caveMix * (0.6f + caveSize * 0.4f));
    currentProfile.distortionAmount     = roughness * 0.6f;
    currentProfile.delayAmount          = (std::fabsf(interval) > 1.f) ? 0.24f : 0.0f;
    currentProfile.chorusDepth          = 0.4f;
    currentProfile.flangerRate          = chant * 0.4f;
    currentProfile.filterCutoff         = (era == 0) ? 4500.f : (era == 1) ? 5000.f : 5500.f;
    currentProfile.compressionRatio     = 2.0f;
    currentProfile.instabilityDepth     = drift * 0.4f;
    currentProfile.vocalDoublerDetuneAmount = juce::jlimit(0.f, 0.15f, numVoices * 0.03f);
    currentProfile.vocalDoublerDelayMs  = 20.0f;
    currentProfile.spacedOutDelayTimeMs = spread * 50.0f;
    currentProfile.spacedOutPanAmount   = spread;
    currentProfile.glitchAmount         = 0.0f;
    currentProfile.outputGain           = juce::Decibels::decibelsToGain(outputGainDb);
    currentProfile.stopAndGoInterval    = 0;
    currentProfile.stopAndGoSilenceRatio = 0.0f;
    currentProfile.stutterRepeatLength  = 0;
    currentProfile.reverseInterval      = 0;
    currentProfile.saturationDrive      = breathe * 1.0f;
    currentProfile.saturationMix        = breathe * 1.0f;
    currentProfile.saturationTone       = 0.5f;
}

//==============================================================================
// ALL DSP FUNCTIONS BELOW ARE VERBATIM FROM MMV — DO NOT MODIFY
//==============================================================================

void AncientVoicesAudioProcessor::applyProfileEffects(juce::AudioBuffer<float>& buffer)
{
    //DBG("Starting applyProfileEffects...");

    try
    {
        // Ensure buffer has samples
        if (buffer.getNumSamples() == 0)
               {
                   //DBG("Buffer is empty, exiting applyProfileEffects.");
                   return;
               }

               const int numChannels = buffer.getNumChannels();
               const double sampleRate = getSampleRate();

               //DBG("Number of Channels: " << numChannels);
               //DBG("Sample Rate: " << sampleRate);

               if (sampleRate <= 0.0 || numChannels <= 0)
               {
                   //DBG("Invalid sample rate or number of channels, exiting.");
                   return;
               }

               // Initialize RubberBand stretcher if not done already
               if (rubberBandStretcher == nullptr)
               {
                   rubberBandStretcher = std::make_unique<RubberBand::RubberBandStretcher>(sampleRate, numChannels);
                   //DBG("Initialized RubberBand stretcher.");
               }

               // Ensure pitch shift amount is within expected bounds
               float pitchShiftAmount = currentProfile.pitchShiftAmount;
               //DBG("Pitch Shift Amount: " << pitchShiftAmount);
               if (pitchShiftAmount < -12.0f || pitchShiftAmount > 12.0f)
               {
                   //DBG("Pitch shift amount out of range, returning.");
                   return;
               }

               // Process pitch shifting
               //DBG("Applying Pitch Shift...");
               applyRubberBandPitchShift(buffer, pitchShiftAmount);

        // Harmonizer effect: Only applied if the harmonizer amount is greater than 0
        //DBG("Starting applyProfileEffects...Harmonizer Effect");
        if (currentProfile.harmonizerAmount > 0.0f)
        {
            //DBG("Applying harmony effect with amount: " << currentProfile.harmonizerAmount);
            const int numSamples = buffer.getNumSamples();

            // Check if rubberBandStretcher is initialized and available for harmony effect
            if (rubberBandStretcher != nullptr)
            {
                size_t availableSamples = rubberBandStretcher->available();
                //DBG("rubberBandStretcher available samples: " << availableSamples);

                if (availableSamples >= static_cast<size_t>(numSamples))
                {
                    applyHarmonization(buffer, currentProfile.harmonizerAmount);
                    //DBG("Finished applying harmonization.");
                }
                else
                {
                    //DBG("Skipping harmony effect due to insufficient processed audio.");
                }
            }
            else
            {
                //DBG("Error: rubberBandStretcher is null.");
            }
        }
        else
        {
            //DBG("Skipping harmony effect because amount is 0.");
        }

        // Distortion effect: Adds grit or overdrive to the signal
        if (currentProfile.distortionAmount > 0.0f)
        {
            //DBG("Applying distortion with amount: " << currentProfile.distortionAmount);
            applyDistortionEffect(buffer, currentProfile.distortionAmount);
        }
        else
        {
            //DBG("Skipping distortion because amount is 0.");
        }

        // Compression: Controls dynamics by reducing louder parts of the audio
        if (currentProfile.compressionRatio > 0.0f)
        {
            //DBG("Applying compression with ratio: " << currentProfile.compressionRatio);
            applyCompressionEffect(buffer, currentProfile.compressionRatio);
        }
        else
        {
            //DBG("Skipping compression because ratio is 0.");
        }

        // Chorus effect: Adds depth and thickness by modulating delayed copies of the signal
        if (currentProfile.chorusDepth > 0.0f)
        {
            //DBG("Applying chorus with depth: " << currentProfile.chorusDepth);
            applyChorusEffect(buffer, currentProfile.chorusDepth);
        }
        else
        {
            //DBG("Skipping chorus because depth is 0.");
        }

        // Flanger effect: Adds a sweeping or swirling sound to the audio
        if (currentProfile.flangerRate > 0.0f)
        {
            //DBG("Applying flanger with rate: " << currentProfile.flangerRate);
            applyFlangerEffect(buffer, currentProfile.flangerRate);
        }
        else
        {
            //DBG("Skipping flanger because rate is 0.");
        }

        // Delay effect: Creates echoes by feeding delayed versions of the signal back into the audio
        if (currentProfile.delayAmount > 0.0f)
        {
            //DBG("Applying delay with amount: " << currentProfile.delayAmount);
            applyDelayEffect(buffer, currentProfile.delayAmount);
        }
        else
        {
            //DBG("Skipping delay because amount is 0.");
        }

        // Reverb: Adds space and depth by simulating room reflections
        if (currentProfile.reverbAmount > 0.0f)
        {
            //DBG("Applying reverb with amount: " << currentProfile.reverbAmount);
            applyReverbEffect(buffer, currentProfile.reverbAmount);
        }
        else
        {
            //DBG("Skipping reverb because amount is 0.");
        }

        // New Effects

        // Instability: Adds slight random modulation for a natural feel
        if (currentProfile.instabilityDepth > 0.0f)
        {
            //DBG("Applying instability with depth: " << currentProfile.instabilityDepth);
            applyInstabilityEffect(buffer, currentProfile.instabilityDepth);
        }

        // Vocal Doubler: Adds doubling effect using detuning and slight delay
        if (currentProfile.vocalDoublerDetuneAmount > 0.0f || currentProfile.vocalDoublerDelayMs > 0.0f)
        {
            //DBG("Applying vocal doubler with detune amount: " << currentProfile.vocalDoublerDetuneAmount
               // << " and delay: " << currentProfile.vocalDoublerDelayMs << "ms");
            applyVocalDoublerEffect(buffer, currentProfile.vocalDoublerDetuneAmount, currentProfile.vocalDoublerDelayMs);
        }

        // Spaced Out Effect: Panning and delay for an ambient, wide-spaced sound
        if (currentProfile.spacedOutDelayTimeMs > 0.0f || currentProfile.spacedOutPanAmount > 0.0f)
        {
            //DBG("Applying spaced-out effect with delay time: " << currentProfile.spacedOutDelayTimeMs
                //<< "ms and pan amount: " << currentProfile.spacedOutPanAmount);
            applySpacedOutDelay(buffer, currentProfile.spacedOutDelayTimeMs, currentProfile.spacedOutPanAmount);
        }

        // Glitch Effect: Adds rhythmic interruptions and "glitches" to the audio
        if (currentProfile.glitchAmount > 0.0f)
        {
            //DBG("Applying glitch effect with amount: " << currentProfile.glitchAmount);
            applyGlitchEffect(buffer, currentProfile.glitchAmount);
        }

        // Stop-and-Go Effect: Adds pauses by alternating between sound and silence
        if (currentProfile.stopAndGoInterval > 0)
        {
            //DBG("Applying stop-and-go effect with interval: " << currentProfile.stopAndGoInterval
                //<< " samples and silence ratio: " << currentProfile.stopAndGoSilenceRatio);
            applyStopAndGoEffect(buffer, currentProfile.stopAndGoInterval, currentProfile.stopAndGoSilenceRatio);
        }

        // Stutter Effect: Repeats short segments of audio for a rhythmic, percussive effect
        if (currentProfile.stutterRepeatLength > 0)
        {
            //DBG("Applying stutter effect with repeat length: " << currentProfile.stutterRepeatLength << " samples");
            applyStutterEffect(buffer, currentProfile.stutterRepeatLength);
        }

        // Reverser Effect: Periodically reverses sections of audio for a unique sound
        if (currentProfile.reverseInterval > 0)
        {
            //DBG("Applying reverser effect with interval: " << currentProfile.reverseInterval << " samples");
            applyReverserEffect(buffer, currentProfile.reverseInterval);
        }

        // Apply output gain adjustment to the entire buffer
        float gain = currentProfile.outputGain;
        //DBG("Applying gain with amount: " << gain);
        buffer.applyGain(gain);

        //DBG("Finished applying profile effects.");
    }
    catch (const std::exception& e)
    {
        //DBG("Exception in applyProfileEffects: " << e.what());
    }
    catch (...)
    {
        //DBG("Unknown exception in applyProfileEffects.");
    }
}

static float saturationCurve(float x)
{
    return std::tanh(x);  // Use tanh for smooth clipping
}


void AncientVoicesAudioProcessor::applyAdvancedSaturation(juce::AudioBuffer<float>& buffer, float saturationAmount, float mixAmount, float cutoffFreq)
{
    //DBG("Starting applySaturationEffect...");
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Loop through each channel and apply saturation effect
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Soft clipping saturation
            float cleanSample = channelData[sample];
            float saturatedSample = cleanSample * (1.0f + saturationAmount);

            // Apply a soft clipping curve to prevent harshness
            if (saturatedSample > 1.0f) saturatedSample = 1.0f - (1.0f / saturatedSample);
            else if (saturatedSample < -1.0f) saturatedSample = -1.0f + (1.0f / saturatedSample);

            // Wet/Dry mix
            channelData[sample] = (mixAmount * saturatedSample) + ((1.0f - mixAmount) * cleanSample);
        }
    }

    // Low-pass filter to smooth harsh high frequencies
    juce::dsp::IIR::Filter<float> lowPassFilter;
    juce::dsp::IIR::Coefficients<float>::Ptr filterCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), cutoffFreq);
    lowPassFilter.coefficients = filterCoefficients;
    lowPassFilter.reset();

    juce::dsp::AudioBlock<float> audioBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> context(audioBlock);
    lowPassFilter.process(context);

    //DBG("Saturation effect applied with saturationAmount: " << saturationAmount << ", mixAmount: " << mixAmount << ", cutoffFreq: " << cutoffFreq);
}

void AncientVoicesAudioProcessor::performPitchShifting(juce::AudioBuffer<float>& buffer, float shiftAmount)
{
    // Advanced phase vocoder pitch shifting implementation
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    float pitchShiftFactor = std::pow(2.0f, shiftAmount / 12.0f);

    // Ensure that the FFT buffer can handle the number of channels
    fftBuffer.setSize(numChannels, fftSize * 2, false, true, true);
    overlapBuffer.setSize(numChannels, fftSize, false, true, true);
    
    // Buffers for magnitude and phase
    std::vector<std::vector<float>> magnitudes(numChannels, std::vector<float>(fftSize / 2));
    std::vector<std::vector<float>> phases(numChannels, std::vector<float>(fftSize / 2));
    std::vector<std::vector<float>> prevPhases(numChannels, std::vector<float>(fftSize / 2, 0.0f));
    std::vector<std::vector<float>> synthPhases(numChannels, std::vector<float>(fftSize / 2, 0.0f));

    for (int channel = 0; channel < numChannels; ++channel)
    {
        int bufferPos = 0;
        while (bufferPos + fftSize < numSamples)
        {
            // Copy samples to FFT buffer
            juce::FloatVectorOperations::copy(fftBuffer.getWritePointer(channel),
                                              buffer.getReadPointer(channel, bufferPos),
                                              fftSize);

            // Apply window function
            windowFunction.multiplyWithWindowingTable(fftBuffer.getWritePointer(channel), fftSize);

            // Perform FFT
            fft.performRealOnlyForwardTransform(fftBuffer.getWritePointer(channel));

            // Analyze magnitude and phase
            for (int i = 0; i < fftSize / 2; ++i)
            {
                float real = fftBuffer.getSample(channel, 2 * i);
                float imag = fftBuffer.getSample(channel, 2 * i + 1);

                magnitudes[channel][i] = std::sqrt(real * real + imag * imag);
                phases[channel][i] = std::atan2(imag, real);
            }

            // Phase vocoder processing
            for (int i = 0; i < fftSize / 2; ++i)
            {
                // Calculate phase differences
                float phaseDiff = phases[channel][i] - prevPhases[channel][i];
                prevPhases[channel][i] = phases[channel][i];

                // Unwrap phase
                phaseDiff -= hopSize * 2.0f * juce::MathConstants<float>::pi * i / fftSize;
                phaseDiff = std::fmod(phaseDiff + juce::MathConstants<float>::pi, 2.0f * juce::MathConstants<float>::pi) - juce::MathConstants<float>::pi;

                // Calculate true frequency
                float trueFreq = 2.0f * juce::MathConstants<float>::pi * i / fftSize + phaseDiff / hopSize;

                // Accumulate phases
                synthPhases[channel][i] += hopSize * trueFreq * pitchShiftFactor;
            }

            // Synthesize new FFT bins
            for (int i = 0; i < fftSize / 2; ++i)
            {
                int newIndex = int(i / pitchShiftFactor);
                if (newIndex < fftSize / 2)
                {
                    float magnitude = magnitudes[channel][newIndex];
                    float phase = synthPhases[channel][newIndex];


                    fftBuffer.setSample(channel, 2 * i, magnitude * std::cos(phase));
                    fftBuffer.setSample(channel, 2 * i + 1, magnitude * std::sin(phase));
                }
                else
                {
                    fftBuffer.setSample(channel, 2 * i, 0.0f);
                    fftBuffer.setSample(channel, 2 * i + 1, 0.0f);
                }
            }

            // Perform inverse FFT
            fft.performRealOnlyInverseTransform(fftBuffer.getWritePointer(channel));

            // Overlap-add
            for (int i = 0; i < fftSize; ++i)
            {
                float sample = fftBuffer.getSample(channel, i);
                overlapBuffer.setSample(channel, (overlapPosition + i) % fftSize,
                                        overlapBuffer.getSample(channel, (overlapPosition + i) % fftSize) + sample);
            }

            // Copy to output buffer
            buffer.copyFrom(channel, bufferPos, overlapBuffer, channel, overlapPosition, hopSize);

            // Clear the used portion of overlap buffer
            for (int i = 0; i < hopSize; ++i)
            {
                overlapBuffer.setSample(channel, (overlapPosition + i) % fftSize, 0.0f);
            }

            // Update positions
            bufferPos += hopSize;
            overlapPosition = (overlapPosition + hopSize) % fftSize;
        }
    }
}

void AncientVoicesAudioProcessor::applyRepeaterEffect(juce::AudioBuffer<float>& buffer, float repeaterAmount)
{
    //DBG("Applying Enhanced Repeater Effect with repeaterAmount: " << repeaterAmount);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();

    // Calculate delay based on tempo and repeater amount
    double bpm = getTempo();
    if (bpm <= 0.0)
        bpm = 120.0; // Default tempo

    double beatDuration = 60.0 / bpm;
    int delaySamples = static_cast<int>(sampleRate * beatDuration * repeaterAmount);
    const float decayFactor = 0.6f; // Decay per repeat
    int repeats = juce::jlimit(2, 6, static_cast<int>(repeaterAmount * 6)); // Repeat 2 to 6 times

    juce::AudioBuffer<float> delayBuffer(numChannels, numSamples + delaySamples * repeats);
    delayBuffer.clear();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        delayBuffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);
    }

    // Add repeats with slight pitch shifts for variety
    for (int repeat = 1; repeat <= repeats; ++repeat)
    {
        int delayOffset = delaySamples * repeat;
        float repeatGain = powf(decayFactor, repeat);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* delayData = delayBuffer.getWritePointer(channel);
            const float* sourceData = buffer.getReadPointer(channel);

            for (int i = 0; i < numSamples; ++i)
            {
                int delayedIndex = i + delayOffset;
                if (delayedIndex >= delayBuffer.getNumSamples())
                    break;

                // Add slight pitch modulation for musical effect
                float pitchShiftFactor = 1.0f + (repeat * 0.02f * repeaterAmount);
                delayData[delayedIndex] += sourceData[i] * repeatGain * pitchShiftFactor;
            }
        }
    }

    // Mix delay buffer back into the main buffer
    for (int channel = 0; channel < numChannels; ++channel)
    {
        buffer.addFrom(channel, 0, delayBuffer, channel, 0, numSamples, 1.0f);
    }

    //DBG("Enhanced Repeater Effect Applied.");
}

void AncientVoicesAudioProcessor::applyDistortionEffect(juce::AudioBuffer<float>& buffer, float distortionAmount)
{
    if (distortionAmount <= 0.0f)
        return; // Skip distortion if amount is zero

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Scale the distortionAmount to make the effect more noticeable
    float drive = distortionAmount * 10.0f; // Increased scaling factor

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Apply distortion function
            float x = channelData[sample] * drive;
            channelData[sample] = std::tanh(x); // Soft clipping distortion
        }
    }
}

//---------------------------- PITCH SHIFTING ------------------------

#include <rubberband/RubberBandStretcher.h>
// Assuming RubberBand is included and initialized properly in your project
void AncientVoicesAudioProcessor::applyRubberBandPitchShift(juce::AudioBuffer<float>& buffer, float pitchShiftAmount)
{
    static double lastSampleRate = getSampleRate();
    static int lastChannelCount = buffer.getNumChannels();

    try
    {
        //DBG("Starting applyRubberBandPitchShift...");
        
        // Check buffer validity
        if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        {
            //DBG("Invalid buffer: zero samples or channels.");
            return;
        }

        // Reinitialize RubberBandStretcher if sample rate or channel count has changed
        if (!rubberBandPitchShifter || lastSampleRate != getSampleRate() || lastChannelCount != buffer.getNumChannels())
        {
            //DBG("Reinitializing RubberBandStretcher due to sample rate or channel count change.");
            rubberBandPitchShifter = std::make_unique<RubberBand::RubberBandStretcher>(
                getSampleRate(),
                buffer.getNumChannels(),
                RubberBand::RubberBandStretcher::OptionProcessRealTime | RubberBand::RubberBandStretcher::OptionPitchHighQuality
            );

            // Update last known values
            lastSampleRate = getSampleRate();
            lastChannelCount = buffer.getNumChannels();
        }

        // Set pitch shift scale
        float pitchScale = std::pow(2.0f, pitchShiftAmount / 12.0f);
        //DBG("Setting pitch scale to: " << pitchScale);
        rubberBandPitchShifter->setPitchScale(pitchScale);

        // Process the audio buffer
        //DBG("Processing audio buffer with " << buffer.getNumSamples() << " samples.");
        rubberBandPitchShifter->process(buffer.getArrayOfWritePointers(), buffer.getNumSamples(), false);

        // Retrieve available samples without exceeding buffer size
        int available = rubberBandPitchShifter->available();
        //DBG("Available samples: " << available);

        if (available > buffer.getNumSamples())
        {
            available = buffer.getNumSamples(); // Limit to buffer size
        }

        if (available > 0)
        {
            //DBG("Retrieving " << available << " samples from RubberBandStretcher.");
            rubberBandPitchShifter->retrieve(buffer.getArrayOfWritePointers(), available);
        }
        else
        {
            //DBG("No available samples to retrieve.");
        }

        //DBG("applyRubberBandPitchShift completed successfully.");
    }
    catch (const std::exception& e)
    {
        //DBG("Exception in applyRubberBandPitchShift: " << e.what());
    }
    catch (...)
    {
        //DBG("Unknown exception in applyRubberBandPitchShift.");
    }
}

//==============================================================================

void AncientVoicesAudioProcessor::applyDelayEffect(juce::AudioBuffer<float>& buffer, float delayAmount)
{
    //DBG("delay called.");
    if (delayAmount <= 0.0f || delayAmount > maxAllowedDelay)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const int delayInSamples = static_cast<int>(delayAmount * getSampleRate());

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (delayInSamples <= delayLines[channel].getDelay())
            {
                float delayedSample = delayLines[channel].popSample(delayInSamples, true);
                delayLines[channel].pushSample(channel, channelData[sample]);
                channelData[sample] = (delayedSample * delayAmount) + (channelData[sample] * (1.0f - delayAmount));
            }
        }
    }
}

void AncientVoicesAudioProcessor::applyChorusEffect(juce::AudioBuffer<float>& buffer, float depth)
{
    //DBG("chorus called.");
    if (depth <= 0.0f)
        return;

    chorus.setRate(0.5f);          // Increased LFO rate for more noticeable modulation
    chorus.setDepth(depth * 0.5f); // Adjust depth scaling as needed
    chorus.setCentreDelay(15.0f);  // Base delay in ms
    chorus.setFeedback(0.0f);      // Feedback amount
    chorus.setMix(0.7f);           // Increased mix for more effect

    // Process chorus
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    chorus.process(context);
}

//==============================================================================
void AncientVoicesAudioProcessor::applyFlangerEffect(juce::AudioBuffer<float>& buffer, float rate)
{
    //DBG("flanger called.");
    if (rate <= 0.0f)
        return;

    flanger.setRate(rate * 1.0f);          // Adjust LFO rate
    flanger.setDepth(0.8f);                 // Increased depth for more pronounced effect
    flanger.setCentreDelay(3.0f);           // Base delay in ms
    flanger.setFeedback(0.5f);              // Feedback amount
    flanger.setMix(0.6f);                   // Increased mix

    // Process flanger
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    flanger.process(context);
}

//==============================================================================
void AncientVoicesAudioProcessor::applyCompressionEffect(juce::AudioBuffer<float>& buffer, float ratio)
{
    //DBG("compression called.");
    if (ratio <= 0.0f)
        return;

    // Apply compression effect
    compressor.setThreshold(-24.0f);  // Threshold in dB
    compressor.setRatio(ratio);       // Compression ratio
    compressor.setAttack(10.0f);      // Attack time in ms
    compressor.setRelease(100.0f);    // Release time in ms

    // Process compressor
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);
}

double AncientVoicesAudioProcessor::getTempo() const
{
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm())
            {
                return *bpm;
            }
        }
    }
    return 120.0;  // Default to 120 BPM if no tempo is available
}

//------------------------------------------------- REPEATER STUTTER NOISE REDUCTION
void AncientVoicesAudioProcessor::applyStutterEffect(juce::AudioBuffer<float>& buffer, float stutterAmount)
{
    //DBG("Starting Simple Stutter Effect with stutterAmount: " << stutterAmount);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();

    // Calculate stutter length based on stutterAmount with a minimum threshold
    int stutterLength = std::max(static_cast<int>(sampleRate * stutterAmount), 8); // Minimum of 8 samples

    // Limit stutter length to prevent excessive buffer use
    stutterLength = juce::jlimit(8, numSamples / 4, stutterLength);

    // Only apply if stutterLength is reasonable
    if (stutterLength < numSamples)
    {
        // Loop over each channel
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);

            // Process each segment of the audio buffer
            for (int position = 0; position < numSamples; position += stutterLength * 3)
            {
                // Repeat the stuttered audio chunk to mimic stuttering
                for (int repeat = 0; repeat < 2; ++repeat)
                {
                    int offset = repeat * stutterLength;
                    if (position + offset + stutterLength < numSamples)
                    {
                        // Copy the stutter segment
                        for (int i = 0; i < stutterLength; ++i)
                        {
                            channelData[position + offset + i] = channelData[position + i];
                        }
                    }
                }

                // Apply volume reduction for stutter effect
                for (int i = 0; i < stutterLength && position + i < numSamples; ++i)
                {
                    channelData[position + i] *= 0.8f;  // Reduce volume slightly
                }

                // Add extended silence if buffer allows
                if (position + stutterLength * 2 < numSamples)
                {
                    for (int i = 0; i < stutterLength && position + stutterLength * 2 + i < numSamples; ++i)
                    {
                        channelData[position + stutterLength * 2 + i] = 0.0f;  // Add extended silence
                    }
                }
            }
        }
        //DBG("Extended Stutter Effect Applied Successfully.");
    }
    else
    {
        //DBG("Stutter Length too long for buffer size; effect not applied.");
    }
}

void AncientVoicesAudioProcessor::applyNoiseReduction(juce::AudioBuffer<float>& buffer, float reductionThreshold)
{
    float noiseReductionThreshold = juce::Decibels::decibelsToGain(-55.0f);  // Around -55 dB
    float reductionAmount = 0.25f;  // 25% reduction

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            if (std::abs(channelData[sample]) < noiseReductionThreshold)
            {
                // Reduce noise smoothly
                channelData[sample] *= (1.0f - reductionAmount);
            }
        }
    }

    //DBG("Noise reduction applied with threshold at " << reductionThreshold << " and reduction amount: " << reductionAmount);
}

//-------------------------------------------------------

void AncientVoicesAudioProcessor::applyReverbEffect(juce::AudioBuffer<float>& buffer, float reverbAmount)
{
    //DBG("delay called.");
    if (reverbAmount <= 0.0f)
        return;

    juce::dsp::Reverb::Parameters reverbParams = reverb.getParameters();
    reverbParams.wetLevel = reverbAmount * 0.7f; // Increased wet level
    reverbParams.dryLevel = 1.0f - reverbParams.wetLevel;
    reverb.setParameters(reverbParams);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    reverb.process(context);
}

void AncientVoicesAudioProcessor::applyDynamicEQ(juce::AudioBuffer<float>& buffer, int profileIndex)
{
    float cutoffLow = 0.0f;
    float cutoffMid = 0.0f;
    float cutoffHigh = 0.0f;

    // Define static EQ settings per profile
    switch (profileIndex)
    {
        case 1:  cutoffHigh = 5000.0f;  break;
        case 2:  cutoffMid = 3000.0f;   break;
        case 3:  cutoffLow = 800.0f;    break;
        case 4:  cutoffMid = 2000.0f;   break;
        case 5:  cutoffHigh = 6000.0f;  break;
        case 6:  cutoffMid = 3500.0f;   break;
        case 7:  cutoffMid = 2500.0f;   break;
        case 8:  cutoffLow = 1500.0f; cutoffMid = 1500.0f; break;
        case 9:  cutoffHigh = 5500.0f;  break;
        case 10: cutoffLow = 1200.0f; cutoffMid = 1200.0f; break;
        case 11: cutoffHigh = 6000.0f;  break;
        case 12: cutoffLow = 1000.0f; cutoffMid = 1000.0f; break;
        case 13: cutoffMid = 4000.0f; cutoffHigh = 4000.0f; break;
        case 14: cutoffHigh = 7000.0f;  break;
        case 15: cutoffMid = 2500.0f;   break;
        case 16: cutoffMid = 3000.0f;   break;
        case 17: cutoffMid = 2500.0f;   break;
        case 18: cutoffLow = 1000.0f; cutoffMid = 1000.0f; break;
        case 19: cutoffMid = 4500.0f; cutoffHigh = 4500.0f; break;
        case 20: cutoffHigh = 6000.0f;  break;
        case 21: cutoffLow = 800.0f;    break;
        case 22: cutoffMid = 3500.0f;   break;
        case 23: cutoffHigh = 5500.0f;  break;
        case 24: cutoffLow = 700.0f;    break;
        case 25: cutoffMid = 3500.0f; cutoffHigh = 3500.0f; break;
        case 26: cutoffLow = 1200.0f;   break;
        case 27: cutoffHigh = 4000.0f;  break;
        case 28: cutoffMid = 2500.0f;   break;
        case 29: cutoffHigh = 5000.0f;  break;
        case 30: cutoffLow = 1000.0f;   break;
        default: cutoffMid = 2000.0f;   break;
    }
}

void AncientVoicesAudioProcessor::applyFormantShifting(juce::AudioBuffer<float>& buffer, int profileIndex)
{
    //DBG("formant shifting called.");
    float shiftInterval = 0.0f;  // Default: no shift

    switch (profileIndex)
    {
        case 1:  shiftInterval = 1.0f;   break;
        case 2:  shiftInterval = 0.5f;   break;
        case 3:  shiftInterval = -1.0f;  break;
        case 4:  shiftInterval = 0.0f;   break;
        case 5:  shiftInterval = 2.0f;   break;
        case 6:  shiftInterval = 1.0f;   break;
        case 7:  shiftInterval = 0.5f;   break;
        case 8:  shiftInterval = -0.5f;  break;
        case 9:  shiftInterval = 1.5f;   break;
        case 10: shiftInterval = -1.0f;  break;
        case 11: shiftInterval = 2.0f;   break;
        case 12: shiftInterval = -1.5f;  break;
        case 13: shiftInterval = 0.5f;   break;
        case 14: shiftInterval = 2.0f;   break;
        case 15: shiftInterval = 0.0f;   break;
        case 16: shiftInterval = 1.0f;   break;
        case 17: shiftInterval = 0.0f;   break;
        case 18: shiftInterval = -1.0f;  break;
        case 19: shiftInterval = 1.5f;   break;
        case 20: shiftInterval = 1.0f;   break;
        case 21: shiftInterval = -2.0f;  break;
        case 22: shiftInterval = 0.5f;   break;
        case 23: shiftInterval = 1.0f;   break;
        case 24: shiftInterval = -1.5f;  break;
        case 25: shiftInterval = 0.5f;   break;
        case 26: shiftInterval = -1.0f;  break;
        case 27: shiftInterval = 1.0f;   break;
        case 28: shiftInterval = 0.5f;   break;
        case 29: shiftInterval = 1.0f;   break;
        case 30: shiftInterval = -1.0f;  break;
        default: break;
    }

    // Use existing pitch shifting method to handle the shift
    applyRubberBandPitchShift(buffer, shiftInterval);
}

//==============================================================================
bool AncientVoicesAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AncientVoicesAudioProcessor::createEditor()
{
    return new AncientVoicesAudioProcessorEditor(*this);
}

//==============================================================================
const juce::String AncientVoicesAudioProcessor::getName() const { return JucePlugin_Name; }

bool AncientVoicesAudioProcessor::acceptsMidi() const { return false; }
bool AncientVoicesAudioProcessor::producesMidi() const { return false; }
bool AncientVoicesAudioProcessor::isMidiEffect() const { return false; }
double AncientVoicesAudioProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
void AncientVoicesAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save parameters state using apvts
    juce::MemoryOutputStream stream(destData, true);
    apvts.state.writeToStream(stream);
}

void AncientVoicesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Load parameters state using apvts
    juce::ValueTree tree = juce::ValueTree::readFromData(data, size_t (sizeInBytes));
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

void AncientVoicesAudioProcessor::applyHarmonization(juce::AudioBuffer<float>& buffer, float harmonizerAmount)
{
    //DBG("Entering applyHarmonization...");

    if (rubberBandStretcher == nullptr)
    {
        //DBG("Error: rubberBandStretcher is null.");
        return;
    }

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // Define three harmony intervals: major third (4 semitones), perfect fifth (7 semitones), and octave (-12 semitones)
    std::vector<float> harmonyIntervals = { 4.0f, 7.0f, -12.0f };

    // Prepare input data for RubberBandStretcher
    std::vector<const float*> inputData(numChannels);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        inputData[channel] = buffer.getReadPointer(channel);
    }

    // Process the three harmonies
    for (float interval : harmonyIntervals)
    {
        // Set pitch shift factor for the harmony
        float pitchFactor = std::pow(2.0f, interval / 12.0f);
        rubberBandStretcher->setPitchScale(pitchFactor);

        // Process the buffer
        rubberBandStretcher->process(inputData.data(), numSamples, false);

        // Retrieve the processed samples
        std::vector<float*> outputData(numChannels);
        for (int channel = 0; channel < numChannels; ++channel)
        {
            outputData[channel] = buffer.getWritePointer(channel);
        }

        rubberBandStretcher->retrieve(outputData.data(), numSamples);
    }

    //DBG("Finished applying harmonization.");
}

//======================================================================
void AncientVoicesAudioProcessor::applyLimiter(juce::AudioBuffer<float>& buffer)
{
    //DBG("limiter called.");
    
    // Check if the buffer has valid samples and channels
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        //DBG("Warning: Buffer is empty or not initialized. Skipping limiter process.");
        return;
    }

    try
    {
        // Set the threshold to a reasonable level
        limiter.setThreshold(-1.0f); // -1 dB

        // Create an audio block from the buffer and process it
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        limiter.process(context);
    }
    catch (const std::exception& e)
    {
        //DBG("Limiter processing exception: " << e.what());
    }
}

void AncientVoicesAudioProcessor::applyVocalDoublerEffect(juce::AudioBuffer<float>& buffer, float detuneAmount, float delayMs)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();
    const int delaySamples = static_cast<int>((delayMs / 1000.0) * sampleRate);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int i = 0; i < numSamples; ++i)
        {
            // Apply detune effect (simple pitch shift for vocal doubler)
            float detunedSample = juce::jmap(detuneAmount, channelData[i], channelData[(i + delaySamples) % numSamples]);

            // Apply delay effect to simulate doubling
            if (i >= delaySamples)
            {
                channelData[i] = (channelData[i] + detunedSample) / 2.0f;
            }
        }
    }
}

void AncientVoicesAudioProcessor::applySpacedOutDelay(juce::AudioBuffer<float>& buffer, float delayTimeMs, float panAmount)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();
    const int delaySamples = static_cast<int>((delayTimeMs / 1000.0) * sampleRate);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Apply delay with panning
            if (i >= delaySamples)
            {
                float panFactor = (channel == 0) ? (1.0f - panAmount) : (1.0f + panAmount);
                channelData[i] = channelData[i] + (channelData[i - delaySamples] * panFactor);
            }
        }
    }
}

void AncientVoicesAudioProcessor::applyInstabilityEffect(juce::AudioBuffer<float>& buffer, float instabilityDepth)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int i = 0; i < numSamples; ++i)
        {
            // Add random fluctuation to simulate instability
            float fluctuation = juce::Random::getSystemRandom().nextFloat() * instabilityDepth;
            channelData[i] *= (1.0f + fluctuation);
        }
    }
}

void AncientVoicesAudioProcessor::applyGlitchEffect(juce::AudioBuffer<float>& buffer, float glitchAmount)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int glitchStep = static_cast<int>(glitchAmount * 100);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int i = 0; i < numSamples; i += glitchStep)
        {
            if (juce::Random::getSystemRandom().nextFloat() < glitchAmount)
            {
                for (int j = 0; j < glitchStep && i + j < numSamples; ++j)
                {
                    channelData[i + j] = 0.0f;  // Apply mute (glitch) in small sections
                }
            }
        }
    }
}

void AncientVoicesAudioProcessor::applyReverserEffect(juce::AudioBuffer<float>& buffer, int reverseInterval)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        for (int startSample = 0; startSample < numSamples; startSample += reverseInterval)
        {
            int endSample = juce::jmin(startSample + reverseInterval, numSamples);
            
            // Reverse the section from startSample to endSample
            for (int i = 0; i < (endSample - startSample) / 2; ++i)
            {
                float temp = buffer.getSample(channel, startSample + i);
                buffer.setSample(channel, startSample + i, buffer.getSample(channel, endSample - i - 1));
                buffer.setSample(channel, endSample - i - 1, temp);
            }
        }
    }
}

void AncientVoicesAudioProcessor::applyStopAndGoEffect(juce::AudioBuffer<float>& buffer, int interval, float silenceRatio)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            int positionInCycle = sample % interval;
            if (positionInCycle > static_cast<int>(interval * silenceRatio))
            {
                // Silence the buffer for the duration of the 'stop' part of the cycle
                buffer.setSample(channel, sample, 0.0f);
            }
        }
    }
}

void AncientVoicesAudioProcessor::applyVocalTransitEffect(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float modulationDepth = 0.3f; // Controls how extreme the variation is
    juce::Random random;

    // Randomly vary the reverb, pitch, and delay in each block
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int i = 0; i < numSamples; ++i)
        {
            // Apply random modulation to pitch
            float pitchMod = 1.0f + (random.nextFloat() - 0.5f) * modulationDepth;
            channelData[i] *= pitchMod;

            // Randomly modulate reverb
            float reverbMod = (random.nextFloat() - 0.5f) * modulationDepth;
            channelData[i] *= (1.0f + reverbMod);

            // Add random delay
            if (random.nextFloat() < 0.01f) // 1% chance of adding delay
            {
                channelData[i] = (i > 50) ? channelData[i - 50] : 0.0f; // Adds a small echo effect
            }
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AncientVoicesAudioProcessor();
}
