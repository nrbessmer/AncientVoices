#pragma once

#include <JuceHeader.h>

//==============================================================================
// Ancient Voices — Preset Reference Table
//
// 50 presets organized into 7 categories, rooted exclusively in Sumerian,
// Akkadian, Babylonian, Assyrian, Egyptian, and wider Mesopotamian traditions.
// Maps 1:1 to SoundProfile::AV_00..AV_49 in PluginProcessor.h.
//
// Index layout:
//   CHANT    : AV_00..AV_06   (7)
//   DRONE    : AV_07..AV_13   (7)
//   CHOIR    : AV_14..AV_20   (7)
//   RITUAL   : AV_21..AV_28   (8)
//   BREATH   : AV_29..AV_35   (7)
//   SHIMMER  : AV_36..AV_42   (7)
//   DARK     : AV_43..AV_49   (7)
//
// Era tags are informational only — they drive filterCutoff selection in
// initializeProfiles() (Sumerian=4500Hz, Egyptian=5000Hz,
// Akkadian/Babylonian=4200Hz, generic Mesopotamian=4800Hz).
//==============================================================================

namespace AncientVoicesPresets
{
    enum class Category
    {
        Chant,
        Drone,
        Choir,
        Ritual,
        Breath,
        Shimmer,
        Dark
    };

    enum class Era
    {
        Sumerian,
        Egyptian,
        AkkadianBabylonian,
        Mesopotamian
    };

    struct PresetInfo
    {
        int         avIndex;     // 0..49, maps to SoundProfile::AV_00 + avIndex
        int         globalIndex; // 10..59, index into APVTS PROFILE_INDEX parameter
        const char* name;        // Display name shown in ComboBox and display label
        Category    category;    // Category grouping
        Era         era;         // Era tag (drives filterCutoff in profile DSP)
    };

    // MMV's 10 profiles occupy indices 0..9; AV presets occupy 10..59.
    static constexpr int kMmvProfileCount = 10;
    static constexpr int kAvPresetCount   = 50;
    static constexpr int kTotalProfiles   = kMmvProfileCount + kAvPresetCount; // 60

    inline const PresetInfo& getPreset(int avIndex)
    {
        static const PresetInfo table[kAvPresetCount] = {
            // ---- CHANT (AV_00..AV_06) -------------------------------------
            {  0, 10, "Eridu Invocation",      Category::Chant, Era::Sumerian           },
            {  1, 11, "Gala Priest Lament",    Category::Chant, Era::Sumerian           },
            {  2, 12, "Heliopolis Sunrise",    Category::Chant, Era::Egyptian           },
            {  3, 13, "Inanna's Descent Chant",Category::Chant, Era::Sumerian           },
            {  4, 14, "Ptah Hymn",             Category::Chant, Era::Egyptian           },
            {  5, 15, "Enlil Decree",          Category::Chant, Era::Sumerian           },
            {  6, 16, "Memphis Morning Rite",  Category::Chant, Era::Egyptian           },

            // ---- DRONE (AV_07..AV_13) -------------------------------------
            {  7, 17, "Ziggurat Resonance",    Category::Drone, Era::Sumerian           },
            {  8, 18, "Nile Reed Drone",       Category::Drone, Era::Egyptian           },
            {  9, 19, "Abzu Deep Waters",      Category::Drone, Era::Sumerian           },
            { 10, 20, "Duat Shadow Drone",     Category::Drone, Era::Egyptian           },
            { 11, 21, "Eridu Mud Plain",       Category::Drone, Era::Sumerian           },
            { 12, 22, "Karnak Pillar Hum",     Category::Drone, Era::Egyptian           },
            { 13, 23, "Marduk's Word",         Category::Drone, Era::AkkadianBabylonian },

            // ---- CHOIR (AV_14..AV_20) -------------------------------------
            { 14, 24, "Temple of Amun Choir",  Category::Choir, Era::Egyptian           },
            { 15, 25, "Ur Nammu Assembly",     Category::Choir, Era::Sumerian           },
            { 16, 26, "Hathor's Daughters",    Category::Choir, Era::Egyptian           },
            { 17, 27, "Gala Ensemble of Uruk", Category::Choir, Era::Sumerian           },
            { 18, 28, "Pharaoh's Court",       Category::Choir, Era::Egyptian           },
            { 19, 29, "Nippur Gathering",      Category::Choir, Era::Sumerian           },
            { 20, 30, "Lagash Processional",   Category::Choir, Era::Sumerian           },

            // ---- RITUAL (AV_21..AV_28) ------------------------------------
            { 21, 31, "Opening of the Mouth",  Category::Ritual, Era::Egyptian          },
            { 22, 32, "Sacred Marriage Rite",  Category::Ritual, Era::Sumerian          },
            { 23, 33, "Akitu Festival",        Category::Ritual, Era::AkkadianBabylonian},
            { 24, 34, "Heb Sed Renewal",       Category::Ritual, Era::Egyptian          },
            { 25, 35, "Funeral of Osiris",     Category::Ritual, Era::Egyptian          },
            { 26, 36, "Anointing the Shedu",   Category::Ritual, Era::AkkadianBabylonian},
            { 27, 37, "Libation to Enki",      Category::Ritual, Era::Sumerian          },
            { 28, 38, "Offering at Edfu",      Category::Ritual, Era::Egyptian          },

            // ---- BREATH (AV_29..AV_35) ------------------------------------
            { 29, 39, "Reed of Ra",            Category::Breath, Era::Egyptian          },
            { 30, 40, "Nefertari's Whisper",   Category::Breath, Era::Egyptian          },
            { 31, 41, "Desert Wind of Giza",   Category::Breath, Era::Egyptian          },
            { 32, 42, "Tigris Dawn Breath",    Category::Breath, Era::Mesopotamian      },
            { 33, 43, "Papyrus Rustle",        Category::Breath, Era::Egyptian          },
            { 34, 44, "Scribe's Murmur",       Category::Breath, Era::Mesopotamian      },
            { 35, 45, "Incense of Ishtar",     Category::Breath, Era::AkkadianBabylonian},

            // ---- SHIMMER (AV_36..AV_42) -----------------------------------
            { 36, 46, "Hathor's Sistrum",      Category::Shimmer, Era::Egyptian         },
            { 37, 47, "Stars of Nut",          Category::Shimmer, Era::Egyptian         },
            { 38, 48, "Lapis of Inanna",       Category::Shimmer, Era::Sumerian         },
            { 39, 49, "Gilded Mask of Tut",    Category::Shimmer, Era::Egyptian         },
            { 40, 50, "Sunboat of Ra",         Category::Shimmer, Era::Egyptian         },
            { 41, 51, "Crystal of Utu",        Category::Shimmer, Era::Sumerian         },
            { 42, 52, "Ziggurat at Dawn",      Category::Shimmer, Era::Sumerian         },

            // ---- DARK (AV_43..AV_49) --------------------------------------
            { 43, 53, "Kur Descent",           Category::Dark, Era::Sumerian            },
            { 44, 54, "Ereshkigal's Throne",   Category::Dark, Era::Sumerian            },
            { 45, 55, "Tomb of Seti",          Category::Dark, Era::Egyptian            },
            { 46, 56, "Apep Rising",           Category::Dark, Era::Egyptian            },
            { 47, 57, "Lamashtu",              Category::Dark, Era::AkkadianBabylonian  },
            { 48, 58, "Nergal's Gate",         Category::Dark, Era::AkkadianBabylonian  },
            { 49, 59, "Pazuzu",                Category::Dark, Era::AkkadianBabylonian  }
        };

        jassert(avIndex >= 0 && avIndex < kAvPresetCount);
        return table[avIndex];
    }

    inline const char* getCategoryName(Category c)
    {
        switch (c)
        {
            case Category::Chant:   return "CHANT";
            case Category::Drone:   return "DRONE";
            case Category::Choir:   return "CHOIR";
            case Category::Ritual:  return "RITUAL";
            case Category::Breath:  return "BREATH";
            case Category::Shimmer: return "SHIMMER";
            case Category::Dark:    return "DARK";
        }
        return "";
    }

    inline float getFilterCutoffForEra(Era era)
    {
        switch (era)
        {
            case Era::Sumerian:           return 4500.0f;
            case Era::Egyptian:           return 5000.0f;
            case Era::AkkadianBabylonian: return 4200.0f;
            case Era::Mesopotamian:       return 4800.0f;
        }
        return 4800.0f;
    }

    // MMV's original 10 profile names, for ComboBox population alongside AV presets.
    inline const char* getMmvProfileName(int mmvIndex)
    {
        static const char* names[kMmvProfileCount] = {
            "Alpha",   "Beta",    "Gamma",  "Delta", "Epsilon",
            "Zeta",    "Eta",     "Theta",  "Iota",  "Kappa"
        };
        jassert(mmvIndex >= 0 && mmvIndex < kMmvProfileCount);
        return names[mmvIndex];
    }
}
