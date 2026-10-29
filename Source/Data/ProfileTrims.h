// ProfileTrims.h - per-voice output trims (dB) that level-match every voice to
// the input. Measured with Tests/render.cpp on a reference sung phrase at the
// default knob settings. Regenerate after changing a voice.
#pragma once
namespace AncientVoicesTrims
{
    static constexpr int kCount = 60;
    static constexpr float kTrimDb[kCount] = {
        1.5f, 3.0f, 0.5f, 1.0f, -0.5f, -1.0f, -0.0f, 4.0f, 0.0f, 2.0f,
        -4.0f, -2.5f, -8.0f, -6.0f, 2.0f, -5.5f, -8.5f, 3.0f, 9.5f, -7.5f,
        -3.0f, -4.0f, -12.0f, 2.5f, -9.0f, 0.0f, -5.5f, -1.0f, -7.0f, 10.0f,
        -2.0f, 1.0f, -3.5f, -7.5f, -10.5f, -10.0f, -5.0f, 6.0f, -8.0f, -1.0f,
        1.0f, -2.0f, -3.5f, 3.5f, -3.5f, -6.0f, 9.5f, -11.0f, 10.5f, -4.0f,
        -10.0f, 1.0f, -7.0f, -11.5f, -5.0f, -9.0f, -9.5f, -1.5f, -2.0f, -4.5f
    };
}
