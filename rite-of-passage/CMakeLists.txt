# RITE OF PASSAGE — transition processor, VST3.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#
# JUCE is fetched on first configure; -DROP_JUCE_DIR=<path> uses a local
# checkout instead. The bench in test/ is a separate JUCE-free project and
# should be built and run FIRST when the engine changes.

cmake_minimum_required(VERSION 3.22)
project(RITEOFPASSAGE VERSION 0.1.0 LANGUAGES C CXX)   # JUCE pulls in C sources

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROP_JUCE_DIR "" CACHE PATH "Path to an existing JUCE checkout")
if(ROP_JUCE_DIR)
    add_subdirectory(${ROP_JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# BWFX — compiled IN, never a DLL (BWFX-DESIGN.md)
set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../BrokildWorldFX" CACHE PATH "Brokild World FX")
add_subdirectory(${BWFX_DIR} bwfx)

juce_add_plugin(RITEOFPASSAGE
    PLUGIN_MANUFACTURER_CODE  Brkd
    PLUGIN_CODE               Rop1
    COMPANY_NAME              "Brokild"
    BUNDLE_ID                 "dk.brokild.riteofpassage"
    FORMATS                   VST3
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Modulation
    PRODUCT_NAME              "Rite of Passage")

juce_generate_juce_header(RITEOFPASSAGE)

target_sources(RITEOFPASSAGE PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    engine/rop_loudness.cpp
    engine/rop_effects.cpp
    engine/rop_rack.cpp)

target_compile_definitions(RITEOFPASSAGE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(RITEOFPASSAGE PRIVATE
    bwfx
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_definitions(RITEOFPASSAGE PUBLIC _USE_MATH_DEFINES)
endif()

# ---------------------------------------------------------------------------
# The host harness: the REAL wrapper with no DAW, no device and no window.
add_executable(ropehost test/hosttest.cpp)
target_link_libraries(ropehost PRIVATE RITEOFPASSAGE)
target_include_directories(ropehost PRIVATE $<TARGET_PROPERTY:RITEOFPASSAGE,INCLUDE_DIRECTORIES>)
target_compile_definitions(ropehost PRIVATE $<TARGET_PROPERTY:RITEOFPASSAGE,COMPILE_DEFINITIONS>)
