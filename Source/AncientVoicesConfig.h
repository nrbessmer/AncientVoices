#pragma once

// =============================================================================
//  ANCIENT VOICES — CENTRALISED CONFIGURATION  v3
// =============================================================================
namespace AVC {

// ── Formant filter ────────────────────────────────────────────────────────────
static constexpr float kFormantPeakGain = 3.5f;

// ── Wet path gain ─────────────────────────────────────────────────────────────
//  v2: kWetBoost = 4.0f stacked +12 dB on top of normGain_, which already
//      targets input-level compensation. The limiter ran constantly even on
//      clean signal, colouring the sound permanently.
//  v3: kWetBoost = 1.0f. normGain_ inside FormantFilter handles all level
//      compensation. The limiter is now a true safety net, not a fixture.
static constexpr float kWetGain  = 1.0f;
static constexpr float kWetBoost = 1.0f;

// ── AGC ───────────────────────────────────────────────────────────────────────
static constexpr float kAGCMinGain   = 0.5f;
static constexpr float kAGCMaxGain   = 6.0f;
static constexpr float kAGCSmoothSec = 0.15f;
static constexpr float kNormMax      = 20.0f;

// ── Micro-detune ──────────────────────────────────────────────────────────────
static constexpr float kMicroDetuneRange = 7.0f;

// ── Breathe noise ─────────────────────────────────────────────────────────────
static constexpr float kBreatheNoiseLevel = 0.12f;

// ── Shimmer ───────────────────────────────────────────────────────────────────
static constexpr float kShimmerPitchMin = 1.5f;
static constexpr float kShimmerPitchMax = 2.0f;

// ── Saturation ────────────────────────────────────────────────────────────────
static constexpr float kSatScale    = 0.2f;
static constexpr float kSatDriveMax = 0.4f;

// ── Chorus ────────────────────────────────────────────────────────────────────
static constexpr float kChorusMix        = 0.35f;
static constexpr float kChorusRateBase   = 0.4f;
static constexpr float kChorusRateScale  = 0.4f;
static constexpr float kChorusDepthScale = 0.4f;
static constexpr float kChorusCentreMs   = 4.0f;

// ── Limiter ───────────────────────────────────────────────────────────────────
static constexpr float kLimiterThresholdDb = -1.0f;
static constexpr float kLimiterReleaseMs   = 50.0f;

// ── Humanizer ─────────────────────────────────────────────────────────────────
static constexpr float kRoughnessMin = 0.02f;
static constexpr float kRoughnessMax = 0.17f;

// ── Param cache throttle ──────────────────────────────────────────────────────
static constexpr int kParamUpdateBlocks = 8;

// ── Voice modes ───────────────────────────────────────────────────────────────
enum class VoiceMode { Ancient=0, Human=1, Robot=2, Ethereal=3 };

struct ModeParams {
    float formantDepthMul;
    float chorusMixMul;
    float satDriveMul;
    float shimmerMixMul;
    float bwMul;
};
static constexpr ModeParams kModeParams[4] = {
    // depthMul  choMul  satMul  shimMul  bwMul
    { 1.0f,  1.0f,  1.2f,  1.0f,  0.8f },   // Ancient
    { 0.8f,  2.0f,  0.2f,  0.8f,  1.2f },   // Human
    { 1.0f,  0.0f,  4.0f,  0.4f,  0.15f},   // Robot
    { 0.5f,  0.6f,  0.1f,  5.0f,  1.5f },   // Ethereal
};

// ── Defaults ──────────────────────────────────────────────────────────────────
namespace Defaults {
    static constexpr float kVowelPos     = 0.f;
    static constexpr float kVowelOpen    = 1.f;
    static constexpr float kGender       = 1.f;
    static constexpr float kBreathe      = 0.1f;
    static constexpr float kRoughness    = 0.05f;
    static constexpr float kFormantShift = 0.f;
    static constexpr float kFormantDepth = 0.8f;
    static constexpr int   kEra          = 0;
    static constexpr int   kNumVoices    = 2;
    static constexpr float kSpread       = 0.6f;
    static constexpr float kDrift        = 0.6f;
    static constexpr float kChant        = 0.f;
    static constexpr int   kInterval     = 0;
    static constexpr int   kVoiceMode    = 0;
    static constexpr float kMix          = 1.0f;
    static constexpr float kCaveMix      = 0.6f;
    static constexpr float kCaveSize     = 0.85f;
    static constexpr float kCaveDamp     = 0.4f;
    static constexpr float kCaveEchoMs   = 20.f;
    static constexpr float kShimmerMix   = 0.0f;
    static constexpr float kShimmerAmt   = 0.5f;
    static constexpr float kMasterVol    = 1.f;
    static constexpr float kMasterGainDb = 0.f;
    // kSnapAmount / kSnapScale removed — never registered in createParams()
}

} // namespace AVC

// =============================================================================
//  PARAMETER IDs
//  Single source of truth for every parameter string. Defined here so all
//  translation units that already include AncientVoicesConfig.h can use
//  ParamID::kFoo without an extra include or .jucer file change.
// =============================================================================
namespace ParamID {
    constexpr const char* kVowelPos     = "vowel_pos";
    constexpr const char* kVowelOpen    = "vowel_open";
    constexpr const char* kGender       = "gender";
    constexpr const char* kBreathe      = "breathe";
    constexpr const char* kRoughness    = "roughness";
    constexpr const char* kFormantShift = "formant_shift";
    constexpr const char* kFormantDepth = "formant_depth";
    constexpr const char* kEra          = "era";
    constexpr const char* kVoiceMode    = "voice_mode";
    constexpr const char* kNumVoices    = "num_voices";
    constexpr const char* kSpread       = "spread";
    constexpr const char* kDrift        = "drift";
    constexpr const char* kChant        = "chant";
    constexpr const char* kInterval     = "interval";
    constexpr const char* kMix          = "mix";
    constexpr const char* kCaveMix      = "cave_mix";
    constexpr const char* kCaveSize     = "cave_size";
    constexpr const char* kCaveDamp     = "cave_damp";
    constexpr const char* kCaveEcho     = "cave_echo";
    constexpr const char* kShimmerMix   = "shimmer_mix";
    constexpr const char* kShimmerAmt   = "shimmer_amt";
    constexpr const char* kEnvAttack    = "env_attack";
    constexpr const char* kEnvDecay     = "env_decay";
    constexpr const char* kEnvSustain   = "env_sustain";
    constexpr const char* kEnvRelease   = "env_release";
    constexpr const char* kMasterVol    = "master_vol";
    constexpr const char* kMasterGain   = "master_gain";
} // namespace ParamID
