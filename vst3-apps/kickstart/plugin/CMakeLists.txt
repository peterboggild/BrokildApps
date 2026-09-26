# KICKSTART - the kick drum synthesiser, VST3 + Standalone.
#
#   cmake -S . -B <build> -A x64 -DJUCE_DIR=<path to JUCE>
#   cmake --build <build> --config Release
#
# The engine bench in test/ is a separate, JUCE-free project; build and run it
# first whenever the engine changes.

cmake_minimum_required(VERSION 3.22)

# The build id goes on the face of the plugin. Bump it AND re-run
# cmake -S . -B <dir>, or it does not take.
set(KS_BUILD_ID "260926.2")
project(Kickstart VERSION 1.0.0 LANGUAGES C CXX)

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

juce_add_plugin(Kickstart
    COMPANY_NAME             "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE              Kick
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "Kickstart"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control can block the freshly built vst3_helper.exe and fail
    # an otherwise good build at the manifest step. It is optional metadata.
    VST3_AUTO_MANIFEST       FALSE
    VST3_CATEGORIES          "Instrument" "Drum")

juce_generate_juce_header(Kickstart)

# the drum family's shared machine-style parts (vst3-apps/machine-art)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../machine-art/machine-art.cmake")
brokild_add_machine_art(Kickstart)

target_sources(Kickstart PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    engine/ks_engine.cpp
    engine/ks_presets.cpp)

target_include_directories(Kickstart PRIVATE "${BWFX_DIR}/adapter")

target_compile_definitions(Kickstart PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    KS_BUILD_ID="${KS_BUILD_ID}")

target_link_libraries(Kickstart
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_definitions(Kickstart PUBLIC _USE_MATH_DEFINES)
endif()

# ---------------------------------------------------------------------------
# The host harness: the REAL wrapper (parameters, MIDI, latency, state,
# presets) with no DAW, no audio device and no window.
add_executable(kshost test/hosttest.cpp)
target_link_libraries(kshost PRIVATE Kickstart)
target_include_directories(kshost PRIVATE $<TARGET_PROPERTY:Kickstart,INCLUDE_DIRECTORIES>)
target_compile_definitions(kshost PRIVATE $<TARGET_PROPERTY:Kickstart,COMPILE_DEFINITIONS>)

# The panel snapshot: renders the real editor to PNGs and measures it.
add_executable(ksshot test/shoteditor.cpp)
target_link_libraries(ksshot PRIVATE Kickstart)
target_include_directories(ksshot PRIVATE $<TARGET_PROPERTY:Kickstart,INCLUDE_DIRECTORIES>)
target_compile_definitions(ksshot PRIVATE $<TARGET_PROPERTY:Kickstart,COMPILE_DEFINITIONS>)
