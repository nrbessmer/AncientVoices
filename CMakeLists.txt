cmake_minimum_required(VERSION 3.22)
project(AncientVoices VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)

if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures" FORCE)
endif()

# ── RubberBand ────────────────────────────────────────────────────────────────
find_path(RUBBERBAND_INCLUDE_DIR
    NAMES rubberband/RubberBandStretcher.h
    PATHS
        /opt/homebrew/include
        /opt/homebrew/opt/rubberband/include
        /usr/local/include
        /usr/local/opt/rubberband/include
    REQUIRED
)
find_library(RUBBERBAND_LIBRARY
    NAMES rubberband
    PATHS
        /opt/homebrew/lib
        /opt/homebrew/opt/rubberband/lib
        /usr/local/lib
        /usr/local/opt/rubberband/lib
    REQUIRED
)
message(STATUS "RubberBand: ${RUBBERBAND_INCLUDE_DIR} / ${RUBBERBAND_LIBRARY}")

# ── JUCE ──────────────────────────────────────────────────────────────────────
add_subdirectory(JUCE)

juce_add_plugin(AncientVoices
    COMPANY_NAME                "Tully EDM Vibe"
    BUNDLE_ID                   "com.TullyEDMVibe.AncientVoices"
    PLUGIN_MANUFACTURER_CODE    Tull
    PLUGIN_CODE                 AV05
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Ancient Voices"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Fx"
    AU_MAIN_TYPE                kAudioUnitType_Effect
    HARDENED_RUNTIME_ENABLED    TRUE
    AU_SANDBOX_SAFE             TRUE
)

juce_generate_juce_header(AncientVoices)

target_sources(AncientVoices PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(AncientVoices PRIVATE
    Source
    ${RUBBERBAND_INCLUDE_DIR}
)

target_compile_definitions(AncientVoices PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    # These define the AUCocoaViewClass name — without them Logic shows generic window
    JucePlugin_ManufacturerCode=0x54756c6c
    JucePlugin_PluginCode=0x41563035
)

target_compile_options(AncientVoices PRIVATE
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-shorten-64-to-32
    -Wno-switch-enum
    -Wno-float-equal
    -Wno-shadow
    -Wno-unused-parameter
    -Wno-pedantic
    -Wno-deprecated
)

# ── Code signing ─────────────────────────────────────────────────────────────
set_target_properties(AncientVoices PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "33N7S7V79Y"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
)

target_link_libraries(AncientVoices
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        ${RUBBERBAND_LIBRARY}
)