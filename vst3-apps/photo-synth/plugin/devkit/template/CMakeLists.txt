cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# HTML → VST3 project template.
#
#   cmake -S . -B build -G "Visual Studio 18 2026" -A x64
#   cmake --build build --config Release
#
# Keep the project path short (e.g. C:\b\MyPlugin): deeply nested paths hit
# MSVC's path-length limit during JUCE builds.
# ---------------------------------------------------------------------------

project(MyPlugin VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point this at your JUCE clone (JUCE 8.x).
set(JUCE_DIR "C:/AudioDev/JUCE")
add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_plugin(MyPlugin
    COMPANY_NAME "YourVendor"
    PLUGIN_MANUFACTURER_CODE Yvnd      # 4 chars — your vendor
    PLUGIN_CODE Mypl                   # 4 chars — UNIQUE per plugin, never
                                       # change after release: it is part of
                                       # the VST3 class id projects store.
    FORMATS VST3 Standalone
    PRODUCT_NAME "My Plugin"
    IS_SYNTH TRUE                      # FALSE for an effect
    NEEDS_MIDI_INPUT TRUE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE)

juce_generate_juce_header(MyPlugin)

# The interface page is compiled in, so the plugin runs with no external files.
juce_add_binary_data(MyPluginAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html)

target_sources(MyPlugin
    PRIVATE
        Source/Engine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(MyPlugin
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MyPlugin
    PRIVATE
        MyPluginAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
