cmake_minimum_required(VERSION 3.22)

project(BrainScan VERSION 1.0.0)

# Build id: date (YYMMDD) plus that day's build number. Re-run cmake -S . -B build after bumping.
set(BS_BUILD_ID "260905.2")

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

# Brokild World FX — the shared rack, compiled in.
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
add_subdirectory("${BWFX_DIR}" bwfx)

juce_add_plugin(BrainScan
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE BrSc
    FORMATS VST3 Standalone
    PRODUCT_NAME "Brain Scan"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control on this machine blocks the freshly built, unsigned
    # vst3_helper.exe, which fails an otherwise good build at the manifest step.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(BrainScan)

juce_add_binary_data(BrainScanAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "${BWFX_DIR}/ui/bwfx-rack.js")

target_sources(BrainScan
    PRIVATE
        Source/Engine.cpp
        Source/Import.cpp
        Source/Specimens.cpp
        Source/Anatomy.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(BrainScan
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        BS_BUILD_ID="${BS_BUILD_ID}")

target_link_libraries(BrainScan
    PRIVATE
        BrainScanAssets
        bwfx
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
