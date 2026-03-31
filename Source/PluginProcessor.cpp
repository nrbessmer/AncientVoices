#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  EDITOR FACTORY
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* AncientVoicesProcessor::createEditor()
{
    return new AncientVoicesEditor(*this);
}
// ─────────────────────────────────────────────────────────────────────────────
//  PARAMETER LAYOUT
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout AncientVoicesProcessor::createParams()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [](float v, int) -> juce::String { return juce::String(int(v * 100)) + "%"; };

    // ── VOICE ────────────────────────────────────────────────────────────────
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"vowel_pos",1}, "Vowel",
        NormalisableRange<float>(0.f, 4.f, 0.01f), 0.f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"vowel_open",1}, "Openness",
        NormalisableRange<float>(0.5f, 1.5f, 0.01f), 1.f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"breathe",1}, "Breathe",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.1f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"roughness",1}, "Roughness",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.05f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"gender",1}, "Gender",
        NormalisableRange<float>(0.7f, 1.4f, 0.01f), 1.0f));

    // ── CHOIR ────────────────────────────────────────────────────────────────
    p.push_back(std::make_unique<AudioParameterInt>(
        ParameterID{"num_voices",1}, "Voices", 1, 8, 4));

    p.push_back(std::make_unique<AudioParameterInt>(
        ParameterID{"interval",1}, "Interval", 0, 6, 0));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"spread",1}, "Spread",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.6f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"drift",1}, "Drift",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.3f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    // ── ANCIENT ──────────────────────────────────────────────────────────────
    p.push_back(std::make_unique<AudioParameterInt>(
        ParameterID{"era",1}, "Era", 0, 3, 0));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"ritual",1}, "Ritual",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"chant",1}, "Chant",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    // ── ENVELOPE ─────────────────────────────────────────────────────────────
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"env_attack",1}, "Attack",
        NormalisableRange<float>(0.001f, 4.f, 0.001f, 0.4f), 0.05f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"env_decay",1}, "Decay",
        NormalisableRange<float>(0.01f, 4.f, 0.001f, 0.4f), 0.2f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"env_sustain",1}, "Sustain",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.85f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"env_release",1}, "Release",
        NormalisableRange<float>(0.01f, 8.f, 0.001f, 0.4f), 1.2f));

    // ── FX — CAVE REVERB ─────────────────────────────────────────────────────
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"cave_mix",1}, "Cave Mix",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.3f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"cave_size",1}, "Cave Size",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.6f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"cave_damp",1}, "Cave Damp",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.4f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"cave_echo",1}, "Echo ms",
        NormalisableRange<float>(0.f, 100.f, 0.1f), 20.f));

    // ── FX — SHIMMER ─────────────────────────────────────────────────────────
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"shimmer_mix",1}, "Shimmer Mix",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.15f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"shimmer_amt",1}, "Shimmer Amt",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    // ── MASTER ───────────────────────────────────────────────────────────────
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"master_vol",1}, "Volume",
        NormalisableRange<float>(0.f, 1.f, 0.001f, 0.5f), 0.85f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"master_gain",1}, "Gain",
        NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f));

    return { p.begin(), p.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
//  CONSTRUCTOR
// ─────────────────────────────────────────────────────────────────────────────
AncientVoicesProcessor::AncientVoicesProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createParams())
{
}

// ─────────────────────────────────────────────────────────────────────────────
//  PREPARE
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    choir_.prepare(sampleRate, samplesPerBlock);
    reverb_.prepare(sampleRate);
    shimmerL_.prepare(sampleRate);
    shimmerR_.prepare(sampleRate);

    scopeBuf_.setSize(1, samplesPerBlock);
    scopeBuf_.clear();
    choirBuf_.setSize(2, samplesPerBlock);
    choirBuf_.clear();
    scopeReady_.store(false);

    updateCacheFromApvts();
}

// ─────────────────────────────────────────────────────────────────────────────
//  PROCESS BLOCK
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    updateCacheFromApvts();

    auto& c = cache_;

    // Push parameters to choir engine
    choir_.setNumVoices(c.numVoices);
    choir_.setInterval(ChoirInterval(c.intervalIdx));
    choir_.setSpread(c.spread);
    choir_.setDrift(c.drift);
    choir_.setChant(c.chant);
    choir_.setBreathe(c.breathe);
    choir_.setRoughness(c.roughness);
    choir_.setOpenness(c.openness);
    choir_.setGender(c.gender);
    choir_.setVowelPos(c.vowelPos);
    choir_.setEra(AncientEra(c.eraIdx));
    choir_.setEnvelope(c.attack, c.decay, c.sustain, c.release);

    // BPM from DAW — use modern JUCE 7/8 getPosition() API
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getBpm().hasValue())
                choir_.setBpm(float(*pos->getBpm()));

    // MIDI → voice events
    choir_.processMidi(midiMessages);

    // Render choir voices — use pre-allocated buffer, resize safely if block size changes
    choirBuf_.setSize(2, buffer.getNumSamples(), false, true, false);
    choir_.process(choirBuf_);

    // Configure FX — single reverb instance handles true stereo
    reverb_.setMix(c.caveMix);
    reverb_.setSize(c.caveSize);
    reverb_.setDamp(c.caveDamp);
    reverb_.setPreDelay(c.caveEcho);

    shimmerL_.setMix(c.shimmerMix);    shimmerR_.setMix(c.shimmerMix);
    shimmerL_.setAmount(c.shimmerAmt); shimmerR_.setAmount(c.shimmerAmt);

    float masterLinear = c.masterVol * juce::Decibels::decibelsToGain(c.masterGain);

    int numSamples = buffer.getNumSamples();
    buffer.clear();

    for (int s = 0; s < numSamples; ++s)
    {
        float inL = choirBuf_.getSample(0, s);
        float inR = (choirBuf_.getNumChannels() > 1) ? choirBuf_.getSample(1, s) : inL;

        // Cave reverb — true stereo (single instance processes L+R independently)
        float revL, revR;
        reverb_.process(inL, inR, revL, revR);

        // Shimmer
        float outL = shimmerL_.tick(revL) * masterLinear;
        float outR = shimmerR_.tick(revR) * masterLinear;

        if (buffer.getNumChannels() >= 2) {
            buffer.setSample(0, s, outL);
            buffer.setSample(1, s, outR);
        } else {
            buffer.setSample(0, s, (outL + outR) * 0.5f);
        }

        // Scope: write to private write buffer — no lock needed, audio thread only
        if (s < scopeBuf_.getNumSamples())
            scopeBuf_.setSample(0, s, outL);
    }
    // After block completes, lock once to hand off the filled buffer to the message thread
    {
        const juce::ScopedLock sl(scopeLock_);
        scopeReady_.store(true, std::memory_order_release);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PARAM CACHE
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::updateCacheFromApvts()
{
    auto get = [&](const char* id) -> float {
        auto* p = apvts.getRawParameterValue(id);
        return p ? p->load() : 0.f;
    };

    cache_.vowelPos   = get("vowel_pos");
    cache_.openness   = get("vowel_open");
    cache_.breathe    = get("breathe");
    cache_.roughness  = get("roughness");
    cache_.gender     = get("gender");

    cache_.numVoices  = int(get("num_voices"));
    cache_.intervalIdx= int(get("interval"));
    cache_.spread     = get("spread");
    cache_.drift      = get("drift");

    cache_.eraIdx     = int(get("era"));
    cache_.ritual     = get("ritual");
    cache_.chant      = get("chant");

    cache_.attack     = get("env_attack");
    cache_.decay      = get("env_decay");
    cache_.sustain    = get("env_sustain");
    cache_.release    = get("env_release");

    cache_.caveMix    = get("cave_mix");
    cache_.caveSize   = get("cave_size");
    cache_.caveDamp   = get("cave_damp");
    cache_.caveEcho   = get("cave_echo");

    cache_.shimmerMix = get("shimmer_mix");
    cache_.shimmerAmt = get("shimmer_amt");

    cache_.masterVol  = get("master_vol");
    cache_.masterGain = get("master_gain");
}

// ─────────────────────────────────────────────────────────────────────────────
//  BUS LAYOUT
// ─────────────────────────────────────────────────────────────────────────────
bool AncientVoicesProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

// ─────────────────────────────────────────────────────────────────────────────
//  STATE SAVE / RESTORE
// ─────────────────────────────────────────────────────────────────────────────
void AncientVoicesProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void AncientVoicesProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// ─────────────────────────────────────────────────────────────────────────────
//  PLUGIN ENTRY POINT
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AncientVoicesProcessor();
}
