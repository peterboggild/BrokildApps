cmake_minimum_required(VERSION 3.22)

project(ArtefactB2311_1 VERSION 1.0.0)

# Build id: date (YYMMDD) plus that day's build number.
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

# Brokild World FX — the shared rack. As with the siblings it belongs to the
# CRATE rather than to the artefact: our packing case carries our effects.
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
add_subdirectory("${BWFX_DIR}" bwfx)

juce_add_plugin(ArtefactB2311_1
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE Ab01
    FORMATS VST3 Standalone
    PRODUCT_NAME "Artefact B2311.1"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control on this machine blocks the freshly built, unsigned
    # vst3_helper.exe, which fails an otherwise good build at the manifest step.
    # moduleinfo.json is optional; hosts scan the binary without it.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(ArtefactB2311_1)

juce_add_binary_data(Artefact1Assets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "${BWFX_DIR}/ui/bwfx-rack.js")

target_sources(ArtefactB2311_1
    PRIVATE
        Source/Engine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(ArtefactB2311_1
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        AB_BUILD_ID="${AB_BUILD_ID}")

target_link_libraries(ArtefactB2311_1
    PRIVATE
        Artefact1Assets
        bwfx
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
