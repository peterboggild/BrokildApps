cmake_minimum_required(VERSION 3.22)

project(Hairfryer VERSION 1.0.0)

# Build id: date (YYMMDD) plus the build number that day.
set(HF_BUILD_ID "260926.1")

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

# Brokild World FX — the shared global rack, which this effect does not carry (BWFX-DESIGN.md).
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
# NOT built here any more: this is an effect, and BWFX is a rack of effects.
# The adapter folder is still on the include path for brokild_paths.h,
# which is header-only and depends on nothing but juce_core.

juce_add_plugin(Hairfryer
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE Hfry
    FORMATS VST3 Standalone
    PRODUCT_NAME "Hairfryer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control blocks the freshly built, unsigned vst3_helper.exe,
    # which fails an otherwise good build at the manifest step. moduleinfo.json
    # is optional metadata; hosts scan the binary without it.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Fx" "Distortion")

juce_generate_juce_header(Hairfryer)

juce_add_binary_data(HairfryerAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html)

target_sources(Hairfryer
    PRIVATE
        Source/Engine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(Hairfryer PRIVATE "${BWFX_DIR}/adapter")

target_compile_definitions(Hairfryer
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        HF_BUILD_ID="${HF_BUILD_ID}")

target_link_libraries(Hairfryer
    PRIVATE
        HairfryerAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
