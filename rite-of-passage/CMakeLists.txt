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
    // Smart App Control on the build machine blocks the freshly linked
    // vst3_helper.exe, and JUCE runs it to write moduleinfo.json — so the
    // manifest step fails the whole build after the DLL is already good.
    // The file is optional and hosts scan the binary without it.
    VST3_AUTO_MANIFEST        FALSE
    VST3_CATEGORIES           Fx Modulation
    PRODUCT_NAME              "Rite of Passage")

juce_generate_juce_header(RITEOFPASSAGE)

# ---------------------------------------------------------------------------
# The decals (assets/rite-decals, ingested by tools/ingest-decals.ps1). The
# panel looks each one up BY FILENAME and draws its procedural fallback when
# it is missing, so this glob may legitimately be empty.
file(GLOB ROP_DECALS "${CMAKE_CURRENT_SOURCE_DIR}/decals/*.png")
if(ROP_DECALS)
    juce_add_binary_data(ropdecals SOURCES ${ROP_DECALS})
    set_target_properties(ropdecals PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    target_link_libraries(RITEOFPASSAGE PRIVATE ropdecals)
endif()

target_sources(RITEOFPASSAGE PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/BwfxPanel.cpp
    engine/rop_loudness.cpp
    engine/rop_effects.cpp
    engine/rop_effects_bwfx.cpp
    engine/rop_effects_more.cpp
    engine/rop_effects_third.cpp
    engine/rop_rack.cpp)

# The build id. Bump it for a new build AND re-run cmake -S . -B <dir>, or
# it will not take — the same rule every other plugin in the fleet has.
set(ROP_BUILD_ID "260922.1")

target_compile_definitions(RITEOFPASSAGE PUBLIC
    ROP_BUILD_ID="${ROP_BUILD_ID}"
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

# ---------------------------------------------------------------------------
# The panel snapshot: renders the REAL editor to PNGs (empty, loaded, midway)
# and checks the lanes take on heat. There is no standalone and no web panel,
# so this is the only way to look at it (Legion's lesson).
add_executable(ropshot test/shoteditor.cpp)
target_link_libraries(ropshot PRIVATE RITEOFPASSAGE)
target_include_directories(ropshot PRIVATE $<TARGET_PROPERTY:RITEOFPASSAGE,INCLUDE_DIRECTORIES>)
target_compile_definitions(ropshot PRIVATE $<TARGET_PROPERTY:RITEOFPASSAGE,COMPILE_DEFINITIONS>)
