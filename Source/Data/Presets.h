#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
//  PRESET SYSTEM  — Effect version
//  Parameters: vowel_pos, vowel_open, gender, era, num_voices, spread,
//              drift, chant, mix, cave_mix, cave_size, cave_damp, cave_echo,
//              shimmer_mix, shimmer_amt, master_vol, master_gain
//
//  50 factory presets across 7 categories:
//    Chant (8) | Drone (8) | Choir (8) | Ritual (7) |
//    Breath (7) | Shimmer (6) | Dark (6)
// ─────────────────────────────────────────────────────────────────────────────

struct ParamSnap { const char* id; float value; };
struct AVPreset  { const char* name; const char* category; const ParamSnap* params; int numParams; };

// Helper macro
#define P(id,v) {id,v}
#define PRESET(name,cat,arr) {name, cat, arr, int(std::size(arr))}

// ─────────────────────────────────────────────────────────────────────────────
//  CHANT  (8 presets)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kSumerianChant[] = {
    P("vowel_pos",0.3f),P("vowel_open",0.85f),P("gender",0.95f),P("era",0),
    P("num_voices",4),P("spread",0.55f),P("drift",0.35f),P("chant",0.4f),P("mix",0.85f),
    P("cave_mix",0.45f),P("cave_size",0.72f),P("cave_damp",0.35f),P("cave_echo",28.f),
    P("shimmer_mix",0.12f),P("shimmer_amt",0.5f),P("master_vol",0.82f),P("master_gain",0.f),
};
static constexpr ParamSnap kByzantineChant[] = {
    P("vowel_pos",1.2f),P("vowel_open",0.75f),P("gender",1.05f),P("era",2),
    P("num_voices",6),P("spread",0.7f),P("drift",0.28f),P("chant",0.2f),P("mix",0.8f),
    P("cave_mix",0.5f),P("cave_size",0.8f),P("cave_damp",0.3f),P("cave_echo",35.f),
    P("shimmer_mix",0.18f),P("shimmer_amt",0.6f),P("master_vol",0.8f),P("master_gain",0.f),
};
static constexpr ParamSnap kVedicMantra[] = {
    P("vowel_pos",0.1f),P("vowel_open",0.9f),P("gender",0.88f),P("era",3),
    P("num_voices",3),P("spread",0.4f),P("drift",0.22f),P("chant",0.55f),P("mix",0.78f),
    P("cave_mix",0.38f),P("cave_size",0.6f),P("cave_damp",0.45f),P("cave_echo",18.f),
    P("shimmer_mix",0.08f),P("shimmer_amt",0.4f),P("master_vol",0.85f),P("master_gain",0.f),
};
static constexpr ParamSnap kEgyptianHymn[] = {
    P("vowel_pos",0.8f),P("vowel_open",0.8f),P("gender",1.0f),P("era",1),
    P("num_voices",5),P("spread",0.6f),P("drift",0.3f),P("chant",0.35f),P("mix",0.82f),
    P("cave_mix",0.42f),P("cave_size",0.68f),P("cave_damp",0.38f),P("cave_echo",22.f),
    P("shimmer_mix",0.1f),P("shimmer_amt",0.45f),P("master_vol",0.82f),P("master_gain",0.f),
};
static constexpr ParamSnap kGregorianDawn[] = {
    P("vowel_pos",2.0f),P("vowel_open",0.7f),P("gender",1.1f),P("era",2),
    P("num_voices",6),P("spread",0.65f),P("drift",0.25f),P("chant",0.3f),P("mix",0.75f),
    P("cave_mix",0.55f),P("cave_size",0.85f),P("cave_damp",0.28f),P("cave_echo",40.f),
    P("shimmer_mix",0.2f),P("shimmer_amt",0.55f),P("master_vol",0.78f),P("master_gain",0.f),
};
static constexpr ParamSnap kOracleVoice[] = {
    P("vowel_pos",3.0f),P("vowel_open",0.65f),P("gender",0.9f),P("era",0),
    P("num_voices",4),P("spread",0.5f),P("drift",0.4f),P("chant",0.5f),P("mix",0.88f),
    P("cave_mix",0.48f),P("cave_size",0.75f),P("cave_damp",0.32f),P("cave_echo",32.f),
    P("shimmer_mix",0.15f),P("shimmer_amt",0.5f),P("master_vol",0.8f),P("master_gain",0.f),
};
static constexpr ParamSnap kSacredNam[] = {
    P("vowel_pos",0.5f),P("vowel_open",0.95f),P("gender",0.85f),P("era",3),
    P("num_voices",3),P("spread",0.35f),P("drift",0.18f),P("chant",0.65f),P("mix",0.72f),
    P("cave_mix",0.35f),P("cave_size",0.55f),P("cave_damp",0.5f),P("cave_echo",14.f),
    P("shimmer_mix",0.06f),P("shimmer_amt",0.3f),P("master_vol",0.88f),P("master_gain",0.f),
};
static constexpr ParamSnap kPriestessCall[] = {
    P("vowel_pos",1.8f),P("vowel_open",0.78f),P("gender",1.25f),P("era",1),
    P("num_voices",4),P("spread",0.58f),P("drift",0.32f),P("chant",0.42f),P("mix",0.8f),
    P("cave_mix",0.44f),P("cave_size",0.7f),P("cave_damp",0.4f),P("cave_echo",26.f),
    P("shimmer_mix",0.14f),P("shimmer_amt",0.52f),P("master_vol",0.82f),P("master_gain",0.f),
};

// ─────────────────────────────────────────────────────────────────────────────
//  DRONE  (8 presets)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kStoneDrone[] = {
    P("vowel_pos",3.8f),P("vowel_open",0.6f),P("gender",0.82f),P("era",0),
    P("num_voices",8),P("spread",0.9f),P("drift",0.45f),P("chant",0.0f),P("mix",0.9f),
    P("cave_mix",0.6f),P("cave_size",0.9f),P("cave_damp",0.25f),P("cave_echo",60.f),
    P("shimmer_mix",0.25f),P("shimmer_amt",0.7f),P("master_vol",0.75f),P("master_gain",0.f),
};
static constexpr ParamSnap kTempleOm[] = {
    P("vowel_pos",3.2f),P("vowel_open",0.7f),P("gender",0.90f),P("era",3),
    P("num_voices",6),P("spread",0.75f),P("drift",0.38f),P("chant",0.3f),P("mix",0.85f),
    P("cave_mix",0.55f),P("cave_size",0.85f),P("cave_damp",0.3f),P("cave_echo",45.f),
    P("shimmer_mix",0.22f),P("shimmer_amt",0.65f),P("master_vol",0.78f),P("master_gain",0.f),
};
static constexpr ParamSnap kDeepCaveWind[] = {
    P("vowel_pos",4.0f),P("vowel_open",0.55f),P("gender",0.75f),P("era",0),
    P("num_voices",8),P("spread",1.0f),P("drift",0.55f),P("chant",0.0f),P("mix",0.95f),
    P("cave_mix",0.7f),P("cave_size",0.95f),P("cave_damp",0.2f),P("cave_echo",80.f),
    P("shimmer_mix",0.3f),P("shimmer_amt",0.8f),P("master_vol",0.7f),P("master_gain",0.f),
};
static constexpr ParamSnap kZigguratEcho[] = {
    P("vowel_pos",2.8f),P("vowel_open",0.62f),P("gender",0.86f),P("era",0),
    P("num_voices",7),P("spread",0.85f),P("drift",0.5f),P("chant",0.15f),P("mix",0.88f),
    P("cave_mix",0.65f),P("cave_size",0.88f),P("cave_damp",0.22f),P("cave_echo",55.f),
    P("shimmer_mix",0.28f),P("shimmer_amt",0.75f),P("master_vol",0.72f),P("master_gain",0.f),
};
static constexpr ParamSnap kNileWaters[] = {
    P("vowel_pos",2.2f),P("vowel_open",0.68f),P("gender",0.92f),P("era",1),
    P("num_voices",6),P("spread",0.8f),P("drift",0.42f),P("chant",0.1f),P("mix",0.82f),
    P("cave_mix",0.58f),P("cave_size",0.82f),P("cave_damp",0.28f),P("cave_echo",48.f),
    P("shimmer_mix",0.2f),P("shimmer_amt",0.6f),P("master_vol",0.75f),P("master_gain",0.f),
};
static constexpr ParamSnap kVaultOfHeaven[] = {
    P("vowel_pos",1.5f),P("vowel_open",0.72f),P("gender",1.0f),P("era",2),
    P("num_voices",8),P("spread",0.95f),P("drift",0.48f),P("chant",0.05f),P("mix",0.9f),
    P("cave_mix",0.62f),P("cave_size",0.92f),P("cave_damp",0.18f),P("cave_echo",70.f),
    P("shimmer_mix",0.35f),P("shimmer_amt",0.85f),P("master_vol",0.7f),P("master_gain",0.f),
};
static constexpr ParamSnap kUnderworld[] = {
    P("vowel_pos",3.5f),P("vowel_open",0.5f),P("gender",0.72f),P("era",0),
    P("num_voices",8),P("spread",1.0f),P("drift",0.6f),P("chant",0.0f),P("mix",1.0f),
    P("cave_mix",0.75f),P("cave_size",1.0f),P("cave_damp",0.15f),P("cave_echo",90.f),
    P("shimmer_mix",0.4f),P("shimmer_amt",0.9f),P("master_vol",0.68f),P("master_gain",0.f),
};
static constexpr ParamSnap kPrimordialOm[] = {
    P("vowel_pos",3.0f),P("vowel_open",0.65f),P("gender",0.88f),P("era",3),
    P("num_voices",7),P("spread",0.88f),P("drift",0.52f),P("chant",0.2f),P("mix",0.87f),
    P("cave_mix",0.6f),P("cave_size",0.88f),P("cave_damp",0.25f),P("cave_echo",62.f),
    P("shimmer_mix",0.26f),P("shimmer_amt",0.72f),P("master_vol",0.74f),P("master_gain",0.f),
};

// ─────────────────────────────────────────────────────────────────────────────
//  CHOIR  (8 presets)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kAncientChoir[] = {
    P("vowel_pos",0.8f),P("vowel_open",0.72f),P("gender",1.0f),P("era",2),
    P("num_voices",8),P("spread",0.85f),P("drift",0.32f),P("chant",0.25f),P("mix",0.8f),
    P("cave_mix",0.42f),P("cave_size",0.75f),P("cave_damp",0.32f),P("cave_echo",30.f),
    P("shimmer_mix",0.15f),P("shimmer_amt",0.55f),P("master_vol",0.8f),P("master_gain",0.f),
};
static constexpr ParamSnap kFemaleChoir[] = {
    P("vowel_pos",2.0f),P("vowel_open",0.8f),P("gender",1.3f),P("era",1),
    P("num_voices",6),P("spread",0.8f),P("drift",0.28f),P("chant",0.15f),P("mix",0.78f),
    P("cave_mix",0.35f),P("cave_size",0.65f),P("cave_damp",0.38f),P("cave_echo",22.f),
    P("shimmer_mix",0.2f),P("shimmer_amt",0.6f),P("master_vol",0.82f),P("master_gain",0.f),
};
static constexpr ParamSnap kMaleEnsemble[] = {
    P("vowel_pos",0.4f),P("vowel_open",0.65f),P("gender",0.78f),P("era",2),
    P("num_voices",7),P("spread",0.82f),P("drift",0.35f),P("chant",0.2f),P("mix",0.82f),
    P("cave_mix",0.48f),P("cave_size",0.78f),P("cave_damp",0.3f),P("cave_echo",28.f),
    P("shimmer_mix",0.12f),P("shimmer_amt",0.48f),P("master_vol",0.8f),P("master_gain",0.f),
};
static constexpr ParamSnap kMixedVoices[] = {
    P("vowel_pos",1.4f),P("vowel_open",0.75f),P("gender",1.0f),P("era",2),
    P("num_voices",8),P("spread",0.9f),P("drift",0.3f),P("chant",0.22f),P("mix",0.78f),
    P("cave_mix",0.45f),P("cave_size",0.77f),P("cave_damp",0.33f),P("cave_echo",32.f),
    P("shimmer_mix",0.16f),P("shimmer_amt",0.52f),P("master_vol",0.8f),P("master_gain",0.f),
};
static constexpr ParamSnap kSoloContralto[] = {
    P("vowel_pos",1.0f),P("vowel_open",0.82f),P("gender",1.15f),P("era",2),
    P("num_voices",2),P("spread",0.3f),P("drift",0.18f),P("chant",0.1f),P("mix",0.7f),
    P("cave_mix",0.3f),P("cave_size",0.58f),P("cave_damp",0.42f),P("cave_echo",16.f),
    P("shimmer_mix",0.1f),P("shimmer_amt",0.4f),P("master_vol",0.85f),P("master_gain",0.f),
};
static constexpr ParamSnap kAncientTenors[] = {
    P("vowel_pos",0.6f),P("vowel_open",0.7f),P("gender",0.92f),P("era",0),
    P("num_voices",5),P("spread",0.72f),P("drift",0.3f),P("chant",0.18f),P("mix",0.8f),
    P("cave_mix",0.4f),P("cave_size",0.7f),P("cave_damp",0.35f),P("cave_echo",24.f),
    P("shimmer_mix",0.12f),P("shimmer_amt",0.45f),P("master_vol",0.82f),P("master_gain",0.f),
};
static constexpr ParamSnap kEchoingHall[] = {
    P("vowel_pos",2.5f),P("vowel_open",0.68f),P("gender",1.0f),P("era",2),
    P("num_voices",8),P("spread",0.95f),P("drift",0.38f),P("chant",0.28f),P("mix",0.85f),
    P("cave_mix",0.55f),P("cave_size",0.88f),P("cave_damp",0.25f),P("cave_echo",44.f),
    P("shimmer_mix",0.22f),P("shimmer_amt",0.62f),P("master_vol",0.76f),P("master_gain",0.f),
};
static constexpr ParamSnap kPsalm[] = {
    P("vowel_pos",1.6f),P("vowel_open",0.72f),P("gender",1.05f),P("era",2),
    P("num_voices",6),P("spread",0.75f),P("drift",0.26f),P("chant",0.3f),P("mix",0.76f),
    P("cave_mix",0.46f),P("cave_size",0.76f),P("cave_damp",0.34f),P("cave_echo",30.f),
    P("shimmer_mix",0.14f),P("shimmer_amt",0.5f),P("master_vol",0.8f),P("master_gain",0.f),
};

// ─────────────────────────────────────────────────────────────────────────────
//  RITUAL  (7 presets)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kSacredRitual[] = {
    P("vowel_pos",1.5f),P("vowel_open",0.65f),P("gender",0.85f),P("era",0),
    P("num_voices",5),P("spread",0.65f),P("drift",0.5f),P("chant",0.6f),P("mix",0.9f),
    P("cave_mix",0.55f),P("cave_size",0.82f),P("cave_damp",0.28f),P("cave_echo",40.f),
    P("shimmer_mix",0.3f),P("shimmer_amt",0.8f),P("master_vol",0.77f),P("master_gain",0.f),
};
static constexpr ParamSnap kShamanicCall[] = {
    P("vowel_pos",0.5f),P("vowel_open",0.85f),P("gender",0.78f),P("era",0),
    P("num_voices",3),P("spread",0.5f),P("drift",0.55f),P("chant",0.7f),P("mix",0.88f),
    P("cave_mix",0.48f),P("cave_size",0.7f),P("cave_damp",0.42f),P("cave_echo",15.f),
    P("shimmer_mix",0.1f),P("shimmer_amt",0.35f),P("master_vol",0.85f),P("master_gain",2.f),
};
static constexpr ParamSnap kFireCeremony[] = {
    P("vowel_pos",2.0f),P("vowel_open",0.7f),P("gender",0.88f),P("era",0),
    P("num_voices",4),P("spread",0.6f),P("drift",0.58f),P("chant",0.65f),P("mix",0.87f),
    P("cave_mix",0.52f),P("cave_size",0.78f),P("cave_damp",0.3f),P("cave_echo",22.f),
    P("shimmer_mix",0.25f),P("shimmer_amt",0.7f),P("master_vol",0.8f),P("master_gain",0.f),
};
static constexpr ParamSnap kLunarRite[] = {
    P("vowel_pos",3.5f),P("vowel_open",0.6f),P("gender",1.1f),P("era",1),
    P("num_voices",5),P("spread",0.7f),P("drift",0.48f),P("chant",0.55f),P("mix",0.85f),
    P("cave_mix",0.5f),P("cave_size",0.8f),P("cave_damp",0.25f),P("cave_echo",38.f),
    P("shimmer_mix",0.28f),P("shimmer_amt",0.75f),P("master_vol",0.78f),P("master_gain",0.f),
};
static constexpr ParamSnap kSolsticeGate[] = {
    P("vowel_pos",1.0f),P("vowel_open",0.68f),P("gender",0.92f),P("era",0),
    P("num_voices",6),P("spread",0.72f),P("drift",0.52f),P("chant",0.48f),P("mix",0.88f),
    P("cave_mix",0.56f),P("cave_size",0.84f),P("cave_damp",0.26f),P("cave_echo",36.f),
    P("shimmer_mix",0.22f),P("shimmer_amt",0.65f),P("master_vol",0.76f),P("master_gain",0.f),
};
static constexpr ParamSnap kAnunnakiChant[] = {
    P("vowel_pos",0.2f),P("vowel_open",0.9f),P("gender",0.8f),P("era",0),
    P("num_voices",7),P("spread",0.8f),P("drift",0.6f),P("chant",0.7f),P("mix",0.92f),
    P("cave_mix",0.62f),P("cave_size",0.9f),P("cave_damp",0.22f),P("cave_echo",50.f),
    P("shimmer_mix",0.32f),P("shimmer_amt",0.82f),P("master_vol",0.74f),P("master_gain",0.f),
};
static constexpr ParamSnap kSerpentDance[] = {
    P("vowel_pos",2.8f),P("vowel_open",0.62f),P("gender",0.95f),P("era",1),
    P("num_voices",4),P("spread",0.55f),P("drift",0.62f),P("chant",0.75f),P("mix",0.9f),
    P("cave_mix",0.5f),P("cave_size",0.75f),P("cave_damp",0.32f),P("cave_echo",28.f),
    P("shimmer_mix",0.18f),P("shimmer_amt",0.6f),P("master_vol",0.8f),P("master_gain",0.f),
};

// ─────────────────────────────────────────────────────────────────────────────
//  BREATH  (7 presets)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kBreathOfGods[] = {
    P("vowel_pos",2.5f),P("vowel_open",0.5f),P("gender",1.1f),P("era",1),
    P("num_voices",4),P("spread",0.7f),P("drift",0.42f),P("chant",0.1f),P("mix",0.82f),
    P("cave_mix",0.5f),P("cave_size",0.78f),P("cave_damp",0.22f),P("cave_echo",50.f),
    P("shimmer_mix",0.35f),P("shimmer_amt",0.9f),P("master_vol",0.72f),P("master_gain",0.f),
};
static constexpr ParamSnap kWhisperOfReed[] = {
    P("vowel_pos",1.8f),P("vowel_open",0.55f),P("gender",1.15f),P("era",3),
    P("num_voices",2),P("spread",0.4f),P("drift",0.35f),P("chant",0.0f),P("mix",0.72f),
    P("cave_mix",0.3f),P("cave_size",0.55f),P("cave_damp",0.5f),P("cave_echo",12.f),
    P("shimmer_mix",0.18f),P("shimmer_amt",0.5f),P("master_vol",0.78f),P("master_gain",0.f),
};
static constexpr ParamSnap kSandstormVoice[] = {
    P("vowel_pos",3.8f),P("vowel_open",0.5f),P("gender",0.85f),P("era",1),
    P("num_voices",5),P("spread",0.85f),P("drift",0.65f),P("chant",0.0f),P("mix",0.88f),
    P("cave_mix",0.45f),P("cave_size",0.72f),P("cave_damp",0.4f),P("cave_echo",20.f),
    P("shimmer_mix",0.2f),P("shimmer_amt",0.55f),P("master_vol",0.8f),P("master_gain",0.f),
};
static constexpr ParamSnap kAncientBreath[] = {
    P("vowel_pos",0.3f),P("vowel_open",0.52f),P("gender",0.9f),P("era",0),
    P("num_voices",3),P("spread",0.5f),P("drift",0.38f),P("chant",0.05f),P("mix",0.75f),
    P("cave_mix",0.38f),P("cave_size",0.65f),P("cave_damp",0.45f),P("cave_echo",18.f),
    P("shimmer_mix",0.15f),P("shimmer_amt",0.45f),P("master_vol",0.82f),P("master_gain",0.f),
};
static constexpr ParamSnap kExhalation[] = {
    P("vowel_pos",2.2f),P("vowel_open",0.5f),P("gender",1.05f),P("era",2),
    P("num_voices",3),P("spread",0.45f),P("drift",0.3f),P("chant",0.0f),P("mix",0.68f),
    P("cave_mix",0.32f),P("cave_size",0.58f),P("cave_damp",0.48f),P("cave_echo",10.f),
    P("shimmer_mix",0.12f),P("shimmer_amt",0.38f),P("master_vol",0.85f),P("master_gain",0.f),
};
static constexpr ParamSnap kDesertWind[] = {
    P("vowel_pos",4.0f),P("vowel_open",0.5f),P("gender",0.95f),P("era",1),
    P("num_voices",6),P("spread",0.9f),P("drift",0.7f),P("chant",0.0f),P("mix",0.85f),
    P("cave_mix",0.42f),P("cave_size",0.7f),P("cave_damp",0.38f),P("cave_echo",25.f),
    P("shimmer_mix",0.22f),P("shimmer_amt",0.6f),P("master_vol",0.78f),P("master_gain",0.f),
};
static constexpr ParamSnap kCaveMist[] = {
    P("vowel_pos",1.2f),P("vowel_open",0.52f),P("gender",1.0f),P("era",0),
    P("num_voices",4),P("spread",0.62f),P("drift",0.45f),P("chant",0.08f),P("mix",0.78f),
    P("cave_mix",0.55f),P("cave_size",0.82f),P("cave_damp",0.35f),P("cave_echo",42.f),
    P("shimmer_mix",0.28f),P("shimmer_amt",0.7f),P("master_vol",0.75f),P("master_gain",0.f),
};

// ─────────────────────────────────────────────────────────────────────────────
//  SHIMMER  (6 presets)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kCelestialGate[] = {
    P("vowel_pos",2.0f),P("vowel_open",0.7f),P("gender",1.2f),P("era",2),
    P("num_voices",6),P("spread",0.88f),P("drift",0.35f),P("chant",0.1f),P("mix",0.9f),
    P("cave_mix",0.52f),P("cave_size",0.85f),P("cave_damp",0.2f),P("cave_echo",55.f),
    P("shimmer_mix",0.6f),P("shimmer_amt",1.0f),P("master_vol",0.72f),P("master_gain",0.f),
};
static constexpr ParamSnap kStarlightChoir[] = {
    P("vowel_pos",1.5f),P("vowel_open",0.75f),P("gender",1.3f),P("era",2),
    P("num_voices",7),P("spread",0.92f),P("drift",0.3f),P("chant",0.12f),P("mix",0.88f),
    P("cave_mix",0.48f),P("cave_size",0.82f),P("cave_damp",0.22f),P("cave_echo",48.f),
    P("shimmer_mix",0.55f),P("shimmer_amt",0.95f),P("master_vol",0.72f),P("master_gain",0.f),
};
static constexpr ParamSnap kGoldenAether[] = {
    P("vowel_pos",0.5f),P("vowel_open",0.8f),P("gender",1.1f),P("era",1),
    P("num_voices",5),P("spread",0.8f),P("drift",0.28f),P("chant",0.08f),P("mix",0.85f),
    P("cave_mix",0.45f),P("cave_size",0.78f),P("cave_damp",0.25f),P("cave_echo",40.f),
    P("shimmer_mix",0.5f),P("shimmer_amt",0.88f),P("master_vol",0.74f),P("master_gain",0.f),
};
static constexpr ParamSnap kOtherworld[] = {
    P("vowel_pos",3.0f),P("vowel_open",0.65f),P("gender",1.05f),P("era",3),
    P("num_voices",6),P("spread",0.85f),P("drift",0.4f),P("chant",0.15f),P("mix",0.92f),
    P("cave_mix",0.58f),P("cave_size",0.9f),P("cave_damp",0.18f),P("cave_echo",65.f),
    P("shimmer_mix",0.65f),P("shimmer_amt",1.0f),P("master_vol",0.7f),P("master_gain",0.f),
};
static constexpr ParamSnap kAscension[] = {
    P("vowel_pos",2.8f),P("vowel_open",0.72f),P("gender",1.2f),P("era",2),
    P("num_voices",8),P("spread",1.0f),P("drift",0.32f),P("chant",0.05f),P("mix",0.9f),
    P("cave_mix",0.55f),P("cave_size",0.92f),P("cave_damp",0.15f),P("cave_echo",72.f),
    P("shimmer_mix",0.7f),P("shimmer_amt",1.0f),P("master_vol",0.68f),P("master_gain",0.f),
};
static constexpr ParamSnap kDivineLight[] = {
    P("vowel_pos",1.2f),P("vowel_open",0.78f),P("gender",1.35f),P("era",2),
    P("num_voices",7),P("spread",0.9f),P("drift",0.25f),P("chant",0.1f),P("mix",0.88f),
    P("cave_mix",0.5f),P("cave_size",0.88f),P("cave_damp",0.2f),P("cave_echo",58.f),
    P("shimmer_mix",0.62f),P("shimmer_amt",0.98f),P("master_vol",0.7f),P("master_gain",0.f),
};

// ─────────────────────────────────────────────────────────────────────────────
//  DARK  (6 presets)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ParamSnap kNetherSong[] = {
    P("vowel_pos",3.8f),P("vowel_open",0.5f),P("gender",0.72f),P("era",0),
    P("num_voices",7),P("spread",0.88f),P("drift",0.65f),P("chant",0.05f),P("mix",0.95f),
    P("cave_mix",0.72f),P("cave_size",0.95f),P("cave_damp",0.18f),P("cave_echo",85.f),
    P("shimmer_mix",0.15f),P("shimmer_amt",0.4f),P("master_vol",0.7f),P("master_gain",0.f),
};
static constexpr ParamSnap kShadowTemple[] = {
    P("vowel_pos",0.2f),P("vowel_open",0.55f),P("gender",0.75f),P("era",0),
    P("num_voices",6),P("spread",0.78f),P("drift",0.58f),P("chant",0.1f),P("mix",0.9f),
    P("cave_mix",0.68f),P("cave_size",0.92f),P("cave_damp",0.2f),P("cave_echo",75.f),
    P("shimmer_mix",0.1f),P("shimmer_amt",0.35f),P("master_vol",0.72f),P("master_gain",0.f),
};
static constexpr ParamSnap kLamentOfSumer[] = {
    P("vowel_pos",1.5f),P("vowel_open",0.6f),P("gender",0.82f),P("era",0),
    P("num_voices",4),P("spread",0.62f),P("drift",0.52f),P("chant",0.3f),P("mix",0.88f),
    P("cave_mix",0.6f),P("cave_size",0.86f),P("cave_damp",0.24f),P("cave_echo",52.f),
    P("shimmer_mix",0.08f),P("shimmer_amt",0.3f),P("master_vol",0.76f),P("master_gain",0.f),
};
static constexpr ParamSnap kGatekeeperSong[] = {
    P("vowel_pos",2.5f),P("vowel_open",0.52f),P("gender",0.78f),P("era",0),
    P("num_voices",5),P("spread",0.7f),P("drift",0.6f),P("chant",0.25f),P("mix",0.92f),
    P("cave_mix",0.65f),P("cave_size",0.9f),P("cave_damp",0.22f),P("cave_echo",68.f),
    P("shimmer_mix",0.12f),P("shimmer_amt",0.38f),P("master_vol",0.74f),P("master_gain",0.f),
};
static constexpr ParamSnap kDuskRitual[] = {
    P("vowel_pos",0.8f),P("vowel_open",0.58f),P("gender",0.85f),P("era",1),
    P("num_voices",5),P("spread",0.68f),P("drift",0.55f),P("chant",0.4f),P("mix",0.9f),
    P("cave_mix",0.62f),P("cave_size",0.88f),P("cave_damp",0.2f),P("cave_echo",60.f),
    P("shimmer_mix",0.1f),P("shimmer_amt",0.32f),P("master_vol",0.75f),P("master_gain",0.f),
};
static constexpr ParamSnap kEreshkigal[] = {
    P("vowel_pos",4.0f),P("vowel_open",0.5f),P("gender",0.7f),P("era",0),
    P("num_voices",8),P("spread",1.0f),P("drift",0.7f),P("chant",0.0f),P("mix",1.0f),
    P("cave_mix",0.78f),P("cave_size",1.0f),P("cave_damp",0.12f),P("cave_echo",100.f),
    P("shimmer_mix",0.05f),P("shimmer_amt",0.2f),P("master_vol",0.68f),P("master_gain",0.f),
};

// ─────────────────────────────────────────────────────────────────────────────
//  PRESET TABLE  — 50 total
// ─────────────────────────────────────────────────────────────────────────────
static constexpr AVPreset kAVPresets[] = {
    // Chant (8)
    PRESET("Sumerian Chant",   "Chant",   kSumerianChant),
    PRESET("Byzantine Chant",  "Chant",   kByzantineChant),
    PRESET("Vedic Mantra",     "Chant",   kVedicMantra),
    PRESET("Egyptian Hymn",    "Chant",   kEgyptianHymn),
    PRESET("Gregorian Dawn",   "Chant",   kGregorianDawn),
    PRESET("Oracle Voice",     "Chant",   kOracleVoice),
    PRESET("Sacred Nam",       "Chant",   kSacredNam),
    PRESET("Priestess Call",   "Chant",   kPriestessCall),
    // Drone (8)
    PRESET("Stone Drone",      "Drone",   kStoneDrone),
    PRESET("Temple Om",        "Drone",   kTempleOm),
    PRESET("Deep Cave Wind",   "Drone",   kDeepCaveWind),
    PRESET("Ziggurat Echo",    "Drone",   kZigguratEcho),
    PRESET("Nile Waters",      "Drone",   kNileWaters),
    PRESET("Vault Of Heaven",  "Drone",   kVaultOfHeaven),
    PRESET("Underworld",       "Drone",   kUnderworld),
    PRESET("Primordial Om",    "Drone",   kPrimordialOm),
    // Choir (8)
    PRESET("Ancient Choir",    "Choir",   kAncientChoir),
    PRESET("Female Choir",     "Choir",   kFemaleChoir),
    PRESET("Male Ensemble",    "Choir",   kMaleEnsemble),
    PRESET("Mixed Voices",     "Choir",   kMixedVoices),
    PRESET("Solo Contralto",   "Choir",   kSoloContralto),
    PRESET("Ancient Tenors",   "Choir",   kAncientTenors),
    PRESET("Echoing Hall",     "Choir",   kEchoingHall),
    PRESET("Psalm",            "Choir",   kPsalm),
    // Ritual (7)
    PRESET("Sacred Ritual",    "Ritual",  kSacredRitual),
    PRESET("Shamanic Call",    "Ritual",  kShamanicCall),
    PRESET("Fire Ceremony",    "Ritual",  kFireCeremony),
    PRESET("Lunar Rite",       "Ritual",  kLunarRite),
    PRESET("Solstice Gate",    "Ritual",  kSolsticeGate),
    PRESET("Anunnaki Chant",   "Ritual",  kAnunnakiChant),
    PRESET("Serpent Dance",    "Ritual",  kSerpentDance),
    // Breath (7)
    PRESET("Breath Of Gods",   "Breath",  kBreathOfGods),
    PRESET("Whisper Of Reed",  "Breath",  kWhisperOfReed),
    PRESET("Sandstorm Voice",  "Breath",  kSandstormVoice),
    PRESET("Ancient Breath",   "Breath",  kAncientBreath),
    PRESET("Exhalation",       "Breath",  kExhalation),
    PRESET("Desert Wind",      "Breath",  kDesertWind),
    PRESET("Cave Mist",        "Breath",  kCaveMist),
    // Shimmer (6)
    PRESET("Celestial Gate",   "Shimmer", kCelestialGate),
    PRESET("Starlight Choir",  "Shimmer", kStarlightChoir),
    PRESET("Golden Aether",    "Shimmer", kGoldenAether),
    PRESET("Otherworld",       "Shimmer", kOtherworld),
    PRESET("Ascension",        "Shimmer", kAscension),
    PRESET("Divine Light",     "Shimmer", kDivineLight),
    // Dark (6)
    PRESET("Nether Song",      "Dark",    kNetherSong),
    PRESET("Shadow Temple",    "Dark",    kShadowTemple),
    PRESET("Lament Of Sumer",  "Dark",    kLamentOfSumer),
    PRESET("Gatekeeper Song",  "Dark",    kGatekeeperSong),
    PRESET("Dusk Ritual",      "Dark",    kDuskRitual),
    PRESET("Ereshkigal",       "Dark",    kEreshkigal),
};

static constexpr int kNumAVPresets = int(std::size(kAVPresets));

// ─────────────────────────────────────────────────────────────────────────────
//  PRESET MANAGER
// ─────────────────────────────────────────────────────────────────────────────
class AVPresetManager {
public:
    int count() const { return kNumAVPresets; }

    const AVPreset& get(int idx) const {
        return kAVPresets[juce::jlimit(0, kNumAVPresets - 1, idx)];
    }

    int current() const { return currentIdx_; }

    void apply(int idx, juce::AudioProcessorValueTreeState& apvts) {
        const auto& pr = get(idx);
        for (int i = 0; i < pr.numParams; ++i) {
            const auto& s = pr.params[i];
            if (auto* p = apvts.getParameter(s.id))
                p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(s.value));
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
