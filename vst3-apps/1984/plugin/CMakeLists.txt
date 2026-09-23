cmake_minimum_required(VERSION 3.22)

project(Nineteen84 VERSION 1.0.0)

# Build id: date (YYMMDD) plus that day's build number. Bump it AND re-run
# `cmake -S . -B build` or it will not take.
set(N84_BUILD_ID "260923.2")

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

# Brokild World FX — the shared global rack, compiled in.
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
add_subdirectory("${BWFX_DIR}" bwfx)

juce_add_plugin(Nineteen84
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE N984
    FORMATS VST3 Standalone
    PRODUCT_NAME "1984"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control blocks the freshly built, unsigned vst3_helper.exe;
    # moduleinfo.json is optional metadata and hosts scan without it.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(Nineteen84)

# The decals (assets/decals/*.png) ride in BinaryData and are served by
# basename; the page falls back to its own drawing for any that is missing.
file(GLOB N84_DECALS "${CMAKE_CURRENT_SOURCE_DIR}/assets/decals/*.png")
juce_add_binary_data(Nineteen84Assets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "${BWFX_DIR}/ui/bwfx-rack.js"
        ${N84_DECALS})

target_sources(Nineteen84
    PRIVATE
        Source/Engine.cpp
        Source/Patches.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(Nineteen84
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    PRIVATE
        N84_BUILD_ID="${N84_BUILD_ID}")

target_link_libraries(Nineteen84
    PRIVATE
        Nineteen84Assets
        bwfx
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Every number in this instrument is a measurement: no fast-math.
if(MSVC)
    target_compile_options(Nineteen84 PRIVATE /fp:precise /bigobj)
else()
    target_compile_options(Nineteen84 PRIVATE -fno-fast-math)
endif()
