#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace AVC;

// ─────────────────────────────────────────────────────────────────────────────
//  PARAMETER LAYOUT
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
AncientVoicesProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto f = [&](const char* id, const char* name, float lo, float hi, float def) {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(id, name, lo, hi, def));
    };
    auto i = [&](const char* id, const char* name, int lo, int hi, int def) {
        p.push_back(std::make_unique<juce::AudioParameterInt>(id, name, lo, hi, def));
    };

    f(ParamID::kVowelPos,     "Vowel",         0.f,   4.f,   Defaults::kVowelPos);
    f(ParamID::kVowelOpen,    "Openness",      0.5f,  1.5f,  Defaults::kVowelOpen);
    f(ParamID::kGender,       "Gender",        0.7f,  1.4f,  Defaults::kGender);
    f(ParamID::kBreathe,      "Breathe",       0.f,   1.f,   Defaults::kBreathe);
    f(ParamID::kRoughness,    "Roughness",     0.f,   1.f,   Defaults::kRoughness);
    f(ParamID::kFormantShift, "Formant Shift", -12.f, 12.f,  Defaults::kFormantShift);
    f(ParamID::kFormantDepth, "Depth",         0.f,   1.f,   Defaults::kFormantDepth);
    i(ParamID::kEra,          "Era",           0, 3,  Defaults::kEra);
    i(ParamID::kVoiceMode,    "Mode",          0, 3,  Defaults::kVoiceMode);
    i(ParamID::kNumVoices,    "Voices",        1, 8,  Defaults::kNumVoices);
    f(ParamID::kSpread,       "Spread",        0.f,   1.f,   Defaults::kSpread);
    f(ParamID::kDrift,        "Drift",         0.f,   1.f,   Defaults::kDrift);
    f(ParamID::kChant,        "Chant",         0.f,   1.f,   Defaults::kChant);
    i(ParamID::kInterval,     "Interval",      0, 6,  Defaults::kInterval);
    f(ParamID::kMix,          "Mix",           0.f,   1.f,   Defaults::kMix);
    f(ParamID::kCaveMix,      "Cave Mix",      0.f,   1.f,   Defaults::kCaveMix);
    f(ParamID::kCaveSize,     "Cave Size",     0.f,   1.f,   Defaults::kCaveSize);
    f(ParamID::kCaveDamp,     "Cave Damp",     0.f,   1.f,   Defaults::kCaveDamp);
    f(ParamID::kCaveEcho,     "Cave Echo ms",  0.f,   100.f, Defaults::kCaveEchoMs);
    f(ParamID::kShimmerMix,   "Shimmer",       0.f,   1.f,   Defaults::kShimmerMix);
    f(ParamID::kShimmerAmt,   "Shimmer Amt",   0.f,   1.f,   Defaults::kShimmerAmt);
    f(ParamID::kEnvAttack,    "Attack",        0.001f,2.f,   0.01f);
    f(ParamID::kEnvDecay,     "Decay",         0.001f,2.f,   0.1f);
    f(ParamID::kEnvSustain,   "Sustain",       0.f,   1.f,   0.9f);
    f(ParamID::kEnvRelease,   "Release",       0.001f,4.f,   0.5f);
    f(ParamID::kMasterVol,    "Volume",        0.f,   1.f,   Defaults::kMasterVol);
    f(ParamID::kMasterGain,   "Gain dB",       -24.f, 24.f,  Defaults::kMasterGainDb);

    return { p.begin(), p.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
//  CONSTRUCTOR / DESTRUCTOR
// ─────────────────────────────────────────────────────────────────────────────
AncientVoicesProcessor::AncientVoicesProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "AncientVoices", createParams())
{}

AncientVoicesProcessor::~AncientVoicesProcessor() { shimmerRB_.reset(); }

// ─────────────────────────────────────────────────────────────────────────────
//  PREPARE
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::prepareToPlay(double sr, int blockSize)
{
    sr_       = sr;
    maxBlock_ = blockSize;

    spec_ = { sr, juce::uint32(blockSize), juce::uint32(getTotalNumOutputChannels()) };

    for (auto& c : copies_) c.prepare(sr);
    humanizer_.prepare(sr);
    for (int i = 0; i < kMaxCopies; ++i) humanizer_.triggerVoice(i);
    reverb_.prepare(sr);

    chorus_.prepare(spec_);
    chorus_.setFeedback(0.f);

    limiter_.prepare(spec_);
    limiter_.setThreshold(kLimiterThresholdDb);
    limiter_.setRelease  (kLimiterReleaseMs);

    // Fixed per-copy micro-detune (deterministic seed)
    { std::mt19937 rng(77777);
      std::uniform_real_distribution<float> d(-kMicroDetuneRange, kMicroDetuneRange);
      for (auto& v : copyDetune_) v = d(rng); }

    // Work buffers — allocated here, never in processBlock
    const int ch = getTotalNumOutputChannels();
    workBuf_   .setSize(2,  blockSize, false, true, false);
    monoInBuf_ .setSize(1,  blockSize, false, true, false);
    dryBuf_    .setSize(ch, blockSize, false, true, false);
    shimmerBuf_.setSize(ch, blockSize, false, true, false);

    // Reset scope FIFO — no lock needed, called before audio thread starts
    scopeFifo_.reset();

    initShimmer(sr, ch);
    updateCache();
    updateModules();
}

void AncientVoicesProcessor::releaseResources() { shimmerRB_.reset(); }

// ─────────────────────────────────────────────────────────────────────────────
//  PROCESS BLOCK
//  Each numbered step matches the behavioral contract in PluginProcessor.h.
//  Dry signal touches output ONLY at step 11.
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;

    const int ns = buffer.getNumSamples();
    const int ni = getTotalNumInputChannels();
    const int no = getTotalNumOutputChannels();
    if (ns <= 0 || no < 2) return;

    // Grow work buffers only if block size increased
    if (ns > maxBlock_) {
        workBuf_   .setSize(2,  ns, false, true, true);
        monoInBuf_ .setSize(1,  ns, false, true, true);
        dryBuf_    .setSize(no, ns, false, true, true);
        shimmerBuf_.setSize(no, ns, false, true, true);
        maxBlock_ = ns;
    }

    // ── 1. Save dry ───────────────────────────────────────────────────────
    dryBuf_.makeCopyOf(buffer);

    // ── Throttled param update ────────────────────────────────────────────
    if (stateRestorePending_.exchange(false) || ++paramCounter_ >= kParamUpdateBlocks) {
        paramCounter_ = 0;
        updateCache();
        updateModules();
    }

    // ── 2. Mix to mono ────────────────────────────────────────────────────
    float* mono = monoInBuf_.getWritePointer(0);
    juce::FloatVectorOperations::copy(mono, buffer.getReadPointer(0), ns);
    if (ni > 1) {
        juce::FloatVectorOperations::add     (mono, buffer.getReadPointer(1), ns);
        juce::FloatVectorOperations::multiply(mono, 0.5f, ns);
    }

    // ── 3. Breathe noise ──────────────────────────────────────────────────
    if (cache_.breathe > 0.001f) {
        for (int s = 0; s < ns; ++s) {
            const float w = dist_(rng_);
            breatheState_ = breatheState_ * 0.99f + w * 0.01f;
            mono[s] += (w + breatheState_ * 3.f) * 0.25f * cache_.breathe * kBreatheNoiseLevel;
        }
    }

    // ── 4. N × FormantFilter → stereo workBuf (CONTRACT: zero dry) ────────
    const int   n        = juce::jlimit(1, kMaxCopies, cache_.numCopies);
    const float copyGain = 1.f / float(n);

    workBuf_.clear();
    float* wL = workBuf_.getWritePointer(0);
    float* wR = workBuf_.getWritePointer(1);

    for (int ci = 0; ci < n; ++ci) {
        const float pan    = copyPan_[ci];
        const float panRad = (pan + 1.f) * 0.5f * juce::MathConstants<float>::halfPi;
        const float panL   = std::cos(panRad);
        const float panR   = std::sin(panRad);

        const float interval   = 12.f * std::log2(juce::jmax(0.001f,
                                     getVoiceRatio(ChoirInterval(cache_.interval), ci)));
        const float roughScale = 0.02f + cache_.roughness * 0.15f;

        for (int s = 0; s < ns; ++s) {
            float in = mono[s];
            if (!std::isfinite(in)) in = 0.f;

            const auto hum = humanizer_.tick(ci);
            copies_[ci].setFormantShift(cache_.formantShift + interval
                                        + copyDetune_[ci]
                                        + hum.pitchOffsetCents * roughScale);

            const float out = copies_[ci].tick(in) * hum.ampOffset * copyGain;
            wL[s] += out * panL;
            wR[s] += out * panR;
        }
    }

    // ── 5. Wet boost ──────────────────────────────────────────────────────
    //  kWetBoost = 1.0f in v3. normGain_ in FormantFilter provides all
    //  level compensation. No additional boost needed here.
    juce::FloatVectorOperations::multiply(wL, kWetBoost, ns);
    juce::FloatVectorOperations::multiply(wR, kWetBoost, ns);

    juce::FloatVectorOperations::copy(buffer.getWritePointer(0), wL, ns);
    juce::FloatVectorOperations::copy(buffer.getWritePointer(1), wR, ns);

    // ── 6. Chorus ─────────────────────────────────────────────────────────
    { juce::dsp::AudioBlock<float> b(buffer);
      chorus_.process(juce::dsp::ProcessContextReplacing<float>(b)); }

    // ── 7. Saturation ─────────────────────────────────────────────────────
    if (cache_.effSat > 0.001f) {
        const float pre  = 1.f + cache_.effSat * 4.f;
        const float post = 1.f / pre;
        for (int ch = 0; ch < no; ++ch) {
            float* d = buffer.getWritePointer(ch);
            for (int s = 0; s < ns; ++s)
                d[s] = std::tanh(d[s] * pre) * post;
        }
    }

    // ── 8. Shimmer ────────────────────────────────────────────────────────
    applyShimmer(buffer);

    // ── 9. Cave reverb ────────────────────────────────────────────────────
    { float* L = buffer.getWritePointer(0);
      float* R = buffer.getWritePointer(1);
      for (int s = 0; s < ns; ++s)
          reverb_.process(L[s], R[s], L[s], R[s]); }

    // ── 10. Limiter ───────────────────────────────────────────────────────
    { juce::dsp::AudioBlock<float> b(buffer);
      limiter_.process(juce::dsp::ProcessContextReplacing<float>(b)); }

    // ── 11. Crossfade — DRY ENTERS HERE ONLY ─────────────────────────────
    {
        const float wet  = cache_.mix;
        const float dry  = 1.f - wet;
        const float gain = cache_.masterVol * std::pow(10.f, cache_.masterGain / 20.f);
        for (int ch = 0; ch < no; ++ch) {
            const float* d = dryBuf_.getReadPointer(juce::jmin(ch, dryBuf_.getNumChannels()-1));
            float*       o = buffer.getWritePointer(ch);
            for (int s = 0; s < ns; ++s)
                o[s] = (o[s] * wet + d[s] * dry) * gain;
        }
    }

    // ── 12. Scope feed — lock-free AbstractFifo write ─────────────────────
    //  No ScopedLock. No atomic flag. No allocation.
    //  Audio thread writes; UI thread reads in consumeScope().
    {
        const float* sL = buffer.getReadPointer(0);
        const float* sR = no > 1 ? buffer.getReadPointer(1) : sL;

        int start1, size1, start2, size2;
        scopeFifo_.prepareToWrite(ns, start1, size1, start2, size2);

        for (int s = 0; s < size1; ++s)
            scopeBuf_[start1 + s] = (sL[s] + sR[s]) * 0.5f;
        for (int s = 0; s < size2; ++s)
            scopeBuf_[start2 + s] = (sL[size1 + s] + sR[size1 + s]) * 0.5f;

        scopeFifo_.finishedWrite(size1 + size2);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PARAM CACHE
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::updateCache()
{
    auto gf = [&](const char* id) {
        auto* p = apvts.getRawParameterValue(id); return p ? p->load() : 0.f; };
    auto gi = [&](const char* id) {
        auto* p = apvts.getRawParameterValue(id); return p ? int(p->load()) : 0; };

    cache_.vowelPos     = gf(ParamID::kVowelPos);
    cache_.openness     = gf(ParamID::kVowelOpen);
    cache_.gender       = gf(ParamID::kGender);
    cache_.breathe      = gf(ParamID::kBreathe);
    cache_.roughness    = gf(ParamID::kRoughness);
    cache_.formantShift = gf(ParamID::kFormantShift);
    cache_.formantDepth = gf(ParamID::kFormantDepth);
    cache_.era          = gi(ParamID::kEra);
    cache_.voiceMode    = gi(ParamID::kVoiceMode);
    cache_.numCopies    = juce::jlimit(1, kMaxCopies, gi(ParamID::kNumVoices));
    cache_.spread       = gf(ParamID::kSpread);
    cache_.drift        = gf(ParamID::kDrift);
    cache_.interval     = gi(ParamID::kInterval);
    cache_.mix          = gf(ParamID::kMix);
    cache_.caveMix      = gf(ParamID::kCaveMix);
    cache_.caveSize     = gf(ParamID::kCaveSize);
    cache_.caveDamp     = gf(ParamID::kCaveDamp);
    cache_.caveEcho     = gf(ParamID::kCaveEcho);
    cache_.shimmerMix   = gf(ParamID::kShimmerMix);
    cache_.shimmerAmt   = gf(ParamID::kShimmerAmt);
    cache_.masterVol    = gf(ParamID::kMasterVol);
    cache_.masterGain   = gf(ParamID::kMasterGain);
    cache_.satDrive     = juce::jlimit(0.f, kSatDriveMax, (1.5f - cache_.openness) * kSatScale);
}

// ─────────────────────────────────────────────────────────────────────────────
//  MODULE UPDATE
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::updateModules()
{
    const auto& mp = kModeParams[juce::jlimit(0, 3, cache_.voiceMode)];
    cache_.effDepth   = juce::jlimit(0.f, 1.f,          cache_.formantDepth * mp.formantDepthMul);
    cache_.effBwMul   = mp.bwMul;
    cache_.effChoMix  = juce::jlimit(0.f, 1.f,          kChorusMix          * mp.chorusMixMul);
    cache_.effSat     = juce::jlimit(0.f, kSatDriveMax, cache_.satDrive     * mp.satDriveMul);
    cache_.effShimmer = juce::jlimit(0.f, 1.f,          cache_.shimmerMix   * mp.shimmerMixMul);

    const int n = juce::jlimit(1, kMaxCopies, cache_.numCopies);
    const AncientEra era = AncientEra(juce::jlimit(0, 3, cache_.era));
    for (int ci = 0; ci < kMaxCopies; ++ci) {
        copies_[ci].setVowelPos (cache_.vowelPos);
        copies_[ci].setEra      (era);
        copies_[ci].setOpenness (cache_.openness);
        copies_[ci].setGender   (cache_.gender);
        copies_[ci].setDepth    (cache_.effDepth);
        copies_[ci].setBwMul    (cache_.effBwMul);
        copyPan_[ci] = (n > 1)
            ? ((float(ci) / float(n-1)) * 2.f - 1.f) * cache_.spread
            : 0.f;
    }

    humanizer_.setDrift(cache_.drift);
    { auto* p = apvts.getRawParameterValue(ParamID::kChant);
      humanizer_.setChant(p ? p->load() : 0.f); }

    chorus_.setMix        (cache_.effChoMix);
    chorus_.setDepth      (juce::jlimit(0.f, 0.99f, cache_.drift * kChorusDepthScale));
    chorus_.setRate       (kChorusRateBase + cache_.drift * kChorusRateScale);
    chorus_.setCentreDelay(kChorusCentreMs);

    reverb_.setMix     (cache_.caveMix);
    reverb_.setSize    (cache_.caveSize);
    reverb_.setDamp    (cache_.caveDamp);
    reverb_.setPreDelay(cache_.caveEcho);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SHIMMER
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::initShimmer(double sr, int ch)
{
    if (shimmerRB_ && shimmerLastSR_ == sr && shimmerLastCh_ == ch) return;
    shimmerRB_ = std::make_unique<RubberBand::RubberBandStretcher>(
        size_t(sr), size_t(ch),
        RubberBand::RubberBandStretcher::OptionProcessRealTime |
        RubberBand::RubberBandStretcher::OptionPitchHighSpeed);
    shimmerRB_->setPitchScale(2.0);
    shimmerLastSR_ = sr;
    shimmerLastCh_ = ch;
}

void AncientVoicesProcessor::applyShimmer(juce::AudioBuffer<float>& buffer)
{
    if (cache_.effShimmer < 0.001f || !shimmerRB_) return;

    const int ns = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();

    const float pitch = kShimmerPitchMin + cache_.shimmerAmt * (kShimmerPitchMax - kShimmerPitchMin);
    shimmerRB_->setPitchScale(double(pitch));
    shimmerRB_->process(buffer.getArrayOfReadPointers(), size_t(ns), false);

    int avail = juce::jmin(shimmerRB_->available(), ns);
    if (avail <= 0) return;

    shimmerBuf_.setSize(ch, avail, false, false, true);
    shimmerRB_->retrieve(shimmerBuf_.getArrayOfWritePointers(), size_t(avail));

    const float wet = cache_.effShimmer;
    const float dry = 1.f - wet;
    for (int c = 0; c < ch; ++c) {
        float*       d = buffer.getWritePointer(c);
        const float* s = shimmerBuf_.getReadPointer(c);
        for (int i = 0; i < avail; ++i)
            d[i] = d[i] * dry + s[i] * wet;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  BOILERPLATE
// ─────────────────────────────────────────────────────────────────────────────
bool AncientVoicesProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void AncientVoicesProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    copyXmlToBinary(*apvts.copyState().createXml(), dest);
}

void AncientVoicesProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        stateRestorePending_.store(true);
    }
}

juce::AudioProcessorEditor* AncientVoicesProcessor::createEditor()
{
    return new AncientVoicesEditor(*this);
}

const juce::String AncientVoicesProcessor::getName() const { return "Ancient Voices"; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AncientVoicesProcessor();
}
