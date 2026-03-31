cmake_minimum_required(VERSION 3.22)
project(AncientVoices VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE ──────────────────────────────────────────────────────────────────────
# Clone JUCE into ./JUCE or set JUCE_DIR to your install path
add_subdirectory(JUCE)

juce_add_plugin(AncientVoices
    COMPANY_NAME            "TullyTech"
    PLUGIN_MANUFACTURER_CODE Tuly
    PLUGIN_CODE             AVoc
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Ancient Voices"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         "Instrument|Synth"
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice
)

juce_generate_juce_header(AncientVoices)

target_sources(AncientVoices PRIVATE
    # Core — only files with real implementations
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    # All DSP and Data classes are header-only; no .cpp needed
)

target_include_directories(AncientVoices PRIVATE Source)

target_compile_definitions(AncientVoices PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AncientVoices
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
