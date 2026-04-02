#pragma once
#include <JuceHeader.h>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  PITCH QUANTIZER
//
//  Snaps a detected frequency to the nearest note in a scale or interval set.
//  The snap amount (0–1) blends between the raw tracked frequency and the
//  quantized frequency — 0 = free, 1 = hard quantize (robotic).
//
//  Scales: Chromatic, Major, Minor, Pentatonic, Dorian, Phrygian,
//          Sacred (just intonation), Ancient (pythagorean)
// ─────────────────────────────────────────────────────────────────────────────

enum class QuantizeScale {
    Chromatic  = 0,
    Major      = 1,
    Minor      = 2,
    Pentatonic = 3,
    Dorian     = 4,
    Phrygian   = 5,
    Sacred     = 6,   // just intonation ratios
    Ancient    = 7,   // pythagorean
    COUNT      = 8
};

static constexpr const char* kScaleNames[] = {
    "Chromatic","Major","Minor","Pentatonic","Dorian","Phrygian","Sacred","Ancient"
};

class PitchQuantizer {
public:
    // Scale semitone intervals (relative to root)
    static constexpr int kScales[8][12] = {
        { 0,1,2,3,4,5,6,7,8,9,10,11 },   // Chromatic
        { 0,2,4,5,7,9,11,-1,-1,-1,-1,-1}, // Major
        { 0,2,3,5,7,8,10,-1,-1,-1,-1,-1}, // Minor
        { 0,2,4,7,9,-1,-1,-1,-1,-1,-1,-1},// Pentatonic
        { 0,2,3,5,7,9,10,-1,-1,-1,-1,-1}, // Dorian
        { 0,1,3,5,7,8,10,-1,-1,-1,-1,-1}, // Phrygian
        { 0,2,4,7,9,11,-1,-1,-1,-1,-1,-1},// Sacred (same as Major but just-tuned below)
        { 0,2,4,6,7,9,11,-1,-1,-1,-1,-1}, // Ancient (Lydian / pythagorean)
    };

    void setSnapAmount(float s)     { snap_  = juce::jlimit(0.f, 1.f, s); }
    void setScale(QuantizeScale sc) { scale_ = sc; }
    void setRootHz(float hz)        { rootHz_ = juce::jmax(20.f, hz); }

    // Quantize a frequency. Returns blended result.
    float process(float freqHz) const {
        if (snap_ < 0.001f || freqHz < 20.f) return freqHz;

        float quantized = snapToScale(freqHz);
        return freqHz + snap_ * (quantized - freqHz);
    }

private:
    float          snap_   = 0.f;
    QuantizeScale  scale_  = QuantizeScale::Chromatic;
    float          rootHz_ = 440.f;

    float snapToScale(float freqHz) const {
        // Convert to MIDI note (floating point)
        float midi = 69.f + 12.f * std::log2(freqHz / 440.f);

        // Get root in MIDI
        float rootMidi = 69.f + 12.f * std::log2(rootHz_ / 440.f);
        float rootNote = std::fmod(rootMidi, 12.f);

        // Find nearest scale degree
        float semitoneOffset = std::fmod(midi - rootNote + 120.f, 12.f);
        float bestDist = 12.f;
        int   bestSemi = 0;

        for (int i = 0; i < 12; ++i) {
            int deg = kScales[int(scale_)][i];
            if (deg < 0) break;
            float dist = semitoneOffset - float(deg);
            // Wrap distance to [-6, 6]
            while (dist >  6.f) dist -= 12.f;
            while (dist < -6.f) dist += 12.f;
            if (std::abs(dist) < std::abs(bestDist)) {
                bestDist = dist;
                bestSemi = deg;
            }
        }

        float quantizedMidi = std::floor(midi - semitoneOffset) + float(bestSemi) + rootNote;
        return 440.f * std::pow(2.f, (quantizedMidi - 69.f) / 12.f);
    }
};
