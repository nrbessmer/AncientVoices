// ============================================================
//  VoiceEngine.h — Ancient Voices voice engine 1.1
//  Tully EDM Vibe
// ------------------------------------------------------------
//  Clean, click-free replacements for the block-based effects
//  in engine 1.0:
//
//    StreamingShifter  RubberBand pitch + formant with a fixed,
//                      reported latency (no raw/shifted splicing)
//    HardTune          YIN pitch tracker -> semitone correction
//    Doubler           two modulated taps, panned L/R
//    StereoEcho        feedback delay with cross-block state
//    Drift             slow random pitch wander (humanises)
//    StutterGate       tempo-synced slice repeat with fades
//    WarmDrive         gain-compensated parallel saturation
//    DelayLine         plain integer delay (dry/latency alignment)
//
//  Everything allocates in prepare() only.
// ============================================================
#pragma once

#include <JuceHeader.h>
#include <rubberband/RubberBandStretcher.h>
#include <vector>
#include <memory>
#include <cmath>
#include <random>

namespace AV2
{

// ------------------------------------------------------------ helpers
class DelayLine
{
public:
    void prepare (int numChannels, int maxDelay)
    {
        size = juce::nextPowerOfTwo (maxDelay + 2);
        buf.setSize (numChannels, size);
        buf.clear();
        pos = 0;
    }
    void reset() { buf.clear(); pos = 0; }

    // y = x delayed by d samples (0 <= d < size), in place
    void process (juce::AudioBuffer<float>& b, int d)
    {
        const int n = b.getNumSamples(), nc = juce::jmin (b.getNumChannels(), buf.getNumChannels());
        const int mask = size - 1;
        for (int ch = 0; ch < nc; ++ch)
        {
            float* x = b.getWritePointer (ch);
            float* r = buf.getWritePointer (ch);
            int p = pos;
            for (int i = 0; i < n; ++i)
            {
                r[p] = x[i];
                x[i] = r[(p - d) & mask];
                p = (p + 1) & mask;
            }
        }
        pos = (pos + n) & mask;
    }

private:
    juce::AudioBuffer<float> buf;
    int size = 0, pos = 0;
};

// ------------------------------------------------------------ StreamingShifter
// Feeds RubberBand in small chunks, collects its output in a ring
// that was pre-filled with `latency` samples of silence, and reads
// the host block out of that ring. Output is therefore the input,
// pitch/formant shifted, delayed by exactly getLatency() samples.
class StreamingShifter
{
public:
    // Pitch:   RubberBand R2, formant preserved. Robust when the pitch moves
    //          (vibrato, drift, hard tune) - R3 glitches on ratio changes.
    // Formant: RubberBand R3 at unity pitch, used only to move the formants.
    enum class Quality { Pitch, Formant };

    // Measures the latency this engine needs at this rate (call before prepare).
    static int measureLatency (double sr, Quality q)
    {
        // worst case over the pitch range we use (output needs more input when shifting)
        int worst = 0;
        const std::vector<double> scales = q == Quality::Pitch ? std::vector<double> { 0.5, 0.71, 1.0001, 1.41, 2.0 }
                                                               : std::vector<double> { 1.0001 };
        for (double scale : scales)
        {
            auto rb = make (sr, 1, q);
            if (q == Quality::Pitch) rb->setPitchScale (scale);
            else                     rb->setFormantScale (0.7);
            std::vector<float> in (kChunk), out (16384);
            float* op[1] = { out.data() };
            const float* ip[1] = { in.data() };
            juce::Random rnd (1);
            long fill = 0, minFill = 0;
            const int iters = (int) (0.6 * sr / kChunk);
            for (int i = 0; i < iters; ++i)
            {
                for (auto& v : in) v = rnd.nextFloat() * 0.02f - 0.01f;
                rb->process (ip, kChunk, false);
                int av;
                while ((av = (int) rb->available()) > 0)
                    fill += (long) rb->retrieve (op, (size_t) juce::jmin (av, 16384));
                fill -= kChunk;
                minFill = juce::jmin (minFill, fill);
            }
            worst = juce::jmax (worst, (int) -minFill);
        }
        return worst + 256;   // safety margin
    }

    void prepare (double sr, int numChannels, int maxBlock, int latencySamples, Quality q)
    {
        sampleRate = sr;
        channels = juce::jmax (1, numChannels);
        quality = q;
        latency = latencySamples;
        rb = make (sr, channels, q);
        ringSize = juce::nextPowerOfTwo (latency + maxBlock + 32768);
        ring.setSize (channels, ringSize);
        scratch.setSize (channels, 8192);
        inChunk.setSize (channels, kChunk);
        reset();
    }

    void reset()
    {
        if (rb) rb->reset();
        ring.clear();
        readPos = 0;
        writePos = latency;     // `latency` samples of silence already queued
        underruns = 0;
        lastScale = -1.0;
        lastFormant = -1.0;
    }

    int getLatency() const noexcept { return latency; }
    int getUnderruns() const noexcept { return underruns; }

    // in/out may alias. semis interpolated from start to end across the block.
    void process (const float* const* in, float* const* out, int numCh, int n,
                  float semisStart, float semisEnd, float formantSemis)
    {
        if (rb == nullptr) return;
        const int nc = juce::jmin (numCh, channels);
        const int mask = ringSize - 1;

        // absolute formant ratio wanted (1 = natural voice whatever the pitch)
        const double fAbs = std::pow (2.0, (double) formantSemis / 12.0);

        for (int off = 0; off < n; off += kChunk)
        {
            const int m = juce::jmin (kChunk, n - off);
            const float t = (off + 0.5f * m) / (float) n;
            const double scale = std::pow (2.0, (double) (semisStart + (semisEnd - semisStart) * t) / 12.0);
            if (quality == Quality::Pitch)
            {
                if (std::abs (scale - lastScale) > 1e-7) { rb->setPitchScale (scale); lastScale = scale; }
            }
            else if (std::abs (fAbs - lastFormant) > 1e-6) { rb->setFormantScale (fAbs); lastFormant = fAbs; }

            const float* ip[8];
            for (int ch = 0; ch < channels; ++ch)
            {
                inChunk.copyFrom (ch, 0, in[juce::jmin (ch, nc - 1)] + off, m);
                ip[ch] = inChunk.getReadPointer (ch);
            }
            rb->process (ip, (size_t) m, false);

            int av;
            while ((av = (int) rb->available()) > 0)
            {
                const int g = (int) rb->retrieve (scratch.getArrayOfWritePointers(), (size_t) juce::jmin (av, scratch.getNumSamples()));
                for (int ch = 0; ch < channels; ++ch)
                {
                    const float* s = scratch.getReadPointer (ch);
                    float* r = ring.getWritePointer (ch);
                    int w = writePos;
                    for (int i = 0; i < g; ++i) { r[w] = s[i]; w = (w + 1) & mask; }
                }
                writePos = (writePos + g) & mask;
            }
        }

        // read n samples
        const int avail = (writePos - readPos) & mask;
        const int take = juce::jmin (avail, n);
        if (take < n) ++underruns;
        for (int ch = 0; ch < nc; ++ch)
        {
            const float* r = ring.getReadPointer (ch);
            float* o = out[ch];
            int p = readPos;
            for (int i = 0; i < take; ++i) { o[i] = r[p]; p = (p + 1) & mask; }
            for (int i = take; i < n; ++i) o[i] = 0.0f;
        }
        readPos = (readPos + take) & mask;
    }

private:
    static constexpr int kChunk = 128;

    static std::unique_ptr<RubberBand::RubberBandStretcher> make (double sr, int ch, Quality q)
    {
        using RB = RubberBand::RubberBandStretcher;
        RB::Options o = RB::OptionProcessRealTime | RB::OptionFormantPreserved | RB::OptionChannelsTogether;
        if (q == Quality::Formant) o |= RB::OptionEngineFiner | RB::OptionWindowShort;
        else                       o |= RB::OptionEngineFaster | RB::OptionPitchHighConsistency;
        return std::make_unique<RB> ((size_t) sr, (size_t) ch, o);
    }

    std::unique_ptr<RubberBand::RubberBandStretcher> rb;
    juce::AudioBuffer<float> ring, scratch, inChunk;
    double sampleRate = 48000.0, lastScale = -1.0, lastFormant = -1.0;
    int channels = 2, latency = 0, ringSize = 0, readPos = 0, writePos = 0, underruns = 0;
    Quality quality = Quality::Pitch;
};

// ------------------------------------------------------------ HardTune
// YIN on a decimated mono copy of the input. Gives the semitone
// correction that snaps the sung pitch to the nearest note
// (chromatic), with a retune time for the glide.
class HardTune
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        decim = juce::jmax (1, (int) std::round (sr / 16000.0));
        rate = sr / decim;
        win = 240;                       // 15 ms @ 16 kHz
        hist.assign ((size_t) (win * 3), 0.0f);
        diff.assign ((size_t) win, 0.0f);
        reset();
    }
    void reset()
    {
        std::fill (hist.begin(), hist.end(), 0.0f);
        hp = 0; acc = 0.0f; accN = 0; sinceHop = 0; correction = 0.0f; target = 0.0f; lp1 = lp2 = 0.0f;
    }
    void setRetuneMs (float ms) noexcept { retuneMs = juce::jmax (1.0f, ms); }

    // Push input (mono mix). Returns correction (semitones) to apply at the end of this block.
    float process (const float* mono, int n)
    {
        const float a = 0.25f;  // simple 2-pole LP before decimation
        for (int i = 0; i < n; ++i)
        {
            lp1 += a * (mono[i] - lp1);
            lp2 += a * (lp1 - lp2);
            acc += lp2; ++accN;
            if (accN == decim)
            {
                hist[(size_t) hp] = acc / (float) decim;
                hp = (hp + 1) % (int) hist.size();
                acc = 0.0f; accN = 0;
                if (++sinceHop >= 80) { sinceHop = 0; analyse(); }   // 5 ms hop
            }
        }
        const float k = 1.0f - std::exp (-(float) n / (float) (retuneMs * 0.001 * sampleRate));
        correction += k * (target - correction);
        return correction;
    }

private:
    void analyse()
    {
        const int W = win, N = (int) hist.size();
        const int tauMaxAll = juce::jmin (W - 1, (int) (rate / 75.0));
        const int base = hp + N - (W + tauMaxAll + 1);   // newest W + tauMax samples
        // energy gate
        double e = 0;
        for (int i = 0; i < W; ++i) { const float v = hist[(size_t) ((base + tauMaxAll + i) % N)]; e += v * v; }
        if (e / W < 1e-6) { target *= 0.9f; return; }

        const int tauMin = (int) (rate / 1000.0), tauMax = tauMaxAll;
        float running = 0.0f;
        diff[0] = 1.0f;
        int best = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double d = 0;
            for (int i = 0; i < W; ++i)
            {
                const float x = hist[(size_t) ((base + i) % N)] - hist[(size_t) ((base + i + tau) % N)];
                d += x * x;
            }
            running += (float) d;
            diff[(size_t) tau] = running > 0.0f ? (float) d * tau / running : 1.0f;
            if (tau >= tauMin && best < 0 && diff[(size_t) tau] < 0.15f)
            {
                // walk to the local minimum
                int t = tau;
                while (t + 1 <= tauMax)
                {
                    double d2 = 0;
                    for (int i = 0; i < W; ++i)
                    {
                        const float x = hist[(size_t) ((base + i) % N)] - hist[(size_t) ((base + i + t + 1) % N)];
                        d2 += x * x;
                    }
                    const float nd = (float) d2 * (t + 1) / (running + (float) d2);
                    if (nd >= diff[(size_t) t]) break;
                    running += (float) d2;
                    diff[(size_t) (t + 1)] = nd;
                    ++t;
                }
                best = t;
                break;
            }
        }
        if (best <= 0) return;   // unvoiced: hold
        // parabolic interpolation: one decimated sample is ~25 cents at 220 Hz
        auto dAt = [&] (int t)
        {
            double d = 0;
            for (int i = 0; i < W; ++i)
            {
                const float x = hist[(size_t) ((base + i) % N)] - hist[(size_t) ((base + i + t) % N)];
                d += x * x;
            }
            return d;
        };
        double tauF = best;
        if (best > 1 && best < tauMax)
        {
            const double a = dAt (best - 1), b = dAt (best), c = dAt (best + 1);
            const double den = a - 2.0 * b + c;
            if (den > 1e-12) tauF = best + 0.5 * (a - c) / den;
        }
        const double f0 = rate / tauF;
        const double midi = 69.0 + 12.0 * std::log2 (f0 / 440.0);
        target = (float) (std::round (midi) - midi);
    }

    std::vector<float> hist, diff;
    double sampleRate = 48000.0, rate = 16000.0;
    int decim = 3, win = 640, hp = 0, accN = 0, sinceHop = 0;
    float acc = 0.0f, correction = 0.0f, target = 0.0f, retuneMs = 8.0f, lp1 = 0.0f, lp2 = 0.0f;
};

// ------------------------------------------------------------ Doubler
class Doubler
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        size = juce::nextPowerOfTwo ((int) (0.08 * sr) + maxBlock);
        buf.setSize (2, size);
        reset();
    }
    void reset() { buf.clear(); pos = 0; ph[0] = 0.0; ph[1] = 0.37; }

    void process (juce::AudioBuffer<float>& b, float detune, float delayMs, float mix)
    {
        if (mix <= 0.0f) return;
        const int n = b.getNumSamples(), nc = b.getNumChannels(), mask = size - 1;
        // modulation depth for ~ (detune*120) cents of pitch wander at 0.3 / 0.23 Hz
        const double rates[2] = { 0.31, 0.23 };
        const double baseMs[2] = { juce::jmax (6.0, (double) delayMs), juce::jmax (9.0, delayMs * 1.6) };
        for (int i = 0; i < n; ++i)
        {
            const float inL = b.getSample (0, i), inR = b.getSample (nc > 1 ? 1 : 0, i);
            const float mono = 0.5f * (inL + inR);
            buf.setSample (0, pos, mono);
            float tap[2];
            for (int v = 0; v < 2; ++v)
            {
                const double cents = detune * 120.0;
                const double depth = (std::pow (2.0, cents / 1200.0) - 1.0) * sampleRate / (2.0 * juce::MathConstants<double>::pi * rates[v]);
                const double d = baseMs[v] * 0.001 * sampleRate + depth * (1.0 + std::sin (ph[v]));
                ph[v] += 2.0 * juce::MathConstants<double>::pi * rates[v] / sampleRate;
                const double rp = pos - d;
                const int i0 = (int) std::floor (rp);
                const float fr = (float) (rp - i0);
                const float a = buf.getSample (0, i0 & mask), c = buf.getSample (0, (i0 + 1) & mask);
                tap[v] = a + fr * (c - a);
            }
            b.setSample (0, i, inL + mix * tap[0]);
            if (nc > 1) b.setSample (1, i, inR + mix * tap[1]);
            pos = (pos + 1) & mask;
        }
        for (auto& p : ph) if (p > 1e6) p = std::fmod (p, 2.0 * juce::MathConstants<double>::pi);
        b.applyGain (1.0f / (1.0f + 0.5f * mix));
    }

private:
    juce::AudioBuffer<float> buf;
    double sampleRate = 48000.0, ph[2] = { 0.0, 0.37 };
    int size = 0, pos = 0;
};

// ------------------------------------------------------------ StereoEcho
class StereoEcho
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        size = juce::nextPowerOfTwo ((int) (2.1 * sr));
        buf.setSize (2, size);
        reset();
    }
    void reset() { buf.clear(); pos = 0; lp[0] = lp[1] = 0.0f; }

    // timeMs: left time; right = time * spread. mix: wet level. fb: feedback.
    void process (juce::AudioBuffer<float>& b, float timeMs, float spread, float fb, float mix)
    {
        if (mix <= 0.0f) return;
        const int n = b.getNumSamples(), nc = b.getNumChannels(), mask = size - 1;
        const int dL = juce::jlimit (1, size - 1, (int) (timeMs * 0.001 * sampleRate));
        const int dR = juce::jlimit (1, size - 1, (int) (timeMs * spread * 0.001 * sampleRate));
        const float damp = 0.35f;
        for (int i = 0; i < n; ++i)
        {
            const float inL = b.getSample (0, i), inR = b.getSample (nc > 1 ? 1 : 0, i);
            float yL = buf.getSample (0, (pos - dL) & mask);
            float yR = buf.getSample (1, (pos - dR) & mask);
            lp[0] += damp * (yL - lp[0]); lp[1] += damp * (yR - lp[1]);
            buf.setSample (0, pos, inL + fb * lp[1]);    // cross-feed: ping-pong
            buf.setSample (1, pos, inR + fb * lp[0]);
            b.setSample (0, i, inL + mix * lp[0]);
            if (nc > 1) b.setSample (1, i, inR + mix * lp[1]);
            pos = (pos + 1) & mask;
        }
    }

private:
    juce::AudioBuffer<float> buf;
    double sampleRate = 48000.0;
    int size = 0, pos = 0;
    float lp[2] = {};
};

// ------------------------------------------------------------ Drift
// Smoothed random walk in semitones; feed into the shifter.
class Drift
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { value = 0.0f; target = 0.0f; countdown = 0; }
    float next (int n, float depthSemis)
    {
        if (depthSemis <= 0.0f) { value *= 0.9f; return value; }
        countdown -= n;
        if (countdown <= 0)
        {
            countdown = (int) (sampleRate * (0.18 + 0.25 * rng.nextFloat()));
            target = (rng.nextFloat() * 2.0f - 1.0f) * depthSemis;
        }
        const float k = 1.0f - std::exp (-(float) n / (float) (0.12 * sampleRate));
        value += k * (target - value);
        return value;
    }
private:
    juce::Random rng { 12345 };
    double sampleRate = 48000.0;
    float value = 0.0f, target = 0.0f;
    int countdown = 0;
};

// ------------------------------------------------------------ StutterGate
// 1/16-note slices; every 2nd slice of each beat repeats the slice before it.
class StutterGate
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        hist.setSize (2, juce::nextPowerOfTwo ((int) (sr * 1.2)));
        reset();
    }
    void reset() { hist.clear(); pos = 0; samplesDone = 0; }

    void process (juce::AudioBuffer<float>& b, double bpm, float amount)
    {
        if (amount <= 0.0f) return;
        const int n = b.getNumSamples(), nc = juce::jmin (2, b.getNumChannels()), mask = hist.getNumSamples() - 1;
        const int slice = juce::jmax (64, (int) (sampleRate * 60.0 / juce::jlimit (60.0, 200.0, bpm) / 4.0));
        const int fade = juce::jmin (slice / 4, (int) (0.003 * sampleRate));
        for (int i = 0; i < n; ++i)
        {
            const long s = samplesDone + i;
            const int k = (int) ((s / slice) % 4);
            const int inSlice = (int) (s % slice);
            const bool repeat = (k == 1 || k == 3);
            // envelope: fade at both edges of a repeated slice and at its neighbours' joins
            float env = 1.0f;
            if (inSlice < fade) env = (float) inSlice / (float) fade;
            else if (inSlice > slice - fade) env = (float) (slice - inSlice) / (float) fade;
            for (int ch = 0; ch < nc; ++ch)
            {
                const float x = b.getSample (ch, i);
                hist.setSample (ch, pos, x);
                if (repeat)
                {
                    const float r = hist.getSample (ch, (pos - slice) & mask);
                    const float y = r * env;
                    b.setSample (ch, i, x * (1.0f - amount) + y * amount);
                }
                else
                    b.setSample (ch, i, x * (1.0f - amount + amount * env));
            }
            pos = (pos + 1) & mask;
        }
        samplesDone += n;
    }
private:
    juce::AudioBuffer<float> hist;
    double sampleRate = 48000.0;
    int pos = 0;
    long samplesDone = 0;
};

// ------------------------------------------------------------ WarmDrive
// Parallel tanh saturation, level compensated, with a low-pass on the
// saturated path so the added harmonics stay warm instead of fizzy.
class WarmDrive
{
public:
    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        filters.clear();
        for (int i = 0; i < numChannels * 2; ++i) filters.emplace_back();
        setTone (5000.0f);
        reset();
    }
    void reset() { for (auto& f : filters) f.reset(); }
    void setTone (float hz)
    {
        hz = juce::jlimit (1500.0f, 12000.0f, hz);
        if (std::abs (hz - toneHz) < 1.0f) return;
        toneHz = hz;
        auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, hz, 0.6f);
        for (auto& f : filters) f.coefficients = c;
    }
    // amount 0..1
    void process (juce::AudioBuffer<float>& b, float amount)
    {
        if (amount <= 0.001f) return;
        amount = juce::jlimit (0.0f, 1.0f, amount);
        const float drive = 1.0f + amount * 7.0f;
        const float norm = 1.0f / std::tanh (drive * 0.5f);   // keep ~-6 dBFS peaks at similar level
        const float wet = juce::jlimit (0.0f, 0.85f, 0.25f + amount * 0.9f);
        const int n = b.getNumSamples();
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            float* x = b.getWritePointer (ch);
            auto& f1 = filters[(size_t) (ch * 2)];
            auto& f2 = filters[(size_t) (ch * 2 + 1)];
            for (int i = 0; i < n; ++i)
            {
                const float s = std::tanh (x[i] * drive) * norm * 0.5f;
                const float t = f2.processSample (f1.processSample (s));
                x[i] = x[i] * (1.0f - wet) + t * wet;
            }
        }
    }
private:
    std::vector<juce::dsp::IIR::Filter<float>> filters;
    double sampleRate = 48000.0;
    float toneHz = -1.0f;
};

} // namespace AV2
