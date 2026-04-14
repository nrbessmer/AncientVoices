
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Data/Presets.h"
#include "Data/Presets.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <rubberband/RubberBandStretcher.h>

//======================================================================nois========
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
      stutterEnabled (false),
      harmonyEnabled (false),
      noiseReductionEnabled (false),
      repeaterEnabled (false),
      apvts (*this, nullptr, "Parameters", createParameters())
{
    // Initialize profiles with their settings
    initializeProfiles();
    // Set currentProfile to the initial profile
    currentProfile = profiles[SoundProfile::Alpha];
    inputBuffer.setSize(getTotalNumInputChannels(), 0);
    // Configure ApplicationProperties with basic settings
    juce::PropertiesFile::Options options;
    options.applicationName = "AncientVoices";
    options.filenameSuffix = "settings";
    options.folderName = "AncientVoicesPlugin";
    options.osxLibrarySubFolder = "Application Support"; // macOS-specific folder
    duckingSensitivity = apvts.getRawParameterValue("DUCKING_SENSITIVITY");
    glitchIntensity = apvts.getRawParameterValue("GLITCH_INTENSITY");
    mixAmount  = apvts.getRawParameterValue("MIX");
    userReverb = apvts.getRawParameterValue("USER_REVERB");
    userDelay  = apvts.getRawParameterValue("USER_DELAY");
    userDrive  = apvts.getRawParameterValue("USER_DRIVE");
    userOutput = apvts.getRawParameterValue("USER_OUTPUT");
    userPitch  = apvts.getRawParameterValue("USER_PITCH");

    appProperties.setStorageParameters(options);}

AncientVoicesAudioProcessor::~AncientVoicesAudioProcessor()
{
    ////DBG("Destroying AncientVoicesAudioProcessor...");

    // Clean up RubberBandStretcher if it exists
    if (rubberBandStretcher != nullptr)
    {
        rubberBandStretcher.reset(); // Explicitly release unique pointer resources
        //DBG("RubberBandStretcher destroyed.");
    }

    // Clear DSP modules
    resetDSPModules();
    //DBG("DSP Modules reset.");

    // Clear delay lines and any other custom buffers
    delayLines.clear();
    lastInputSamples.clear();
    lastOutputSamples.clear();
    //DBG("Custom buffers cleared.");

    // Close any open threads or audio resources if necessary
    // If using any custom threads, ensure to signal them to stop and join here.
    
    //DBG("AncientVoicesAudioProcessor destruction complete.");
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AncientVoicesAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Profile Parameter using AudioParameterInt
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "PROFILE_INDEX",                // Parameter ID
        "Profile Index",                // Parameter name
        0,                              // Minimum value
        static_cast<int>(SoundProfile::NumProfiles) - 1, // Maximum value
        0                               // Default index (Alpha)
    ));

    // Add Bool Parameters
    params.push_back(std::make_unique<juce::AudioParameterBool>("STUTTER", "Stutter Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("HARMONY", "Harmony Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("NOISE_REDUCTION", "Noise Reduction Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("REPEATER", "Repeater Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DUCKING_SENSITIVITY", "Ducking Sensitivity", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GLITCH_INTENSITY", "Glitch Intensity", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX",         "Mix",    0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("USER_REVERB", "Reverb", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("USER_DELAY",  "Delay",  0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("USER_DRIVE",  "Drive",  0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("USER_OUTPUT", "Output", 0.0f, 2.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("USER_PITCH",  "Pitch",  -12.0f, 12.0f, 0.0f));

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

const juce::File licenseFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("AncientVoices").getChildFile("license.dat");// Function to check saved license status

bool AncientVoicesAudioProcessor::isLicenseValid(){
    if (licenseFile.existsAsFile()) {
        juce::String status = licenseFile.loadFileAsString();
        return (status.trim() == "valid");
    }
    return false;
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
    if (!isLicenseValid()) {
        // Set a flag to indicate invalid license instead of showing a message box
        isLicensed = false;
    } else {
        isLicensed = true;
        // Proceed with normal setup if licensed
    }
    this->sampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    //DBG("prepareToPlay called.");

    try
    {
        if (sampleRate <= 0)
        {
            //DBG("Invalid sample rate: " + juce::String(sampleRate));
            return;
        }

        //DBG("prepareToPlay called.2");

        // Initialize DSP Modules
        initializeDSPModules(spec);
        //DBG("prepareToPlay called.6");

        // Ensure the output channel count is valid
        int numOutputChannels = getTotalNumOutputChannels();
        if (numOutputChannels <= 0)
        {
            //DBG("Invalid number of output channels: " + juce::String(numOutputChannels));
            return;
        }

        // Ensure the input channel count is valid
        int numInputChannels = getTotalNumInputChannels();
        if (numInputChannels <= 0)
        {
            //DBG("Invalid number of input channels: " + juce::String(numInputChannels));
            return;
        }

        // Resize and initialize delay lines per channel
        delayLines.clear();
        delayLines.resize(numOutputChannels, juce::dsp::DelayLine<float>(2 * sampleRate));  // 2 seconds max delay

        const float maxAllowedDelayTimeInSeconds = 2.0f;
        const int maxDelaySamples = static_cast<int>(maxAllowedDelayTimeInSeconds * sampleRate);

        for (int i = 0; i < numOutputChannels; ++i)
        {
            delayLines[i].reset();
            delayLines[i].setMaximumDelayInSamples(maxDelaySamples);
        }

        //DBG("prepareToPlay called.7");
        maxAllowedDelay = maxAllowedDelayTimeInSeconds;

        //DBG("Delay lines initialized for " << numOutputChannels << " channels.");

        // Initialize waveform buffers ensuring correct size
        waveformBufferSize = std::max(1, samplesPerBlock * 2);  // Prevent zero size
        lastInputSamples.assign(waveformBufferSize, 0.0f);
        lastOutputSamples.assign(waveformBufferSize, 0.0f);

        // Log buffer size and channel info for validation
        //DBG("Waveform buffers resized to: " + juce::String(waveformBufferSize) + " samples.");

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
                options |= RubberBand::RubberBandStretcher::OptionWindowShort; // Short window for low latency
                //DBG("Configuring RubberBandStretcher for real-time performance.");
            } else {
                options |= RubberBand::RubberBandStretcher::OptionProcessOffline |
                           RubberBand::RubberBandStretcher::OptionWindowLong |
                           RubberBand::RubberBandStretcher::OptionPhaseIndependent;
                //DBG("Configuring RubberBandStretcher for offline processing.");
            }

            rubberBandStretcher = std::make_unique<RubberBand::RubberBandStretcher>(
                static_cast<size_t>(sampleRate),
                numInputChannels,
                options
            );
            //DBG("Initialized rubberBandStretcher with " << (isRealTime ? "real-time" : "offline") << " options.");
        }

        // Reset and configure rubberBandStretcher
        rubberBandStretcher->reset();
        rubberBandStretcher->setPitchScale(1.0f); // Neutral pitch scaling initially

        // Report latency to the host
        int latencySamples = rubberBandStretcher->getLatency();
        setLatencySamples(latencySamples);
        //DBG("rubberBandStretcher latency: " << latencySamples << " samples.");

        // Initialize AbstractFifo for harmonizer buffer
        harmonizerFifo.setTotalSize(16384); // Adjust size as needed
        harmonizerBuffer.setSize(numInputChannels, harmonizerFifo.getTotalSize());
        harmonizerBuffer.clear();
        //DBG("Initialized harmonizerFifo and harmonizerBuffer.");
        duckingGain.setGainLinear(1.0f);
        glitchEffect.functionToUse = [](float x) { return std::sin(x); }; // Example wave-shaping function
    }
    catch (const std::exception& e)
    {
        //DBG("PrepareToPlay exception: " << e.what());
    }
}


void AncientVoicesAudioProcessor::adjustForSampleRate(float currentSampleRate)
{
    float scalingFactor = (currentSampleRate < 16000.0f) ? 1.0f : (16000.0f / currentSampleRate);

    for (auto& [profile, settings] : profiles)
    {
        settings.reverbAmount *= scalingFactor;
        settings.delayAmount *= scalingFactor;
        settings.pitchShiftAmount *= scalingFactor;
        settings.flangerRate *= scalingFactor;
        settings.instabilityDepth *= scalingFactor;
        settings.saturationDrive *= scalingFactor;
    }

    //DBG("Adjusted profile parameters for sample rate: " << currentSampleRate);
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

        // --- Session 1 modules ---
        lfo1.prepare(spec.sampleRate);
        envFollower.prepare(spec.sampleRate);
        formantShifter.prepare(spec.sampleRate, (int) spec.numChannels);

        // --- Session 2 modules ---
        multiTap.prepare(spec.sampleRate, (int) spec.maximumBlockSize);
        reverseReverb.prepare(spec.sampleRate, (int) spec.numChannels);
        combFilter.prepare(spec.sampleRate);
        enhancedReverb.prepare(spec.sampleRate, (int) spec.maximumBlockSize);

        // --- Session 3 modules ---
        phaseVocoder.prepare(spec.sampleRate, (int) spec.numChannels);
        granular.prepare(spec.sampleRate, 4);
        bitcrusher.prepare(spec.sampleRate);
        dynamicEq.prepare(spec.sampleRate);
        noiseGen.prepare(spec.sampleRate);
        transientShaper.prepare(spec.sampleRate);

        // Additional modules as needed
    }
    catch (const std::exception& e)
    {
        // Log exceptions if debugging
    }
}


void AncientVoicesAudioProcessor::resetDSPModules()
{
    //DBG("resetDSPModules called.");
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
void AncientVoicesAudioProcessor::setLicenseStatus(bool status) {
    isLicensed = status;
}
void AncientVoicesAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // License state tracked in isLicensed / isLicenseValid() / setLicenseStatus()
    // but NOT gating audio — plugin passes audio regardless so the host signal
    // chain and waveform display remain functional during evaluation.

    juce::ScopedNoDenormals noDenormals;
    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // === DRY CAPTURE for MIX crossfade ===
    juce::AudioBuffer<float> dryBuffer(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    // Logging start of processBlock
    //DBG("==> 1 Processing block with " << numOutputChannels << " output channels and " << numSamples << " samples.");

    // Capture input buffer for GUI graph before processing
    juce::AudioBuffer<float> inputBufferBeforeProcessing(buffer.getNumChannels(), buffer.getNumSamples());
    inputBufferBeforeProcessing.makeCopyOf(buffer);

    try {
        // Ensure buffer is valid
        if (numSamples <= 0 || numOutputChannels <= 0)
        {
            //DBG("Invalid buffer or channel configuration.");
            return;
        }

        // Ensure no NaNs or infinities in the buffer
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (!std::isfinite(channelData[i]))  // Use std::isfinite instead of juce::isFinite
                    channelData[i] = 0.0f;
            }
        }

        // Clear unused output channels
        for (int i = numInputChannels; i < numOutputChannels; ++i)
        {
            //DBG("2 Clearing output channel " << i << " from sample 0 to " << numSamples);
            buffer.clear(i, 0, numSamples);
        }

        // Create an audio block from the buffer for processing
        juce::dsp::AudioBlock<float> audioBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(audioBlock);

        // Capture input samples before processing
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            lastInputSamples.resize(waveformBufferSize, 0.0f); // Initialize with zeros
            //DBG("3 Capturing input samples...");

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                //DBG("4 Processing input channel: " << channel);
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    lastInputSamples[sample] += buffer.getReadPointer(channel)[sample];
                }
            }

            // Average if multiple channels and normalize
            for (auto& sample : lastInputSamples)
            {
                sample /= buffer.getNumChannels();
            }
            //DBG("5 Finished capturing input samples.");
        }

        // Retrieve parameters
        int newProfileIndex = 0;

        if (auto profileParam = apvts.getRawParameterValue("PROFILE_INDEX"))
            newProfileIndex = profileParam->load();

        // Log the retrieved profile index
        //DBG("6 Retrieved Profile Index: " << juce::String(newProfileIndex));

        // Update member variables if they have changed
        profileIndex = newProfileIndex;
        //DBG("7 Profile Index changed to: " + juce::String(profileIndex));

        if (profileIndex >= 0 && profileIndex < static_cast<int>(SoundProfile::NumProfiles))
        {
            currentProfile = profiles[static_cast<SoundProfile>(profileIndex)];
            //DBG("------------------8 Updated to Current Profile: " + juce::String(profileIndex));
        }
        else
        {
            //DBG("9 Error: Invalid profile index.");
            currentProfile = profiles[SoundProfile::Alpha]; // Default to Alpha
        }

        // Apply the profile-specific effects
        //DBG("11 Applying profile effects...");
        applyProfileEffects(buffer);
        
        // Apply ducking effect (only if user turned it up)
            const float sensitivity = *duckingSensitivity;
            if (sensitivity > 0.001f) {
                duckingGain.setGainLinear(1.0f - sensitivity);
                duckingGain.process(context);
            }

            // Apply glitch effect (only if user turned it up)
            const float intensity = *glitchIntensity;
            if (intensity > 0.001f) {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto* channelData = buffer.getWritePointer(channel);
                for (int i = 0; i < numSamples; ++i)
                {
                    if (std::rand() % 100 < static_cast<int>(intensity * 100.0f))
                    {
                        channelData[i] *= (std::rand() % 2 == 0 ? -1.0f : 1.0f); // Flip phase
                        if (std::rand() % 2 == 0)
                        {
                            channelData[i] = 0.0f; // Randomly mute
                        }
                    }
                }
            }
            }
        // Apply limiter to prevent clipping
        //DBG("12 Applying limiter to the buffer...");
        applyLimiter(buffer);

        // === USER OUTPUT GAIN ===
        if (userOutput != nullptr) {
            buffer.applyGain(userOutput->load());
        }

        // === MIX CROSSFADE: mix=1.0 => pure wet (no dry bleed); mix=0.0 => pure dry ===
        if (mixAmount != nullptr) {
            const float mix = mixAmount->load();
            const float wetGain = mix;
            const float dryGain = 1.0f - mix;
            for (int ch = 0; ch < numChannels; ++ch) {
                auto* wetData = buffer.getWritePointer(ch);
                const auto* dryData = dryBuffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    wetData[i] = wetGain * wetData[i] + dryGain * dryData[i];
            }
        }
        
        // Capture output buffer for GUI graph after processing
        juce::AudioBuffer<float> outputBufferAfterProcessing(buffer.getNumChannels(), buffer.getNumSamples());
        outputBufferAfterProcessing.makeCopyOf(buffer);

        // Capture output samples after processing
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            lastOutputSamples.resize(waveformBufferSize, 0.0f); // Initialize with zeros
            //DBG("14 Capturing output samples...");

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                //DBG("15 Processing output channel: " << channel);
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    lastOutputSamples[sample] += buffer.getReadPointer(channel)[sample];
                }
            }

            // Average if multiple channels and normalize
            for (auto& sample : lastOutputSamples)
            {
                sample /= buffer.getNumChannels();
            }
            //DBG("16 Finished capturing output samples.");
        }
        // Apply ducking effect

            /*duckingGain.setGainLinear(1.0f - sensitivity);
            duckingGain.process(context);

            // Apply glitch effect

            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto* channelData = buffer.getWritePointer(channel);
                for (int i = 0; i < numSamples; ++i)
                {
                    if (std::rand() % 100 < static_cast<int>(intensity * 100.0f)) // Random glitch occurrence
                    {
                        channelData[i] *= (std::rand() % 2 == 0 ? -1.0f : 1.0f); // Flip phase
                        if (std::rand() % 4 == 0) // Occasionally mute
                        {
                            channelData[i] = 0.0f;
                        }
                    }
                }
            }*/
    }
    catch (const std::exception& e)
    {
        //DBG("ProcessBlock exception: " << e.what());
    }

    // Finish processBlock
    //DBG("==> 17 Finished processBlock with " << numSamples << " samples.");
}
//==============================================================================
void AncientVoicesAudioProcessor::initializeProfiles()
{
    // Profile Alpha: Power Vocal (Enhanced clarity, moderate effects)
    profiles[SoundProfile::Alpha] = {
        0.0f,        // pitchShiftAmount: Neutral pitch for clarity
        0.4f,        // harmonizerAmount: Moderate harmonies for vocal presence
        0.2f,        // reverbAmount: Light reverb to add space
        0.1f,        // distortionAmount: Minimal distortion to prevent muddiness
        0.1f,        // delayAmount: Subtle delay for depth
        0.2f,        // chorusDepth: Moderate chorus for warmth
        0.1f,        // flangerRate: Subtle movement for liveliness
        5000.0f,     // filterCutoff: Midrange cutoff for clarity
        2.5f,        // compressionRatio: Moderate compression for control
        0.05f,       // instabilityDepth: Minor modulation for energy
        0.05f,       // vocalDoublerDetuneAmount: Minor doubling for depth
        10.0f,       // vocalDoublerDelayMs: Quick doubling effect
        0.0f,        // spacedOutDelayTimeMs: No spaced effect
        0.0f,        // spacedOutPanAmount: No panning
        0.0f,        // glitchAmount: No glitch
        0.9f,        // outputGain: Slight reduction for balance
        0,           // stopAndGoInterval: Disabled for stability
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Beta: Dreamscape (Simplified for ambient focus)
    profiles[SoundProfile::Beta] = {
        6.0f,        // pitchShiftAmount: Gentle shift for ethereal tone
        0.3f,        // harmonizerAmount: Soft harmonies for dreaminess
        0.4f,        // reverbAmount: Light reverb to avoid overwhelming depth
        0.05f,       // distortionAmount: Low distortion for a clean tone
        0.1f,        // delayAmount: Subtle spaciousness
        0.2f,        // chorusDepth: Light chorus to prevent muddiness
        0.1f,        // flangerRate: Mild movement for ambient feel
        6000.0f,     // filterCutoff: Higher cutoff for clarity
        2.0f,        // compressionRatio: Minimal dynamics control
        0.0f,        // instabilityDepth: Disabled for stability
        0.1f,        // vocalDoublerDetuneAmount: Adds subtle width
        15.0f,       // vocalDoublerDelayMs: Slight doubling delay
        30.0f,       // spacedOutDelayTimeMs: Adds spaciousness
        0.2f,        // spacedOutPanAmount: Gentle panning
        0.0f,        // glitchAmount: None for stability
        0.85f,       // outputGain: Slightly reduced for balance
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Gamma: Glitch Riser (Stripped down for cleaner rise)
    profiles[SoundProfile::Gamma] = {
        4.0f,        // pitchShiftAmount: Rising pitch for buildup
        0.15f,       // harmonizerAmount: Light harmonies for depth
        0.1f,        // reverbAmount: Minimal reverb for presence
        0.15f,       // distortionAmount: Light distortion for riser effect
        0.2f,        // delayAmount: Subtle rhythmic delay
        0.1f,        // chorusDepth: Mild richness
        0.15f,       // flangerRate: Light modulation for texture
        4000.0f,     // filterCutoff: Mid cutoff to retain clarity
        2.0f,        // compressionRatio: Reduced compression for openness
        0.0f,        // instabilityDepth: No instability for stability
        0.0f,        // vocalDoublerDetuneAmount: Minimal width
        5.0f,        // vocalDoublerDelayMs: Short doubling delay
        0.0f,        // spacedOutDelayTimeMs: No spaced effect
        0.0f,        // spacedOutPanAmount: No panning
        0.0f,        // glitchAmount: None
        0.85f,       // outputGain: Full gain
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Delta: Deep Bass Vox (Focused on bass presence)
    profiles[SoundProfile::Delta] = {
        -12.0f,      // pitchShiftAmount: Low pitch for bass presence
        0.5f,        // harmonizerAmount: Full harmonies for bass depth
        0.3f,        // reverbAmount: Larger reverb for space
        0.2f,        // distortionAmount: Slight grit for depth
        0.1f,        // delayAmount: Mild delay for atmosphere
        0.15f,       // chorusDepth: Adds weight without muddiness
        0.1f,        // flangerRate: Minimal modulation
        3000.0f,     // filterCutoff: Lower cutoff for warmth
        3.0f,        // compressionRatio: Moderate compression
        0.1f,        // instabilityDepth: Subtle modulation
        0.0f,        // vocalDoublerDetuneAmount: Minimal width
        0.0f,        // vocalDoublerDelayMs: No delay
        0.0f,        // spacedOutDelayTimeMs: None
        0.0f,        // spacedOutPanAmount: No panning
        0.0f,        // glitchAmount: None
        0.9f,        // outputGain
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Epsilon: Hardstyle Shout (Simplified for clarity)
    profiles[SoundProfile::Epsilon] = {
        -6.0f,       // pitchShiftAmount: Low pitch for aggression
        0.3f,        // harmonizerAmount: Mild harmonies for presence
        0.1f,        // reverbAmount: Short reverb for clarity
        0.2f,        // distortionAmount: Mild distortion
        0.1f,        // delayAmount: Light delay
        0.2f,        // chorusDepth: Subtle chorus for depth
        0.05f,       // flangerRate: Minimal modulation
        6000.0f,     // filterCutoff: High cutoff for clarity
        2.5f,        // compressionRatio: Moderate compression
        0.1f,        // instabilityDepth: Adds slight texture
        0.0f,        // vocalDoublerDetuneAmount: Minimal width
        0.0f,        // vocalDoublerDelayMs: No delay
        0.0f,        // spacedOutDelayTimeMs: None
        0.0f,        // spacedOutPanAmount: No panning
        0.0f,        // glitchAmount: None
        0.85f,       // outputGain
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Zeta: Vocal Synth (Reduced for clarity)
    profiles[SoundProfile::Zeta] = {
        2.0f,        // pitchShiftAmount: Higher pitch for synthetic tone
        0.3f,        // harmonizerAmount: Light harmonies for robotic effect
        0.1f,        // reverbAmount: Minimal reverb
        0.1f,        // distortionAmount: Light grit for texture
        0.1f,        // delayAmount: Light spaciousness
        0.2f,        // chorusDepth: Synth width
        0.1f,        // flangerRate: Minimal movement
        6000.0f,     // filterCutoff: High cutoff for clarity
        2.5f,        // compressionRatio: Light compression
        0.1f,        // instabilityDepth: Minimal for texture
        0.1f,        // vocalDoublerDetuneAmount: Slight width
        10.0f,       // vocalDoublerDelayMs: Light delay for width
        10.0f,       // spacedOutDelayTimeMs: Mild effect
        0.1f,        // spacedOutPanAmount: Light panning
        0.0f,        // glitchAmount: None
        1.0f,        // outputGain
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Eta: EDM Riser (Focused on pitch and reverb)
    profiles[SoundProfile::Eta] = {
        4.0f,        // pitchShiftAmount: Rising pitch for riser
        0.2f,        // harmonizerAmount: Light harmony
        0.1f,        // reverbAmount: Minimal for buildup
        0.05f,       // distortionAmount: Very light distortion
        0.2f,        // delayAmount: Light delay for tension
        0.1f,        // chorusDepth: Mild thickness
        0.1f,        // flangerRate: Light modulation for texture
        7000.0f,     // filterCutoff: High for clarity
        2.0f,        // compressionRatio
        0.1f,        // instabilityDepth
        0.05f,       // vocalDoublerDetuneAmount: Subtle width
        5.0f,        // vocalDoublerDelayMs
        0.0f,        // spacedOutDelayTimeMs
        0.0f,        // spacedOutPanAmount
        0.0f,        // glitchAmount
        1.0f,        // outputGain
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Theta: Echo Vox (Focused on delay and reverb for echo)
    profiles[SoundProfile::Theta] = {
        0.0f,        // pitchShiftAmount
        0.2f,        // harmonizerAmount
        0.3f,        // reverbAmount
        0.05f,       // distortionAmount
        0.3f,        // delayAmount
        0.15f,       // chorusDepth
        0.05f,       // flangerRate
        6000.0f,     // filterCutoff
        2.5f,        // compressionRatio
        0.05f,       // instabilityDepth
        0.1f,        // vocalDoublerDetuneAmount
        10.0f,       // vocalDoublerDelayMs
        30.0f,       // spacedOutDelayTimeMs
        0.2f,        // spacedOutPanAmount
        0.0f,        // glitchAmount
        0.9f,        // outputGain
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Iota: Trance Vox (Minimal processing for trance clarity)
    profiles[SoundProfile::Iota] = {
        0.0f,        // pitchShiftAmount
        0.2f,        // harmonizerAmount
        0.2f,        // reverbAmount
        0.05f,       // distortionAmount
        0.1f,        // delayAmount
        0.2f,        // chorusDepth
        0.05f,       // flangerRate
        7000.0f,     // filterCutoff
        2.0f,        // compressionRatio
        0.05f,       // instabilityDepth
        0.1f,        // vocalDoublerDetuneAmount
        5.0f,        // vocalDoublerDelayMs
        0.0f,        // spacedOutDelayTimeMs
        0.0f,        // spacedOutPanAmount
        0.0f,        // glitchAmount
        1.0f,        // outputGain
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        0,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // Profile Kappa: Breakdown FX (simplified)
    profiles[SoundProfile::Kappa] = {
        0.0f,        // pitchShiftAmount
        0.3f,        // harmonizerAmount
        0.1f,        // reverbAmount
        0.15f,       // distortionAmount
        0.2f,        // delayAmount
        0.1f,        // chorusDepth
        0.1f,        // flangerRate
        6000.0f,     // filterCutoff
        3.0f,        // compressionRatio
        0.05f,       // instabilityDepth
        0.1f,        // vocalDoublerDetuneAmount
        10.0f,       // vocalDoublerDelayMs
        20.0f,       // spacedOutDelayTimeMs
        0.1f,        // spacedOutPanAmount
        0.0f,        // glitchAmount
        0.95f,       // outputGain
        0,           // stopAndGoInterval
        0.0f,        // stopAndGoSilenceRatio
        2,           // stutterRepeatLength
        0,           // reverseInterval
        0.0f,        // saturationDrive
        0.0f,        // saturationMix
        0.0f         // saturationTone
    };

    // ============================================================
    // Ancient Voices — 50 presets (AV_00..AV_49), SESSION 1
    //
    // Each preset is individually configured from the authoritative
    // spec document. Using session-1 modules where possible:
    //   - FormantShifter (formantShiftSemitones)
    //   - LFO            (lfo1* fields)
    //   - EnvelopeFollower (envFollow* fields)
    //
    // Spec elements NOT yet available (granular, phase vocoder,
    // reverse reverb, dynamic EQ, bitcrusher, multi-tap delay)
    // are approximated using existing DSP; faithful implementations
    // come in session 2+. Comments mark each preset's deferred bits.
    //
    // Helper lambda to reduce boilerplate.
    // ============================================================
    auto makeAV = [&](int avIdx, ProfileSettings s) {
        const auto& preset = AncientVoicesPresets::getPreset(avIdx);
        s.filterCutoff = AncientVoicesPresets::getFilterCutoffForEra(preset.era);
        // Defaults for fields not set by the preset (safe zero/neutral)
        if (s.compressionRatio == 0.0f) s.compressionRatio = 2.0f;
        if (s.outputGain       == 0.0f) s.outputGain       = 0.9f;
        const SoundProfile key = static_cast<SoundProfile>(
            static_cast<int>(SoundProfile::AV_00) + avIdx);
        profiles[key] = s;
    };

    // =====================================================
    //   CHANT (AV_00 .. AV_06)
    // =====================================================

    // AV_00 Eridu Invocation (Sumerian) — first-city invocation, grounded voice
    //   Signature: TAPE SATURATION on low-mids + slow pitch wobble (mud-brick walls)
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -2.0f;                         // Sumerian earthy downward bias
        s.formantShiftSemitones  = -2.0f;                         // deeper than original
        s.reverbAmount           = 0.40f;                         // short non-reflective mud
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 15.0f;
        s.reverbHfDampingDb      = -8.0f;                         // mud-brick absorbs highs
        s.chorusDepth            = 0.08f;
        s.harmonizerAmount       = 0.12f;
        s.distortionAmount       = 0.32f;                         // tape-saturation color (deeper)
        s.lfo1RateHz             = 0.7f;
        s.lfo1Depth              = 0.18f;                         // wider mud-brick wobble
        s.lfo1Shape              = AV::LFO::Random;
        s.lfo1Dest               = AV::ModDest::Pitch;
        s.outputGain             = 0.95f;
        makeAV(0, s);
    }

    // AV_01 Gala Priest Lament (Sumerian) — mournful castrated-priest singer
    //   Signature: TIGHT CHORUS DOUBLING + slow grief vibrato + 2kHz cut
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -1.0f;                         // slightly low Sumerian base
        s.formantShiftSemitones  = 2.5f;                          // gala priests: raised formants
        s.reverbAmount           = 0.50f;                         // intimate ritual chamber
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 25.0f;
        s.reverbHfDampingDb      = -3.0f;
        s.chorusDepth            = 0.55f;                         // deeper tight doubling signature
        s.harmonizerAmount       = 0.30f;
        s.lfo1RateHz             = 4.5f;
        s.lfo1Depth              = 0.38f;                         // deeper grief vibrato
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Pitch;
        s.dynEqMix               = 0.6f;
        s.dynEqFreqHz            = 2000.0f;
        s.dynEqQ                 = 1.5f;
        s.dynEqTargetGainDb      = -4.0f;
        s.dynEqThreshold         = 0.12f;
        s.outputGain             = 0.92f;
        makeAV(1, s);
    }

    // AV_02 Heliopolis Sunrise (Egyptian) — sun temple at dawn, opening ray of light
    //   Signature: RISING PITCH + bright airy ping-pong + granular shimmer on tail
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 3.0f;                          // elevated Egyptian brightness
        s.formantShiftSemitones  = 1.5f;                          // brighter formants
        s.reverbAmount           = 0.95f;                         // 4.0s huge hall
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 60.0f;
        s.reverbHfDampingDb      = 0.0f;                          // bright, no HF cut
        s.chorusDepth            = 0.10f;
        s.multiTapMix            = 0.70f;                         // dominant ping-pong
        s.multiTapTimesMs[0] = 300.0f; s.multiTapGains[0] = 0.7f;  s.multiTapPans[0] = -1.0f;
        s.multiTapTimesMs[1] = 450.0f; s.multiTapGains[1] = 0.65f; s.multiTapPans[1] = +1.0f;
        s.granularMix            = 0.30f;                         // stronger tail shimmer
        s.granularGrainSizeMs    = 15.0f;
        s.granularDensity        = 15.0f;
        s.granularPitchBaseSemi  = 12.0f;                         // octave-up sparkle
        s.granularPitchSpreadSemi = 0.3f;
        s.granularPositionJitter = 0.4f;
        s.outputGain             = 0.9f;
        makeAV(2, s);
    }

    // AV_03 Inanna's Descent Chant (Sumerian) — goddess descending to underworld
    //   Signature: OCTAVE-DOWN + env-ducked reverb (oppressive descent pulse)
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -12.0f;                        // full octave descent
        s.formantShiftSemitones  = -1.5f;                         // darker than sumerian base
        s.reverbAmount           = 0.75f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 30.0f;
        s.reverbHfDampingDb      = -6.0f;                         // pulling into dark
        s.harmonizerAmount       = 0.50f;
        s.distortionAmount       = 0.28f;                         // oppressive undercurrent
        s.envFollowAttackMs      = 8.0f;
        s.envFollowReleaseMs     = 180.0f;
        s.envFollowDepth         = -0.75f;                        // dramatic duck = signature
        s.envFollowDest          = AV::ModDest::ReverbWet;
        s.outputGain             = 0.85f;
        makeAV(3, s);
    }

    // AV_04 Ptah Hymn (Egyptian) — hymn to god of craftsmen, stone workshop
    //   Signature: RHYTHMIC GRANULAR CHISEL + 5kHz dyn boost + metallic plate
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 1.5f;                          // Egyptian elevated
        s.formantShiftSemitones  = 1.0f;
        s.reverbAmount           = 0.70f;                         // dense plate
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 8.0f;
        s.reverbHfDampingDb      = -2.0f;                         // plate stays bright
        s.flangerRate            = 0.2f;                          // metallic sheen
        s.chorusDepth            = 0.12f;
        s.granularMix            = 0.55f;                         // chiseling rhythm is signature
        s.granularGrainSizeMs    = 120.0f;
        s.granularDensity        = 5.0f;
        s.granularPositionJitter = 0.1f;
        s.dynEqMix               = 0.70f;
        s.dynEqFreqHz            = 5000.0f;
        s.dynEqQ                 = 2.2f;
        s.dynEqTargetGainDb      = 6.5f;                          // stronger stone sparkle
        s.dynEqThreshold         = 0.15f;
        s.outputGain             = 0.9f;
        makeAV(4, s);
    }

    // AV_05 Enlil Decree (Sumerian) — god of wind/storm pronouncing judgment
    //   Signature: AGGRESSIVE 1.5kHz PRESENCE PUSH + fast vibrato + mid-range distortion
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -3.0f;                         // deeper authoritative Sumerian
        s.formantShiftSemitones  = -1.0f;                         // commanding lower formant
        s.reverbAmount           = 0.55f;
        s.distortionAmount       = 0.32f;                         // pushed mid aggression
        s.lfo1RateHz             = 5.5f;                          // fast commanding vibrato
        s.lfo1Depth              = 0.12f;
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Pitch;
        s.dynEqMix               = 0.85f;                         // dominant authority signature
        s.dynEqFreqHz            = 1500.0f;
        s.dynEqQ                 = 2.5f;
        s.dynEqTargetGainDb      = 8.0f;                          // commanding presence
        s.dynEqThreshold         = 0.08f;                         // always engaged
        s.outputGain             = 0.9f;
        makeAV(5, s);
    }

    // AV_06 Memphis Morning Rite (Egyptian) — dawn ceremony at old capital
    //   Signature: RHYTHMIC MULTI-TAP DUAL ECHO + slow opening filter sweep
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 2.0f;                          // Egyptian elevated morning
        s.formantShiftSemitones  = 1.0f;
        s.reverbAmount           = 0.80f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 45.0f;
        s.reverbHfDampingDb      = -1.0f;
        s.multiTapMix            = 0.70f;                         // dominant rhythmic identity
        s.multiTapTimesMs[0] = 500.0f; s.multiTapGains[0] = 0.65f; s.multiTapPans[0] = -0.6f;
        s.multiTapTimesMs[1] = 375.0f; s.multiTapGains[1] = 0.55f; s.multiTapPans[1] = +0.6f;
        s.multiTapTimesMs[2] = 750.0f; s.multiTapGains[2] = 0.35f; s.multiTapPans[2] = -0.2f;
        s.chorusDepth            = 0.10f;
        s.lfo1RateHz             = 0.08f;                          // very slow sunrise sweep
        s.lfo1Depth              = 0.25f;                          // wider than before
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::FilterCutoff;
        s.outputGain             = 0.9f;
        makeAV(6, s);
    }

    // =====================================================
    //   DRONE (AV_07 .. AV_13)
    // =====================================================

    // AV_07 Ziggurat Resonance (Sumerian) — continuous blurred pad
    //   [session 2: enhanced reverb with long pre-delay + heavy HF damping]
    //   [session 3: phase vocoder smear for true continuous-pad blurring]
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -7.0f;
        s.formantShiftSemitones  = -1.0f;
        s.reverbAmount           = 0.98f;                         // 3.0s
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 80.0f;
        s.reverbHfDampingDb      = -8.0f;                         // floor-heavy
        s.chorusDepth            = 0.20f;
        s.pvSmearAmount          = 0.92f;                         // near-total smear = pad identity
        s.pvMix                  = 0.75f;                         // dominant
        s.lfo1RateHz             = 0.08f;
        s.lfo1Depth              = 0.15f;
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::DelayTime;
        s.delayAmount            = 0.30f;
        s.outputGain             = 0.85f;
        makeAV(7, s);
    }

    // AV_08 Nile Reed Drone (Egyptian) — papyrus reed instrument continuous buzz
    //   Signature: 5ms GRANULAR BUZZ + narrow 800Hz resonant focus
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -3.0f;                         // reed-pitched low
        s.formantShiftSemitones  = -2.5f;                         // nasal reed quality
        s.reverbAmount           = 0.65f;                         // dry-ish reed focus
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 20.0f;
        s.reverbHfDampingDb      = -4.0f;
        s.granularMix            = 0.85f;                         // near-total reed buzz
        s.granularGrainSizeMs    = 5.0f;
        s.granularDensity        = 50.0f;
        s.granularPitchSpreadSemi = 0.2f;
        s.granularPositionJitter = 0.05f;
        s.dynEqMix               = 0.80f;                         // dominant reed-body resonance
        s.dynEqFreqHz            = 800.0f;
        s.dynEqQ                 = 4.5f;                          // narrower = more vocal-tract
        s.dynEqTargetGainDb      = 7.0f;
        s.dynEqThreshold         = 0.04f;
        s.outputGain             = 0.85f;
        makeAV(8, s);
    }

    // AV_09 Abzu Deep Waters (Sumerian) — primordial underground freshwater abyss
    //   Signature: HEAVY UNDERWATER HF CUT + very slow chorus sway + env-swelling wet
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -9.0f;                         // deeper than before
        s.formantShiftSemitones  = -1.0f;
        s.reverbAmount           = 0.99f;                         // 5s wash
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 40.0f;
        s.reverbHfDampingDb      = -24.0f;                        // max underwater kill
        s.chorusDepth            = 0.75f;                         // dominant water sway
        s.envFollowAttackMs      = 60.0f;
        s.envFollowReleaseMs     = 700.0f;
        s.envFollowDepth         = 0.55f;                         // pronounced swell
        s.envFollowDest          = AV::ModDest::ReverbWet;
        s.outputGain             = 0.85f;
        makeAV(9, s);
    }

    // AV_10 Duat Shadow Drone (Egyptian) — underworld realm of Osiris at night
    //   Signature: DOMINANT REVERSE REVERB + slow formant morph (disembodied shadow)
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -6.0f;                         // deeper than before
        s.formantShiftSemitones  = -1.5f;
        s.reverbAmount           = 0.45f;                         // reduced — reverse dominates
        s.reverseReverbMix       = 0.88f;                         // near-total reverse identity
        s.reverseReverbWindowSec = 2.5f;
        s.distortionAmount       = 0.18f;
        s.lfo1RateHz             = 0.04f;                         // 25s slow morph
        s.lfo1Depth              = 2.5f;                          // deeper formant swing
        s.lfo1Shape              = AV::LFO::Triangle;
        s.lfo1Dest               = AV::ModDest::Formant;
        s.outputGain             = 0.82f;
        makeAV(10, s);
    }

    // AV_11 Eridu Mud Plain (Sumerian) — barren mud plains around Sumer's first city
    //   Signature: DRY + EMPTY FIFTH/FOURTH HARMONY STACK + windy noise bed
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -2.0f;                         // grounded
        s.formantShiftSemitones  = -0.5f;
        s.reverbAmount           = 0.15f;                         // nearly dry — open plain
        s.harmonizerAmount       = 0.75f;                         // dominant empty fifth stack
        s.chorusDepth            = 0.05f;
        s.noiseLevel             = 0.20f;                         // pronounced wind bed
        s.noiseColor             = AV::NoiseGenerator::Pink;
        s.noiseHighPassHz        = 250.0f;
        s.noiseGated             = true;
        s.outputGain             = 0.95f;
        makeAV(11, s);
    }

    // AV_12 Karnak Pillar Hum (Egyptian) — colossal temple column forest
    //   Signature: MAX PRE-DELAY (pillar distance) + 120Hz sub resonance + multi-tap pillar echoes
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -11.0f;                        // deep temple bass
        s.formantShiftSemitones  = -0.5f;
        s.reverbAmount           = 0.99f;                         // 6s
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 200.0f;                        // maximum pillar spacing
        s.reverbHfDampingDb      = -7.0f;
        s.multiTapMix            = 0.55f;
        s.multiTapTimesMs[0] = 280.0f; s.multiTapGains[0] = 0.6f; s.multiTapPans[0] = -0.8f;
        s.multiTapTimesMs[1] = 420.0f; s.multiTapGains[1] = 0.5f; s.multiTapPans[1] = +0.8f;
        s.dynEqMix               = 0.85f;                         // dominant sub resonance
        s.dynEqFreqHz            = 120.0f;
        s.dynEqQ                 = 4.0f;                          // tighter sub peak
        s.dynEqTargetGainDb      = 9.0f;                          // massive low rumble
        s.dynEqThreshold         = 0.03f;                         // always on
        s.chorusDepth            = 0.15f;
        s.outputGain             = 0.82f;
        makeAV(12, s);
    }

    // AV_13 Marduk's Word (Akkadian/Babylonian) — supreme god's utterance
    //   Signature: UNNATURAL +2 FORMANT (inhuman) + granular thunder + PV freeze
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -10.0f;                        // deep commanding
        s.formantShiftSemitones  = 4.0f;                          // even more extreme unnatural
        s.reverbAmount           = 0.85f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 70.0f;
        s.reverbHfDampingDb      = -3.0f;
        s.distortionAmount       = 0.22f;
        s.granularMix            = 0.50f;                         // stronger thunder rolls
        s.granularGrainSizeMs    = 80.0f;
        s.granularDensity        = 7.0f;
        s.granularPitchSpreadSemi = 0.0f;
        s.granularReverseProb    = 0.35f;
        s.granularPositionJitter = 0.05f;
        s.pvFreezeAmount         = 0.65f;                         // stronger held utterance
        s.pvMix                  = 0.40f;
        s.outputGain             = 0.82f;
        makeAV(13, s);
    }

    // =====================================================
    //   CHOIR (AV_14 .. AV_20)
    // =====================================================

    // AV_14 Temple of Amun Choir (Egyptian) — massive Karnak choir of priest-singers
    //   Signature: 4-VOICE MULTI-TAP with micro-pitch detune + bright hall
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 2.0f;                          // Egyptian elevated
        s.formantShiftSemitones  = 1.5f;
        s.reverbAmount           = 0.92f;                         // 4s bright
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 30.0f;
        s.reverbHfDampingDb      = 0.0f;
        s.harmonizerAmount       = 0.45f;
        s.chorusDepth            = 0.40f;                         // wide voice blur
        s.multiTapMix            = 0.85f;                         // dominant voice-stack
        s.multiTapTimesMs[0] = 17.0f; s.multiTapGains[0] = 0.7f;  s.multiTapPans[0] = -1.0f;
        s.multiTapTimesMs[1] = 23.0f; s.multiTapGains[1] = 0.7f;  s.multiTapPans[1] = +1.0f;
        s.multiTapTimesMs[2] = 31.0f; s.multiTapGains[2] = 0.6f;  s.multiTapPans[2] = -0.5f;
        s.multiTapTimesMs[3] = 41.0f; s.multiTapGains[3] = 0.6f;  s.multiTapPans[3] = +0.5f;
        s.vocalDoublerDetuneAmount = 0.18f;                       // stronger micro-detune
        s.vocalDoublerDelayMs    = 18.0f;
        s.outputGain             = 0.88f;
        makeAV(14, s);
    }

    // AV_15 Ur Nammu Assembly (Sumerian) — king's ceremonial court gathering
    //   Signature: DOMINANT HARMONIZER FIFTH+OCTAVE + random per-voice timing jitter
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -1.5f;                         // Sumerian grounded
        s.formantShiftSemitones  = 0.5f;
        s.reverbAmount           = 0.65f;                         // assembly courtyard
        s.harmonizerAmount       = 0.88f;                         // max stacked assembly voices
        s.chorusDepth            = 0.25f;
        s.multiTapMix            = 0.45f;
        s.multiTapTimesMs[0] = 14.0f; s.multiTapGains[0] = 0.55f; s.multiTapPans[0] = -0.6f;
        s.multiTapTimesMs[1] = 27.0f; s.multiTapGains[1] = 0.5f;  s.multiTapPans[1] = +0.6f;
        s.lfo1RateHz             = 1.8f;
        s.lfo1Depth              = 0.35f;                         // more unsynced chanting
        s.lfo1Shape              = AV::LFO::Random;
        s.lfo1Dest               = AV::ModDest::DelayTime;
        s.outputGain             = 0.9f;
        makeAV(15, s);
    }

    // AV_16 Hathor's Daughters (Egyptian) — temple priestesses, airy feminine choir
    //   Signature: BIG FEMININE FORMANT (+3) + sine vibrato + bright air (no HF cut)
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 4.0f;                          // airy upward
        s.formantShiftSemitones  = 3.0f;                          // strong feminine
        s.reverbAmount           = 0.90f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 15.0f;
        s.reverbHfDampingDb      = 0.0f;                          // sparkle preserved
        s.chorusDepth            = 0.30f;
        s.lfo1RateHz             = 5.0f;                          // slightly faster
        s.lfo1Depth              = 0.45f;                         // more pronounced vibrato
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Pitch;
        s.granularMix            = 0.15f;                         // feedback shimmer
        s.granularGrainSizeMs    = 20.0f;
        s.granularDensity        = 10.0f;
        s.granularPitchBaseSemi  = 12.0f;
        s.granularPitchSpreadSemi = 0.4f;
        s.outputGain             = 0.88f;
        makeAV(16, s);
    }

    // AV_17 Gala Ensemble of Uruk (Sumerian) — professional cultic-song singers
    //   Signature: 3-VOICE WIDE STEREO with micro-time staggering + strong harmonizer
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -2.5f;                         // Sumerian grounded
        s.formantShiftSemitones  = 2.0f;                          // gala priests' raised formant
        s.reverbAmount           = 0.60f;
        s.harmonizerAmount       = 0.85f;                         // dominant stacking
        s.multiTapMix            = 0.75f;                         // stronger voice spread
        s.multiTapTimesMs[0] = 12.0f; s.multiTapGains[0] = 0.65f; s.multiTapPans[0] = -1.0f;
        s.multiTapTimesMs[1] = 22.0f; s.multiTapGains[1] = 0.65f; s.multiTapPans[1] = +1.0f;
        s.multiTapTimesMs[2] = 35.0f; s.multiTapGains[2] = 0.5f;  s.multiTapPans[2] = 0.0f;
        s.chorusDepth            = 0.20f;
        s.outputGain             = 0.88f;
        makeAV(17, s);
    }

    // AV_18 Pharaoh's Court (Egyptian) — throne room with attendants
    //   Signature: TIGHT MULTI-TAP BG VOICES + throne-room 3kHz presence boost + tape glue
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 0.5f;                          // near-neutral central
        s.formantShiftSemitones  = 0.5f;
        s.reverbAmount           = 0.78f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 8.0f;                          // tight
        s.reverbHfDampingDb      = -2.0f;
        s.multiTapMix            = 0.60f;                         // stronger BG chorus
        s.multiTapTimesMs[0] = 20.0f; s.multiTapGains[0] = 0.6f; s.multiTapPans[0] = -1.0f;
        s.multiTapTimesMs[1] = 30.0f; s.multiTapGains[1] = 0.6f; s.multiTapPans[1] = +1.0f;
        s.dynEqMix               = 0.70f;                         // stronger throne presence
        s.dynEqFreqHz            = 3000.0f;
        s.dynEqQ                 = 1.6f;
        s.dynEqTargetGainDb      = 4.5f;
        s.dynEqThreshold         = 0.12f;
        s.distortionAmount       = 0.13f;                         // tape glue
        s.chorusDepth            = 0.10f;
        s.outputGain             = 0.88f;
        makeAV(18, s);
    }

    // AV_19 Nippur Gathering (Sumerian) — holy city assembly, harmonic smear
    //   Signature: PV SMEAR DOMINANT (harmonic blur) + high-freq crowd-shuffle noise
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -2.0f;                         // Sumerian grounded
        s.formantShiftSemitones  = 1.0f;
        s.reverbAmount           = 0.80f;
        s.pvSmearAmount          = 0.92f;                         // near-total harmonic blur
        s.pvMix                  = 0.80f;                         // dominant identity
        s.noiseLevel             = 0.10f;                         // stronger crowd shuffle
        s.noiseColor             = AV::NoiseGenerator::White;
        s.noiseHighPassHz        = 2500.0f;                       // tight shuffle band
        s.noiseGated             = true;
        s.chorusDepth            = 0.12f;
        s.spacedOutPanAmount     = 0.75f;
        s.spacedOutDelayTimeMs   = 45.0f;
        s.outputGain             = 0.88f;
        makeAV(19, s);
    }

    // AV_20 Lagash Processional (Sumerian) — ceremonial walking procession
    //   Signature: DOMINANT DOPPLER PITCH LFO (passing-by sweep) + tight street echoes
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -1.5f;                         // Sumerian grounded
        s.formantShiftSemitones  = -0.5f;
        s.reverbAmount           = 0.45f;                         // narrow street
        s.multiTapMix            = 0.35f;
        s.multiTapTimesMs[0] = 55.0f; s.multiTapGains[0] = 0.5f; s.multiTapPans[0] = -0.8f;
        s.multiTapTimesMs[1] = 95.0f; s.multiTapGains[1] = 0.4f; s.multiTapPans[1] = +0.8f;
        s.lfo1RateHz             = 0.08f;                         // 12s Doppler cycle
        s.lfo1Depth              = 1.8f;                          // deeper passing-by sweep
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Pitch;
        s.compressionRatio       = 3.5f;                          // focus on mids
        s.outputGain             = 0.9f;
        makeAV(20, s);
    }

    // =====================================================
    //   RITUAL (AV_21 .. AV_28)
    // =====================================================

    // AV_21 Opening of the Mouth (Egyptian) — funerary ritual reanimating the deceased
    //   Signature: GRANULAR LOOP-REPEATER (dead-voice echo) + tomb reflections
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 1.0f;                          // Egyptian elevated
        s.formantShiftSemitones  = 0.5f;
        s.reverbAmount           = 0.55f;                         // tomb interior
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 12.0f;
        s.reverbHfDampingDb      = -4.0f;
        s.delayAmount            = 0.25f;                         // incantation echo
        s.granularMix            = 0.75f;                         // dominant dead-voice echo
        s.granularGrainSizeMs    = 120.0f;
        s.granularDensity        = 6.0f;
        s.granularPositionJitter = 0.3f;
        s.granularReverseProb    = 0.25f;                         // more eerie reversal
        s.harmonizerAmount       = 0.2f;
        s.outputGain             = 0.9f;
        makeAV(21, s);
    }

    // AV_22 Sacred Marriage Rite (Sumerian) — Inanna-Dumuzid hieros gamos ceremony
    //   Signature: HYPNOTIC DEEP CHORUS SWIRL + octave+fifth harmonizer stack
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -1.0f;                         // Sumerian grounded
        s.formantShiftSemitones  = 1.5f;                          // raised sacred quality
        s.reverbAmount           = 0.82f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 25.0f;
        s.reverbHfDampingDb      = -2.0f;
        s.harmonizerAmount       = 0.88f;                         // max sacred octave+fifth
        s.chorusDepth            = 0.95f;                         // nearly max hypnotic
        s.flangerRate            = 0.18f;
        s.lfo1RateHz             = 0.2f;
        s.lfo1Depth              = 0.25f;                         // deeper stereo drift
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Pan;              // hypnotic stereo drift
        s.outputGain             = 0.88f;
        makeAV(22, s);
    }

    // AV_23 Akitu Festival (Akkadian/Babylonian) — New Year festival of renewal
    //   Signature: 4-TAP CHAOTIC STEREO + random pan LFO + 4.2kHz era boost
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 2.0f;                          // Akkadian elevated festival voice
        s.formantShiftSemitones  = 0.5f;
        s.reverbAmount           = 0.75f;
        s.distortionAmount       = 0.38f;                         // festival drive
        s.multiTapMix            = 0.78f;                         // dominant chaos stereo
        s.multiTapTimesMs[0] = 120.0f; s.multiTapGains[0] = 0.65f; s.multiTapPans[0] = -1.0f;
        s.multiTapTimesMs[1] = 185.0f; s.multiTapGains[1] = 0.6f;  s.multiTapPans[1] = +1.0f;
        s.multiTapTimesMs[2] = 270.0f; s.multiTapGains[2] = 0.55f; s.multiTapPans[2] = -0.5f;
        s.multiTapTimesMs[3] = 340.0f; s.multiTapGains[3] = 0.5f;  s.multiTapPans[3] = +0.5f;
        s.lfo1RateHz             = 3.5f;
        s.lfo1Depth              = 0.4f;                          // more random pan chaos
        s.dynEqMix               = 0.55f;
        s.dynEqFreqHz            = 4200.0f;                       // Akkadian era cutoff
        s.dynEqQ                 = 1.5f;
        s.dynEqTargetGainDb      = 5.0f;
        s.dynEqThreshold         = 0.1f;
        s.lfo1Shape              = AV::LFO::Random;
        s.lfo1Dest               = AV::ModDest::Pan;
        s.outputGain             = 0.82f;
        makeAV(23, s);
    }

    // AV_24 Heb Sed Renewal (Egyptian) — pharaoh's jubilee rejuvenation rite
    //   Signature: DOMINANT 20-SEC SLOW PITCH SWEEP (renewal arc) + env-ducked big tail
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -2.0f;                         // starts low, sweeps up
        s.formantShiftSemitones  = 1.5f;                          // Egyptian bright
        s.reverbAmount           = 0.95f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 55.0f;
        s.reverbHfDampingDb      = -3.0f;
        s.lfo1RateHz             = 0.04f;                         // 25s sweep — slower
        s.lfo1Depth              = 3.0f;                          // ±3 semitones renewal arc
        s.lfo1Shape              = AV::LFO::Triangle;
        s.lfo1Dest               = AV::ModDest::Pitch;
        s.envFollowAttackMs      = 20.0f;
        s.envFollowReleaseMs     = 300.0f;
        s.envFollowDepth         = -0.6f;                         // pronounced tail duck
        s.envFollowDest          = AV::ModDest::ReverbWet;
        s.outputGain             = 0.88f;
        makeAV(24, s);
    }

    // AV_25 Funeral of Osiris (Egyptian) — descent of slain god into underworld
    //   Signature: DEEPEST PITCH (-12 + -3 formant) + extreme dark wash + 12-bit decay
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -12.0f;
        s.formantShiftSemitones  = -3.5f;                         // even deeper formant than spec
        s.reverbAmount           = 0.99f;                         // 6s washed
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 75.0f;
        s.reverbHfDampingDb      = -18.0f;                        // even darker wash
        s.bitcrushMix            = 0.32f;                         // more prominent decay
        s.bitcrushBitDepth       = 10.0f;
        s.bitcrushSampleRateHz   = 16000.0f;
        s.distortionAmount       = 0.12f;
        s.chorusDepth            = 0.15f;
        s.outputGain             = 0.78f;
        makeAV(25, s);
    }

    // AV_26 Anointing the Shedu (Akkadian/Babylonian) — consecrating winged-bull guardian
    //   Signature: METALLIC BRONZE-GATE PLATE + 80ms slapback + 2-4k liquid drive
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -2.5f;                         // Akkadian grounded
        s.formantShiftSemitones  = -1.5f;                         // spec
        s.reverbAmount           = 0.70f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 5.0f;                          // instant plate attack
        s.reverbHfDampingDb      = 1.0f;                          // actually brighter — metallic
        s.delayAmount            = 0.14f;                         // stronger slapback
        s.distortionAmount       = 0.32f;                         // more liquid drive
        s.flangerRate            = 0.4f;                          // more metallic sheen
        s.dynEqMix               = 0.7f;
        s.dynEqFreqHz            = 3200.0f;
        s.dynEqQ                 = 2.2f;
        s.dynEqTargetGainDb      = 5.5f;                          // stronger bronze edge
        s.dynEqThreshold         = 0.1f;
        s.outputGain             = 0.88f;
        makeAV(26, s);
    }

    // AV_27 Libation to Enki (Sumerian) — pouring offering to god of freshwater
    //   Signature: DENSE 2ms GRANULAR (water pouring) + dominant formant sweep LFO
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -1.5f;                         // Sumerian grounded
        s.formantShiftSemitones  = 0.5f;
        s.reverbAmount           = 0.75f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 25.0f;
        s.reverbHfDampingDb      = -3.0f;
        s.chorusDepth            = 0.40f;                         // water modulation
        s.granularMix            = 0.65f;                         // dominant water-pour
        s.granularGrainSizeMs    = 2.0f;
        s.granularDensity        = 50.0f;
        s.granularPitchSpreadSemi = 1.2f;                         // wider water color
        s.granularPositionJitter = 0.25f;
        s.lfo1RateHz             = 0.15f;
        s.lfo1Depth              = 3.0f;                          // deeper formant sweep
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Formant;
        s.outputGain             = 0.88f;
        makeAV(27, s);
    }

    // AV_28 Offering at Edfu (Egyptian) — temple of Horus, pristine stone ritual
    //   Signature: PRISTINE BRIGHT REVERB + strong dyn 5k boost + hard-panned doubling
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 1.5f;                          // Egyptian elevated ceremonial
        s.formantShiftSemitones  = 0.5f;
        s.reverbAmount           = 0.85f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 22.0f;
        s.reverbHfDampingDb      = 1.0f;                          // actually brighter — stone
        s.multiTapMix            = 0.45f;                         // stronger hard-panned doubling
        s.multiTapTimesMs[0] = 13.0f; s.multiTapGains[0] = 0.65f; s.multiTapPans[0] = -1.0f;
        s.multiTapTimesMs[1] = 17.0f; s.multiTapGains[1] = 0.65f; s.multiTapPans[1] = +1.0f;
        s.chorusDepth            = 0.08f;
        s.dynEqMix               = 0.85f;                         // dominant Egyptian cutoff peak
        s.dynEqFreqHz            = 5000.0f;
        s.dynEqQ                 = 1.8f;
        s.dynEqTargetGainDb      = 7.5f;                          // powerful pristine sparkle
        s.dynEqThreshold         = 0.08f;
        s.outputGain             = 0.92f;
        makeAV(28, s);
    }

    // =====================================================
    //   BREATH (AV_29 .. AV_35)
    // =====================================================

    // AV_29 Reed of Ra (Egyptian) — sun god's breath through marsh reed
    //   Signature: FAST FLANGER SWEEP on breath band + HP-isolated high air
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 2.0f;                          // reed-pitched Egyptian
        s.formantShiftSemitones  = 1.5f;
        s.reverbAmount           = 0.30f;                         // dry outdoor
        s.flangerRate            = 1.2f;                          // faster breathy sweep
        s.chorusDepth            = 0.15f;
        s.dynEqMix               = 0.75f;                         // dominant low-body cut
        s.dynEqFreqHz            = 400.0f;
        s.dynEqQ                 = 2.2f;
        s.dynEqTargetGainDb      = -9.0f;                         // severe body cut — pure breath
        s.dynEqThreshold         = 0.04f;
        s.noiseLevel             = 0.09f;                         // more audible high air
        s.noiseColor             = AV::NoiseGenerator::White;
        s.noiseHighPassHz        = 3500.0f;
        s.noiseGated             = true;
        s.outputGain             = 0.92f;
        makeAV(29, s);
    }

    // AV_30 Nefertari's Whisper (Egyptian) — beloved queen's intimate utterance
    //   Signature: OCTAVE-UP + +3 FORMANT + prominent sibilance-band pink noise (consonants)
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 12.0f;                         // octave up — feminine
        s.formantShiftSemitones  = 3.5f;                          // stronger than before
        s.reverbAmount           = 0.50f;                         // intimate tight
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 5.0f;                          // tight close-mic feel
        s.reverbHfDampingDb      = -2.0f;
        s.chorusDepth            = 0.05f;
        s.compressionRatio       = 5.0f;                          // whispered intimacy
        s.noiseLevel             = 0.38f;                         // max dominant sibilance
        s.noiseColor             = AV::NoiseGenerator::Pink;
        s.noiseHighPassHz        = 4500.0f;                       // sibilance range
        s.noiseGated             = true;
        s.outputGain             = 0.85f;
        makeAV(30, s);
    }

    // AV_31 Desert Wind of Giza (Egyptian) — sandstorm howling across pyramid plateau
    //   Signature: NEAR-FULLY-WET GRANULAR HOWL with wide random pitch + sand noise bed
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 0.0f;                          // base voice preserved
        s.formantShiftSemitones  = 0.0f;
        s.reverbAmount           = 0.95f;                         // big outdoor space
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 45.0f;
        s.reverbHfDampingDb      = -4.0f;
        s.granularMix            = 0.92f;                         // near-total howl identity
        s.granularGrainSizeMs    = 85.0f;
        s.granularDensity        = 20.0f;
        s.granularPitchSpreadSemi = 7.0f;                         // widest howl
        s.granularPositionJitter = 0.98f;
        s.granularReverseProb    = 0.4f;                          // more eerie reversal
        s.noiseLevel             = 0.12f;                         // stronger sand bed
        s.noiseColor             = AV::NoiseGenerator::Pink;
        s.noiseHighPassHz        = 1000.0f;
        s.lfo1RateHz             = 0.4f;
        s.lfo1Depth              = 0.6f;
        s.lfo1Shape              = AV::LFO::Random;
        s.lfo1Dest               = AV::ModDest::FilterCutoff;
        s.outputGain             = 0.82f;
        makeAV(31, s);
    }

    // AV_32 Tigris Dawn Breath (Mesopotamian) — morning mist rolling over river
    //   Signature: DOMINANT 3Hz TREMOLO + mist-like formant rise + gated low-air noise
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 0.5f;                          // neutral Mesopotamian
        s.formantShiftSemitones  = 2.0f;                          // rising mist
        s.reverbAmount           = 0.58f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 18.0f;
        s.reverbHfDampingDb      = -5.0f;                         // soft mist
        s.lfo1RateHz             = 3.0f;
        s.lfo1Depth              = 0.55f;                         // deep mist-roll tremolo
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Amplitude;
        s.noiseLevel             = 0.08f;                         // stronger river-bed noise
        s.noiseColor             = AV::NoiseGenerator::Pink;
        s.noiseHighPassHz        = 300.0f;                        // river bed
        s.noiseGated             = true;
        s.chorusDepth            = 0.12f;
        s.outputGain             = 0.92f;
        makeAV(32, s);
    }

    // AV_33 Papyrus Rustle (Egyptian) — crinkling of ancient scribe's paper
    //   Signature: AGGRESSIVE TRANSIENT BOOST + high-jitter random granular crackle
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 1.0f;                          // Egyptian neutral-bright
        s.formantShiftSemitones  = 1.0f;
        s.reverbAmount           = 0.12f;                         // bone-dry
        s.transientAttackDb      = 14.0f;                         // max crackling attack
        s.transientMix           = 0.90f;                         // dominant signature
        s.granularMix            = 0.80f;                         // max crackle presence
        s.granularGrainSizeMs    = 12.0f;
        s.granularDensity        = 45.0f;
        s.granularPitchSpreadSemi = 0.5f;
        s.granularPositionJitter = 0.95f;                         // near-full randomization
        s.noiseLevel             = 0.10f;                         // stronger paper crackle
        s.noiseColor             = AV::NoiseGenerator::White;
        s.noiseHighPassHz        = 5000.0f;
        s.noiseGated             = true;
        s.chorusDepth            = 0.03f;
        s.outputGain             = 0.92f;
        makeAV(33, s);
    }

    // AV_34 Scribe's Murmur (Mesopotamian) — clay-tablet scribe muttering as he writes
    //   Signature: DOMINANT HARD-L/R MULTI-TAP MUTTER + intimate dry + low formant
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -1.0f;                         // low neutral Mesopotamian
        s.formantShiftSemitones  = -2.0f;                         // very low muttering quality
        s.reverbAmount           = 0.22f;                         // tight intimate
        s.multiTapMix            = 0.90f;                         // near-max mutter identity
        s.multiTapTimesMs[0] = 30.0f; s.multiTapGains[0] = 0.85f; s.multiTapPans[0] = -1.0f;
        s.multiTapTimesMs[1] = 45.0f; s.multiTapGains[1] = 0.85f; s.multiTapPans[1] = +1.0f;
        s.compressionRatio       = 5.5f;                          // even more intimate
        s.dynEqMix               = 0.7f;
        s.dynEqFreqHz            = 250.0f;
        s.dynEqQ                 = 2.0f;
        s.dynEqTargetGainDb      = 5.0f;                          // more mumble body
        s.dynEqThreshold         = 0.06f;
        s.outputGain             = 0.88f;
        makeAV(34, s);
    }

    // AV_35 Incense of Ishtar (Akkadian/Babylonian) — temple smoke rising
    //   Signature: SLOW-BLOOM LATE REVERB + env-driven filter RISE (smoke lift)
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 0.0f;                          // Akkadian neutral
        s.formantShiftSemitones  = 1.0f;
        s.reverbAmount           = 0.92f;                         // big slow bloom
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 150.0f;                        // even later bloom
        s.reverbHfDampingDb      = -1.0f;
        s.chorusDepth            = 0.25f;
        s.envFollowAttackMs      = 200.0f;
        s.envFollowReleaseMs     = 800.0f;                        // slower rise
        s.envFollowDepth         = 0.85f;                         // near-max rising smoke
        s.envFollowDest          = AV::ModDest::FilterCutoff;
        s.granularMix            = 0.28f;                         // stronger smoke shimmer
        s.granularGrainSizeMs    = 25.0f;
        s.granularDensity        = 10.0f;
        s.granularPitchBaseSemi  = 7.0f;                          // drifting fifth
        s.granularPitchSpreadSemi = 0.3f;
        s.outputGain             = 0.88f;
        makeAV(35, s);
    }

    // =====================================================
    //   SHIMMER (AV_36 .. AV_42)
    // =====================================================

    // AV_36 Hathor's Sistrum (Egyptian) — goddess's ritual metallic rattle
    //   Signature: EXTREME-SPEED GRANULAR SHAKE + bright +7 pitch + rattle dyn EQ
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 7.0f;
        s.formantShiftSemitones  = 2.0f;                          // stronger bright
        s.reverbAmount           = 0.72f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 12.0f;
        s.reverbHfDampingDb      = 1.0f;                          // brighter — metallic
        s.granularMix            = 0.88f;                         // max sistrum rattle
        s.granularGrainSizeMs    = 5.0f;                          // even shorter — faster shake
        s.granularDensity        = 48.0f;
        s.granularPitchSpreadSemi = 3.5f;
        s.granularPositionJitter = 0.7f;
        s.dynEqMix               = 0.70f;                         // stronger rattle band
        s.dynEqFreqHz            = 6500.0f;
        s.dynEqQ                 = 3.2f;
        s.dynEqTargetGainDb      = 7.0f;
        s.dynEqThreshold         = 0.06f;
        s.distortionAmount       = 0.15f;
        s.outputGain             = 0.88f;
        makeAV(36, s);
    }

    // AV_37 Stars of Nut (Egyptian) — stars on goddess's body, celestial twinkle
    //   Signature: +12 PITCH + PRIME-NUMBER 4-TAP MATRIX (un-synced twinkling stars)
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 12.0f;
        s.formantShiftSemitones  = 1.0f;
        s.reverbAmount           = 0.95f;                         // 4.5s pristine celestial
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 30.0f;
        s.reverbHfDampingDb      = 0.0f;                          // bright
        s.multiTapMix            = 0.80f;                         // dominant twinkle matrix
        s.multiTapTimesMs[0] = 317.0f; s.multiTapGains[0] = 0.65f; s.multiTapPans[0] = -0.8f;
        s.multiTapTimesMs[1] = 439.0f; s.multiTapGains[1] = 0.60f; s.multiTapPans[1] = +0.8f;
        s.multiTapTimesMs[2] = 601.0f; s.multiTapGains[2] = 0.55f; s.multiTapPans[2] = -0.4f;
        s.multiTapTimesMs[3] = 751.0f; s.multiTapGains[3] = 0.50f; s.multiTapPans[3] = +0.4f;
        s.harmonizerAmount       = 0.55f;                         // stronger octave stack
        s.outputGain             = 0.88f;
        makeAV(37, s);
    }

    // AV_38 Lapis of Inanna (Sumerian) — sacred blue stone of the goddess
    //   Signature: HIGH PV FREEZE (crystalline sustained pad) + fifth pitch + airy
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 5.0f;                          // fifth up
        s.formantShiftSemitones  = 2.5f;                          // strong crystalline quality
        s.reverbAmount           = 0.85f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 20.0f;
        s.reverbHfDampingDb      = 0.0f;                          // crystalline brightness
        s.pvFreezeAmount         = 0.95f;                         // near-total freeze pad
        s.pvMix                  = 0.80f;                         // dominant
        s.chorusDepth            = 0.18f;
        s.outputGain             = 0.88f;
        makeAV(38, s);
    }

    // AV_39 Gilded Mask of Tut (Egyptian) — Tutankhamun's golden death mask
    //   Signature: STRONG 2kHz GOLDEN PEAK + fast bronze flanger + tight plate
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 3.0f;                          // Egyptian elevated
        s.formantShiftSemitones  = 2.0f;                          // stronger gilded quality
        s.reverbAmount           = 0.55f;                         // tight plate
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 4.0f;                          // instant plate
        s.reverbHfDampingDb      = 2.0f;                          // actually brighter
        s.dynEqMix               = 0.90f;                         // max dominant golden peak
        s.dynEqFreqHz            = 2000.0f;
        s.dynEqQ                 = 3.0f;                          // tighter, more metallic
        s.dynEqTargetGainDb      = 10.0f;                         // max harsh golden
        s.dynEqThreshold         = 0.05f;
        s.flangerRate            = 3.0f;                          // faster bronze sheen
        s.distortionAmount       = 0.15f;
        s.outputGain             = 0.85f;
        makeAV(39, s);
    }

    // AV_40 Sunboat of Ra (Egyptian) — sun god's vessel crossing the sky
    //   Signature: EXTREME WIDE STEREO + fifth harmonizer stack + burning-heat saturation
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 8.0f;                          // radiant upward
        s.formantShiftSemitones  = 1.5f;
        s.reverbAmount           = 0.93f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 35.0f;
        s.reverbHfDampingDb      = -1.0f;
        s.harmonizerAmount       = 0.70f;                         // stronger fifth stack
        s.flangerRate            = 0.6f;                          // faster phaser sweep
        s.distortionAmount       = 0.42f;                         // more burning tape sat
        s.multiTapMix            = 0.65f;                         // stronger width
        s.multiTapTimesMs[0] = 60.0f;  s.multiTapGains[0] = 0.7f;  s.multiTapPans[0] = -1.0f;
        s.multiTapTimesMs[1] = 95.0f;  s.multiTapGains[1] = 0.7f;  s.multiTapPans[1] = +1.0f;
        s.multiTapTimesMs[2] = 140.0f; s.multiTapGains[2] = 0.55f; s.multiTapPans[2] = -0.5f;
        s.outputGain             = 0.85f;
        makeAV(40, s);
    }

    // AV_41 Crystal of Utu (Sumerian) — sun god's radiant crystal
    //   Signature: DOMINANT 1200Hz COMB FILTER (crystalline spikes) + pitch-up granular
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 12.0f;                         // octave up — radiant
        s.formantShiftSemitones  = 1.5f;
        s.reverbAmount           = 0.78f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 18.0f;
        s.reverbHfDampingDb      = 0.0f;                          // crystalline bright
        s.combFreqHz             = 1200.0f;
        s.combFeedback           = 0.88f;                         // extreme resonance
        s.combMix                = 0.72f;                         // dominant crystal
        s.chorusDepth            = 0.15f;
        s.granularMix            = 0.2f;                          // subtle crystal breath
        s.granularGrainSizeMs    = 12.0f;
        s.granularDensity        = 12.0f;
        s.granularPitchBaseSemi  = 12.0f;
        s.granularReverseProb    = 0.4f;                          // reversed crystalline
        s.outputGain             = 0.88f;
        makeAV(41, s);
    }

    // AV_42 Ziggurat at Dawn (Sumerian) — stepped temple pyramid catching first light
    //   Signature: DOMINANT SLOW PAN SWEEP + env-driven filter opening over time
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 6.0f;                          // elevated dawn brightness
        s.formantShiftSemitones  = 1.0f;
        s.reverbAmount           = 0.82f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 40.0f;                         // stepped temple distance
        s.reverbHfDampingDb      = -3.0f;
        s.lfo1RateHz             = 0.06f;                         // 16s pan — slower wider
        s.lfo1Depth              = 1.0f;
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Pan;
        s.envFollowAttackMs      = 400.0f;
        s.envFollowReleaseMs     = 1200.0f;
        s.envFollowDepth         = 0.80f;                         // dramatic filter opening
        s.envFollowDest          = AV::ModDest::FilterCutoff;
        s.chorusDepth            = 0.18f;
        s.outputGain             = 0.92f;
        makeAV(42, s);
    }

    // =====================================================
    //   DARK (AV_43 .. AV_49)
    // =====================================================

    // AV_43 Kur Descent (Sumerian) — falling into the underworld abyss
    //   Signature: OCTAVE-DOWN + EXTREME HF KILL + ULTRA-DARK VOID (no sparkle at all)
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -12.0f;                        // octave descent
        s.formantShiftSemitones  = -4.0f;                         // deeper than before
        s.reverbAmount           = 0.99f;                         // 5s void
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 60.0f;
        s.reverbHfDampingDb      = -28.0f;                        // absolute void — maximum kill
        s.distortionAmount       = 0.10f;
        s.envFollowAttackMs      = 30.0f;
        s.envFollowReleaseMs     = 1000.0f;
        s.envFollowDepth         = -0.70f;                        // dramatic tail collapse
        s.envFollowDest          = AV::ModDest::ReverbWet;
        s.outputGain             = 0.78f;
        makeAV(43, s);
    }

    // AV_44 Ereshkigal's Throne (Sumerian) — queen of the underworld on her throne
    //   Signature: DOMINANT REVERSE REVERB + heavy throne-room distortion + fifth-down harmonizer
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -7.0f;                         // fifth down
        s.formantShiftSemitones  = -2.0f;                         // lower than before
        s.reverbAmount           = 0.40f;                         // reduced — reverse dominates
        s.reverseReverbMix       = 0.92f;                         // total reverse-reverb identity
        s.reverseReverbWindowSec = 3.0f;                          // longer window
        s.distortionAmount       = 0.55f;                         // stronger throne menace
        s.harmonizerAmount       = 0.60f;                         // stronger downward stack
        s.delayAmount            = 0.35f;
        s.outputGain             = 0.8f;
        makeAV(44, s);
    }

    // AV_45 Tomb of Seti (Egyptian) — sealed pharaoh's burial chamber
    //   Signature: DOMINANT COMB FILTER @ 350Hz (hollow cancellation) + 10Hz motor growl
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -2.0f;                         // near-neutral Egyptian tomb
        s.formantShiftSemitones  = -3.0f;                         // even lower than before
        s.reverbAmount           = 0.85f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 3.0f;                          // instant sealed-box
        s.reverbHfDampingDb      = -8.0f;                         // sealed stone darkness
        s.combFreqHz             = 350.0f;
        s.combFeedback           = 0.85f;                         // extreme hollow
        s.combMix                = 0.72f;                         // dominant identity
        s.distortionAmount       = 0.35f;
        s.lfo1RateHz             = 12.0f;                         // faster motor
        s.lfo1Depth              = 0.32f;                         // stronger growl
        s.lfo1Shape              = AV::LFO::Sine;
        s.lfo1Dest               = AV::ModDest::Amplitude;
        s.outputGain             = 0.82f;
        makeAV(45, s);
    }

    // AV_46 Apep Rising (Egyptian) — chaos serpent coiling to swallow the sun
    //   Signature: DOMINANT WRITHING ±OCTAVE PITCH LFO + aggressive soft-clip + dyn boost low-mids
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 0.0f;                          // pitch LFO does the work
        s.formantShiftSemitones  = -1.0f;
        s.reverbAmount           = 0.82f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 30.0f;
        s.reverbHfDampingDb      = -4.0f;
        s.distortionAmount       = 0.58f;                         // even more soft-clip
        s.lfo1RateHz             = 0.07f;
        s.lfo1Depth              = 18.0f;                         // MAX writhing — ±18 semitones
        s.lfo1Shape              = AV::LFO::Triangle;
        s.lfo1Dest               = AV::ModDest::Pitch;
        s.dynEqMix               = 0.75f;
        s.dynEqFreqHz            = 220.0f;
        s.dynEqQ                 = 1.8f;
        s.dynEqTargetGainDb      = 6.5f;                          // stronger snake body
        s.dynEqThreshold         = 0.06f;
        s.outputGain             = 0.78f;
        makeAV(46, s);
    }

    // AV_47 Lamashtu (Akkadian/Babylonian) — she-demon who preys on infants
    //   Signature: VIOLENT RANDOM PITCH STABS + 8-bit aliasing + screeching high
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = 12.0f;                         // demonic upward
        s.formantShiftSemitones  = 2.5f;                          // stronger unnatural
        s.reverbAmount           = 0.50f;                         // damped
        s.lfo1RateHz             = 12.0f;                         // even faster violent stabs
        s.lfo1Depth              = 4.0f;                          // ±4 semi — max demonic
        s.lfo1Shape              = AV::LFO::Random;
        s.lfo1Dest               = AV::ModDest::Pitch;
        s.bitcrushMix            = 0.60f;                         // max dominant aliasing
        s.bitcrushBitDepth       = 6.0f;                          // harsher 6-bit
        s.bitcrushSampleRateHz   = 12000.0f;
        s.distortionAmount       = 0.40f;
        s.outputGain             = 0.75f;
        makeAV(47, s);
    }

    // AV_48 Nergal's Gate (Akkadian/Babylonian) — gate of plague and underworld war
    //   Signature: PV FREEZE on transients (held divine wrath) + booming multi-tap gates
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -4.0f;                         // commanding but not deepest
        s.formantShiftSemitones  = 1.0f;                          // polar Akkadian bright
        s.reverbAmount           = 0.92f;
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 55.0f;
        s.reverbHfDampingDb      = -2.0f;
        s.pvFreezeAmount         = 0.90f;                         // max held divine wrath
        s.pvMix                  = 0.65f;
        s.multiTapMix            = 0.75f;                         // stronger booming gates
        s.multiTapTimesMs[0] = 500.0f; s.multiTapGains[0] = 0.75f; s.multiTapPans[0] = -0.8f;
        s.multiTapTimesMs[1] = 250.0f; s.multiTapGains[1] = 0.7f;  s.multiTapPans[1] = +0.8f;
        s.multiTapTimesMs[2] = 750.0f; s.multiTapGains[2] = 0.55f; s.multiTapPans[2] = -0.3f;
        s.bitcrushMix            = 0.15f;                         // more corruption
        s.bitcrushBitDepth       = 10.0f;
        s.bitcrushSampleRateHz   = 22000.0f;
        s.flangerRate            = 0.25f;
        s.distortionAmount       = 0.22f;
        s.outputGain             = 0.82f;
        makeAV(48, s);
    }

    // AV_49 Pazuzu (Akkadian/Babylonian) — demon-king of wind storms, locusts, fevers
    //   Signature: EXTREME -4 FORMANT (inhuman swarm) + dominant 2ms granular + dark wind bed
    {
        ProfileSettings s{};
        s.pitchShiftAmount       = -2.0f;                         // low but not deepest
        s.formantShiftSemitones  = -5.0f;                         // even more inhuman
        s.reverbAmount           = 0.48f;                         // dry oppressive close
        s.useEnhancedReverb      = true;
        s.reverbPreDelayMs       = 0.0f;                          // no space — in-your-face
        s.reverbHfDampingDb      = -6.0f;
        s.chorusDepth            = 0.98f;                         // near-max swarm voicing
        s.granularMix            = 0.80f;                         // dominant choking
        s.granularGrainSizeMs    = 2.0f;
        s.granularDensity        = 50.0f;                         // max choking density
        s.granularPitchSpreadSemi = 4.0f;                         // wider swarm
        s.granularPositionJitter = 0.85f;
        s.granularReverseProb    = 0.35f;
        s.noiseLevel             = 0.25f;                         // max wind bed
        s.noiseColor             = AV::NoiseGenerator::Pink;
        s.noiseHighPassHz        = 80.0f;
        s.distortionAmount       = 0.48f;                         // more grit
        s.flangerRate            = 2.0f;
        s.outputGain             = 0.75f;
        makeAV(49, s);
    }
}
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

               // ----------------------------------------------------
               //  SESSION 1: Formant / LFO / EnvFollower / ModMatrix
               // ----------------------------------------------------
               // Configure modules from the current profile each block
               // (cheap — just setters, no allocation).
               formantShifter.setShiftSemitones (currentProfile.formantShiftSemitones);

               lfo1.setRateHz (currentProfile.lfo1RateHz);
               lfo1.setDepth  (currentProfile.lfo1Depth);
               lfo1.setShape  (currentProfile.lfo1Shape);

               envFollower.setAttackMs  (currentProfile.envFollowAttackMs);
               envFollower.setReleaseMs (currentProfile.envFollowReleaseMs);
               envFollower.setDepth     (currentProfile.envFollowDepth);

               // Sample modulation sources this block
               AV::ModSources srcs;
               srcs.lfo1Dest      = currentProfile.lfo1Dest;
               srcs.envFollowDest = currentProfile.envFollowDest;
               srcs.lfo1          = lfo1.tickBlock (buffer.getNumSamples());
               srcs.envFollow     = envFollower.processBlock (buffer);
               const auto modded  = AV::ModMatrix::apply (srcs);

               // Apply formant BEFORE pitch shift (so it's truly independent of the
               // speed-of-sound pitch manipulation). Zero-shift is an early-out inside.
               // Combine preset formant with formant-destined modulation.
               if (std::fabs (currentProfile.formantShiftSemitones + modded.formantOffsetSemi) > 0.01f)
               {
                   formantShifter.setShiftSemitones (currentProfile.formantShiftSemitones + modded.formantOffsetSemi);
                   formantShifter.process (buffer);
               }

               // Combine preset pitch with user-knob pitch offset, clamp to ±24st
               float pitchShiftAmount = currentProfile.pitchShiftAmount + modded.pitchOffsetSemi;
               if (userPitch != nullptr)
                   pitchShiftAmount += userPitch->load();
               pitchShiftAmount = juce::jlimit(-24.0f, 24.0f, pitchShiftAmount);
               //DBG("Pitch Shift Amount (preset+user+mod, clamped): " << pitchShiftAmount);

               // Process pitch shifting
               //DBG("Applying Pitch Shift...");
               applyRubberBandPitchShift(buffer, pitchShiftAmount);

               // Apply amplitude modulation (tremolo) per block if destined
               if (std::fabs (modded.amplitudeMul - 1.0f) > 0.001f)
               {
                   buffer.applyGain (juce::jlimit (0.0f, 2.0f, modded.amplitudeMul));
               }

               // Apply stereo pan offset (simple L/R gain) if destined
               if (std::fabs (modded.panOffset) > 0.001f && buffer.getNumChannels() >= 2)
               {
                   const float p = juce::jlimit (-1.0f, 1.0f, modded.panOffset);
                   // Equal-power-ish: left gets (1-p)/2-ish, right gets (1+p)/2-ish
                   const float lGain = std::sqrt (juce::jlimit (0.0f, 1.0f, 0.5f - 0.5f * p));
                   const float rGain = std::sqrt (juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * p));
                   buffer.applyGain (0, 0, buffer.getNumSamples(), lGain * 1.414f);
                   buffer.applyGain (1, 0, buffer.getNumSamples(), rGain * 1.414f);
               }

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

        // Reverb: Adds space and depth by simulating room reflections.
        // When useEnhancedReverb is true, route through EnhancedReverb instead
        // for pre-delay + HF damping; otherwise use the existing basic reverb.
        if (currentProfile.useEnhancedReverb && currentProfile.reverbAmount > 0.0f)
        {
            enhancedReverb.setRoomSize (juce::jlimit (0.0f, 1.0f, currentProfile.reverbAmount));
            enhancedReverb.setDamping  (0.5f);
            enhancedReverb.setWidth    (1.0f);
            enhancedReverb.setWetLevel (currentProfile.reverbAmount);
            enhancedReverb.setDryLevel (1.0f - currentProfile.reverbAmount * 0.5f);
            enhancedReverb.setPreDelayMs (currentProfile.reverbPreDelayMs);
            enhancedReverb.setHfDampingDb (currentProfile.reverbHfDampingDb);
            enhancedReverb.process (buffer);
        }
        else if (currentProfile.reverbAmount > 0.0f)
        {
            //DBG("Applying reverb with amount: " << currentProfile.reverbAmount);
            applyReverbEffect(buffer, currentProfile.reverbAmount);
        }
        else
        {
            //DBG("Skipping reverb because amount is 0.");
        }

        // --- Session 2: multi-tap delay, comb filter, reverse reverb ---
        if (currentProfile.multiTapMix > 0.0f)
        {
            multiTap.setMix (currentProfile.multiTapMix);
            for (int t = 0; t < AV::MultiTapDelay::kMaxTaps; ++t)
                multiTap.setTap (t,
                                 currentProfile.multiTapTimesMs[t],
                                 currentProfile.multiTapGains[t],
                                 currentProfile.multiTapPans[t]);
            multiTap.process (buffer);
        }

        if (currentProfile.combMix > 0.0f && currentProfile.combFreqHz > 0.0f)
        {
            combFilter.setFrequencyHz (currentProfile.combFreqHz);
            combFilter.setFeedback    (currentProfile.combFeedback);
            combFilter.setMix         (currentProfile.combMix);
            combFilter.process (buffer);
        }

        if (currentProfile.reverseReverbMix > 0.0f)
        {
            reverseReverb.setWindowSec (currentProfile.reverseReverbWindowSec);
            reverseReverb.setMix       (currentProfile.reverseReverbMix);
            reverseReverb.process (buffer);
        }

        // --- Session 3: phase vocoder, granular, bitcrusher, dyn EQ,
        //     noise, transient shaper ---
        if (currentProfile.transientMix > 0.0f)
        {
            transientShaper.setAttackBoostDb (currentProfile.transientAttackDb);
            transientShaper.setMix           (currentProfile.transientMix);
            transientShaper.process (buffer);
        }

        if (currentProfile.pvMix > 0.0f)
        {
            phaseVocoder.setFreezeAmount (currentProfile.pvFreezeAmount);
            phaseVocoder.setSmearAmount  (currentProfile.pvSmearAmount);
            phaseVocoder.setMix          (currentProfile.pvMix);
            phaseVocoder.process (buffer);
        }

        if (currentProfile.granularMix > 0.0f && currentProfile.granularDensity > 0.0f)
        {
            granular.setGrainSizeMs     (currentProfile.granularGrainSizeMs);
            granular.setDensity         (currentProfile.granularDensity);
            granular.setPitchSpreadSemi (currentProfile.granularPitchSpreadSemi);
            granular.setPitchBaseOffset (currentProfile.granularPitchBaseSemi);
            granular.setReverseProb     (currentProfile.granularReverseProb);
            granular.setPositionJitter  (currentProfile.granularPositionJitter);
            granular.setMix             (currentProfile.granularMix);
            granular.process (buffer);
        }

        if (currentProfile.dynEqMix > 0.0f)
        {
            dynamicEq.setFrequency    (currentProfile.dynEqFreqHz);
            dynamicEq.setQ            (currentProfile.dynEqQ);
            dynamicEq.setTargetGainDb (currentProfile.dynEqTargetGainDb);
            dynamicEq.setThreshold    (currentProfile.dynEqThreshold);
            dynamicEq.setMix          (currentProfile.dynEqMix);
            dynamicEq.process (buffer);
        }

        if (currentProfile.bitcrushMix > 0.0f)
        {
            bitcrusher.setBitDepth     (currentProfile.bitcrushBitDepth);
            bitcrusher.setSampleRateHz (currentProfile.bitcrushSampleRateHz);
            bitcrusher.setMix          (currentProfile.bitcrushMix);
            bitcrusher.process (buffer);
        }

        if (currentProfile.noiseLevel > 0.0f)
        {
            noiseGen.setColor        (currentProfile.noiseColor);
            noiseGen.setLevel        (currentProfile.noiseLevel);
            noiseGen.setHighPassHz   (currentProfile.noiseHighPassHz);
            noiseGen.setGated        (currentProfile.noiseGated);
            noiseGen.process (buffer);
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
    // Note: This implementation is complex and may introduce distortion if not handled correctly.
    // Consider using JUCE's built-in pitch shifter or verifying the logic thoroughly.

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    float pitchShiftFactor = std::pow(2.0f, shiftAmount / 12.0f);  // Convert semitone shift to pitch factor

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
    if (auto* playHead = getPlayHead())  // Get the playhead
    {
        if (auto position = playHead->getPosition())  // Use getPosition to get the current position
        {
            if (auto bpm = position->getBpm())  // Check if BPM is available
            {
                return *bpm;  // Return the BPM
            }
        }
    }
    return 120.0;  // Default to 120 BPM if no tempo is available
}

//------------------------------------------------- REPEATER STUTTER NOSIE REDUCTION
void AncientVoicesAudioProcessor::applyStutterEffect(juce::AudioBuffer<float>& buffer, float stutterAmount)
{
    //DBG("Starting Simple Stutter Effect with stutterAmount: " << stutterAmount);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();

    //DBG("Number of Channels: " << numChannels);
    //DBG("Number of Samples: " << numSamples);
    //DBG("Sample Rate: " << sampleRate);

    // Calculate stutter length based on stutterAmount with a minimum threshold
    int stutterLength = std::max(static_cast<int>(sampleRate * stutterAmount), 8); // Minimum of 8 samples

    // Limit stutter length to prevent excessive buffer use
    stutterLength = juce::jlimit(8, numSamples / 4, stutterLength);
    //DBG("Calculated Stutter Length: " << stutterLength);

    // Only apply if stutterLength is reasonable
    if (stutterLength < numSamples)
    {
        // Loop over each channel
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            //DBG("Processing Channel: " << channel);

            // Process each segment of the audio buffer
            for (int position = 0; position < numSamples; position += stutterLength * 3) // Adjust for longer silent intervals
            {
                //DBG("Processing Position: " << position);

                // Repeat the stuttered audio chunk to mimic stuttering
                for (int repeat = 0; repeat < 2; ++repeat)
                {
                    int offset = repeat * stutterLength;
                    if (position + offset + stutterLength < numSamples)
                    {
                        //DBG("Copying Stutter Segment from Position: " << position << " to " << position + offset);
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
                    //DBG("Adding Extended Silence after Stutter at Position: " << position + stutterLength * 2);
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
        case 1:  // Alpha: Boost highs for ethereal, airy sound
            cutoffHigh = 5000.0f;  // Boost highs
            break;
        case 2:  // Beta: Bright, boost mids
            cutoffMid = 3000.0f;   // Boost mids
            break;
        case 3:  // Gamma: Deep and resonant, boost lows
            cutoffLow = 800.0f;    // Boost lows
            break;
        case 4:  // Delta: Balanced, no significant EQ change
            cutoffMid = 2000.0f;   // Balanced EQ
            break;
        case 5:  // Epsilon: Sharp and bright, boost highs
            cutoffHigh = 6000.0f;  // Boost highs
            break;
        case 6:  // Zeta: Gentle and melodic, boost mids slightly
            cutoffMid = 3500.0f;   // Boost mids
            break;
        case 7:  // Eta: Rhythmic, boost mids for movement
            cutoffMid = 2500.0f;   // Boost mids
            break;
        case 8:  // Theta: Warm and rich, boost lows and mids
            cutoffLow = 1500.0f;   // Boost lows
            cutoffMid = 1500.0f;   // Boost mids
            break;
        case 9:  // Iota: Crisp and clear, boost highs
            cutoffHigh = 5500.0f;  // Boost highs
            break;
        case 10: // Kappa: Powerful, boost low/mid for richness
            cutoffLow = 1200.0f;   // Boost lows
            cutoffMid = 1200.0f;   // Boost mids
            break;
        case 11: // Lambda: Spacious, boost highs
            cutoffHigh = 6000.0f;  // Boost highs
            break;
        case 12: // Mu: Warm and embracing, boost low/mid
            cutoffLow = 1000.0f;   // Boost lows
            cutoffMid = 1000.0f;   // Boost mids
            break;
        case 13: // Nu: Bright, boost mids/highs
            cutoffMid = 4000.0f;   // Boost mids
            cutoffHigh = 4000.0f;  // Boost highs
            break;
        case 14: // Xi: Ethereal, boost highs
            cutoffHigh = 7000.0f;  // Boost highs
            break;
        case 15: // Omicron: Balanced
            cutoffMid = 2500.0f;   // Balanced EQ
            break;
        case 16: // Pi: Dynamic and lively, boost mids
            cutoffMid = 3000.0f;   // Boost mids
            break;
        case 17: // Rho: Neutral and consistent
            cutoffMid = 2500.0f;   // Balanced EQ
            break;
        case 18: // Sigma: Thick and layered, boost low/mid
            cutoffLow = 1000.0f;   // Boost lows
            cutoffMid = 1000.0f;   // Boost mids
            break;
        case 19: // Tau: Textured, boost mids/highs
            cutoffMid = 4500.0f;   // Boost mids
            cutoffHigh = 4500.0f;  // Boost highs
            break;
        case 20: // Upsilon: Sharp, boost highs
            cutoffHigh = 6000.0f;  // Boost highs
            break;
        case 21: // Phi: Powerful, boost lows
            cutoffLow = 800.0f;    // Boost lows
            break;
        case 22: // Chi: Smooth, boost mids
            cutoffMid = 3500.0f;   // Boost mids
            break;
        case 23: // Psi: Articulate, boost highs
            cutoffHigh = 5500.0f;  // Boost highs
            break;
        case 24: // Omega: Deep, boost lows
            cutoffLow = 700.0f;    // Boost lows
            break;
        case 25: // Alpha Beta: Balanced, boost mids and highs
            cutoffMid = 3500.0f;   // Boost mids
            cutoffHigh = 3500.0f;  // Boost highs
            break;
        case 26: // Gamma Delta: Boost lows for depth
            cutoffLow = 1200.0f;   // Boost lows
            break;
        case 27: // Epsilon Zeta: Gentle boost to highs
            cutoffHigh = 4000.0f;  // Boost highs
            break;
        case 28: // Eta Theta: Smooth boost to mids
            cutoffMid = 2500.0f;   // Boost mids
            break;
        case 29: // Iota Kappa: Boost highs for crispness
            cutoffHigh = 5000.0f;  // Boost highs
            break;
        case 30: // Lambda Mu: Boost lows for warmth
            cutoffLow = 1000.0f;   // Boost lows
            break;
        default:
            cutoffMid = 2000.0f;   // Default balanced EQ
            break;
    }
}

void AncientVoicesAudioProcessor::applyFormantShifting(juce::AudioBuffer<float>& buffer, int profileIndex)
{
    //DBG("formant shifting called.");
    float shiftInterval = 0.0f;  // Default: no shift

    switch (profileIndex)
    {
        case 1:  // Alpha: Slight upward shift for airy sound
            shiftInterval = 1.0f;  // 1 semitone up
            break;
        case 2:  // Beta: Upward shift for brightness
            shiftInterval = 0.5f;  // Half a semitone up
            break;
        case 3:  // Gamma: Downward shift for deep sound
            shiftInterval = -1.0f; // 1 semitone down
            break;
        case 4:  // Delta: Neutral, no shift
            shiftInterval = 0.0f;  // No shift
            break;
        case 5:  // Epsilon: Upward shift for sharpness
            shiftInterval = 2.0f;  // 2 semitones up
            break;
        case 6:  // Zeta: Mild upward shift for melodic sound
            shiftInterval = 1.0f;
            break;
        case 7:  // Eta: Mid-range shift for rhythmic sound
            shiftInterval = 0.5f;
            break;
        case 8:  // Theta: Downward shift for warmth
            shiftInterval = -0.5f;
            break;
        case 9:  // Iota: Upward shift for clarity
            shiftInterval = 1.5f;
            break;
        case 10: // Kappa: Downward shift for power
            shiftInterval = -1.0f;
            break;
        case 11: // Lambda: Upward shift for spaciousness
            shiftInterval = 2.0f;
            break;
        case 12: // Mu: Downward shift for warmth
            shiftInterval = -1.5f;
            break;
        case 13: // Nu: Upward shift for brightness
            shiftInterval = 0.5f;
            break;
        case 14: // Xi: Upward shift for ethereal quality
            shiftInterval = 2.0f;
            break;
        case 15: // Omicron: Neutral, no shift
            shiftInterval = 0.0f;
            break;
        case 16: // Pi: Upward shift for lively tone
            shiftInterval = 1.0f;
            break;
        case 17: // Rho: Neutral, no shift
            shiftInterval = 0.0f;
            break;
        case 18: // Sigma: Downward shift for layered tone
            shiftInterval = -1.0f;
            break;
        case 19: // Tau: Upward shift for texture
            shiftInterval = 1.5f;
            break;
        case 20: // Upsilon: Upward shift for sharpness
            shiftInterval = 1.0f;
            break;
        case 21: // Phi: Downward shift for power
            shiftInterval = -2.0f;
            break;
        case 22: // Chi: Slight upward shift for smoothness
            shiftInterval = 0.5f;
            break;
        case 23: // Psi: Upward shift for clarity and articulation
            shiftInterval = 1.0f;
            break;
        case 24: // Omega: Downward shift for deep, powerful sound
            shiftInterval = -1.5f;
            break;
        case 25: // Alpha Beta: Balanced, slight upward shift
            shiftInterval = 0.5f;
            break;
        case 26: // Gamma Delta: Downward shift for depth
            shiftInterval = -1.0f;
            break;
        case 27: // Epsilon Zeta: Mild upward shift for clarity
            shiftInterval = 1.0f;
            break;
        case 28: // Eta Theta: Mid-range shift for smooth, rhythmic tone
            shiftInterval = 0.5f;
            break;
        case 29: // Iota Kappa: Slight upward shift for clarity and power
            shiftInterval = 1.0f;
            break;
        case 30: // Lambda Mu: Downward shift for warmth and depth
            shiftInterval = -1.0f;
            break;
        default:
            break;
    }

    // Use your existing performFormantCorrectedPitchShifting method to handle the shift
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
// Implement getter for last input samples
std::vector<float> AncientVoicesAudioProcessor::getLastInputSamples()
{
    std::lock_guard<std::mutex> lock(inputMutex);
    return lastInputSamples;
}

// Implement getter for last output samples
std::vector<float> AncientVoicesAudioProcessor::getLastOutputSamples()
{
    std::lock_guard<std::mutex> lock(outputMutex);
    return lastOutputSamples;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AncientVoicesAudioProcessor();
}
