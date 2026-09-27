cmake_minimum_required(VERSION 3.22)

project(PhotoSynth VERSION 0.1.0)

# Build id: date (YYMMDD) plus the build number that day. Bump the trailing
# number for another build the same day, roll the date over otherwise.
set(PS_BUILD_ID "260926.1")

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

# Brokild World FX - the shared global rack, compiled in (BWFX-DESIGN.md).
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
add_subdirectory("${BWFX_DIR}" bwfx)

juce_add_plugin(PhotoSynth
    COMPANY_NAME "Brokild"
    # Smart App Control blocks the freshly built vst3_helper.exe on this
    # machine, which fails the build at the manifest step. moduleinfo.json is
    # optional and hosts scan the binary without it.
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE Psy2
    FORMATS VST3 Standalone
    PRODUCT_NAME "Photo Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE)

juce_generate_juce_header(PhotoSynth)

juce_add_binary_data(PhotoSynthAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "${BWFX_DIR}/ui/bwfx-rack.js")

target_sources(PhotoSynth
    PRIVATE
        Source/Engine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(PhotoSynth
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        PS_BUILD_ID="${PS_BUILD_ID}")

target_link_libraries(PhotoSynth
    PRIVATE
        PhotoSynthAssets
        bwfx
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
