// AVRender: runs every Ancient Voices profile on a synthetic sung vocal and measures
// artefacts. Usage: AVRender <outdir> [profileIndex] [userPitch]
#include <JuceHeader.h>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <random>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "../Source/DSPModules.h"
#define private public
#define protected public
#include "../Source/PluginProcessor.h"
#undef private
#undef protected

static double SR = 48000.0;
static int BS = 512;

// ---- synthetic singer: glottal pulse train -> vowel formants, vibrato, jitter, breath
static juce::AudioBuffer<float> makeVocal (double seconds)
{
    const int N = (int) (seconds * SR);
    juce::AudioBuffer<float> out (2, N);
    out.clear();
    std::mt19937 rng (7);
    std::normal_distribution<float> gauss (0.0f, 1.0f);
    // vowels: F1..F3 (Hz) and bandwidths
    const float vow[5][3] = { {730,1090,2440}, {530,1840,2480}, {270,2290,3010}, {570,840,2410}, {300,870,2240} };
    const float notes[] = { 57, 60, 62, 64, 62, 60, 57, 55, 57, 60, 64, 65 }; // midi
    const double noteLen = 0.6, gap = 0.12;
    double ph = 0.0;
    // two-pole resonators state per formant
    double y1[3] = {}, y2[3] = {};
    for (int n = 0; n < N; ++n)
    {
        const double t = n / SR;
        const int k = (int) (t / noteLen);
        const double within = t - k * noteLen;
        const bool silentSection = (t > 4.4 && t < 5.2);          // a real pause to measure the noise floor / tails
        double env = 0.0;
        if (! silentSection && within < noteLen - gap)
        {
            const double a = std::min (1.0, within / 0.04), r = std::min (1.0, (noteLen - gap - within) / 0.06);
            env = std::min (a, r);
        }
        const float midi = notes[k % 12];
        const double vib = 0.3 * std::sin (2 * M_PI * 5.5 * t) * std::min (1.0, within / 0.25);
        const double f0 = 440.0 * std::pow (2.0, (midi - 69 + vib) / 12.0) * (1.0 + 0.003 * gauss (rng));
        ph += f0 / SR;
        double src = 0.0;
        if (ph >= 1.0) { ph -= 1.0; }
        // Rosenberg-like glottal derivative approximation: smooth pulse
        const double op = 0.6;
        if (ph < op) src = std::sin (M_PI * ph / op) * std::sin (M_PI * ph / op);
        src = src - 0.32;                            // remove DC-ish
        src += 0.04 * gauss (rng);                  // breath
        const auto& v = vow[k % 5];
        double y = 0.0;
        for (int f = 0; f < 3; ++f)
        {
            const double bw = 80.0 + 40.0 * f;
            const double R = std::exp (-M_PI * bw / SR);
            const double c = 2 * R * std::cos (2 * M_PI * v[f] / SR);
            const double yn = (1 - R) * src + c * y1[f] - R * R * y2[f];
            y2[f] = y1[f]; y1[f] = yn;
            y += yn * (f == 0 ? 1.0 : (f == 1 ? 0.6 : 0.35));
        }
        const float s = (float) (y * env * 0.9);
        out.setSample (0, n, s);
        out.setSample (1, n, s);
    }
    // normalise to -12 dBFS peak
    const float pk = out.getMagnitude (0, N);
    out.applyGain (0.25f / pk);
    return out;
}

struct Metrics { double rmsDb, peakDb, clicks, dryClicks, gapDb, nanCount, dropouts; double corrDry; };

static double rmsDb (const float* d, int a, int b)
{
    double s = 0; for (int i = a; i < b; ++i) s += d[i] * (double) d[i];
    return 10 * std::log10 (s / std::max (1, b - a) + 1e-20);
}

// discontinuity detector: |x[n]-2x[n-1]+x[n-2]| relative to a local level estimate.
static int countClicks (const float* d, int N)
{
    int c = 0; double lvl = 1e-4;
    for (int n = 2; n < N; ++n)
    {
        const double sd = std::abs (d[n] - 2 * d[n - 1] + d[n - 2]);
        lvl = 0.999 * lvl + 0.001 * sd;
        if (sd > 12.0 * lvl + 0.02) { ++c; n += 32; }
    }
    return c;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (auto* e = std::getenv ("AV_SR")) SR = atof (e);
    if (auto* e = std::getenv ("AV_BS")) BS = atoi (e);
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("/tmp/avr"));
    outDir.createDirectory();
    const int only = argc > 2 ? atoi (argv[2]) : -1;
    const float userPitch = argc > 3 ? (float) atof (argv[3]) : 0.0f;

    auto vocal = makeVocal (8.0);
    const int N = vocal.getNumSamples();
    const int dryClicks = countClicks (vocal.getReadPointer (0), N);
    {
        juce::WavAudioFormat wav;
        auto f = outDir.getChildFile ("dry.wav"); f.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (vocal, 0, N);
    }

    printf ("idx,name,rmsDb,peakDb,clicks,gapDb,dropouts,nans,latency,underruns,cpu\n");
    for (int p = 0; p < 60; ++p)
    {
        if (only >= 0 && p != only) continue;
        AncientVoicesAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, SR, BS);
        proc.prepareToPlay (SR, BS);
        if (auto* prm = proc.getAPVTS().getParameter ("PROFILE_INDEX")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) p));
        if (auto* prm = proc.getAPVTS().getParameter ("USER_PITCH")) prm->setValueNotifyingHost (prm->convertTo0to1 (userPitch));
        for (auto id : { "USER_REVERB", "USER_DELAY", "USER_DRIVE", "MIX" })
            if (auto* e = std::getenv (id))
                if (auto* prm = proc.getAPVTS().getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) atof (e)));
        const int tail = (int) (2.0 * SR);
        juce::AudioBuffer<float> out (2, N + tail);
        out.clear();
        juce::AudioBuffer<float> blk (2, BS);
        juce::MidiBuffer midi;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int pos = 0; pos < N + tail; pos += BS)
        {
            const int n = std::min (BS, N + tail - pos);
            blk.setSize (2, n, false, false, true);
            blk.clear();
            for (int ch = 0; ch < 2; ++ch)
                if (pos < N) blk.copyFrom (ch, 0, vocal, ch, pos, std::min (n, N - pos));
            if (std::getenv ("AV_SWITCH") != nullptr && (pos / BS) % 47 == 0)
                if (auto* prm = proc.getAPVTS().getParameter ("PROFILE_INDEX")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ((pos / BS / 47 * 17 + p) % 60)));
            proc.processBlock (blk, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, blk, ch, 0, n);
        }
        const double cpu = (juce::Time::getMillisecondCounterHiRes() - t0) / ((N + tail) / SR * 1000.0);
        // remove the reported latency so timings line up with the dry signal
        const int lat = proc.getLatencySamples();
        for (int ch = 0; ch < 2; ++ch) { auto* d = out.getWritePointer (ch); std::memmove (d, d + lat, sizeof (float) * (size_t) (out.getNumSamples() - lat)); std::fill (d + out.getNumSamples() - lat, d + out.getNumSamples(), 0.0f); }
        const float* L = out.getReadPointer (0);
        int nans = 0; for (int i = 0; i < out.getNumSamples(); ++i) if (! std::isfinite (L[i])) ++nans;
        // dropouts: runs of >= 64 exact zeros while the input is singing (t in 0.2..4.2 s)
        int drop = 0, run = 0;
        for (int i = (int) (0.3 * SR); i < (int) (4.2 * SR); ++i) { if (L[i] == 0.0f) { if (++run == 64) ++drop; } else run = 0; }
        const double r = rmsDb (L, 0, N), pk = 20 * std::log10 (out.getMagnitude (0, 0, out.getNumSamples()) + 1e-12);
        const double gap = rmsDb (L, (int) (4.9 * SR), (int) (5.15 * SR));
        const int clicks = countClicks (L, out.getNumSamples());
        auto name = juce::String (p < 10 ? "MMV" : "AV") + juce::String (p);
        printf ("%d,%s,%.1f,%.1f,%d,%.1f,%d,%d,%d,%d,%.3f\n", p, name.toRawUTF8(), r, pk, clicks, gap, drop, nans, proc.getLatencySamples(),
                proc.mainShifter.getUnderruns() + proc.harmShifterA.getUnderruns() + proc.harmShifterB.getUnderruns(), cpu);
        fflush (stdout);
        juce::WavAudioFormat wav;
        auto f = outDir.getChildFile (juce::String::formatted ("p%02d.wav", p)); f.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    }
    fprintf (stderr, "dry clicks: %d\n", dryClicks);
    return 0;
}
