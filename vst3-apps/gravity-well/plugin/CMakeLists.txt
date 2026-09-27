cmake_minimum_required(VERSION 3.22)

project(GravityWell VERSION 1.0.0)

# Build id: date (YYMMDD) plus that day's build number.
# Re-run cmake -S . -B <dir> after bumping or it does not take.
set(GW_BUILD_ID "260927.8")

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

# Brokild World FX - the shared rack, compiled in. Gravity Well is an
# instrument, so it carries the rack (the Martian Gain / Hairfryer rule
# cuts the other way for effects).
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
add_subdirectory("${BWFX_DIR}" bwfx)

juce_add_plugin(GravityWell
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE GrvW
    FORMATS VST3 Standalone
    PRODUCT_NAME "Gravity Well"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control on this machine blocks the freshly built, unsigned
    # vst3_helper.exe, which fails an otherwise good build at the manifest
    # step after the DLL is already good. The file is optional.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(GravityWell)

juce_add_binary_data(GravityWellAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "${BWFX_DIR}/ui/bwfx-rack.js")

target_sources(GravityWell
    PRIVATE
        Source/Engine.cpp
        Source/Presets.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(GravityWell
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        #  Every other WebView plug-in in the fleet loads WebView2Loader.dll
        #  dynamically and is found on this machine only because the Windows
        #  Performance Toolkit happens to be on PATH. Peter's work PC showed
        #  Internet Explorer's error page instead of a panel. Link it in.
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        GW_BUILD_ID="${GW_BUILD_ID}")

target_link_libraries(GravityWell
    PRIVATE
        GravityWellAssets
        bwfx
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_definitions(GravityWell PUBLIC _USE_MATH_DEFINES)
endif()

# ---------------------------------------------------------------------------
# The host harness: the REAL wrapper - parameters, buses, MIDI, state - with
# no DAW, no audio device and no window.
add_executable(gwhost test/hosttest.cpp)
target_link_libraries(gwhost PRIVATE GravityWell)
target_include_directories(gwhost PRIVATE $<TARGET_PROPERTY:GravityWell,INCLUDE_DIRECTORIES>)
target_include_directories(gwhost PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(gwhost PRIVATE $<TARGET_PROPERTY:GravityWell,COMPILE_DEFINITIONS>)
