#include <JuceHeader.h>
#include "../Source/VoiceEngine.h"
#include <cstdio>
static int countClicks (const float* d, int N, int from)
{
    int c = 0; double lvl = 1e-4;
    for (int n = 2; n < N; ++n) { const double sd = std::abs (d[n] - 2 * d[n - 1] + d[n - 2]); lvl = 0.999 * lvl + 0.001 * sd; if (n > from && sd > 12.0 * lvl + 0.02) { ++c; n += 32; } }
    return c;
}
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File ("/home/claude/avbuild/after/dry.wav")));
    const int N = (int) r->lengthInSamples; juce::AudioBuffer<float> x (1, N); r->read (&x, 0, N, 0, true, false);
    const double sr = 48000;
    printf ("dry clicks %d\n", countClicks (x.getReadPointer (0), N, 24000));
    for (auto q : { AV2::StreamingShifter::Quality::Pitch, AV2::StreamingShifter::Quality::Formant })
    for (float semis : { 0.075f, 0.5f })
    for (double stepS : { 0.0 })
    for (int bs : { 512 })
    {
        const int L = 6000;
        AV2::StreamingShifter s; s.prepare (sr, 1, 1024, L, q);
        juce::AudioBuffer<float> y (1, N);
        float last = semis;
        for (int p = 0; p < N; p += bs) { const int n = juce::jmin (bs, N - p); const float* ip[1] = { x.getReadPointer (0) + p }; float* op[1] = { y.getWritePointer (0) + p };
            const float t = semis == 0.075f ? 0.075f * std::sin (p / sr * 3.0f) : 0.5f * std::sin (p / sr * 2 * M_PI * 5.0); if (q == AV2::StreamingShifter::Quality::Pitch) s.process (ip, op, 1, n, last, t, 0.0f); else s.process (ip, op, 1, n, 0, 0, -3.0f + 2.0f * std::sin (p / sr * 2 * M_PI * 0.1)); last = t; }
        printf ("step %.2f q%d semis %+6.3f bs %d: clicks %d underruns %d\n", stepS, (int) q, semis, bs, countClicks (y.getReadPointer (0) + L, N - L, 24000), s.getUnderruns());
    }
}
