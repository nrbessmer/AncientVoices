// ============================================================
//  DSPModules.h — Ancient Voices extended DSP (Session 1)
//  Tully EDM Vibe
// ------------------------------------------------------------
//  Contains four new modules that plug into the existing
//  AncientVoicesAudioProcessor:
//
//    LFO                 — sine / triangle / saw / random-stepped
//    EnvelopeFollower    — RMS + attack/release smoothing
//    FormantShifter      — RubberBand-based formant shift, independent of pitch
//    ModMatrix           — minimal routing from sources → param destinations
//
//  All classes are self-contained and do not alter any existing
//  DSP. The processor calls into these opt-in per preset, based
//  on new fields in ProfileSettings (defaulted to 0 = off, so
//  MMV's original 10 profiles behave identically).
// ============================================================

#pragma once

#include <JuceHeader.h>
#include <rubberband/RubberBandStretcher.h>
#include <atomic>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

namespace AV
{

// ============================================================
// Modulation destination enum — used by both LFO and EnvelopeFollower
// When adding new destinations, also update ModMatrix::apply().
// ============================================================
enum class ModDest : int
{
    None = 0,
    Pitch,          // semitones, added to preset pitchShiftAmount
    Pan,            // [-1..+1], replaces/adds to spacedOutPanAmount
    FilterCutoff,   // Hz multiplier around filterCutoff
    Amplitude,      // tremolo, gain multiplier around 1.0
    Formant,        // semitones, added to formantShiftSemitones
    ReverbWet,      // 0..1, added to reverbAmount (clamped)
    DelayTime       // ms multiplier around delay time
};

// ============================================================
// LFO — single-voice modulator.
// Phase advances at blockStart; one value sampled per block
// (cheap, and fine for slow modulation at typical block sizes).
// For audio-rate modulation, call tickSample() per sample.
// ============================================================
class LFO
{
public:
    enum Shape : int { Sine = 0, Triangle = 1, Saw = 2, Random = 3 };

    void prepare (double sampleRate_) noexcept
    {
        sampleRate = sampleRate_;
        phase = 0.0;
        rng.seed (std::random_device{}());
        currentRandom = sampleNoise();
        lastRandomPhase = 0.0;
    }

    void setRateHz (float hz) noexcept    { rateHz = juce::jmax (0.0f, hz); }
    void setDepth  (float d)  noexcept    { depth  = d; }
    void setShape  (int  s)   noexcept    { shape  = (Shape) juce::jlimit (0, 3, s); }
    void setPhaseOffset (double p) noexcept { phase = p; }

    float getDepth() const noexcept       { return depth; }
    bool  isActive() const noexcept       { return rateHz > 0.0f && depth != 0.0f; }

    // Advance by one block, return current value in [-1, +1] * depth.
    // blockSize = samples consumed this block.
    float tickBlock (int blockSize) noexcept
    {
        if (! isActive() || sampleRate <= 0.0) return 0.0f;
        const double advance = (double) blockSize * (double) rateHz / sampleRate;
        phase += advance;
        while (phase >= 1.0) phase -= 1.0;

        // For random shape, re-sample when crossing integer-phase boundaries
        if (shape == Random)
        {
            // Sample a new noise value roughly every half-cycle
            if (std::fabs (phase - lastRandomPhase) > 0.5)
            {
                currentRandom = sampleNoise();
                lastRandomPhase = phase;
            }
        }

        return shapeValue() * depth;
    }

    // Per-sample tick for fast audio-rate modulation (e.g. tremolo).
    float tickSample() noexcept
    {
        if (! isActive() || sampleRate <= 0.0) return 0.0f;
        phase += (double) rateHz / sampleRate;
        while (phase >= 1.0) phase -= 1.0;
        if (shape == Random && std::fabs (phase - lastRandomPhase) > 0.5)
        {
            currentRandom = sampleNoise();
            lastRandomPhase = phase;
        }
        return shapeValue() * depth;
    }

private:
    float shapeValue() const noexcept
    {
        switch (shape)
        {
            case Sine:     return std::sin (phase * juce::MathConstants<double>::twoPi);
            case Triangle: return 1.0f - 4.0f * std::fabs ((float) phase - 0.5f);   // [-1..+1] triangle
            case Saw:      return 2.0f * (float) phase - 1.0f;                       // ramp up [-1..+1]
            case Random:   return currentRandom;
        }
        return 0.0f;
    }

    float sampleNoise() noexcept
    {
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        return dist (rng);
    }

    double sampleRate    = 44100.0;
    double phase         = 0.0;
    double lastRandomPhase = 0.0;
    float  rateHz        = 0.0f;
    float  depth         = 0.0f;
    Shape  shape         = Sine;
    float  currentRandom = 0.0f;
    std::mt19937 rng { 0xA17C5 };
};

// ============================================================
// EnvelopeFollower — RMS → smoothed [0..1] amplitude estimate.
// ============================================================
class EnvelopeFollower
{
public:
    void prepare (double sampleRate_) noexcept
    {
        sampleRate = sampleRate_;
        setAttackMs  (5.0f);
        setReleaseMs (150.0f);
        envelope = 0.0f;
    }

    void setAttackMs (float ms) noexcept
    {
        attackCoeff = coeffFromMs (ms);
    }

    void setReleaseMs (float ms) noexcept
    {
        releaseCoeff = coeffFromMs (ms);
    }

    void setDepth (float d) noexcept   { depth = d; }
    float getDepth() const noexcept    { return depth; }
    bool  isActive() const noexcept    { return depth != 0.0f; }

    // Process one block, updating internal envelope. Returns value in [0..1] * depth.
    float processBlock (const juce::AudioBuffer<float>& buf) noexcept
    {
        if (! isActive()) return 0.0f;
        const int nch = buf.getNumChannels();
        const int ns  = buf.getNumSamples();
        if (nch <= 0 || ns <= 0) return envelope * depth;

        // Average absolute value across channels (cheap, block-rate)
        double sum = 0.0;
        for (int ch = 0; ch < nch; ++ch)
        {
            const float* data = buf.getReadPointer (ch);
            for (int i = 0; i < ns; ++i) sum += std::fabs (data[i]);
        }
        const float target = (float) (sum / (double)(nch * ns));

        // One-pole smoothing with asymmetric attack/release
        const float coeff = (target > envelope) ? attackCoeff : releaseCoeff;
        envelope = coeff * envelope + (1.0f - coeff) * target;
        return juce::jlimit (0.0f, 1.0f, envelope) * depth;
    }

private:
    float coeffFromMs (float ms) const noexcept
    {
        if (ms <= 0.0f || sampleRate <= 0.0) return 0.0f;
        return std::exp (-1.0f / ((ms * 0.001f) * (float) sampleRate));
    }

    double sampleRate    = 44100.0;
    float  attackCoeff   = 0.0f;
    float  releaseCoeff  = 0.0f;
    float  envelope      = 0.0f;
    float  depth         = 0.0f;
};

// ============================================================
// FormantShifter — dedicated RubberBand stretcher with
// OptionFormantPreserved, used to shift formants independently
// of pitch. Semitone-valued input; 0 = neutral (bypass).
// ============================================================
class FormantShifter
{
public:
    void prepare (double sampleRate_, int numChannels_)
    {
        sampleRate  = sampleRate_;
        numChannels = juce::jmax (1, numChannels_);
        rebuild();
    }

    void reset()
    {
        if (stretcher) stretcher->reset();
    }

    void setShiftSemitones (float semitones) noexcept
    {
        targetSemitones = semitones;
    }

    bool isBypass() const noexcept
    {
        return std::fabs (targetSemitones) < 0.01f;
    }

    // Process in-place. Buffer channel count must match numChannels given to prepare();
    // if it doesn't, we rebuild.
    void process (juce::AudioBuffer<float>& buffer)
    {
        if (isBypass() || sampleRate <= 0.0) return;
        if (buffer.getNumChannels() != numChannels)
        {
            numChannels = buffer.getNumChannels();
            rebuild();
        }
        if (stretcher == nullptr) return;

        // RubberBand: setFormantScale operates in frequency ratio, not semitones
        const double scale = std::pow (2.0, (double) targetSemitones / 12.0);
        stretcher->setFormantScale (scale);

        const int ns = buffer.getNumSamples();
        stretcher->process (buffer.getArrayOfWritePointers(), (size_t) ns, false);

        int available = (int) stretcher->available();
        if (available <= 0) return;
        if (available > ns) available = ns;
        stretcher->retrieve (buffer.getArrayOfWritePointers(), (size_t) available);
    }

private:
    void rebuild()
    {
        if (sampleRate <= 0.0 || numChannels <= 0)
        {
            stretcher.reset();
            return;
        }
        using RB = RubberBand::RubberBandStretcher;
        const auto opts = RB::OptionProcessRealTime
                        | RB::OptionPitchHighQuality
                        | RB::OptionFormantPreserved;
        stretcher = std::make_unique<RB> ((size_t) sampleRate, (size_t) numChannels, opts);
        stretcher->setPitchScale (1.0);   // pitch stays neutral; only formant moves
    }

    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    double sampleRate   = 0.0;
    int    numChannels  = 2;
    float  targetSemitones = 0.0f;
};

// ============================================================
// ModMatrix — resolves LFO + envelope sources into param deltas.
// Stateless w.r.t. the audio processor; the processor owns the
// LFOs and env-follower and just queries them each block, then
// calls ModMatrix::apply() to compute final effective values.
// ============================================================
struct ModSources
{
    float lfo1       = 0.0f;   // current LFO value * depth
    float envFollow  = 0.0f;   // current env value * depth
    ModDest lfo1Dest      = ModDest::None;
    ModDest envFollowDest = ModDest::None;
};

struct ModulatedParams
{
    float pitchOffsetSemi   = 0.0f;
    float panOffset         = 0.0f;
    float filterCutoffMul   = 1.0f;
    float amplitudeMul      = 1.0f;   // for tremolo — sample rate sensitive at high LFO freq
    float formantOffsetSemi = 0.0f;
    float reverbWetOffset   = 0.0f;
    float delayTimeMul      = 1.0f;
};

class ModMatrix
{
public:
    // Apply sources → modulated params. Additive/multiplicative
    // mapping per destination.
    static ModulatedParams apply (const ModSources& s) noexcept
    {
        ModulatedParams out;
        applyOne (s.lfo1,      s.lfo1Dest,      out);
        applyOne (s.envFollow, s.envFollowDest, out);
        return out;
    }

private:
    static void applyOne (float value, ModDest dest, ModulatedParams& out) noexcept
    {
        switch (dest)
        {
            case ModDest::None:          break;
            case ModDest::Pitch:         out.pitchOffsetSemi   += value;           break;
            case ModDest::Pan:           out.panOffset         += value;           break;
            case ModDest::FilterCutoff:  out.filterCutoffMul   *= (1.0f + value);  break;
            case ModDest::Amplitude:     out.amplitudeMul      *= (1.0f + value);  break;
            case ModDest::Formant:       out.formantOffsetSemi += value;           break;
            case ModDest::ReverbWet:     out.reverbWetOffset   += value;           break;
            case ModDest::DelayTime:     out.delayTimeMul      *= (1.0f + value);  break;
        }
    }
};

// ============================================================
// SESSION 2 MODULES
// ============================================================

// ============================================================
// MultiTapDelay — up to 4 independent stereo taps with their
// own time / gain / pan. Supports ping-pong (2 taps, hard L/R,
// offset times), prime-number matrices (arbitrary ms per tap),
// and classic multi-voice doubling.
// ============================================================
class MultiTapDelay
{
public:
    static constexpr int kMaxTaps = 4;

    void prepare (double sampleRate_, int maxBlockSize)
    {
        sampleRate = sampleRate_;
        const int maxDelaySamples = (int) (sampleRate * 2.0) + maxBlockSize; // 2 sec max
        bufferSize = juce::nextPowerOfTwo (maxDelaySamples);
        mask = bufferSize - 1;
        bufferL.assign ((size_t) bufferSize, 0.0f);
        bufferR.assign ((size_t) bufferSize, 0.0f);
        writeIdx = 0;
        for (auto& t : taps) t = Tap{};
    }

    void setMix (float m) noexcept { mix = juce::jlimit (0.0f, 1.0f, m); }
    float getMix() const noexcept  { return mix; }
    bool  isActive() const noexcept { return mix > 0.0001f && hasAnyTap(); }

    void setTap (int i, float timeMs, float gain, float pan) noexcept
    {
        if (i < 0 || i >= kMaxTaps) return;
        taps[(size_t) i].timeMs = juce::jmax (0.0f, timeMs);
        taps[(size_t) i].gain   = gain;
        taps[(size_t) i].pan    = juce::jlimit (-1.0f, 1.0f, pan);
    }

    void clearTaps() noexcept { for (auto& t : taps) t = Tap{}; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive() || bufferSize == 0) return;
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        float* L = buffer.getWritePointer (0);
        float* R = nch > 1 ? buffer.getWritePointer (1) : L;

        for (int n = 0; n < ns; ++n)
        {
            // Write dry to delay line
            bufferL[(size_t) writeIdx] = L[n];
            bufferR[(size_t) writeIdx] = R[n];

            // Accumulate taps
            float wetL = 0.0f, wetR = 0.0f;
            for (const auto& t : taps)
            {
                if (t.timeMs <= 0.0f || t.gain == 0.0f) continue;
                const int d = juce::jlimit (0, bufferSize - 1,
                                             (int) (t.timeMs * 0.001 * sampleRate));
                const int idx = (writeIdx - d + bufferSize) & mask;
                const float srcL = bufferL[(size_t) idx];
                const float srcR = bufferR[(size_t) idx];
                // Equal-power pan
                const float pLeft  = std::sqrt (juce::jlimit (0.0f, 1.0f, 0.5f - 0.5f * t.pan));
                const float pRight = std::sqrt (juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * t.pan));
                wetL += t.gain * (srcL * pLeft  + srcR * pLeft  * 0.3f);
                wetR += t.gain * (srcR * pRight + srcL * pRight * 0.3f);
            }

            // Dry/wet mix — dry stays at unity, wet scaled by mix
            L[n] = L[n] + wetL * mix;
            if (nch > 1) R[n] = R[n] + wetR * mix;

            writeIdx = (writeIdx + 1) & mask;
        }
    }

    void reset() noexcept
    {
        std::fill (bufferL.begin(), bufferL.end(), 0.0f);
        std::fill (bufferR.begin(), bufferR.end(), 0.0f);
        writeIdx = 0;
    }

private:
    struct Tap { float timeMs = 0.0f; float gain = 0.0f; float pan = 0.0f; };

    bool hasAnyTap() const noexcept
    {
        for (const auto& t : taps) if (t.timeMs > 0.0f && t.gain != 0.0f) return true;
        return false;
    }

    std::vector<float> bufferL, bufferR;
    std::array<Tap, kMaxTaps> taps {};
    double sampleRate = 44100.0;
    int    bufferSize = 0;
    int    mask       = 0;
    int    writeIdx   = 0;
    float  mix        = 0.0f;
};

// ============================================================
// ReverseReverb — buffers `windowSec` of input, on window boundary
// reverses the contents and plays them out while the next window
// fills. Feeds an internal reverb for the characteristic
// "swoosh-into-event" tail.
// ============================================================
class ReverseReverb
{
public:
    void prepare (double sampleRate_, int numChannels_)
    {
        sampleRate  = sampleRate_;
        numChannels = juce::jmax (1, numChannels_);
        setWindowSec (currentWindowSec);
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate_;
        spec.maximumBlockSize = 2048;
        spec.numChannels = (juce::uint32) numChannels;
        reverb.prepare (spec);
        juce::dsp::Reverb::Parameters p;
        p.roomSize = 0.85f;
        p.damping  = 0.4f;
        p.width    = 1.0f;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        reverb.setParameters (p);
    }

    void setMix (float m) noexcept          { mix = juce::jlimit (0.0f, 1.0f, m); }
    float getMix() const noexcept           { return mix; }
    bool  isActive() const noexcept         { return mix > 0.0001f && windowSamples > 0; }

    void setWindowSec (float sec) noexcept
    {
        currentWindowSec = juce::jlimit (0.25f, 5.0f, sec);
        if (sampleRate <= 0.0) return;
        windowSamples = (int) (currentWindowSec * sampleRate);
        captureL.assign ((size_t) windowSamples, 0.0f);
        captureR.assign ((size_t) windowSamples, 0.0f);
        playbackL.assign ((size_t) windowSamples, 0.0f);
        playbackR.assign ((size_t) windowSamples, 0.0f);
        captureIdx = 0;
        playbackIdx = windowSamples; // start empty
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive()) return;
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0 || windowSamples <= 0) return;

        float* L = buffer.getWritePointer (0);
        float* R = nch > 1 ? buffer.getWritePointer (1) : L;

        // Work buffer for the reversed signal that we'll feed into the reverb
        juce::AudioBuffer<float> revBuf (2, ns);
        revBuf.clear();
        float* rL = revBuf.getWritePointer (0);
        float* rR = revBuf.getWritePointer (1);

        for (int n = 0; n < ns; ++n)
        {
            // Capture current sample
            captureL[(size_t) captureIdx] = L[n];
            captureR[(size_t) captureIdx] = R[n];
            ++captureIdx;

            // When window fills, swap → reverse into playback buffer
            if (captureIdx >= windowSamples)
            {
                for (int i = 0; i < windowSamples; ++i)
                {
                    playbackL[(size_t) i] = captureL[(size_t)(windowSamples - 1 - i)];
                    playbackR[(size_t) i] = captureR[(size_t)(windowSamples - 1 - i)];
                }
                captureIdx = 0;
                playbackIdx = 0;
            }

            // Output reversed sample if available
            if (playbackIdx < windowSamples)
            {
                rL[n] = playbackL[(size_t) playbackIdx];
                rR[n] = playbackR[(size_t) playbackIdx];
                ++playbackIdx;
            }
        }

        // Feed reversed signal into internal reverb
        juce::dsp::AudioBlock<float> block (revBuf);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);

        // Mix reverb-of-reversed into the main buffer
        for (int n = 0; n < ns; ++n)
        {
            L[n] += rL[n] * mix;
            if (nch > 1) R[n] += rR[n] * mix;
        }
    }

    void reset() noexcept
    {
        std::fill (captureL.begin(), captureL.end(), 0.0f);
        std::fill (captureR.begin(), captureR.end(), 0.0f);
        std::fill (playbackL.begin(), playbackL.end(), 0.0f);
        std::fill (playbackR.begin(), playbackR.end(), 0.0f);
        captureIdx = 0;
        playbackIdx = windowSamples;
        reverb.reset();
    }

private:
    std::vector<float> captureL, captureR;
    std::vector<float> playbackL, playbackR;
    juce::dsp::Reverb reverb;
    double sampleRate    = 44100.0;
    int    numChannels   = 2;
    int    windowSamples = 0;
    int    captureIdx    = 0;
    int    playbackIdx   = 0;
    float  currentWindowSec = 2.0f;
    float  mix           = 0.0f;
};

// ============================================================
// CombFilter — simple stereo feedback comb. Produces resonant
// peaks spaced at 1/(delay) Hz. Used for metallic/crystalline
// highs (AV_41 Crystal of Utu, AV_45 Tomb of Seti).
// ============================================================
class CombFilter
{
public:
    void prepare (double sampleRate_)
    {
        sampleRate = sampleRate_;
        const int maxSamples = (int) (sampleRate * 0.05); // up to 50ms delay
        const int size = juce::nextPowerOfTwo (maxSamples + 64);
        bufferL.assign ((size_t) size, 0.0f);
        bufferR.assign ((size_t) size, 0.0f);
        bufferSize = size;
        mask = size - 1;
        writeIdx = 0;
    }

    void setFrequencyHz (float hz) noexcept
    {
        freqHz = juce::jmax (0.0f, hz);
        delaySamples = (freqHz > 0.0f && sampleRate > 0.0)
                        ? (int) (sampleRate / freqHz)
                        : 0;
        delaySamples = juce::jlimit (1, bufferSize - 1, delaySamples);
    }

    void setFeedback (float fb) noexcept { feedback = juce::jlimit (0.0f, 0.95f, fb); }
    void setMix      (float m)  noexcept { mix      = juce::jlimit (0.0f, 1.0f,  m);  }

    bool isActive() const noexcept { return mix > 0.0001f && freqHz > 0.0f; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive() || bufferSize == 0) return;
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        float* L = buffer.getWritePointer (0);
        float* R = nch > 1 ? buffer.getWritePointer (1) : L;

        for (int n = 0; n < ns; ++n)
        {
            const int readIdx = (writeIdx - delaySamples + bufferSize) & mask;
            const float delayedL = bufferL[(size_t) readIdx];
            const float delayedR = bufferR[(size_t) readIdx];

            const float inL = L[n] + delayedL * feedback;
            const float inR = R[n] + delayedR * feedback;

            bufferL[(size_t) writeIdx] = inL;
            bufferR[(size_t) writeIdx] = inR;

            L[n] = L[n] * (1.0f - mix) + inL * mix;
            if (nch > 1) R[n] = R[n] * (1.0f - mix) + inR * mix;

            writeIdx = (writeIdx + 1) & mask;
        }
    }

    void reset() noexcept
    {
        std::fill (bufferL.begin(), bufferL.end(), 0.0f);
        std::fill (bufferR.begin(), bufferR.end(), 0.0f);
        writeIdx = 0;
    }

private:
    std::vector<float> bufferL, bufferR;
    double sampleRate   = 44100.0;
    int    bufferSize   = 0;
    int    mask         = 0;
    int    writeIdx     = 0;
    int    delaySamples = 0;
    float  freqHz       = 0.0f;
    float  feedback     = 0.0f;
    float  mix          = 0.0f;
};

// ============================================================
// EnhancedReverb — wraps juce::dsp::Reverb with pre-delay buffer
// and a high-shelf filter for HF damping. Used by presets that
// need either (a) pre-delay > 0 for massive-space separation or
// (b) aggressive HF rolloff on the wet tail.
// ============================================================
class EnhancedReverb
{
public:
    void prepare (double sampleRate_, int maxBlockSize)
    {
        sampleRate = sampleRate_;
        const int maxPreSamples = (int) (sampleRate * 0.2); // 200ms max
        preBufferSize = juce::nextPowerOfTwo (maxPreSamples + maxBlockSize);
        preMask = preBufferSize - 1;
        preL.assign ((size_t) preBufferSize, 0.0f);
        preR.assign ((size_t) preBufferSize, 0.0f);
        preWrite = 0;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate_;
        spec.maximumBlockSize = (juce::uint32) maxBlockSize;
        spec.numChannels = 2;
        reverb.prepare (spec);

        shelfL.reset();
        shelfR.reset();
        updateShelf();
    }

    void setRoomSize  (float s) noexcept { rev.roomSize = s; reverb.setParameters (rev); }
    void setDamping   (float d) noexcept { rev.damping  = d; reverb.setParameters (rev); }
    void setWidth     (float w) noexcept { rev.width    = w; reverb.setParameters (rev); }
    void setWetLevel  (float w) noexcept { rev.wetLevel = w; reverb.setParameters (rev); }
    void setDryLevel  (float d) noexcept { rev.dryLevel = d; reverb.setParameters (rev); }
    void setPreDelayMs (float ms) noexcept { preDelaySamples = (int) (juce::jmax (0.0f, ms) * 0.001 * sampleRate); }
    void setHfDampingDb (float db) noexcept { hfDampingDb = db; updateShelf(); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        float* L = buffer.getWritePointer (0);
        float* R = nch > 1 ? buffer.getWritePointer (1) : L;

        // Pre-delay: capture into ring, read delayed into a temp block
        if (preDelaySamples > 0)
        {
            for (int n = 0; n < ns; ++n)
            {
                preL[(size_t) preWrite] = L[n];
                preR[(size_t) preWrite] = R[n];
                const int rd = (preWrite - preDelaySamples + preBufferSize) & preMask;
                L[n] = preL[(size_t) rd];
                R[n] = preR[(size_t) rd];
                preWrite = (preWrite + 1) & preMask;
            }
        }

        // Reverb
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);

        // HF damping on the wet-heavy result
        if (std::fabs (hfDampingDb) > 0.1f)
        {
            for (int n = 0; n < ns; ++n)
            {
                L[n] = shelfL.processSingleSampleRaw (L[n]);
                if (nch > 1) R[n] = shelfR.processSingleSampleRaw (R[n]);
            }
        }
    }

    void reset() noexcept
    {
        std::fill (preL.begin(), preL.end(), 0.0f);
        std::fill (preR.begin(), preR.end(), 0.0f);
        preWrite = 0;
        reverb.reset();
        shelfL.reset();
        shelfR.reset();
    }

private:
    void updateShelf()
    {
        if (sampleRate <= 0.0) return;
        // High-shelf at 6 kHz with gain = hfDampingDb (negative = cut)
        const float gain = juce::Decibels::decibelsToGain (hfDampingDb);
        auto coeffs = juce::IIRCoefficients::makeHighShelf (sampleRate, 6000.0, 0.707f, gain);
        shelfL.setCoefficients (coeffs);
        shelfR.setCoefficients (coeffs);
    }

    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters rev;
    juce::IIRFilter shelfL, shelfR;
    std::vector<float> preL, preR;
    double sampleRate      = 44100.0;
    int    preBufferSize   = 0;
    int    preMask         = 0;
    int    preWrite        = 0;
    int    preDelaySamples = 0;
    float  hfDampingDb     = 0.0f;
};

// ============================================================
// SESSION 3 MODULES
// ============================================================

// ============================================================
// PhaseVocoderFreeze — FFT-based frame freeze and spectral smear.
// Operates per-channel, size 2048 with 75% overlap (hopSize=512).
// Two independent features that can combine:
//   Freeze: when freezeAmount > 0, magnitudes are held constant
//           and phase randomly advances — classic infinite-sustain
//           spectral freeze used by drones.
//   Smear:  when smearAmount > 0, each new frame's magnitudes are
//           averaged with a rolling window of previous frames,
//           blurring transients into a continuous pad.
// ============================================================
class PhaseVocoderFreeze
{
public:
    static constexpr int kFFTOrder = 11;                 // 2^11 = 2048
    static constexpr int kFFTSize  = 1 << kFFTOrder;     // 2048
    static constexpr int kHopSize  = kFFTSize / 4;       // 512 (75% overlap)

    PhaseVocoderFreeze()
        : fft(kFFTOrder),
          window(kFFTSize, juce::dsp::WindowingFunction<float>::hann) {}

    void prepare (double sampleRate_, int numChannels_)
    {
        (void) sampleRate_;
        numChannels = juce::jmax (1, juce::jmin (numChannels_, 2));
        for (int ch = 0; ch < numChannels; ++ch)
        {
            state[ch].ringInput.assign (kFFTSize, 0.0f);
            state[ch].outputBuffer.assign (kFFTSize, 0.0f);
            state[ch].fftData.assign (2 * kFFTSize, 0.0f);
            state[ch].frozenMagnitudes.assign (kFFTSize / 2 + 1, 0.0f);
            state[ch].smearMags.assign (kFFTSize / 2 + 1, 0.0f);
            state[ch].phase.assign (kFFTSize / 2 + 1, 0.0f);
            state[ch].ringWrite = 0;
            state[ch].readPos   = 0;
            state[ch].samplesUntilNextFFT = kHopSize;
        }
        rng.seed (0x5EEDC0DE);
        hasFrozen = false;
    }

    void setFreezeAmount (float a) noexcept   { freezeAmount = juce::jlimit (0.0f, 1.0f, a); }
    void setSmearAmount  (float a) noexcept   { smearAmount  = juce::jlimit (0.0f, 1.0f, a); }
    void setMix          (float m) noexcept   { mix          = juce::jlimit (0.0f, 1.0f, m); }

    bool isActive() const noexcept
    {
        return mix > 0.0001f && (freezeAmount > 0.0001f || smearAmount > 0.0001f);
    }

    void triggerFreeze() noexcept { freezeRequested = true; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive()) return;
        const int nch = juce::jmin (buffer.getNumChannels(), numChannels);
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        for (int ch = 0; ch < nch; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            processChannel (state[(size_t) ch], data, ns);
        }
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            std::fill (state[ch].ringInput.begin(), state[ch].ringInput.end(), 0.0f);
            std::fill (state[ch].outputBuffer.begin(), state[ch].outputBuffer.end(), 0.0f);
            std::fill (state[ch].frozenMagnitudes.begin(), state[ch].frozenMagnitudes.end(), 0.0f);
            std::fill (state[ch].smearMags.begin(), state[ch].smearMags.end(), 0.0f);
            std::fill (state[ch].phase.begin(), state[ch].phase.end(), 0.0f);
            state[ch].ringWrite = 0;
            state[ch].readPos   = 0;
            state[ch].samplesUntilNextFFT = kHopSize;
        }
        hasFrozen = false;
    }

private:
    struct ChannelState
    {
        std::vector<float> ringInput;             // ring-buffer input, size kFFTSize
        std::vector<float> outputBuffer;          // output accumulator (overlap-add)
        std::vector<float> fftData;               // complex, size 2*N
        std::vector<float> frozenMagnitudes;      // N/2+1 bins
        std::vector<float> smearMags;             // N/2+1 bins
        std::vector<float> phase;                 // running phase per bin
        int ringWrite = 0;                        // next write index into ringInput
        int readPos   = 0;                        // output read index
        int samplesUntilNextFFT = kHopSize;
    };

    void processChannel (ChannelState& s, float* data, int ns)
    {
        std::uniform_real_distribution<float> phaseDist (-juce::MathConstants<float>::pi,
                                                          juce::MathConstants<float>::pi);

        for (int n = 0; n < ns; ++n)
        {
            // Ring-buffer write (O(1) instead of memmove of the whole buffer)
            s.ringInput[(size_t) s.ringWrite] = data[n];
            s.ringWrite = (s.ringWrite + 1) % kFFTSize;

            float drySample = data[n];
            float wetSample = s.outputBuffer[(size_t) s.readPos];
            s.outputBuffer[(size_t) s.readPos] = 0.0f;
            s.readPos = (s.readPos + 1) % kFFTSize;

            data[n] = drySample * (1.0f - mix) + wetSample * mix;

            if (--s.samplesUntilNextFFT == 0)
            {
                s.samplesUntilNextFFT = kHopSize;
                performFFT (s, phaseDist);
            }
        }
    }

    void performFFT (ChannelState& s,
                     std::uniform_real_distribution<float>& phaseDist)
    {
        // Linearize the ring buffer into fftData so the FFT sees oldest→newest
        for (int i = 0; i < kFFTSize; ++i)
        {
            const int idx = (s.ringWrite + i) % kFFTSize;   // oldest sample first
            s.fftData[(size_t) i] = s.ringInput[(size_t) idx];
        }
        std::fill (s.fftData.begin() + kFFTSize, s.fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (s.fftData.data(), kFFTSize);

        // Forward real FFT (JUCE: in-place, produces freq data in first N+1 as complex pairs)
        fft.performRealOnlyForwardTransform (s.fftData.data());

        const int nBins = kFFTSize / 2 + 1;
        const bool freezing = (freezeAmount > 0.0001f);
        const bool smearing = (smearAmount > 0.0001f);

        // Pull magnitudes from fftData (interleaved [re, im] per bin)
        for (int k = 0; k < nBins; ++k)
        {
            const float re = s.fftData[(size_t)(2 * k)];
            const float im = s.fftData[(size_t)(2 * k + 1)];
            const float mag = std::sqrt (re * re + im * im);

            float targetMag = mag;

            if (smearing)
            {
                // Rolling average toward current — smearAmount=1 = full blur
                const float blend = smearAmount;
                s.smearMags[(size_t) k] = blend * s.smearMags[(size_t) k] + (1.0f - blend) * mag;
                targetMag = s.smearMags[(size_t) k];
            }

            if (freezing)
            {
                // Crossfade between live and frozen magnitudes
                if (! hasFrozen)
                {
                    s.frozenMagnitudes[(size_t) k] = mag;
                }
                const float fa = freezeAmount;
                targetMag = fa * s.frozenMagnitudes[(size_t) k] + (1.0f - fa) * targetMag;

                // Random phase walk while frozen, producing shimmer
                s.phase[(size_t) k] += phaseDist (rng) * 0.3f;
                if (s.phase[(size_t) k] > juce::MathConstants<float>::pi)
                    s.phase[(size_t) k] -= juce::MathConstants<float>::twoPi;
                else if (s.phase[(size_t) k] < -juce::MathConstants<float>::pi)
                    s.phase[(size_t) k] += juce::MathConstants<float>::twoPi;

                s.fftData[(size_t)(2 * k)]     = targetMag * std::cos (s.phase[(size_t) k]);
                s.fftData[(size_t)(2 * k + 1)] = targetMag * std::sin (s.phase[(size_t) k]);
            }
            else if (smearing)
            {
                // Preserve original phase but swap magnitude for smeared one
                const float origMag = mag > 1e-6f ? mag : 1e-6f;
                const float scale = targetMag / origMag;
                s.fftData[(size_t)(2 * k)]     *= scale;
                s.fftData[(size_t)(2 * k + 1)] *= scale;
            }
        }

        if (freezing) hasFrozen = true;

        // Inverse FFT
        fft.performRealOnlyInverseTransform (s.fftData.data());

        // Window + overlap-add into output buffer
        window.multiplyWithWindowingTable (s.fftData.data(), kFFTSize);
        const float scale = 1.0f / (kFFTSize * 0.5f); // compensate for 4x overlap + window

        for (int i = 0; i < kFFTSize; ++i)
        {
            const int idx = (s.readPos + i) % kFFTSize;
            s.outputBuffer[(size_t) idx] += s.fftData[(size_t) i] * scale;
        }
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<ChannelState, 2> state;
    int numChannels = 2;
    float freezeAmount    = 0.0f;
    float smearAmount     = 0.0f;
    float mix             = 0.0f;
    bool  hasFrozen       = false;
    bool  freezeRequested = false;
    std::mt19937 rng { 0x5EEDC0DE };
};

// ============================================================
// GranularEngine — overlapping grain player with ring buffer.
// N grains (kMaxGrains) cycle through the input, each with
// independent pitch, start position, and reverse flag.
// Parameters:
//   grainSizeMs:        2..500 ms
//   density:            grains per second (0..50)
//   pitchSpreadSemi:    random spread per grain, ±N semitones
//   pitchBaseOffset:    fixed pitch shift applied to every grain
//   reverseProb:        0..1 probability each grain plays reversed
//   positionJitter:     0..1 random pick in buffer (1 = full random)
//   mix:                dry/wet blend
// Output is added to the existing buffer contents at (1-mix) dry.
// ============================================================
class GranularEngine
{
public:
    static constexpr int kMaxGrains = 16;

    void prepare (double sampleRate_, int bufferSeconds = 4)
    {
        sampleRate = sampleRate_;
        const int total = juce::nextPowerOfTwo ((int)(sampleRate * bufferSeconds));
        ringL.assign ((size_t) total, 0.0f);
        ringR.assign ((size_t) total, 0.0f);
        ringSize = total;
        ringMask = total - 1;
        writeIdx = 0;
        nextGrainTicks = 0;
        for (auto& g : grains) g = Grain{};
        rng.seed (0x6AABBCCD);
    }

    void setGrainSizeMs     (float ms) noexcept { grainSizeMs = juce::jlimit (2.0f, 500.0f, ms); }
    void setDensity         (float d)  noexcept { density = juce::jlimit (0.0f, 50.0f, d); }
    void setPitchSpreadSemi (float s)  noexcept { pitchSpreadSemi = juce::jmax (0.0f, s); }
    void setPitchBaseOffset (float s)  noexcept { pitchBaseSemi   = s; }
    void setReverseProb     (float p)  noexcept { reverseProb = juce::jlimit (0.0f, 1.0f, p); }
    void setPositionJitter  (float j)  noexcept { positionJitter = juce::jlimit (0.0f, 1.0f, j); }
    void setMix             (float m)  noexcept { mix = juce::jlimit (0.0f, 1.0f, m); }

    bool isActive() const noexcept { return mix > 0.0001f && density > 0.0f; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive() || ringSize == 0) return;
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        float* L = buffer.getWritePointer (0);
        float* R = nch > 1 ? buffer.getWritePointer (1) : L;

        const int samplesPerGrainTrigger = (density > 0.0f)
            ? juce::jmax (1, (int)(sampleRate / density))
            : INT_MAX;
        const int grainSamples = juce::jmax (1, (int)(grainSizeMs * 0.001 * sampleRate));

        std::uniform_real_distribution<float> dist01 (0.0f, 1.0f);
        std::uniform_real_distribution<float> distPm1 (-1.0f, 1.0f);

        for (int n = 0; n < ns; ++n)
        {
            // Write dry to ring
            ringL[(size_t) writeIdx] = L[n];
            ringR[(size_t) writeIdx] = R[n];

            // Trigger new grains
            if (--nextGrainTicks <= 0)
            {
                nextGrainTicks = samplesPerGrainTrigger;
                for (auto& g : grains)
                {
                    if (g.active) continue;
                    g.active        = true;
                    g.position      = 0;
                    g.length        = grainSamples;
                    const int jitterSpan = (int)(positionJitter * (ringSize / 2));
                    const int jitter = jitterSpan > 0
                        ? (int)(dist01 (rng) * jitterSpan)
                        : 0;
                    g.startIdx = (writeIdx - grainSamples - jitter + ringSize) & ringMask;
                    g.pitchRatio = std::pow (2.0f,
                        (pitchBaseSemi + pitchSpreadSemi * distPm1 (rng)) / 12.0f);
                    g.reverse = (dist01 (rng) < reverseProb);
                    g.readPosF = 0.0f;
                    break;
                }
            }

            // Sum all active grains
            float wetL = 0.0f, wetR = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;

                // Hann envelope
                const float env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi
                                          * (float) g.position / (float) g.length));

                // Current read position (fractional)
                const float offset = g.reverse
                    ? (g.length - 1) - g.readPosF
                    : g.readPosF;
                const int   idx0 = (g.startIdx + (int) offset) & ringMask;
                const int   idx1 = (idx0 + 1) & ringMask;
                const float frac = offset - std::floor (offset);

                const float sL = ringL[(size_t) idx0] * (1.0f - frac) + ringL[(size_t) idx1] * frac;
                const float sR = ringR[(size_t) idx0] * (1.0f - frac) + ringR[(size_t) idx1] * frac;

                wetL += sL * env;
                wetR += sR * env;

                g.readPosF += g.pitchRatio;
                ++g.position;
                if (g.position >= g.length)
                    g.active = false;
            }

            // Adaptive output scaling: higher density = more simultaneous
            // overlapping grains, so scale harder. Empirically tuned:
            //   density 5   → scale ~0.7
            //   density 15  → scale ~0.5
            //   density 40+ → scale ~0.3
            const float outScale = juce::jlimit (0.25f, 0.75f,
                                                 1.0f / (1.0f + density * 0.04f));
            L[n] = L[n] * (1.0f - mix) + wetL * mix * outScale;
            if (nch > 1) R[n] = R[n] * (1.0f - mix) + wetR * mix * outScale;

            writeIdx = (writeIdx + 1) & ringMask;
        }
    }

    void reset() noexcept
    {
        std::fill (ringL.begin(), ringL.end(), 0.0f);
        std::fill (ringR.begin(), ringR.end(), 0.0f);
        for (auto& g : grains) g = Grain{};
        writeIdx = 0;
        nextGrainTicks = 0;
    }

private:
    struct Grain
    {
        bool  active     = false;
        int   startIdx   = 0;
        int   length     = 0;
        int   position   = 0;
        float readPosF   = 0.0f;
        float pitchRatio = 1.0f;
        bool  reverse    = false;
    };

    std::vector<float> ringL, ringR;
    std::array<Grain, kMaxGrains> grains {};
    double sampleRate   = 44100.0;
    int    ringSize     = 0;
    int    ringMask     = 0;
    int    writeIdx     = 0;
    int    nextGrainTicks = 0;
    float  grainSizeMs     = 50.0f;
    float  density         = 0.0f;
    float  pitchSpreadSemi = 0.0f;
    float  pitchBaseSemi   = 0.0f;
    float  reverseProb     = 0.0f;
    float  positionJitter  = 0.5f;
    float  mix             = 0.0f;
    std::mt19937 rng { 0x6AABBCCD };
};

// ============================================================
// Bitcrusher — sample-rate decimation + bit-depth quantization.
// ============================================================
class Bitcrusher
{
public:
    void prepare (double sampleRate_) noexcept { sampleRate = sampleRate_; phase = 0.0f; heldL = heldR = 0.0f; }

    void setBitDepth    (float b) noexcept { bitDepth = juce::jlimit (1.0f, 16.0f, b); }
    void setSampleRateHz(float sr) noexcept { targetSR = juce::jlimit (100.0f, 48000.0f, sr); }
    void setMix         (float m) noexcept { mix = juce::jlimit (0.0f, 1.0f, m); }

    bool isActive() const noexcept { return mix > 0.0001f; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive() || sampleRate <= 0.0) return;
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        float* L = buffer.getWritePointer (0);
        float* R = nch > 1 ? buffer.getWritePointer (1) : L;

        const float step = targetSR / (float) sampleRate;
        const float q = std::pow (2.0f, bitDepth) - 1.0f;

        for (int n = 0; n < ns; ++n)
        {
            phase += step;
            if (phase >= 1.0f)
            {
                phase -= std::floor (phase);
                heldL = std::round (L[n] * q) / q;
                heldR = std::round (R[n] * q) / q;
            }
            L[n] = L[n] * (1.0f - mix) + heldL * mix;
            if (nch > 1) R[n] = R[n] * (1.0f - mix) + heldR * mix;
        }
    }

private:
    double sampleRate = 44100.0;
    float  bitDepth   = 12.0f;
    float  targetSR   = 22050.0f;
    float  mix        = 0.0f;
    float  phase      = 0.0f;
    float  heldL = 0.0f, heldR = 0.0f;
};

// ============================================================
// DynamicEqBand — single peaking band whose gain is driven by
// an envelope follower. When the input amplitude exceeds the
// threshold, the gain ramps toward targetGainDb; otherwise it
// falls to 0. Used for era-triggered dynamic EQ (AV_28, AV_35).
// ============================================================
class DynamicEqBand
{
public:
    void prepare (double sampleRate_) noexcept
    {
        sampleRate = sampleRate_;
        envFollower.prepare (sampleRate_);
        envFollower.setAttackMs (10.0f);
        envFollower.setReleaseMs (200.0f);
        envFollower.setDepth (1.0f);
        filterL.reset();
        filterR.reset();
        currentGainDb = 0.0f;
    }

    void setFrequency   (float hz)   noexcept { freqHz = juce::jmax (20.0f, hz); updateCoeffs(); }
    void setQ           (float q)    noexcept { Q = juce::jmax (0.1f, q); updateCoeffs(); }
    void setTargetGainDb(float db)   noexcept { targetGainDb = db; }
    void setThreshold   (float t)    noexcept { threshold = juce::jlimit (0.0f, 1.0f, t); }
    void setMix         (float m)    noexcept { mix = juce::jlimit (0.0f, 1.0f, m); }

    bool isActive() const noexcept { return mix > 0.0001f && std::fabs (targetGainDb) > 0.1f; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive()) return;
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        const float env = envFollower.processBlock (buffer);
        const float targetNow = (env > threshold) ? targetGainDb : 0.0f;
        // Smooth gain changes across block
        currentGainDb += (targetNow - currentGainDb) * 0.15f;
        if (std::fabs (currentGainDb) < 0.1f) return;
        lastAppliedDb = currentGainDb;
        updateCoeffs();

        float* L = buffer.getWritePointer (0);
        float* R = nch > 1 ? buffer.getWritePointer (1) : L;
        for (int n = 0; n < ns; ++n)
        {
            const float dryL = L[n];
            const float wetL = filterL.processSingleSampleRaw (L[n]);
            L[n] = dryL * (1.0f - mix) + wetL * mix;
            if (nch > 1)
            {
                const float dryR = R[n];
                const float wetR = filterR.processSingleSampleRaw (R[n]);
                R[n] = dryR * (1.0f - mix) + wetR * mix;
            }
        }
    }

private:
    void updateCoeffs()
    {
        if (sampleRate <= 0.0) return;
        auto coeffs = juce::IIRCoefficients::makePeakFilter (sampleRate, freqHz, Q,
                        juce::Decibels::decibelsToGain (lastAppliedDb));
        filterL.setCoefficients (coeffs);
        filterR.setCoefficients (coeffs);
    }

    juce::IIRFilter filterL, filterR;
    EnvelopeFollower envFollower;
    double sampleRate    = 44100.0;
    float  freqHz        = 2000.0f;
    float  Q             = 1.2f;
    float  targetGainDb  = 0.0f;
    float  currentGainDb = 0.0f;
    float  lastAppliedDb = 0.0f;
    float  threshold     = 0.1f;
    float  mix           = 0.0f;
};

// ============================================================
// NoiseGenerator — white or pink noise, optionally gated by an
// envelope follower's output (so noise only audible when voice
// is active). Use: consonant enhancement (AV_30), crowd hiss
// (AV_19), wind bed (AV_49), mud plain drones (AV_11).
// ============================================================
class NoiseGenerator
{
public:
    enum Color : int { White = 0, Pink = 1 };

    void prepare (double sampleRate_) noexcept
    {
        sampleRate = sampleRate_;
        envFollower.prepare (sampleRate_);
        envFollower.setAttackMs (5.0f);
        envFollower.setReleaseMs (80.0f);
        envFollower.setDepth (1.0f);
        rng.seed (0xFEED1234);
        for (auto& v : pinkState) v = 0.0f;
    }

    void setColor    (int c)    noexcept { color = (Color) juce::jlimit (0, 1, c); }
    void setLevel    (float l)  noexcept { level = juce::jlimit (0.0f, 1.0f, l); }
    void setGated    (bool g)   noexcept { gated = g; }
    void setHighPassHz (float hz) noexcept { hpHz = hz; }

    bool isActive() const noexcept { return level > 0.0001f; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive()) return;
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        const float envLevel = gated
            ? juce::jlimit (0.0f, 1.0f, envFollower.processBlock (buffer) * 3.0f)
            : 1.0f;
        const float amp = level * envLevel;
        if (amp < 0.0001f) return;

        // Simple one-pole HP coefficient
        const float hpCoeff = (hpHz > 0.0f && sampleRate > 0.0)
            ? std::exp (-juce::MathConstants<float>::twoPi * hpHz / (float) sampleRate)
            : 0.0f;

        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        float* L = buffer.getWritePointer (0);
        float* R = nch > 1 ? buffer.getWritePointer (1) : L;

        for (int n = 0; n < ns; ++n)
        {
            float nL = dist (rng);
            float nR = dist (rng);

            if (color == Pink)
            {
                nL = pinkFilter (nL, 0);
                nR = pinkFilter (nR, 1);
            }

            if (hpCoeff > 0.0f)
            {
                nL = nL - hpCoeff * hpLastL; hpLastL = nL;
                nR = nR - hpCoeff * hpLastR; hpLastR = nR;
            }

            L[n] += nL * amp;
            if (nch > 1) R[n] += nR * amp;
        }
    }

private:
    float pinkFilter (float white, int ch) noexcept
    {
        // Paul Kellet's economy-sized pink filter (7 coefficients per channel)
        const int base = ch * 7;
        pinkState[(size_t)(base + 0)] = 0.99886f * pinkState[(size_t)(base + 0)] + white * 0.0555179f;
        pinkState[(size_t)(base + 1)] = 0.99332f * pinkState[(size_t)(base + 1)] + white * 0.0750759f;
        pinkState[(size_t)(base + 2)] = 0.96900f * pinkState[(size_t)(base + 2)] + white * 0.1538520f;
        pinkState[(size_t)(base + 3)] = 0.86650f * pinkState[(size_t)(base + 3)] + white * 0.3104856f;
        pinkState[(size_t)(base + 4)] = 0.55000f * pinkState[(size_t)(base + 4)] + white * 0.5329522f;
        pinkState[(size_t)(base + 5)] = -0.7616f * pinkState[(size_t)(base + 5)] - white * 0.0168980f;
        const float pink = pinkState[(size_t)(base + 0)] + pinkState[(size_t)(base + 1)]
                         + pinkState[(size_t)(base + 2)] + pinkState[(size_t)(base + 3)]
                         + pinkState[(size_t)(base + 4)] + pinkState[(size_t)(base + 5)]
                         + pinkState[(size_t)(base + 6)] + white * 0.5362f;
        pinkState[(size_t)(base + 6)] = white * 0.115926f;
        return pink * 0.11f;
    }

    EnvelopeFollower envFollower;
    double sampleRate = 44100.0;
    Color  color = White;
    float  level = 0.0f;
    float  hpHz  = 0.0f;
    float  hpLastL = 0.0f, hpLastR = 0.0f;
    bool   gated = false;
    std::array<float, 14> pinkState {};   // 7 per channel
    std::mt19937 rng { 0xFEED1234 };
};

// ============================================================
// TransientShaper — boosts attack transients by comparing fast
// vs slow envelope followers. Attack boost in dB, sustain attenuation
// optional. Used for AV_33 Papyrus Rustle (massively boost attacks).
// ============================================================
class TransientShaper
{
public:
    void prepare (double sampleRate_) noexcept
    {
        sampleRate = sampleRate_;
        fastEnv.prepare (sampleRate_);
        slowEnv.prepare (sampleRate_);
        fastEnv.setAttackMs (1.0f);   fastEnv.setReleaseMs (30.0f);  fastEnv.setDepth (1.0f);
        slowEnv.setAttackMs (30.0f);  slowEnv.setReleaseMs (150.0f); slowEnv.setDepth (1.0f);
    }

    void setAttackBoostDb (float db) noexcept { attackBoostDb = db; }
    void setMix           (float m)  noexcept { mix = juce::jlimit (0.0f, 1.0f, m); }

    bool isActive() const noexcept { return mix > 0.0001f && std::fabs (attackBoostDb) > 0.1f; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! isActive()) return;
        const int nch = buffer.getNumChannels();
        const int ns  = buffer.getNumSamples();
        if (nch <= 0 || ns <= 0) return;

        const float fast = fastEnv.processBlock (buffer);
        const float slow = slowEnv.processBlock (buffer);
        // Positive when a transient is present
        const float transient = juce::jmax (0.0f, fast - slow);
        // Scale by boost
        const float linGain = 1.0f + transient * (juce::Decibels::decibelsToGain (attackBoostDb) - 1.0f) * 4.0f;
        const float effGain = 1.0f * (1.0f - mix) + linGain * mix;
        buffer.applyGain (effGain);
    }

private:
    EnvelopeFollower fastEnv, slowEnv;
    double sampleRate    = 44100.0;
    float  attackBoostDb = 0.0f;
    float  mix           = 0.0f;
};

} // namespace AV
