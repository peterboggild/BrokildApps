# LEGION — vocal harmoniser, VST3.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#
# JUCE is fetched on first configure. Set -DLEGION_JUCE_DIR=<path> to use a
# local checkout instead (offline machines, or to pin the version a release
# was built with).
#
# The bench in test/ is a separate, JUCE-free project — build that one first
# when changing the engine: cmake -S test -B test/build && cmake --build test/build

cmake_minimum_required(VERSION 3.22)

# The build id goes on the face of the plugin. Every other Brokild plugin
# carries one, and Rite of Passage cost a round of "i didnt see the new
# version in Ableton?" precisely because it did not: it is the one question
# a version number exists to answer BY LOOKING. Bump it AND re-run
# cmake -S . -B <dir> or it does not take.
set(LEGION_BUILD_ID "260926.1")
project(LEGION VERSION 0.1.0 LANGUAGES C CXX)   # JUCE pulls in C sources

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE
set(LEGION_JUCE_DIR "" CACHE PATH "Path to an existing JUCE checkout")
if(LEGION_JUCE_DIR)
    add_subdirectory(${LEGION_JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# BWFX — compiled IN, never a DLL (BWFX-DESIGN.md: Smart App Control blocks
# fresh DLL hashes per file, and patches must stay a promise on every machine)
set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX" CACHE PATH "Brokild World FX")
add_subdirectory(${BWFX_DIR} bwfx)

# ---------------------------------------------------------------------------
juce_add_plugin(LEGION
    PLUGIN_MANUFACTURER_CODE  Brkd
    PLUGIN_CODE               Lgn1
    COMPANY_NAME              "Brokild"
    BUNDLE_ID                 "dk.brokild.legion"
    FORMATS                   VST3
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    // Smart App Control on the build machine blocks the freshly linked
    // vst3_helper.exe, and JUCE runs it to write moduleinfo.json - so the
    // manifest step fails the whole build after the DLL is already good.
    // The file is optional and hosts scan the binary without it.
    VST3_AUTO_MANIFEST        FALSE
    VST3_CATEGORIES           Fx Pitch Shift
    PRODUCT_NAME              "Legion")

#  bwfx_juce.h (the shared adapter) includes <JuceHeader.h>, so the umbrella
#  header has to exist
juce_generate_juce_header(LEGION)

target_sources(LEGION PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/BwfxPanel.cpp
    engine/legion_fft.cpp
    engine/legion_analysis.cpp
    engine/legion_shifter.cpp
    engine/legion_harmonizer.cpp
    engine/legion_leveller.cpp)

target_compile_definitions(LEGION PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    LEGION_BUILD_ID="${LEGION_BUILD_ID}"
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(LEGION PRIVATE
    bwfx
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_definitions(LEGION PUBLIC _USE_MATH_DEFINES)
endif()

# ---------------------------------------------------------------------------
# The host harness. Runs the REAL wrapper — parameters, buses, latency, state,
# BWFX routing — with no DAW, no audio device and no window. A plain executable
# linked against the plugin's own shared-code target, so it inherits the same
# JuceHeader.h, the same JucePlugin_* defines and the same modules the VST3 is
# built from, and cannot drift from it.
add_executable(legionhost test/hosttest.cpp)
target_link_libraries(legionhost PRIVATE LEGION)
target_include_directories(legionhost PRIVATE $<TARGET_PROPERTY:LEGION,INCLUDE_DIRECTORIES>)
target_compile_definitions(legionhost PRIVATE $<TARGET_PROPERTY:LEGION,COMPILE_DEFINITIONS>)

# ---------------------------------------------------------------------------
# The panel snapshot. Legion has no standalone and no web panel, so rendering
# the real editor to a PNG is the only way to LOOK at it — and the only way to
# catch a layer that paints nothing, which is how the BWFX rack first shipped.
add_executable(legionshot test/shoteditor.cpp)
target_link_libraries(legionshot PRIVATE LEGION)
target_include_directories(legionshot PRIVATE $<TARGET_PROPERTY:LEGION,INCLUDE_DIRECTORIES>)
target_compile_definitions(legionshot PRIVATE $<TARGET_PROPERTY:LEGION,COMPILE_DEFINITIONS>)
