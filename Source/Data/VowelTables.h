#pragma once
#include <JuceHeader.h>
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
//  VOWEL TABLES
//  Formant frequencies (Hz) for F1–F4 and bandwidths (Hz) for each vowel
//  across four ancient vocal traditions.
//
//  Vowel index: 0=A  1=E  2=I  3=O  4=U
//  Era    index: 0=Sumerian  1=Egyptian  2=Byzantine  3=Vedic
// ─────────────────────────────────────────────────────────────────────────────

enum class AncientEra { Sumerian=0, Egyptian=1, Byzantine=2, Vedic=3, COUNT=4 };

struct FormantPoint {
    float f1, f2, f3, f4;          // center frequencies Hz
    float b1, b2, b3, b4;          // bandwidths Hz
};

// ── SUMERIAN — open, chest-resonant, ritual drone quality ────────────────────
static constexpr std::array<FormantPoint,5> kSumerianVowels = {{
    // A          E          I          O          U
    {730,1090,2440,3400,  80,70,160,300},   // A — wide open
    {530, 980,2480,3500,  60,80,170,280},   // E — forward
    {270,2290,3010,3500,  50,90,160,270},   // I — bright narrow
    {570, 840,2410,3400,  70,80,170,290},   // O — rounded
    {300, 870,2240,3200,  60,70,160,280},   // U — dark rounded
}};

// ── EGYPTIAN — nasal, forward placement, ceremonial brightness ───────────────
static constexpr std::array<FormantPoint,5> kEgyptianVowels = {{
    {760,1180,2500,3500,  90,75,180,310},   // A — bright open
    {560,1050,2530,3550,  70,85,185,295},   // E — nasal forward
    {290,2350,3080,3580,  55,95,175,280},   // I — very bright
    {600, 900,2450,3450,  80,85,180,300},   // O — nasal rounded
    {330, 940,2300,3250,  65,75,165,285},   // U — dark nasal
}};

// ── BYZANTINE — choral, chest-mixed, liturgical warmth ──────────────────────
static constexpr std::array<FormantPoint,5> kByzantineVowels = {{
    {700,1060,2400,3350,  75,65,155,285},   // A — warm chest
    {510, 960,2460,3450,  55,75,165,270},   // E — liturgical clear
    {255,2260,2980,3470,  45,85,155,260},   // I — piercing choir
    {550, 820,2380,3380,  65,75,165,280},   // O — warm rounded
    {285, 850,2210,3180,  55,65,155,270},   // U — deep covered
}};

// ── VEDIC — chant, throat resonance, overtone emphasis ──────────────────────
static constexpr std::array<FormantPoint,5> kVedicVowels = {{
    {750,1120,2460,3420,  85,72,162,295},   // A — open Sanskrit a
    {545,1000,2500,3520,  62,82,172,282},   // E — Sanskrit e
    {280,2310,3020,3510,  52,92,162,272},   // I — Sanskrit i
    {585, 860,2420,3410,  72,82,172,285},   // O — Sanskrit o
    {315, 890,2260,3210,  60,72,160,275},   // U — Sanskrit u
}};

// ── ACCESSOR ─────────────────────────────────────────────────────────────────
inline const FormantPoint& getFormant(AncientEra era, int vowelIdx)
{
    vowelIdx = juce::jlimit(0, 4, vowelIdx);
    switch (era) {
        case AncientEra::Sumerian:  return kSumerianVowels [size_t(vowelIdx)];
        case AncientEra::Egyptian:  return kEgyptianVowels [size_t(vowelIdx)];
        case AncientEra::Byzantine: return kByzantineVowels[size_t(vowelIdx)];
        case AncientEra::Vedic:     return kVedicVowels    [size_t(vowelIdx)];
        default:                    return kSumerianVowels [size_t(vowelIdx)];
    }
}

// Interpolate between two adjacent vowel positions (0.0–4.0)
inline FormantPoint interpolateFormant(AncientEra era, float vowelPos)
{
    vowelPos = juce::jlimit(0.f, 4.f, vowelPos);
    int lo = int(vowelPos);
    int hi = juce::jmin(lo + 1, 4);
    float t = vowelPos - float(lo);

    const auto& A = getFormant(era, lo);
    const auto& B = getFormant(era, hi);

    auto lerp = [&](float a, float b) { return a + t * (b - a); };

    return {
        lerp(A.f1,B.f1), lerp(A.f2,B.f2), lerp(A.f3,B.f3), lerp(A.f4,B.f4),
        lerp(A.b1,B.b1), lerp(A.b2,B.b2), lerp(A.b3,B.b3), lerp(A.b4,B.b4)
    };
}
