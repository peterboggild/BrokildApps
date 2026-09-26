cmake_minimum_required(VERSION 3.22)

project(ArtefactB2311 VERSION 0.1.0)

# Build id: date (YYMMDD) plus the build number that day. Bump it AND re-run
# cmake -S . -B build, or the compiled-in value does not change.
set(AB_BUILD_ID "260926.1")

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

# Brokild World FX — the shared global rack, compiled in (BWFX-DESIGN.md).
# Diegetically it is the expedition's signal chain, bolted to the harness.
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
add_subdirectory("${BWFX_DIR}" bwfx)

juce_add_plugin(ArtefactB2311
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE Ab22
    FORMATS VST3 Standalone
    PRODUCT_NAME "Artefact B2311.22"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control blocks the freshly built vst3_helper.exe on this
    # machine; moduleinfo.json is optional and hosts scan the binary anyway.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(ArtefactB2311)

juce_add_binary_data(ArtefactB2311Assets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        Source/ui/img/tx-planet.png
        Source/ui/img/tx-alu.png
        Source/ui/img/tx-wear.png
        "${BWFX_DIR}/ui/bwfx-rack.js")

target_sources(ArtefactB2311
    PRIVATE
        Source/Specimen.cpp
        Source/Engine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(ArtefactB2311
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        _USE_MATH_DEFINES=1
        AB_BUILD_ID="${AB_BUILD_ID}")

target_link_libraries(ArtefactB2311
    PRIVATE
        ArtefactB2311Assets
        bwfx
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
