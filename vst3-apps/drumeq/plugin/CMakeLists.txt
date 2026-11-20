# DRUMEQ - the acoustic drum channel: one plug-in, a SOURCE per microphone,
# a shared KIT across every instance. VST3 + Standalone.
#
#   cmake -S . -B <build> -A x64 -DJUCE_DIR=<path to JUCE>
#   cmake --build <build> --config Release
#
# The engine bench in test/ is a separate, JUCE-free project; build and run it
# first whenever the engine changes.

cmake_minimum_required(VERSION 3.22)

# The build id goes on the face of the plugin. Bump it AND re-run
# cmake -S . -B <dir>, or it does not take.
set(DE_BUILD_ID "261120.2")
project(DrumEQ VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# the shared house helpers (the patch folder) live with BWFX
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()

juce_add_plugin(DrumEQ
    COMPANY_NAME             "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE              DrEq
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "DrumEQ"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    # Smart App Control can block the freshly built vst3_helper.exe and fail
    # an otherwise good build at the manifest step. It is optional metadata.
    VST3_AUTO_MANIFEST       FALSE
    VST3_CATEGORIES          "Fx" "EQ" "Dynamics")

juce_generate_juce_header(DrumEQ)

# the drum family's shared machine-style parts (vst3-apps/machine-art)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../machine-art/machine-art.cmake")
brokild_add_machine_art(DrumEQ)

target_sources(DrumEQ PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    engine/de_engine.cpp
    engine/de_params.cpp
    engine/de_sources.cpp
    engine/de_find.cpp)

target_include_directories(DrumEQ PRIVATE "${BWFX_DIR}/adapter")

target_compile_definitions(DrumEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    DE_BUILD_ID="${DE_BUILD_ID}")

target_link_libraries(DrumEQ
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_definitions(DrumEQ PUBLIC _USE_MATH_DEFINES)
endif()

# ---------------------------------------------------------------------------
# The host harness: the REAL wrapper (parameters, buses, latency, state,
# presets, and the KIT between two instances) with no DAW and no window.
add_executable(dehost test/hosttest.cpp)
target_link_libraries(dehost PRIVATE DrumEQ)
target_include_directories(dehost PRIVATE $<TARGET_PROPERTY:DrumEQ,INCLUDE_DIRECTORIES>)
target_compile_definitions(dehost PRIVATE $<TARGET_PROPERTY:DrumEQ,COMPILE_DEFINITIONS>)

# The panel snapshot: renders the real editor to PNGs and measures it.
add_executable(deshot test/shoteditor.cpp)
target_link_libraries(deshot PRIVATE DrumEQ)
target_include_directories(deshot PRIVATE $<TARGET_PROPERTY:DrumEQ,INCLUDE_DIRECTORIES>)
target_compile_definitions(deshot PRIVATE $<TARGET_PROPERTY:DrumEQ,COMPILE_DEFINITIONS>)
