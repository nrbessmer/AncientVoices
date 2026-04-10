#pragma once

/*  ===================================================================
    Ancient Voices — Factory Presets
    50 presets in 7 categories:
      CHANT   (0–6)    Sacred vocal intonations
      DRONE   (7–13)   Sustained tonal foundations
      CHOIR   (14–21)  Layered vocal ensembles
      RITUAL  (22–29)  Ceremonial processing
      BREATH  (30–37)  Textural air & whisper
      SHIMMER (38–43)  Bright harmonic overtones
      DARK    (44–49)  Sub-bass & shadow vocals

    Each preset is parameterised by 14 fields drawn from
    ethnomusicological characteristics of four ancient traditions:

    Era 0 — SUMERIAN  : Heptatonic diatonic scales tuned by 4ths/5ths,
                         lyre timbres, gala-priest temple chanting,
                         consonant intervals, ceremonial percussion.
    Era 1 — EGYPTIAN  : Monophonic temple chant, sistrum rattles,
                         single melody with no dissonance, resonant
                         temple acoustics, sacred hymns & funerary rites.
    Era 2 — BYZANTINE : Exclusively vocal monophonic chant, ison (drone),
                         octoechos eight-mode system, melismatic ornament,
                         antiphonal texture, microtonal inflections.
    Era 3 — VEDIC     : Three-tone Rig Veda chant (udatta/anudatta/svarita),
                         Sama Veda seven-tone descending melodies, tanpura
                         drone, Just Intonation, stobha syllable extensions.
    =================================================================== */

#include <array>
#include <cmath>

namespace AncientVoices
{

// ----- Era identifiers (plain ints, no dependency on VowelTables) -----
enum Era : int
{
    Sumerian  = 0,   // ~2600 BCE  Mesopotamian temple rites
    Egyptian  = 1,   // ~1500 BCE  Nile valley sacred music
    Byzantine = 2,   // ~500 CE    Eastern Orthodox psaltic art
    Vedic     = 3    // ~1500 BCE  Sanskrit Sama-Veda chant
};

// ----- Voice-mode tags (cosmetic — not mapped to DSP directly) -----
enum VoiceMode : int
{
    Ancient   = 0,
    Human     = 1,
    Robot     = 2,
    Ethereal  = 3
};

// ----- Preset struct — 14 fields -----
struct Preset
{
    const char* name;       // Display name
    int         category;   // 0-6 index into category names
    int         era;        // Era enum
    int         voiceMode;  // VoiceMode enum
    int         numVoices;  // 1–8 polyphony hint
    float       spread;     // 0–1 stereo / voice spread
    float       drift;      // 0–1 pitch instability
    float       chant;      // 0–1 rhythmic modulation depth
    float       formantDepth; // 0.85–1.0 formant preservation
    float       mix;        // always 1.0 (dry killed)
    float       caveMix;    // 0–1 reverb send
    float       caveSize;   // 0–1 reverb room size
    float       shimmerMix; // 0–1 pitch-shifted reverb tail
    float       breathe;    // 0–1 saturation / air
    float       roughness;  // 0–1 distortion amount
    float       interval;   // –24 to +24 semitones
};

// ----- Category display names -----
static constexpr const char* kCategoryNames[] = {
    "CHANT", "DRONE", "CHOIR", "RITUAL", "BREATH", "SHIMMER", "DARK"
};

static constexpr int kNumPresets = 50;

// =====================================================================
//  FACTORY PRESETS
// =====================================================================

static constexpr Preset kFactoryPresets[kNumPresets] =
{
    // ======================== CHANT (0–6) ============================
    // Sacred vocal intonations inspired by temple chanting traditions.

    // 0 — Sumerian Chant: Gala-priest style, consonant 4ths/5ths,
    //     moderate reverb evoking ziggurat acoustics
    { "Sumerian Chant",      0, Sumerian, Ancient, 3, 0.3f, 0.15f, 0.4f,
      0.92f, 1.0f, 0.55f, 0.65f, 0.1f, 0.2f, 0.08f, 5.0f },

    // 1 — Egyptian Hymn: Monophonic temple vocal, sistrum-like shimmer,
    //     resonant stone-chamber reverb, no dissonance
    { "Egyptian Hymn",       0, Egyptian, Ancient, 2, 0.2f, 0.1f, 0.3f,
      0.95f, 1.0f, 0.6f, 0.7f, 0.15f, 0.15f, 0.05f, 0.0f },

    // 2 — Byzantine Troparion: Monodic chant with ison drone character,
    //     melismatic ornament via drift, antiphonal spread
    { "Byzantine Troparion", 0, Byzantine, Human, 2, 0.25f, 0.2f, 0.5f,
      0.90f, 1.0f, 0.5f, 0.6f, 0.12f, 0.18f, 0.06f, 0.0f },

    // 3 — Vedic Recitation: Sama-Veda three-tone intonation, tanpura
    //     drone feel, descending interval, precise formant
    { "Vedic Recitation",    0, Vedic, Ancient, 2, 0.15f, 0.08f, 0.35f,
      0.98f, 1.0f, 0.45f, 0.55f, 0.08f, 0.12f, 0.04f, -5.0f },

    // 4 — Temple Invocation: Egyptian priests summoning divine presence,
    //     deep reverb, moderate voices, ascending interval
    { "Temple Invocation",   0, Egyptian, Human, 4, 0.35f, 0.12f, 0.45f,
      0.93f, 1.0f, 0.65f, 0.75f, 0.18f, 0.22f, 0.07f, 3.0f },

    // 5 — Psalm of Ur: Sumerian lamentation, wider spread for antiphonal
    //     effect, moderate drift for emotional instability
    { "Psalm of Ur",         0, Sumerian, Human, 3, 0.4f, 0.18f, 0.38f,
      0.91f, 1.0f, 0.58f, 0.68f, 0.14f, 0.25f, 0.1f, 7.0f },

    // 6 — Kontakion: Byzantine hymn form by Romanos the Melodist,
    //     ornate melismatic chant, moderate reverb
    { "Kontakion",           0, Byzantine, Human, 2, 0.2f, 0.22f, 0.55f,
      0.89f, 1.0f, 0.48f, 0.58f, 0.1f, 0.16f, 0.05f, 0.0f },


    // ======================== DRONE (7–13) ===========================
    // Sustained tonal foundations: ison, tanpura, temple resonance.

    // 7 — Ison Drone: Byzantine continuous drone note (ison), the root
    //     of the tetrachord, providing tonal stability
    { "Ison Drone",          1, Byzantine, Ancient, 1, 0.1f, 0.05f, 0.1f,
      0.96f, 1.0f, 0.7f, 0.8f, 0.2f, 0.3f, 0.12f, 0.0f },

    // 8 — Tanpura Wash: Vedic drone inspired by tanpura cascading
    //     overtones, Sa-Pa tuning, deep reverb
    { "Tanpura Wash",        1, Vedic, Ethereal, 1, 0.15f, 0.06f, 0.08f,
      0.97f, 1.0f, 0.75f, 0.85f, 0.25f, 0.28f, 0.1f, -12.0f },

    // 9 — Ziggurat Resonance: Sumerian temple bass drone, massive reverb
    //     simulating stone architecture, subtle drift
    { "Ziggurat Resonance",  1, Sumerian, Ancient, 2, 0.2f, 0.1f, 0.12f,
      0.94f, 1.0f, 0.8f, 0.9f, 0.15f, 0.35f, 0.15f, -7.0f },

    // 10 — Nile Chamber: Egyptian burial chamber resonance, hollow
    //      reverberant drone, minimal modulation
    { "Nile Chamber",        1, Egyptian, Ethereal, 1, 0.12f, 0.07f, 0.06f,
      0.95f, 1.0f, 0.78f, 0.88f, 0.22f, 0.32f, 0.08f, 0.0f },

    // 11 — Cosmic Om: Vedic nada brahma concept — universe as vibration,
    //      deep octave-down drone with shimmer
    { "Cosmic Om",           1, Vedic, Ancient, 1, 0.1f, 0.04f, 0.05f,
      0.99f, 1.0f, 0.72f, 0.82f, 0.3f, 0.2f, 0.06f, -12.0f },

    // 12 — Hagia Sophia: Byzantine cathedral acoustic, massive reverb
    //      tail, gentle drift, ison character
    { "Hagia Sophia",        1, Byzantine, Ethereal, 2, 0.18f, 0.08f, 0.1f,
      0.93f, 1.0f, 0.85f, 0.92f, 0.2f, 0.22f, 0.05f, 0.0f },

    // 13 — Abzu Deep: Sumerian primordial waters, sub-bass drone,
    //      heavy saturation for primal rumble
    { "Abzu Deep",           1, Sumerian, Ancient, 1, 0.08f, 0.12f, 0.08f,
      0.90f, 1.0f, 0.68f, 0.78f, 0.1f, 0.4f, 0.18f, -12.0f },


    // ======================== CHOIR (14–21) ==========================
    // Layered vocal ensembles: temple choirs, antiphonal singing.

    // 14 — Temple Choir: Egyptian multi-voice choir in stone temple,
    //      wide spread, harmonic intervals
    { "Temple Choir",        2, Egyptian, Human, 6, 0.6f, 0.15f, 0.3f,
      0.92f, 1.0f, 0.55f, 0.65f, 0.18f, 0.2f, 0.08f, 5.0f },

    // 15 — Gala Ensemble: Sumerian gala priest choir singing Emesal
    //      dialect prayers, consonant 4ths
    { "Gala Ensemble",       2, Sumerian, Human, 5, 0.5f, 0.18f, 0.4f,
      0.91f, 1.0f, 0.5f, 0.6f, 0.12f, 0.22f, 0.1f, 5.0f },

    // 16 — Antiphonal Voices: Byzantine two-choir antiphonal tradition,
    //      wide stereo, moderate ornament
    { "Antiphonal Voices",   2, Byzantine, Human, 6, 0.7f, 0.2f, 0.45f,
      0.90f, 1.0f, 0.52f, 0.62f, 0.15f, 0.18f, 0.06f, 7.0f },

    // 17 — Vedic Assembly: Group Sama-Veda chanting, multiple voices
    //      in unison with subtle drift creating thickness
    { "Vedic Assembly",      2, Vedic, Human, 5, 0.45f, 0.12f, 0.35f,
      0.94f, 1.0f, 0.48f, 0.58f, 0.1f, 0.15f, 0.05f, 0.0f },

    // 18 — Pharaoh's Court: Grand Egyptian royal ceremony, full choir,
    //      rich reverb, ascending 5th
    { "Pharaoh's Court",     2, Egyptian, Human, 8, 0.65f, 0.14f, 0.32f,
      0.93f, 1.0f, 0.6f, 0.72f, 0.2f, 0.25f, 0.09f, 7.0f },

    // 19 — Celestial Chorus: Byzantine heavenly choir evoking angelic
    //      transmission of sacred chant
    { "Celestial Chorus",    2, Byzantine, Ethereal, 7, 0.7f, 0.16f, 0.42f,
      0.92f, 1.0f, 0.65f, 0.75f, 0.28f, 0.18f, 0.04f, 12.0f },

    // 20 — Udgatri Choir: Vedic Udgatri priest ensemble performing
    //      Sama-Veda samans, seven-tone melodies
    { "Udgatri Choir",       2, Vedic, Ancient, 6, 0.55f, 0.1f, 0.38f,
      0.96f, 1.0f, 0.5f, 0.62f, 0.14f, 0.16f, 0.06f, -5.0f },

    // 21 — Lyre Voices: Sumerian vocal accompaniment with lyre character,
    //      bright interval, moderate voices
    { "Lyre Voices",         2, Sumerian, Human, 4, 0.4f, 0.16f, 0.36f,
      0.91f, 1.0f, 0.45f, 0.55f, 0.16f, 0.2f, 0.08f, 4.0f },


    // ======================== RITUAL (22–29) =========================
    // Ceremonial processing: sacrifice, purification, invocation.

    // 22 — Funerary Rites: Egyptian ka-guiding chant for the dead,
    //      deep reverb, solemn descent
    { "Funerary Rites",      3, Egyptian, Ancient, 3, 0.3f, 0.1f, 0.25f,
      0.94f, 1.0f, 0.72f, 0.82f, 0.12f, 0.3f, 0.12f, -7.0f },

    // 23 — Sacred Fire: Vedic yajna fire ceremony, rhythmic chant with
    //      percussion feel, ascending energy
    { "Sacred Fire",         3, Vedic, Human, 4, 0.4f, 0.14f, 0.55f,
      0.92f, 1.0f, 0.42f, 0.52f, 0.15f, 0.28f, 0.14f, 3.0f },

    // 24 — Trisagion: Byzantine "Holy God" triple invocation,
    //      solemn repetitive melody, cathedral reverb
    { "Trisagion",           3, Byzantine, Human, 3, 0.25f, 0.08f, 0.48f,
      0.93f, 1.0f, 0.62f, 0.72f, 0.18f, 0.15f, 0.05f, 0.0f },

    // 25 — Balag Prayer: Sumerian balag instrument ritual, gala priest
    //      lamentation, rich saturation
    { "Balag Prayer",        3, Sumerian, Ancient, 3, 0.35f, 0.2f, 0.5f,
      0.90f, 1.0f, 0.55f, 0.65f, 0.1f, 0.35f, 0.15f, 0.0f },

    // 26 — Osiris Descent: Egyptian underworld journey, dark reverb,
    //      octave-down shift, heavy cave
    { "Osiris Descent",      3, Egyptian, Ancient, 2, 0.2f, 0.12f, 0.2f,
      0.91f, 1.0f, 0.78f, 0.88f, 0.08f, 0.38f, 0.16f, -12.0f },

    // 27 — Mantra Circle: Vedic group mantra recitation, voices in
    //      unison with natural drift
    { "Mantra Circle",       3, Vedic, Human, 5, 0.45f, 0.1f, 0.42f,
      0.95f, 1.0f, 0.5f, 0.6f, 0.12f, 0.2f, 0.06f, 0.0f },

    // 28 — Procession: Byzantine liturgical procession, stately rhythm,
    //      moderate reverb, ascending 4th
    { "Procession",          3, Byzantine, Human, 4, 0.35f, 0.1f, 0.4f,
      0.92f, 1.0f, 0.52f, 0.62f, 0.14f, 0.18f, 0.07f, 5.0f },

    // 29 — Enlil Storm: Sumerian storm-god invocation, turbulent drift,
    //      rough saturation, powerful
    { "Enlil Storm",         3, Sumerian, Ancient, 4, 0.5f, 0.25f, 0.52f,
      0.88f, 1.0f, 0.48f, 0.58f, 0.1f, 0.4f, 0.2f, -5.0f },


    // ======================== BREATH (30–37) =========================
    // Textural air and whisper: wind instruments, breath sounds.

    // 30 — Reed Whisper: Sumerian reed pipe (gi-di) breathy tone,
    //      gentle saturation, minimal reverb
    { "Reed Whisper",        4, Sumerian, Ethereal, 2, 0.2f, 0.15f, 0.15f,
      0.93f, 1.0f, 0.35f, 0.45f, 0.1f, 0.45f, 0.08f, 0.0f },

    // 31 — Ney Spirit: Egyptian ney flute character, airy saturation,
    //      gentle shimmer, mid-range
    { "Ney Spirit",          4, Egyptian, Ethereal, 2, 0.18f, 0.12f, 0.12f,
      0.94f, 1.0f, 0.4f, 0.5f, 0.15f, 0.5f, 0.06f, 0.0f },

    // 32 — Hesychasm Breath: Byzantine contemplative breathing prayer,
    //      minimal processing, intimate reverb
    { "Hesychasm Breath",    4, Byzantine, Ethereal, 1, 0.1f, 0.06f, 0.08f,
      0.97f, 1.0f, 0.38f, 0.48f, 0.08f, 0.42f, 0.04f, 0.0f },

    // 33 — Prana Flow: Vedic breath-energy concept, smooth air texture,
    //      gentle tanpura-like shimmer
    { "Prana Flow",          4, Vedic, Ethereal, 2, 0.15f, 0.08f, 0.1f,
      0.96f, 1.0f, 0.42f, 0.52f, 0.18f, 0.48f, 0.05f, 0.0f },

    // 34 — Desert Wind: Egyptian desert atmosphere, sandstorm texture,
    //      wide breathy saturation
    { "Desert Wind",         4, Egyptian, Ancient, 3, 0.35f, 0.2f, 0.18f,
      0.92f, 1.0f, 0.45f, 0.55f, 0.12f, 0.55f, 0.12f, 0.0f },

    // 35 — Steppe Murmur: Sumerian pastoral character, shepherd's pipe,
    //      drift for wind modulation
    { "Steppe Murmur",       4, Sumerian, Human, 2, 0.22f, 0.18f, 0.2f,
      0.91f, 1.0f, 0.38f, 0.48f, 0.08f, 0.42f, 0.1f, 3.0f },

    // 36 — Incense Haze: Byzantine/Egyptian temple incense ritual,
    //      smoky saturation, medium reverb
    { "Incense Haze",        4, Egyptian, Ethereal, 2, 0.2f, 0.1f, 0.14f,
      0.93f, 1.0f, 0.5f, 0.6f, 0.14f, 0.52f, 0.08f, 0.0f },

    // 37 — Nada Breath: Vedic nada brahma — universe as cosmic breath,
    //      sustained air, deep shimmer
    { "Nada Breath",         4, Vedic, Ethereal, 1, 0.1f, 0.05f, 0.06f,
      0.98f, 1.0f, 0.48f, 0.58f, 0.22f, 0.46f, 0.04f, -7.0f },


    // ======================== SHIMMER (38–43) ========================
    // Bright harmonic overtones: celestial, crystalline, ascending.

    // 38 — Celestial Harp: Sumerian Lyre of Ur golden shimmer,
    //      bright pitch shift, rich overtones
    { "Celestial Harp",      5, Sumerian, Ethereal, 4, 0.45f, 0.12f, 0.25f,
      0.92f, 1.0f, 0.55f, 0.65f, 0.35f, 0.15f, 0.05f, 12.0f },

    // 39 — Hathor's Light: Egyptian goddess of music, radiant shimmer,
    //      sistrum-like brightness, ascending
    { "Hathor's Light",      5, Egyptian, Ethereal, 3, 0.35f, 0.1f, 0.2f,
      0.94f, 1.0f, 0.5f, 0.6f, 0.4f, 0.12f, 0.04f, 7.0f },

    // 40 — Paschal Dawn: Byzantine Easter (Pascha) celebration,
    //      triumphant ascending octave, bright reverb
    { "Paschal Dawn",        5, Byzantine, Human, 5, 0.5f, 0.14f, 0.35f,
      0.91f, 1.0f, 0.52f, 0.62f, 0.38f, 0.14f, 0.06f, 12.0f },

    // 41 — Saraswati Bells: Vedic goddess of music, crystalline
    //      overtones, tanpura shimmer cascade
    { "Saraswati Bells",     5, Vedic, Ethereal, 3, 0.3f, 0.08f, 0.18f,
      0.96f, 1.0f, 0.48f, 0.58f, 0.42f, 0.1f, 0.03f, 7.0f },

    // 42 — Golden Bull: Sumerian Bull Lyre of Ur, golden overtones,
    //      warm bright shimmer with moderate spread
    { "Golden Bull",         5, Sumerian, Ancient, 3, 0.38f, 0.14f, 0.22f,
      0.90f, 1.0f, 0.52f, 0.62f, 0.32f, 0.2f, 0.08f, 5.0f },

    // 43 — Pyramid Light: Egyptian ascending light from pyramid apex,
    //      bright ascending shimmer, airy
    { "Pyramid Light",       5, Egyptian, Ethereal, 4, 0.4f, 0.1f, 0.16f,
      0.93f, 1.0f, 0.55f, 0.68f, 0.45f, 0.12f, 0.04f, 12.0f },


    // ======================== DARK (44–49) ===========================
    // Sub-bass and shadow vocals: underworld, caves, deep ritual.

    // 44 — Kur Descent: Sumerian underworld (Kur), Inanna's descent,
    //      massive sub-octave, heavy cave reverb
    { "Kur Descent",         6, Sumerian, Ancient, 2, 0.2f, 0.2f, 0.3f,
      0.88f, 1.0f, 0.8f, 0.9f, 0.06f, 0.45f, 0.22f, -12.0f },

    // 45 — Duat Shadow: Egyptian Duat (underworld), Anubis guide,
    //      deep funeral chant, dark reverb
    { "Duat Shadow",         6, Egyptian, Ancient, 2, 0.18f, 0.15f, 0.22f,
      0.90f, 1.0f, 0.75f, 0.85f, 0.05f, 0.42f, 0.2f, -12.0f },

    // 46 — Crypt Echoes: Byzantine catacomb worship, deep echoing
    //      monophonic chant in underground chambers
    { "Crypt Echoes",        6, Byzantine, Human, 2, 0.15f, 0.12f, 0.35f,
      0.91f, 1.0f, 0.82f, 0.92f, 0.08f, 0.35f, 0.14f, -7.0f },

    // 47 — Abyss Chant: Vedic cosmic dissolution (pralaya), deepest
    //      octave-down, minimal modulation, dark drone
    { "Abyss Chant",         6, Vedic, Ancient, 1, 0.1f, 0.08f, 0.15f,
      0.92f, 1.0f, 0.7f, 0.8f, 0.04f, 0.38f, 0.18f, -12.0f },

    // 48 — Ereshkigal: Sumerian underworld queen, grinding dark texture,
    //      heavy roughness, oppressive reverb
    { "Ereshkigal",          6, Sumerian, Ancient, 3, 0.25f, 0.22f, 0.35f,
      0.86f, 1.0f, 0.72f, 0.82f, 0.05f, 0.5f, 0.25f, -7.0f },

    // 49 — Tartarus Gate: Byzantine/Greek underworld gate concept,
    //      sub-bass with harsh saturation, cavernous
    { "Tartarus Gate",       6, Byzantine, Ancient, 2, 0.2f, 0.18f, 0.28f,
      0.89f, 1.0f, 0.78f, 0.88f, 0.06f, 0.48f, 0.22f, -12.0f },
};

// ----- Compile-time enforcement -----
static_assert(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]) == kNumPresets,
              "kFactoryPresets must contain exactly kNumPresets entries");

// Verify all presets have mix == 1.0f and formantDepth >= 0.85f
// (Checked at compile time via constexpr helper)
namespace detail
{
    constexpr bool validatePresets()
    {
        for (int i = 0; i < kNumPresets; ++i)
        {
            if (kFactoryPresets[i].mix != 1.0f)
                return false;
            if (kFactoryPresets[i].formantDepth < 0.85f)
                return false;
        }
        return true;
    }
}

static_assert(detail::validatePresets(),
              "All presets must have mix == 1.0f and formantDepth >= 0.85f");

} // namespace AncientVoices
