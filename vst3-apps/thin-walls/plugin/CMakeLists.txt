cmake_minimum_required(VERSION 3.22)

project(ThinWalls VERSION 1.0.0)

# Build id: date (YYMMDD) plus that day's build number. Bump it AND re-run
# `cmake -S . -B build` or it will not take.
set(TW_BUILD_ID "260925.1")

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

# No Brokild World FX: this is an effect (the same call as Martian Gain and
# Battlestar Overdrive). The adapter folder is on the include path only for
# brokild_paths.h, the house patch folder, which is header-only.
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()

juce_add_plugin(ThinWalls
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE ThWl
    FORMATS VST3 Standalone
    PRODUCT_NAME "Thin Walls"
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
    VST3_CATEGORIES "Fx" "Spatial" "Reverb")

juce_generate_juce_header(ThinWalls)

juce_add_binary_data(ThinWallsAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html)

target_sources(ThinWalls
    PRIVATE
        Source/Engine.cpp
        Source/TakeExport.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(ThinWalls PRIVATE "${BWFX_DIR}/adapter")

target_compile_definitions(ThinWalls
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_FLAC=1
    PRIVATE
        TW_BUILD_ID="${TW_BUILD_ID}")

target_link_libraries(ThinWalls
    PRIVATE
        ThinWallsAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# The MP4 export: Media Foundation, part of Windows (H.264 + AAC), no extra libraries.
if(WIN32)
    target_link_libraries(ThinWalls PRIVATE mfplat mfreadwrite mfuuid)
endif()

# Every number in this plug-in is a measurement (RT60, DRR, ITD): no fast-math.
if(MSVC)
    target_compile_options(ThinWalls PRIVATE /fp:precise /bigobj)
else()
    target_compile_options(ThinWalls PRIVATE -fno-fast-math)
endif()
