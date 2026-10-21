cmake_minimum_required(VERSION 3.22)

project(Marmalade120 VERSION 1.0.0)

# Build id: date (YYMMDD) plus that day's build number. Bump it AND re-run
# `cmake -S . -B build` or it will not take.
set(MM_BUILD_ID "261020.2")

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

# Brokild World FX is deliberately NOT built in: this is an amplifier, and the
# point is that every stage of it is a circuit - a rack of effects after it
# would be the one part that is not.

juce_add_plugin(Marmalade120
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE Mm12
    FORMATS VST3 Standalone
    PRODUCT_NAME "Marmalade 120"
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

juce_generate_juce_header(Marmalade120)

juce_add_binary_data(Marmalade120Assets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html)

target_sources(Marmalade120
    PRIVATE
        Source/Circuit.cpp
        Source/Engine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(Marmalade120
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    PRIVATE
        MM_BUILD_ID="${MM_BUILD_ID}")

target_link_libraries(Marmalade120
    PRIVATE
        Marmalade120Assets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# The circuit solver is checked against ngspice to a fraction of a percent;
# fast-math would reorder its sums and void that check.
if(MSVC)
    target_compile_options(Marmalade120 PRIVATE /fp:precise)
else()
    target_compile_options(Marmalade120 PRIVATE -fno-fast-math)
endif()
