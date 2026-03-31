#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
//  PRESET SYSTEM
//  Each preset stores a snapshot of all APVTS parameter values by ID.
//  Categories: Chant | Drone | Choir | Ritual | Breath
// ─────────────────────────────────────────────────────────────────────────────

struct ParamSnap {
    const char* id;
    float       value;
};

struct AVPreset {
    const char* name;
    const char* category;
    const ParamSnap* params;
    int numParams;
};

// ─────────────────────────────────────────────────────────────────────────────
//  FACTORY PRESETS
// ─────────────────────────────────────────────────────────────────────────────

// ── CHANT ────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kSumerianChant[] = {
    {"vowel_pos",0.3f},{"vowel_open",0.8f},{"breathe",0.1f},{"roughness",0.04f},{"gender",0.95f},
    {"num_voices",4},{"interval",3},{"spread",0.55f},{"drift",0.35f},
    {"era",0},{"ritual",0.2f},{"chant",0.4f},
    {"env_attack",0.08f},{"env_decay",0.3f},{"env_sustain",0.9f},{"env_release",1.8f},
    {"cave_mix",0.45f},{"cave_size",0.72f},{"cave_damp",0.35f},{"cave_echo",28.f},
    {"shimmer_mix",0.12f},{"shimmer_amt",0.5f},
    {"master_vol",0.82f},{"master_gain",0.f},
};
static constexpr ParamSnap kByzantineChoir[] = {
    {"vowel_pos",1.2f},{"vowel_open",0.75f},{"breathe",0.08f},{"roughness",0.03f},{"gender",1.05f},
    {"num_voices",6},{"interval",3},{"spread",0.7f},{"drift",0.28f},
    {"era",2},{"ritual",0.1f},{"chant",0.2f},
    {"env_attack",0.12f},{"env_decay",0.4f},{"env_sustain",0.88f},{"env_release",2.5f},
    {"cave_mix",0.5f},{"cave_size",0.8f},{"cave_damp",0.3f},{"cave_echo",35.f},
    {"shimmer_mix",0.18f},{"shimmer_amt",0.6f},
    {"master_vol",0.8f},{"master_gain",0.f},
};
static constexpr ParamSnap kVedicMantras[] = {
    {"vowel_pos",0.1f},{"vowel_open",0.9f},{"breathe",0.05f},{"roughness",0.06f},{"gender",0.88f},
    {"num_voices",3},{"interval",1},{"spread",0.4f},{"drift",0.22f},
    {"era",3},{"ritual",0.3f},{"chant",0.55f},
    {"env_attack",0.04f},{"env_decay",0.2f},{"env_sustain",0.95f},{"env_release",1.4f},
    {"cave_mix",0.38f},{"cave_size",0.6f},{"cave_damp",0.45f},{"cave_echo",18.f},
    {"shimmer_mix",0.08f},{"shimmer_amt",0.4f},
    {"master_vol",0.85f},{"master_gain",0.f},
};

// ── DRONE ────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kStoneDrone[] = {
    {"vowel_pos",3.8f},{"vowel_open",0.6f},{"breathe",0.18f},{"roughness",0.08f},{"gender",0.82f},
    {"num_voices",8},{"interval",6},{"spread",0.9f},{"drift",0.45f},
    {"era",0},{"ritual",0.0f},{"chant",0.0f},
    {"env_attack",0.6f},{"env_decay",0.8f},{"env_sustain",1.0f},{"env_release",4.f},
    {"cave_mix",0.6f},{"cave_size",0.9f},{"cave_damp",0.25f},{"cave_echo",60.f},
    {"shimmer_mix",0.25f},{"shimmer_amt",0.7f},
    {"master_vol",0.75f},{"master_gain",0.f},
};
static constexpr ParamSnap kTempleOm[] = {
    {"vowel_pos",3.2f},{"vowel_open",0.7f},{"breathe",0.12f},{"roughness",0.05f},{"gender",0.90f},
    {"num_voices",6},{"interval",6},{"spread",0.75f},{"drift",0.38f},
    {"era",3},{"ritual",0.15f},{"chant",0.3f},
    {"env_attack",0.4f},{"env_decay",0.5f},{"env_sustain",1.0f},{"env_release",5.f},
    {"cave_mix",0.55f},{"cave_size",0.85f},{"cave_damp",0.3f},{"cave_echo",45.f},
    {"shimmer_mix",0.22f},{"shimmer_amt",0.65f},
    {"master_vol",0.78f},{"master_gain",0.f},
};

// ── CHOIR ────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kAncientChoir[] = {
    {"vowel_pos",0.8f},{"vowel_open",0.72f},{"breathe",0.1f},{"roughness",0.04f},{"gender",1.0f},
    {"num_voices",8},{"interval",4},{"spread",0.85f},{"drift",0.32f},
    {"era",2},{"ritual",0.08f},{"chant",0.25f},
    {"env_attack",0.1f},{"env_decay",0.35f},{"env_sustain",0.85f},{"env_release",2.0f},
    {"cave_mix",0.42f},{"cave_size",0.75f},{"cave_damp",0.32f},{"cave_echo",30.f},
    {"shimmer_mix",0.15f},{"shimmer_amt",0.55f},
    {"master_vol",0.8f},{"master_gain",0.f},
};
static constexpr ParamSnap kFemaleChoir[] = {
    {"vowel_pos",2.0f},{"vowel_open",0.8f},{"breathe",0.12f},{"roughness",0.02f},{"gender",1.3f},
    {"num_voices",6},{"interval",4},{"spread",0.8f},{"drift",0.28f},
    {"era",1},{"ritual",0.05f},{"chant",0.15f},
    {"env_attack",0.06f},{"env_decay",0.25f},{"env_sustain",0.9f},{"env_release",1.6f},
    {"cave_mix",0.35f},{"cave_size",0.65f},{"cave_damp",0.38f},{"cave_echo",22.f},
    {"shimmer_mix",0.2f},{"shimmer_amt",0.6f},
    {"master_vol",0.82f},{"master_gain",0.f},
};

// ── RITUAL ───────────────────────────────────────────────────────────────────
static constexpr ParamSnap kSacredRitual[] = {
    {"vowel_pos",1.5f},{"vowel_open",0.65f},{"breathe",0.2f},{"roughness",0.12f},{"gender",0.85f},
    {"num_voices",5},{"interval",5},{"spread",0.65f},{"drift",0.5f},
    {"era",0},{"ritual",0.7f},{"chant",0.6f},
    {"env_attack",0.15f},{"env_decay",0.5f},{"env_sustain",0.8f},{"env_release",3.f},
    {"cave_mix",0.55f},{"cave_size",0.82f},{"cave_damp",0.28f},{"cave_echo",40.f},
    {"shimmer_mix",0.3f},{"shimmer_amt",0.8f},
    {"master_vol",0.77f},{"master_gain",0.f},
};
static constexpr ParamSnap kShamanicCall[] = {
    {"vowel_pos",0.5f},{"vowel_open",0.85f},{"breathe",0.25f},{"roughness",0.18f},{"gender",0.78f},
    {"num_voices",3},{"interval",1},{"spread",0.5f},{"drift",0.55f},
    {"era",0},{"ritual",0.6f},{"chant",0.7f},
    {"env_attack",0.02f},{"env_decay",0.15f},{"env_sustain",0.75f},{"env_release",1.0f},
    {"cave_mix",0.48f},{"cave_size",0.7f},{"cave_damp",0.42f},{"cave_echo",15.f},
    {"shimmer_mix",0.1f},{"shimmer_amt",0.35f},
    {"master_vol",0.85f},{"master_gain",2.f},
};

// ── BREATH ───────────────────────────────────────────────────────────────────
static constexpr ParamSnap kBreathOfGods[] = {
    {"vowel_pos",2.5f},{"vowel_open",0.5f},{"breathe",0.55f},{"roughness",0.03f},{"gender",1.1f},
    {"num_voices",4},{"interval",0},{"spread",0.7f},{"drift",0.42f},
    {"era",1},{"ritual",0.0f},{"chant",0.1f},
    {"env_attack",0.3f},{"env_decay",0.6f},{"env_sustain",0.7f},{"env_release",3.5f},
    {"cave_mix",0.5f},{"cave_size",0.78f},{"cave_damp",0.22f},{"cave_echo",50.f},
    {"shimmer_mix",0.35f},{"shimmer_amt",0.9f},
    {"master_vol",0.72f},{"master_gain",0.f},
};
static constexpr ParamSnap kWhisperOfReed[] = {
    {"vowel_pos",1.8f},{"vowel_open",0.55f},{"breathe",0.45f},{"roughness",0.06f},{"gender",1.15f},
    {"num_voices",2},{"interval",0},{"spread",0.4f},{"drift",0.35f},
    {"era",3},{"ritual",0.05f},{"chant",0.0f},
    {"env_attack",0.2f},{"env_decay",0.4f},{"env_sustain",0.65f},{"env_release",2.5f},
    {"cave_mix",0.3f},{"cave_size",0.55f},{"cave_damp",0.5f},{"cave_echo",12.f},
    {"shimmer_mix",0.18f},{"shimmer_amt",0.5f},
    {"master_vol",0.78f},{"master_gain",0.f},
};

// ─────────────────────────────────────────────────────────────────────────────
//  PRESET TABLE
// ─────────────────────────────────────────────────────────────────────────────
#define PRESET(name, cat, arr) { name, cat, arr, int(std::size(arr)) }

static constexpr AVPreset kAVPresets[] = {
    // Chant
    PRESET("Sumerian Chant",   "Chant",  kSumerianChant),
    PRESET("Byzantine Choir",  "Chant",  kByzantineChoir),
    PRESET("Vedic Mantras",    "Chant",  kVedicMantras),
    // Drone
    PRESET("Stone Drone",      "Drone",  kStoneDrone),
    PRESET("Temple Om",        "Drone",  kTempleOm),
    // Choir
    PRESET("Ancient Choir",    "Choir",  kAncientChoir),
    PRESET("Female Choir",     "Choir",  kFemaleChoir),
    // Ritual
    PRESET("Sacred Ritual",    "Ritual", kSacredRitual),
    PRESET("Shamanic Call",    "Ritual", kShamanicCall),
    // Breath
    PRESET("Breath Of Gods",   "Breath", kBreathOfGods),
    PRESET("Whisper Of Reed",  "Breath", kWhisperOfReed),
};

static constexpr int kNumAVPresets = int(std::size(kAVPresets));

// ─────────────────────────────────────────────────────────────────────────────
//  PRESET MANAGER
// ─────────────────────────────────────────────────────────────────────────────
class AVPresetManager {
public:
    AVPresetManager() = default;

    int count() const { return kNumAVPresets; }

    const AVPreset& get(int idx) const {
        return kAVPresets[juce::jlimit(0, kNumAVPresets - 1, idx)];
    }

    int current() const { return currentIdx_; }

    void apply(int idx, juce::AudioProcessorValueTreeState& apvts) {
        const auto& preset = get(idx);
        for (int i = 0; i < preset.numParams; ++i) {
            const auto& snap = preset.params[i];
            if (auto* p = apvts.getParameter(snap.id))
                p->setValueNotifyingHost(
                    p->getNormalisableRange().convertTo0to1(snap.value));
        }
        currentIdx_ = idx;
    }

    void next(juce::AudioProcessorValueTreeState& apvts) {
        apply((currentIdx_ + 1) % kNumAVPresets, apvts);
    }
    void prev(juce::AudioProcessorValueTreeState& apvts) {
        apply((currentIdx_ - 1 + kNumAVPresets) % kNumAVPresets, apvts);
    }

private:
    int currentIdx_ = 0;
};
