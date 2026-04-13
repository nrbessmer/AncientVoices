
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
    // Ancient Voices — 50 appended presets (AV_00..AV_49)
    //
    // Redesigned for authentic Sumerian / Egyptian / Mesopotamian
    // character, with aggressive removal of modern-producer artifacts:
    //   - Drone:   NO flanger, NO distortion, NO doubler, reverb-only
    //              (pure sustained ziggurat/temple tone)
    //   - Shimmer: NO flanger, minimal chorus (pitch + reverb only for
    //              sistrum-like air; stops the metallic-grain artifact)
    //   - Dark:    NO flanger, NO distortion at deep pitch (removes
    //              the low-end buzz from stacked modulation)
    //   - Chant:   Dry sanctuary voice with subtle instability wobble
    //   - Choir:   Stacked fifths/octaves via harmonizer, wide stereo
    //   - Ritual:  Temple echoes (delay + big reverb), no modulation
    //   - Breath:  Intimate, dry, minimal processing
    //
    // Era tag drives filterCutoff (via Presets.h):
    //   Sumerian 4500 / Egyptian 5000 / Akkadian-Babylonian 4200 /
    //   generic Mesopotamian 4800.
    // ============================================================
    for (int i = 0; i < AncientVoicesPresets::kAvPresetCount; ++i)
    {
        const auto& preset = AncientVoicesPresets::getPreset(i);
        const SoundProfile key = static_cast<SoundProfile>(
            static_cast<int>(SoundProfile::AV_00) + i);

        ProfileSettings s{};
        s.filterCutoff             = AncientVoicesPresets::getFilterCutoffForEra(preset.era);
        s.compressionRatio         = 2.0f;
        s.instabilityDepth         = 0.0f;
        s.vocalDoublerDetuneAmount = 0.0f;
        s.saturationDrive          = 0.0f;
        s.saturationMix            = 0.0f;
        s.saturationTone           = 0.0f;
        s.glitchAmount             = 0.0f;
        s.stopAndGoInterval        = 0;
        s.stopAndGoSilenceRatio    = 0.0f;
        s.stutterRepeatLength      = 0;
        s.reverseInterval          = 0;

        const int sub = i % 8;
        const float v = static_cast<float>(sub);

        // Era micro-adjustment factor for pitch/reverb variety
        const bool isEgyptian = (preset.era == AncientVoicesPresets::Era::Egyptian);
        const bool isSumerian = (preset.era == AncientVoicesPresets::Era::Sumerian);
        const bool isAkkadian = (preset.era == AncientVoicesPresets::Era::AkkadianBabylonian);

        switch (preset.category)
        {
            case AncientVoicesPresets::Category::Chant:
                // Sanctuary voice: subtle pitch, dry-ish reverb, a touch of
                // natural wobble (instability) for authentic chant waver.
                s.pitchShiftAmount     = (isEgyptian ? 1.0f : -1.0f) + v * 0.4f;  // era-biased
                s.harmonizerAmount     = 0.15f + (sub % 3) * 0.05f;               // 0.15..0.25 (modest)
                s.reverbAmount         = 0.55f + (sub % 3) * 0.05f;               // 0.55..0.65
                s.delayAmount          = 0.0f;
                s.chorusDepth          = 0.08f;
                s.flangerRate          = 0.0f;
                s.distortionAmount     = 0.0f;
                s.instabilityDepth     = 0.03f;                                   // tiny pitch waver
                s.vocalDoublerDelayMs  = 0.0f;
                s.spacedOutDelayTimeMs = 0.0f;
                s.spacedOutPanAmount   = 0.0f;
                s.outputGain           = 0.95f;
                break;

            case AncientVoicesPresets::Category::Drone:
                // Pure ziggurat/temple sustained tone.
                // REVERB ONLY. Zero modulation. Zero distortion. Zero doubler.
                s.pitchShiftAmount     = -4.0f - v * 1.0f;                        // -4..-11
                s.harmonizerAmount     = 0.0f;
                s.reverbAmount         = 0.85f + (sub % 2) * 0.05f;               // 0.85/0.90 (deep)
                s.delayAmount          = 0.0f;
                s.chorusDepth          = 0.0f;                                    // was 0.2 — artifact
                s.flangerRate          = 0.0f;                                    // was 0.05 — artifact
                s.distortionAmount     = 0.0f;                                    // was 0.1 — artifact
                s.vocalDoublerDelayMs  = 0.0f;                                    // was 25 — artifact
                s.spacedOutDelayTimeMs = 0.0f;
                s.spacedOutPanAmount   = 0.0f;
                s.outputGain           = 0.9f;
                break;

            case AncientVoicesPresets::Category::Choir:
                // Stacked voices in temple: harmonizer + wide stereo + medium reverb.
                s.pitchShiftAmount     = (isSumerian ? -1.0f : 0.5f) + v * 0.3f;
                s.harmonizerAmount     = 0.55f + (sub % 3) * 0.08f;               // 0.55..0.71
                s.reverbAmount         = 0.72f;
                s.delayAmount          = 0.0f;
                s.chorusDepth          = 0.18f + (sub % 2) * 0.04f;               // 0.18/0.22
                s.flangerRate          = 0.0f;                                    // was 0.05
                s.distortionAmount     = 0.0f;
                s.vocalDoublerDelayMs  = 18.0f + (sub % 3) * 4.0f;                // 18..26
                s.spacedOutDelayTimeMs = 35.0f + (sub % 3) * 8.0f;                // 35..51 (stereo width)
                s.spacedOutPanAmount   = 0.55f + (sub % 3) * 0.08f;               // 0.55..0.71
                s.outputGain           = 0.9f;
                break;

            case AncientVoicesPresets::Category::Ritual:
                // Processional drama: temple delay echoes + big reverb.
                s.pitchShiftAmount     = -3.0f + v * 0.75f;                       // -3..+2.25
                s.harmonizerAmount     = 0.25f;
                s.reverbAmount         = 0.78f;
                s.delayAmount          = 0.18f + (sub % 3) * 0.03f;               // 0.18..0.24
                s.chorusDepth          = 0.1f;
                s.flangerRate          = 0.0f;                                    // was 0.1
                s.distortionAmount     = 0.0f;                                    // was 0.15
                s.vocalDoublerDelayMs  = 12.0f;
                s.spacedOutDelayTimeMs = 25.0f;
                s.spacedOutPanAmount   = 0.35f;
                s.outputGain           = 0.9f;
                break;

            case AncientVoicesPresets::Category::Breath:
                // Scribe's whisper, desert reed — intimate, nearly dry.
                s.pitchShiftAmount     = -0.5f + (sub % 3) * 0.5f;                // -0.5..+0.5
                s.harmonizerAmount     = 0.05f;
                s.reverbAmount         = 0.25f + (sub % 2) * 0.08f;               // 0.25/0.33
                s.delayAmount          = 0.0f;
                s.chorusDepth          = 0.05f;
                s.flangerRate          = 0.0f;
                s.distortionAmount     = 0.0f;
                s.vocalDoublerDelayMs  = 0.0f;
                s.spacedOutDelayTimeMs = 0.0f;
                s.spacedOutPanAmount   = 0.0f;
                s.outputGain           = 0.95f;
                break;

            case AncientVoicesPresets::Category::Shimmer:
                // Sistrum / lapis sparkle: HIGH PITCH + REVERB only.
                // Removed flanger + reduced chorus drastically — these were
                // stacking with the pitch-shift grain to produce the static.
                s.pitchShiftAmount     = 5.0f + (sub % 6) * 1.0f;                 // +5..+10 (was +7..+12)
                s.harmonizerAmount     = 0.15f;                                   // was 0.3
                s.reverbAmount         = 0.75f;                                   // was 0.65 (more air)
                s.delayAmount          = 0.0f;                                    // was 0.08
                s.chorusDepth          = 0.05f;                                   // was 0.25 — artifact
                s.flangerRate          = 0.0f;                                    // was 0.08 — artifact
                s.distortionAmount     = 0.0f;
                s.vocalDoublerDelayMs  = 0.0f;                                    // was 10
                s.spacedOutDelayTimeMs = 15.0f;                                   // was 20
                s.spacedOutPanAmount   = 0.3f;                                    // was 0.4
                s.outputGain           = 0.95f;
                break;

            case AncientVoicesPresets::Category::Dark:
                // Kur / tomb descent: deep pitch + long reverb, nothing else.
                s.pitchShiftAmount     = -6.0f - (sub % 6) * 1.0f;                // -6..-11 (was -7..-12)
                s.harmonizerAmount     = 0.1f;                                    // was 0.2
                s.reverbAmount         = 0.9f;                                    // was 0.8
                s.delayAmount          = 0.12f;
                s.chorusDepth          = 0.05f;                                   // was 0.15
                s.flangerRate          = 0.0f;                                    // was 0.05
                s.distortionAmount     = 0.0f;                                    // was 0.18 — low-end buzz
                s.vocalDoublerDelayMs  = 0.0f;                                    // was 30
                s.spacedOutDelayTimeMs = 0.0f;                                    // was 10
                s.spacedOutPanAmount   = 0.15f;
                s.outputGain           = 0.85f;
                // Akkadian Dark presets (Lamashtu, Nergal's Gate, Pazuzu) get
                // slightly darker cutoff — era tag already handles this via 4200Hz.
                if (isAkkadian) s.reverbAmount = 0.95f;
                break;
        }

        profiles[key] = s;
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

               // Combine preset pitch with user-knob pitch offset, clamp to ±24st
               float pitchShiftAmount = currentProfile.pitchShiftAmount;
               if (userPitch != nullptr)
                   pitchShiftAmount += userPitch->load();
               pitchShiftAmount = juce::jlimit(-24.0f, 24.0f, pitchShiftAmount);
               //DBG("Pitch Shift Amount (preset+user, clamped): " << pitchShiftAmount);

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
