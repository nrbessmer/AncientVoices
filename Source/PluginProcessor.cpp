#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace AVC;

// ─────────────────────────────────────────────────────────────────────────────
//  MEASUREMENT HELPERS — audio thread only, no allocation, no lock
//
//  stereoMidRmsDb: measures (L+R)/2 energy — correct mono-equivalent for a
//    stereo signal. Using channel-0 only under-represents voices panned away
//    from centre (8 voices with spread=0.6 causes ~6 dB false C2 failure).
//
//  bufferPeakLinear: finds the peak absolute value across ALL channels.
//    Used for limiter GR — immune to the JUCE Limiter's lookahead delay.
//    (RMS comparison gives false positive GR when signal transitions loud→quiet
//    because preLimDb measures the current quiet input while postLimDb measures
//    the delayed loud block still flushing through the lookahead buffer.)
// ─────────────────────────────────────────────────────────────────────────────
static float stereoMidRmsDb(const float* L, const float* R, int n) noexcept
{
    float sum = 0.f;
    for (int i = 0; i < n; ++i) {
        const float m = (L[i] + R[i]) * 0.5f;
        sum += m * m;
    }
    const float rms = std::sqrt(sum / float(juce::jmax(1, n)));
    return (rms > 1e-9f && std::isfinite(rms)) ? 20.f * std::log10(rms) : -96.f;
}

static float bufferPeakLinear(const juce::AudioBuffer<float>& buf, int ns) noexcept
{
    float peak = 0.f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch) {
        const float* d = buf.getReadPointer(ch);
        for (int s = 0; s < ns; ++s) peak = juce::jmax(peak, std::abs(d[s]));
    }
    return peak;
}

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

    { std::mt19937 rng(77777);
      std::uniform_real_distribution<float> d(-kMicroDetuneRange, kMicroDetuneRange);
      for (auto& v : copyDetune_) v = d(rng); }

    const int ch = getTotalNumOutputChannels();
    workBuf_   .setSize(2,  blockSize, false, true, false);
    monoInBuf_ .setSize(1,  blockSize, false, true, false);
    dryBuf_    .setSize(ch, blockSize, false, true, false);
    shimmerBuf_.setSize(ch, blockSize, false, true, false);

    scopeFifo_.reset();
    limiterConsecBlocks_ = 0;

    diagState_.sampleRateHz.store(float(sr));
    diagState_.blockSizeSmps.store(float(blockSize));

    initShimmer(sr, ch);
    updateCache();
    updateModules();
}

void AncientVoicesProcessor::releaseResources() { shimmerRB_.reset(); }

// ─────────────────────────────────────────────────────────────────────────────
//  PROCESS BLOCK
//
//  Step 12 runs contract checks C1–C5. All measurements use stereo-mid RMS
//  (L+R)/2 so that panned voices count equally, and peak-based GR for the
//  limiter so the JUCE lookahead delay cannot invert the sign.
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;

    const int ns = buffer.getNumSamples();
    const int ni = getTotalNumInputChannels();
    const int no = getTotalNumOutputChannels();
    if (ns <= 0 || no < 2) return;

    if (ns > maxBlock_) {
        workBuf_   .setSize(2,  ns, false, true, true);
        monoInBuf_ .setSize(1,  ns, false, true, true);
        dryBuf_    .setSize(no, ns, false, true, true);
        shimmerBuf_.setSize(no, ns, false, true, true);
        maxBlock_ = ns;
    }

    // ── 1. Save dry ───────────────────────────────────────────────────────
    dryBuf_.makeCopyOf(buffer);

    // Stereo-mid RMS: uses (L+R)/2 so centred and panned content weighs equally
    const float dryRmsDb = stereoMidRmsDb(dryBuf_.getReadPointer(0),
                                           dryBuf_.getReadPointer(1), ns);
    diagState_.dryRmsDb.store(dryRmsDb);
    diagState_.sampleRateHz.store(float(sr_));
    diagState_.blockSizeSmps.store(float(ns));

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

    // ── 4. N × FormantFilter → stereo workBuf  (CONTRACT: zero dry) ───────
    const int   n        = juce::jlimit(1, kMaxCopies, cache_.numCopies);
    const float copyGain = 1.f / float(n);
    bool nanDetectedThisBlock = false;

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
            if (!std::isfinite(in)) { in = 0.f; nanDetectedThisBlock = true; }

            const auto hum = humanizer_.tick(ci);
            copies_[ci].setFormantShift(cache_.formantShift + interval
                                        + copyDetune_[ci]
                                        + hum.pitchOffsetCents * roughScale);

            const float out = copies_[ci].tick(in) * hum.ampOffset * copyGain;
            wL[s] += out * panL;
            wR[s] += out * panR;
        }
    }

    // C5 sticky — once set, survives until plugin reload
    if (nanDetectedThisBlock) diagState_.C5_noNanInf.store(false);

    // ── 5. Wet boost (kWetBoost = 1.0 — unity) ────────────────────────────
    juce::FloatVectorOperations::multiply(wL, kWetBoost, ns);
    juce::FloatVectorOperations::multiply(wR, kWetBoost, ns);
    juce::FloatVectorOperations::copy(buffer.getWritePointer(0), wL, ns);
    juce::FloatVectorOperations::copy(buffer.getWritePointer(1), wR, ns);

    // Wet RMS at step 5: stereo-mid so all panned voices contribute equally
    const float wetRmsDb = stereoMidRmsDb(buffer.getReadPointer(0),
                                           buffer.getReadPointer(1), ns);
    diagState_.wetRmsDb.store(wetRmsDb);
    diagState_.wetDryDiffDb.store(wetRmsDb - dryRmsDb);
    diagState_.mixRatio.store(cache_.mix);
    diagState_.voiceCount.store(float(cache_.numCopies));
    diagState_.copyCount.store(float(cache_.numCopies));

    // Per-copy normGain_ — read on audio thread (same thread that writes it via tick())
    {
        float ngMin = 1e9f, ngMax = 0.f;
        for (int ci = 0; ci < n; ++ci) {
            const float ng = copies_[ci].getNormGain();
            ngMin = juce::jmin(ngMin, ng);
            ngMax = juce::jmax(ngMax, ng);
        }
        diagState_.normGainMin.store(n > 0 ? ngMin : 0.f);
        diagState_.normGainMax.store(ngMax);
    }

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
    //
    //  GR is measured using PEAK comparison, not RMS.
    //
    //  Why peak? The JUCE dsp::Limiter uses a lookahead delay. If the signal
    //  transitions from loud→quiet, postRms > preRms (the output is the delayed
    //  loud block; the input is already quiet), giving a spurious positive GR.
    //  Peak comparison measures the SAME buffer snapshot before and after — the
    //  lookahead delay shifts the content but cannot create a larger peak than
    //  the input provided (limiter can only reduce).
    //
    //  GR is reported as a positive value (0 = idle, 4.8 = 4.8 dB of reduction).
    //  This is the standard VU/gain-reduction convention.
    float limGrDb = 0.f;
    float postLimStereoRmsDb = -96.f;
    {
        const float peakBefore = bufferPeakLinear(buffer, ns);
        juce::dsp::AudioBlock<float> b(buffer);
        limiter_.process(juce::dsp::ProcessContextReplacing<float>(b));
        const float peakAfter  = bufferPeakLinear(buffer, ns);

        // GR = positive when reducing, 0 when idle
        // jmax(0) prevents false reduction report from lookahead temporal mismatch
        if (peakBefore > 1e-9f && peakAfter > 1e-9f)
            limGrDb = juce::jmax(0.f,
                        20.f * std::log10(peakBefore) - 20.f * std::log10(peakAfter));

        diagState_.limiterGainDb.store(limGrDb);

        // Also capture stereo-mid RMS of the limiter output for C3
        postLimStereoRmsDb = stereoMidRmsDb(buffer.getReadPointer(0),
                                             buffer.getReadPointer(1), ns);
    }

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

    // ── 12. Contract checks ───────────────────────────────────────────────
    {
        const float outDb    = stereoMidRmsDb(buffer.getReadPointer(0),
                                               buffer.getReadPointer(1), ns);
        const bool  hasAudio = dryRmsDb > -60.f;
        const bool  nanClean = diagState_.C5_noNanInf.load();
        diagState_.outputRmsDb.store(outDb);

        // C1 — Limiter idle
        //  GR now uses peak convention (positive = reducing).
        //  Fire threshold: > 0.5 dB of peak reduction for ≥ 5 consecutive blocks.
        if (limGrDb > 0.5f)
            limiterConsecBlocks_++;
        else
            limiterConsecBlocks_ = 0;
        diagState_.limiterConsecBlocks.store(float(limiterConsecBlocks_));
        diagState_.C1_limiterIdle.store(limiterConsecBlocks_ < 5);

        // C2 — Wet within 6 dB of dry (stereo-mid, step 5, before reverb/chorus)
        //  Suppressed when NaN present — NaN in dry makes dryRmsDb = -96, false alarm.
        if (hasAudio && nanClean)
            diagState_.C2_wetWithin6dB.store(std::abs(wetRmsDb - dryRmsDb) <= 6.f);

        // C3 — No dry in wet path at mix=1
        //  Expected output = postLimiter(stereo-mid) + masterGain.
        //  Deviation > ±1.5 dB at mix=1 indicates dry leaking through the
        //  formant filter or an error in the crossfade step.
        if (hasAudio && nanClean && cache_.mix > 0.99f) {
            const float expectedGainDb = 20.f * std::log10(juce::jmax(0.001f, cache_.masterVol))
                                       + cache_.masterGain;
            const float leakDb = outDb - (postLimStereoRmsDb + expectedGainDb);
            diagState_.dryLeakDb.store(leakDb);
            diagState_.C3_noDryLeak.store(std::abs(leakDb) < 1.5f);
        }

        // C4 — NormGain not pegged at ceiling (kNormMax = 20.0)
        diagState_.C4_normGainSane.store(diagState_.normGainMax.load() < 18.f);
    }

    // ── Scope feed — lock-free AbstractFifo write ─────────────────────────
    {
        const float* sL = buffer.getReadPointer(0);
        const float* sR = no > 1 ? buffer.getReadPointer(1) : sL;
        int s1, n1, s2, n2;
        scopeFifo_.prepareToWrite(ns, s1, n1, s2, n2);
        for (int s = 0; s < n1; ++s)
            scopeBuf_[s1 + s] = (sL[s] + sR[s]) * 0.5f;
        for (int s = 0; s < n2; ++s)
            scopeBuf_[s2 + s] = (sL[n1 + s] + sR[n1 + s]) * 0.5f;
        scopeFifo_.finishedWrite(n1 + n2);
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
    shimmerLastSR_ = sr; shimmerLastCh_ = ch;
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
    const float wet = cache_.effShimmer, dry = 1.f - wet;
    for (int c = 0; c < ch; ++c) {
        float* d = buffer.getWritePointer(c);
        const float* s = shimmerBuf_.getReadPointer(c);
        for (int i = 0; i < avail; ++i) d[i] = d[i] * dry + s[i] * wet;
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
{ copyXmlToBinary(*apvts.copyState().createXml(), dest); }

void AncientVoicesProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        stateRestorePending_.store(true);
    }
}

juce::AudioProcessorEditor* AncientVoicesProcessor::createEditor()
{ return new AncientVoicesEditor(*this); }

const juce::String AncientVoicesProcessor::getName() const { return "Ancient Voices"; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{ return new AncientVoicesProcessor(); }
