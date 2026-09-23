cmake_minimum_required(VERSION 3.22)

project(BattlestarOverdrive VERSION 1.0.0)

# Build id: date (YYMMDD) plus that day's build number. Bump it AND re-run
# `cmake -S . -B build` or it will not take.
set(BO_BUILD_ID "260919.2")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is not vendored. A local build passes -DJUCE_DIR=<your checkout>;
# a fresh clone or CI fetches it.
if(DEFINED JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.13
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Brokild World FX is deliberately NOT built in: this is an effect, and BWFX is
# a rack of effects - the same call already made for Martian Gain and Hairfryer.
# The adapter folder is still on the include path for brokild_paths.h, which is
# header-only and depends on nothing but juce_core.
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()

juce_add_plugin(BattlestarOverdrive
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE BsOd
    FORMATS VST3 Standalone
    PRODUCT_NAME "Battlestar Overdrive"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control blocks the freshly built, unsigned vst3_helper.exe,
    # which fails an otherwise good build at the manifest step and leaves a
    # zero-byte moduleinfo.json. It is optional metadata; hosts scan without it.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Fx" "Distortion")

juce_generate_juce_header(BattlestarOverdrive)

juce_add_binary_data(BattlestarOverdriveAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        Source/ui/art/panel.jpg
        Source/ui/art/screen-glass.jpg
        Source/ui/art/knobs-chrome.png
        Source/ui/art/knobs-orange.png
        Source/ui/art/button.png
        Source/ui/art/fuel.png)

target_sources(BattlestarOverdrive
    PRIVATE
        Source/Engine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(BattlestarOverdrive PRIVATE "${BWFX_DIR}/adapter")

target_compile_definitions(BattlestarOverdrive
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    PRIVATE
        BO_BUILD_ID="${BO_BUILD_ID}")

target_link_libraries(BattlestarOverdrive
    PRIVATE
        BattlestarOverdriveAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# A distortion's output is the thing being judged, so no fast-math: a
# reciprocal approximation has no business in the signal path here.
if(MSVC)
    target_compile_options(BattlestarOverdrive PRIVATE /fp:precise)
else()
    target_compile_options(BattlestarOverdrive PRIVATE -fno-fast-math)
endif()
