#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================================
//  PARAMETER LAYOUT
// =============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
AncientVoicesProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // Voice character
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "vowel_pos",     "Vowel Position",   0.f,   4.f,   0.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "vowel_open",    "Openness",         0.5f,  1.5f,  1.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "gender",        "Gender",           0.7f,  1.4f,  1.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "breathe",       "Breathe",          0.f,   1.f,   0.1f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "roughness",     "Roughness",        0.f,   1.f,   0.05f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "formant_shift", "Formant Shift",   -12.f,  12.f,  0.f));

    // Era & Choir
    p.push_back(std::make_unique<juce::AudioParameterInt>(
        "era",           "Era",              0, 3, 0));
    p.push_back(std::make_unique<juce::AudioParameterInt>(
        "num_voices",    "Num Voices",       1, 8, 4));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "spread",        "Spread",           0.f,  1.f,  0.6f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "drift",         "Drift",            0.f,  1.f,  0.3f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "chant",         "Chant",            0.f,  1.f,  0.f));
    p.push_back(std::make_unique<juce::AudioParameterInt>(
        "interval",      "Interval",         0, 6, 0));

    // Pitch snap
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "snap_amount",   "Snap Amount",      0.f,  1.f,  0.f));
    p.push_back(std::make_unique<juce::AudioParameterInt>(
        "snap_scale",    "Snap Scale",       0, 7, 0));

    // Wet/dry
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix",           "Mix",              0.f,  1.f,  0.8f));

    // Cave reverb (maps to juce::dsp::Reverb::Parameters)
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "cave_mix",      "Cave Mix",         0.f,  1.f,   0.3f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "cave_size",     "Cave Size",        0.f,  1.f,   0.6f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "cave_damp",     "Cave Damp",        0.f,  1.f,   0.4f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "cave_echo",     "Cave Echo ms",     0.f,  100.f, 20.f));  // kept for preset compat, unused in JUCE reverb

    // Shimmer (RubberBand pitch shift)
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "shimmer_mix",   "Shimmer Mix",      0.f,  1.f,  0.15f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "shimmer_amt",   "Shimmer Amount",   0.f,  1.f,  0.5f));

    // Envelope (UI display)
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "env_attack",    "Attack",   0.001f, 2.f, 0.01f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "env_decay",     "Decay",    0.001f, 2.f, 0.1f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "env_sustain",   "Sustain",  0.f,    1.f, 0.9f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "env_release",   "Release",  0.001f, 4.f, 0.5f));

    // Master
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "master_vol",    "Volume",  0.f,   1.f,   1.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "master_gain",   "Gain dB", -24.f, 24.f,  0.f));

    return { p.begin(), p.end() };
}

// =============================================================================
//  CONSTRUCTOR / DESTRUCTOR
// =============================================================================
AncientVoicesProcessor::AncientVoicesProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "AncientVoices", createParams())
{
    for (int i = 0; i < kMaxCopies; ++i)
        copyPan_[i] = 0.f;

    // WaveShaper: tanh soft clip, identical to MisterMultiVox's distortion
    waveshaper_.functionToUse = [](float x) { return std::tanh(x); };
}

AncientVoicesProcessor::~AncientVoicesProcessor()
{
    shimmerRB_.reset();
}

// =============================================================================
//  BUSES LAYOUT
// =============================================================================
bool AncientVoicesProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return (in == juce::AudioChannelSet::stereo() ||
            in == juce::AudioChannelSet::mono());
}

// =============================================================================
//  PREPARE TO PLAY
// =============================================================================
void AncientVoicesProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr_           = sampleRate;
    maxBlockSize_ = samplesPerBlock;

    // Build ProcessSpec once — same pattern MisterMultiVox uses
    spec_.sampleRate       = sampleRate;
    spec_.maximumBlockSize = uint32(samplesPerBlock);
    spec_.numChannels      = uint32(getTotalNumOutputChannels());

    // FormantFilter copies
    for (auto& f : copies_)
        f.prepare(sampleRate);

    // JUCE DSP modules — prepared exactly as MisterMultiVox does
    chorus_.prepare(spec_);
    chorus_.setRate(0.5f);
    chorus_.setDepth(0.3f);
    chorus_.setCentreDelay(8.0f);
    chorus_.setFeedback(0.0f);
    chorus_.setMix(0.5f);

    waveshaper_.prepare(spec_);

    reverb_.prepare(spec_);
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = 0.6f;
        rp.damping    = 0.4f;
        rp.wetLevel   = 0.3f;
        rp.dryLevel   = 0.7f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb_.setParameters(rp);
    }

    limiter_.prepare(spec_);
    limiter_.setThreshold(-1.0f);   // -1 dBFS ceiling, same as MisterMultiVox
    limiter_.setRelease(50.0f);

    // HumanizeEngine — FIX: was never triggered in v2, voices were always silent
    humanizer_.prepare(sampleRate);
    for (int i = 0; i < kMaxCopies; ++i)
        humanizer_.triggerVoice(i);

    // Pitch tracker
    pitchTracker_.prepare(sampleRate, samplesPerBlock);
    pitchBlockCounter_ = 0;
    trackedPitchSmooth_.reset(sampleRate, 0.1);
    trackedPitchSmooth_.setCurrentAndTargetValue(220.f);
    pitchQuantizer_.setSnapAmount(0.f);
    pitchQuantizer_.setScale(QuantizeScale::Chromatic);

    // RubberBand shimmer — initialise now
    initShimmerRB(sampleRate, getTotalNumOutputChannels());

    // Heap work buffers — FIX: was float outL[2048] on stack (crash if block>2048)
    workBuf_.setSize   (2, samplesPerBlock, false, true, false);
    shimmerBuf_.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, true, false);
    dryBuf_.setSize    (getTotalNumOutputChannels(), samplesPerBlock, false, true, false);

    // Scope
    {
        const juce::ScopedLock sl(scopeLock_);
        scopeBuf_.setSize(1, samplesPerBlock, false, true, false);
        scopeBuf_.clear();
        scopeReady_.store(false);
    }

    updateCacheFromApvts();
    updateModules();
}

void AncientVoicesProcessor::releaseResources()
{
    shimmerRB_.reset();
}

// =============================================================================
//  INIT RUBBERBAND SHIMMER
//  Same initialisation pattern as MisterMultiVox's applyRubberBandPitchShift
// =============================================================================
void AncientVoicesProcessor::initShimmerRB(double sampleRate, int numChannels)
{
    if (shimmerRB_ && shimmerLastSR_ == sampleRate && shimmerLastChannels_ == numChannels)
        return;

    shimmerRB_ = std::make_unique<RubberBand::RubberBandStretcher>(
        static_cast<size_t>(sampleRate),
        size_t(numChannels),
        RubberBand::RubberBandStretcher::OptionProcessRealTime |
        RubberBand::RubberBandStretcher::OptionPitchHighQuality
    );
    shimmerRB_->setPitchScale(2.0f);   // default: 1 octave up
    shimmerLastSR_       = sampleRate;
    shimmerLastChannels_ = numChannels;
}

// =============================================================================
//  APPLY SHIMMER  (RubberBand pitch shift, wet/dry blended)
//  Follows the exact same push/retrieve pattern as MisterMultiVox
//  applyRubberBandPitchShift() — proven stable.
// =============================================================================
void AncientVoicesProcessor::applyShimmer(juce::AudioBuffer<float>& buffer)
{
    if (cache_.shimmerMix < 0.001f) return;
    if (!shimmerRB_) return;

    const int  numSamples  = buffer.getNumSamples();
    const int  numChannels = buffer.getNumChannels();

    // Set pitch: shimmerAmt 0=perfect fifth (1.5x), 1=octave (2.0x)
    // Matches MisterMultiVox harmonizer interval choices
    float pitchScale = 1.5f + cache_.shimmerAmt * 0.5f;
    shimmerRB_->setPitchScale(double(pitchScale));

    // Push input — same as MMV: process() then retrieve()
    shimmerRB_->process(buffer.getArrayOfReadPointers(), size_t(numSamples), false);

    int available = shimmerRB_->available();
    if (available <= 0) return;

    available = juce::jmin(available, numSamples);

    shimmerBuf_.setSize(numChannels, available, false, false, true);
    shimmerRB_->retrieve(shimmerBuf_.getArrayOfWritePointers(), size_t(available));

    // Blend shimmer into buffer (wet/dry)
    const float wet = cache_.shimmerMix;
    const float dry = 1.f - wet;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* shim = shimmerBuf_.getReadPointer(ch);
        float*       dest = buffer.getWritePointer(ch);
        for (int s = 0; s < available; ++s)
            dest[s] = dest[s] * dry + shim[s] * wet;
    }
}

// =============================================================================
//  PROCESS BLOCK
// =============================================================================
void AncientVoicesProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numInCh    = getTotalNumInputChannels();
    const int numOutCh   = getTotalNumOutputChannels();

    if (numSamples <= 0 || numOutCh < 2) return;

    // Resize work buffer if Logic passes a larger block than prepared for
    if (numSamples > maxBlockSize_)
    {
        workBuf_.setSize   (2, numSamples, false, true, true);
        shimmerBuf_.setSize(numOutCh, numSamples, false, true, true);
        dryBuf_.setSize    (numOutCh, numSamples, false, true, true);
        maxBlockSize_ = numSamples;
    }

    // ── Save dry input BEFORE any processing (for wet/dry blend at end) ───
    dryBuf_.makeCopyOf(buffer);

    // ── Update parameters once per block ─────────────────────────────────
    updateCacheFromApvts();
    updateModules();

    // ── Pitch tracker (every 8 blocks — expensive YIN) ────────────────────
    if (++pitchBlockCounter_ >= 8)
    {
        pitchBlockCounter_ = 0;
        pitchTracker_.process(buffer.getReadPointer(0), numSamples);
        if (pitchTracker_.isVoiced())
        {
            float q = pitchQuantizer_.process(pitchTracker_.getFrequency());
            trackedPitchSmooth_.setTargetValue(q);
        }
    }
    trackedPitchSmooth_.skip(numSamples);

    // ── Build mono input sum ──────────────────────────────────────────────
    const int n = juce::jlimit(1, kMaxCopies, cache_.numCopies);

    // Equal-power normalisation: keeps summed output at unity regardless of N
    const float copyGain = 1.f / std::sqrt(float(n));

    // Clear work buffers (heap allocated — safe for any block size)
    workBuf_.clear();
    float* wL = workBuf_.getWritePointer(0);
    float* wR = workBuf_.getWritePointer(1);

    // ── Per-copy processing ───────────────────────────────────────────────
    for (int ci = 0; ci < n; ++ci)
    {
        const float pan    = copyPan_[ci];
        const float panRad = (pan + 1.f) * 0.5f * juce::MathConstants<float>::halfPi;
        const float panL   = std::cos(panRad);
        const float panR   = std::sin(panRad);

        for (int s = 0; s < numSamples; ++s)
        {
            // Mono input: average all input channels
            float in = 0.f;
            for (int ch = 0; ch < numInCh; ++ch)
                in += buffer.getReadPointer(ch)[s];
            if (numInCh > 1) in *= 0.5f;

            // Sanity check — kill NaN/inf before it enters the filter
            if (!std::isfinite(in)) in = 0.f;

            // Humanizer amp + pitch modulation
            auto hum = humanizer_.tick(ci);
            float amp = hum.ampOffset;

            // Apply pitch vibrato from humanizer cents offset
            // (stored in FormantFilter via formantShift — we offset per-copy)
            // Small per-copy pitch deviation creates natural choir thickness
            copies_[ci].setFormantShift(cache_.formantShift +
                                         hum.pitchOffsetCents * 0.08f);  // scale cents to formant semitones

            // FormantFilter — the core vocal transformation
            float formanted = copies_[ci].tick(in);

            // Accumulate to stereo work buffer
            float sample = formanted * amp * copyGain;
            wL[s] += sample * panL;
            wR[s] += sample * panR;
        }
    }

    // ── Copy work buffer into main buffer for JUCE DSP chain ──────────────
    float* outL = buffer.getWritePointer(0);
    float* outR = (numOutCh > 1) ? buffer.getWritePointer(1) : buffer.getWritePointer(0);
    juce::FloatVectorOperations::copy(outL, wL, numSamples);
    juce::FloatVectorOperations::copy(outR, wR, numSamples);

    // ── Chorus (JUCE module, same as MisterMultiVox applyChorusEffect) ────
    {
        juce::dsp::AudioBlock<float>           block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        chorus_.process(ctx);
    }

    // ── WaveShaper saturation (tanh, same as MisterMultiVox distortion) ───
    // Only apply if drive > 0 to avoid touching silent signal
    if (cache_.satDrive > 0.01f)
    {
        // Pre-gain into tanh, then compensate — exactly as MisterMultiVox does
        const float preGain  = 1.f + cache_.satDrive * 3.f;
        const float postGain = 1.f / preGain;  // unity compensation

        for (int ch = 0; ch < numOutCh; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int s = 0; s < numSamples; ++s)
                data[s] = std::tanh(data[s] * preGain) * postGain;
        }
    }

    // ── RubberBand shimmer (octave-up pitch shift, wet/dry blended) ───────
    applyShimmer(buffer);

    // ── Reverb (JUCE module, same as MisterMultiVox applyReverbEffect) ────
    {
        juce::dsp::AudioBlock<float>           block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        reverb_.process(ctx);
    }

    // ── Limiter (JUCE module, same as MisterMultiVox applyLimiter) ────────
    {
        juce::dsp::AudioBlock<float>           block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        limiter_.process(ctx);
    }

    // ── Wet/dry blend + master gain ───────────────────────────────────────
    // dryBuf_ was saved at the top of processBlock before any DSP ran.
    // This is the correct approach — same concept as MisterMultiVox's
    // inputBufferBeforeProcessing that was made at the top of its processBlock.
    const float wetMix  = cache_.mix;
    const float dryMix  = 1.f - wetMix;
    const float gainLin = cache_.masterVol *
                          std::pow(10.f, cache_.masterGain / 20.f);

    for (int ch = 0; ch < numOutCh; ++ch)
    {
        const float* dry = dryBuf_.getReadPointer(juce::jmin(ch, dryBuf_.getNumChannels() - 1));
        float*       out = buffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s)
            out[s] = (dry[s] * dryMix + out[s] * wetMix) * gainLin;
    }

    // ── Scope capture ─────────────────────────────────────────────────────
    {
        const juce::ScopedLock sl(scopeLock_);
        scopeBuf_.setSize(1, numSamples, false, false, true);
        float* dest  = scopeBuf_.getWritePointer(0);
        const float* sL = buffer.getReadPointer(0);
        const float* sR = (numOutCh > 1) ? buffer.getReadPointer(1) : sL;
        for (int s = 0; s < numSamples; ++s)
            dest[s] = (sL[s] + sR[s]) * 0.5f;
        scopeReady_.store(true, std::memory_order_relaxed);
    }
}

// =============================================================================
//  UPDATE CACHE FROM APVTS
// =============================================================================
void AncientVoicesProcessor::updateCacheFromApvts()
{
    auto get  = [&](const char* id) -> float {
        auto* p = apvts.getRawParameterValue(id);
        return p ? p->load() : 0.f;
    };
    auto geti = [&](const char* id) -> int {
        auto* p = apvts.getRawParameterValue(id);
        return p ? int(p->load()) : 0;
    };

    cache_.vowelPos     = get("vowel_pos");
    cache_.openness     = get("vowel_open");
    cache_.gender       = get("gender");
    cache_.formantShift = get("formant_shift");
    cache_.numCopies    = juce::jlimit(1, kMaxCopies, geti("num_voices"));
    cache_.spread       = get("spread");
    cache_.drift        = get("drift");
    cache_.chant        = get("chant");
    cache_.mix          = get("mix");
    cache_.eraIdx       = geti("era");
    cache_.snapAmount   = get("snap_amount");
    cache_.snapScale    = geti("snap_scale");

    // Map cave_* params to juce::dsp::Reverb equivalents
    cache_.reverbRoom   = get("cave_size");
    cache_.reverbDamp   = get("cave_damp");
    cache_.reverbWet    = get("cave_mix");
    cache_.reverbWidth  = 1.0f;

    cache_.shimmerMix   = get("shimmer_mix");
    cache_.shimmerAmt   = get("shimmer_amt");

    // Saturation drive: derived from inverse openness (closed=more harmonic warmth)
    // Same logic as MisterMultiVox profile distortionAmount
    cache_.satDrive     = juce::jlimit(0.f, 0.4f, (1.5f - cache_.openness) * 0.2f);

    cache_.masterVol    = get("master_vol");
    cache_.masterGain   = get("master_gain");
}

// =============================================================================
//  UPDATE MODULES
//  Push cached parameters into DSP objects once per block.
//  No allocation. Same pattern as MisterMultiVox processBlock parameter read.
// =============================================================================
void AncientVoicesProcessor::updateModules()
{
    const int        n      = juce::jlimit(1, kMaxCopies, cache_.numCopies);
    const AncientEra era    = AncientEra(juce::jlimit(0, 3, cache_.eraIdx));
    const float      spread = cache_.spread;

    // FormantFilter copies — push vocal parameters
    for (int i = 0; i < kMaxCopies; ++i)
    {
        copies_[i].setVowelPos    (cache_.vowelPos);
        copies_[i].setEra         (era);
        copies_[i].setOpenness    (cache_.openness);
        copies_[i].setGender      (cache_.gender);
        copies_[i].setFormantShift(cache_.formantShift);  // base shift; per-copy vibrato added in processBlock

        // Equal-power stereo spread
        float t     = (n > 1) ? float(i) / float(n - 1) : 0.5f;
        copyPan_[i] = (t * 2.f - 1.f) * spread;
    }

    // Humanizer
    humanizer_.setDrift(cache_.drift);
    humanizer_.setChant(cache_.chant);

    // Chorus depth scales with drift — same relationship as MisterMultiVox
    chorus_.setDepth(juce::jlimit(0.f, 0.99f, cache_.drift * 0.6f));
    chorus_.setRate(0.4f + cache_.drift * 0.4f);   // 0.4–0.8 Hz

    // Reverb — update JUCE Reverb parameters (same as MMV applyReverbEffect)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = juce::jlimit(0.f, 1.f, cache_.reverbRoom);
        rp.damping    = juce::jlimit(0.f, 1.f, cache_.reverbDamp);
        rp.wetLevel   = juce::jlimit(0.f, 1.f, cache_.reverbWet);
        rp.dryLevel   = juce::jlimit(0.f, 1.f, 1.f - cache_.reverbWet);
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb_.setParameters(rp);
    }

    // Pitch quantizer
    pitchQuantizer_.setSnapAmount(cache_.snapAmount);
    pitchQuantizer_.setScale(QuantizeScale(juce::jlimit(0, 7, cache_.snapScale)));
}

// =============================================================================
//  STATE SAVE / RESTORE
// =============================================================================
void AncientVoicesProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    // Identical to Ancient Synth — XML binary, proven to work in Logic
    copyXmlToBinary(*apvts.copyState().createXml(), dest);
}

void AncientVoicesProcessor::setStateInformation(const void* data, int size)
{
    // Identical to Ancient Synth
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// =============================================================================
//  BOILERPLATE
// =============================================================================
juce::AudioProcessorEditor* AncientVoicesProcessor::createEditor()
{
    return new AncientVoicesEditor(*this);
}

const juce::String AncientVoicesProcessor::getName()  const { return "Ancient Voices"; }
bool AncientVoicesProcessor::acceptsMidi()  const { return false; }
bool AncientVoicesProcessor::producesMidi() const { return false; }
bool AncientVoicesProcessor::isMidiEffect() const { return false; }
double AncientVoicesProcessor::getTailLengthSeconds() const { return 4.0; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AncientVoicesProcessor();
}
