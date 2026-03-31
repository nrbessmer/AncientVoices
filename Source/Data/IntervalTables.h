#pragma once
#include <JuceHeader.h>
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
//  INTERVAL TABLES
//  Frequency ratios for choir voice stacking.
//  Each mode defines up to 8 ratio offsets from the root (ratio 1.0).
//  Ratios are in just intonation / pythagorean, not equal temperament.
// ─────────────────────────────────────────────────────────────────────────────

enum class ChoirInterval {
    Unison    = 0,   // all voices on root (humanized)
    Fifth     = 1,   // root + perfect 5th (3:2)
    Octave    = 2,   // root + octave
    Sacred    = 3,   // root + 4th + 5th (pythagorean power chord)
    JustTriad = 4,   // root + just 3rd + just 5th
    Ancient   = 5,   // Sumerian-inspired quartal stack
    Drone     = 6,   // root + sub-octave + 5th + octave
    COUNT     = 7
};

static constexpr const char* kIntervalNames[] = {
    "Unison","Fifth","Octave","Sacred","Just Triad","Ancient","Drone"
};

// Ratios for up to 8 voices per mode
// Voice 0 is always root (1.0)
struct IntervalMode {
    const char* name;
    std::array<float,8> ratios;  // frequency multiplier per voice slot
};

static constexpr std::array<IntervalMode, 7> kIntervalModes = {{
    // Unison — all voices root, humanization provides width
    {"Unison",     {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f}},

    // Fifth — alternates root and perfect 5th (3:2)
    {"Fifth",      {1.f, 1.5f, 1.f, 1.5f, 0.5f, 1.5f, 1.f, 1.5f}},

    // Octave — alternates root and octave above/below
    {"Octave",     {1.f, 2.f, 0.5f, 2.f, 1.f, 2.f, 0.5f, 1.f}},

    // Sacred — root + 4th (4:3) + 5th (3:2) + octave
    {"Sacred",     {1.f, 1.3333f, 1.5f, 2.f, 1.f, 1.3333f, 1.5f, 2.f}},

    // Just Triad — root + just major 3rd (5:4) + just 5th (3:2)
    {"Just Triad", {1.f, 1.25f, 1.5f, 2.f, 0.5f, 1.25f, 1.5f, 2.f}},

    // Ancient — quartal stack inspired by Sumerian tetrachords
    // Ratios: 1 : 9/8 : 4/3 : 3/2 : 27/16
    {"Ancient",    {1.f, 1.125f, 1.3333f, 1.5f, 1.6875f, 2.f, 1.125f, 1.5f}},

    // Drone — sub-octave + root + 5th + octave (tamboura style)
    {"Drone",      {0.5f, 1.f, 1.5f, 2.f, 0.5f, 1.f, 1.5f, 2.f}},
}};

inline float getVoiceRatio(ChoirInterval mode, int voiceIndex)
{
    int m = juce::jlimit(0, 6, int(mode));
    int v = juce::jlimit(0, 7, voiceIndex);
    return kIntervalModes[size_t(m)].ratios[size_t(v)];
}
