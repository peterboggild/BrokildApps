cmake_minimum_required(VERSION 3.22)

project(EscapeRoom VERSION 1.0.0)

# Build id: date (YYMMDD) plus the build number that day.
set(ER_BUILD_ID "260827.1")

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

juce_add_plugin(EscapeRoom
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE EscR
    FORMATS VST3 Standalone
    PRODUCT_NAME "Escape Room"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(EscapeRoom)

juce_add_binary_data(EscapeRoomAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "${BWFX_DIR}/ui/bwfx-rack.js")

target_sources(EscapeRoom
    PRIVATE
        Source/Engine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(EscapeRoom
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        ER_BUILD_ID="${ER_BUILD_ID}")

target_link_libraries(EscapeRoom
    PRIVATE
        EscapeRoomAssets
        bwfx
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
