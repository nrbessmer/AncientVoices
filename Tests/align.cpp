#include <JuceHeader.h>
#include "../Source/VoiceEngine.h"
#include <cstdio>
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000; const int N = 96256, imp = 30000;
    for (auto q : { AV2::StreamingShifter::Quality::Pitch, AV2::StreamingShifter::Quality::Formant })
    for (float semis : { 0.0f, 5.0f, -12.0f })
    {
        const int L = AV2::StreamingShifter::measureLatency (sr, q);
        AV2::StreamingShifter s; s.prepare (sr, 1, 512, L, q);
        std::vector<float> x (N, 0.0f), y (N);
        for (int i = 0; i < 400; ++i) x[imp + i] = std::sin (i * 0.3f) * std::sin (M_PI * i / 400.0); // short burst
        for (int p = 0; p < N; p += 512) { const float* ip[1] = { x.data() + p }; float* op[1] = { y.data() + p };
            if (q == AV2::StreamingShifter::Quality::Pitch) s.process (ip, op, 1, 512, semis, semis, 0); else s.process (ip, op, 1, 512, 0, 0, semis * 0.3f); }
        // energy centroid of output burst
        double num = 0, den = 0; for (int i = 0; i < N; ++i) { num += i * (double) y[i] * y[i]; den += (double) y[i] * y[i]; }
        printf ("q%d semis %+.0f latency %d  burst centre %.0f  expected %d\n", (int) q, semis, L, num / den, imp + 200 + L);
    }
}
