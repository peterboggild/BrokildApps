cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Clone Wars — 16-voice analogue drone monster (VST3 + Standalone).
# Built on the BrokildApps HTML→VST3 devkit conventions: JUCE 8, WebView2 UI
# compiled into the binary, native C++ engine (Source/Core, JUCE-free).
#
# Local build (Windows, per devkit — keep the path short, e.g. C:\b\CloneWars):
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR=C:/AudioDev/JUCE
#   cmake --build build --config Release
#
# Without -DJUCE_DIR, JUCE 8 is fetched automatically (used by CI).
# The engine alone needs no JUCE at all:  see test/render_test.cpp.
# ---------------------------------------------------------------------------

project(CloneWars VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(CloneWars
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE Cwar                    # unique, never change after release
    FORMATS VST3 Standalone
    PRODUCT_NAME "Clone Wars"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE)

juce_generate_juce_header(CloneWars)

# The panel page is compiled in; the plugin runs with no external files.
juce_add_binary_data(CloneWarsAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html)

target_sources(CloneWars
    PRIVATE
        Source/Core/cw_core.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(CloneWars
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CloneWars
    PRIVATE
        CloneWarsAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
