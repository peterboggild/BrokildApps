cmake_minimum_required(VERSION 3.22)

project(MagnetoTransport VERSION 0.1.0)

# Build id: date (YYMMDD) plus that day's build number, shown on the panel.
# Bump it AND re-run `cmake -S . -B <dir>` or it will not take.
set(MT_BUILD_ID "261012.1")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is not vendored. A local build passes -DJUCE_DIR=<your checkout>;
# a fresh clone fetches it.
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

# No Brokild World FX: this is an effect (the Martian Gain / Battlestar call).
# The adapter folder is on the include path for brokild_paths.h only.
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()

juce_add_plugin(MagnetoTransport
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE MgTr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Magneto Transport"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control blocks the freshly built, unsigned vst3_helper.exe and
    # fails the build at the manifest step. moduleinfo.json is optional.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Fx" "Delay")

juce_generate_juce_header(MagnetoTransport)

# The decals that passed tools/ingest-decals.ps1. CONFIGURE_DEPENDS re-runs the glob at
# every build, so art copied in after configuring is not silently left out; the count
# is printed so a build without its art says so.
file(GLOB MT_DECALS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/decals/*.png")
list(LENGTH MT_DECALS MT_DECAL_COUNT)
message(STATUS "Magneto Transport: ${MT_DECAL_COUNT} decals in assets/decals")

juce_add_binary_data(MagnetoTransportAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        Source/ui/tips.json
        Source/ui/tips-numbers.json
        Source/ui/figdata.json
        ${MT_DECALS})

target_sources(MagnetoTransport
    PRIVATE
        Source/Engine.cpp
        Source/Playback.cpp
        Source/Equaliser.cpp
        Source/Transport.cpp
        Source/Stock.cpp
        Source/Medium.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(MagnetoTransport PRIVATE "${BWFX_DIR}/adapter")

target_compile_definitions(MagnetoTransport
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        # Link the WebView2 loader statically: dynamically it is found on the
        # home PC only because a Windows SDK tool folder is on the PATH.
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    PRIVATE
        MT_BUILD_ID="${MT_BUILD_ID}")

target_link_libraries(MagnetoTransport
    PRIVATE
        MagnetoTransportAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Every number in this plug-in is a measurement: no fast-math.
if(MSVC)
    target_compile_definitions(MagnetoTransport PRIVATE _USE_MATH_DEFINES)
    target_compile_options(MagnetoTransport PRIVATE /fp:precise)
else()
    target_compile_options(MagnetoTransport PRIVATE -fno-fast-math)
endif()
