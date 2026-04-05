cmake_minimum_required(VERSION 3.22)
project(AncientVoices VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)

if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures" FORCE)
endif()

# ── Disable Xcode automatic code signing ─────────────────────────────────────
# Prevents "No signing certificate Mac Development found" error when the
# build machine does not have a Mac Development certificate in its keychain.
# Ad-hoc signing is sufficient for local AU/VST3 development and matches
# how MisterMultiVox is built via Projucer without explicit signing config.
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

# ── RubberBand ────────────────────────────────────────────────────────────────
if(EXISTS "/opt/homebrew/lib/librubberband.dylib")
    set(RUBBERBAND_INCLUDE_DIR "/opt/homebrew/include")
    set(RUBBERBAND_LIBRARY     "/opt/homebrew/lib/librubberband.dylib")
elseif(EXISTS "/opt/homebrew/lib/librubberband.a")
    set(RUBBERBAND_INCLUDE_DIR "/opt/homebrew/include")
    set(RUBBERBAND_LIBRARY     "/opt/homebrew/lib/librubberband.a")
elseif(EXISTS "/usr/local/lib/librubberband.dylib")
    set(RUBBERBAND_INCLUDE_DIR "/usr/local/include")
    set(RUBBERBAND_LIBRARY     "/usr/local/lib/librubberband.dylib")
elseif(EXISTS "/usr/local/lib/librubberband.a")
    set(RUBBERBAND_INCLUDE_DIR "/usr/local/include")
    set(RUBBERBAND_LIBRARY     "/usr/local/lib/librubberband.a")
else()
    message(FATAL_ERROR "RubberBand not found. Fix: brew install rubberband")
endif()

message(STATUS "RubberBand include: ${RUBBERBAND_INCLUDE_DIR}")
message(STATUS "RubberBand library: ${RUBBERBAND_LIBRARY}")

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
    # Prevents JUCE from injecting network.client and
    # temporary-exception.files.all.read-write into the Info.plist
    # resourceUsage block. Logic Pro's sandbox host blocks any AU
    # that declares those exceptions and shows a black window instead
    # of loading the custom Cocoa UI.
    NEEDS_CURL                  FALSE
    NEEDS_WEB_BROWSER           FALSE
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
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
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