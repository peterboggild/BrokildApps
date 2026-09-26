cmake_minimum_required(VERSION 3.22)

project(Beetmachine VERSION 1.0.0)

# Build id: date (YYMMDD) plus that day's build number. Bump it AND re-run
# `cmake -S . -B <dir>` or it will not take.
set(BEET_BUILD_ID "260926.7")

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

if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
# BWFX, compiled IN (never a DLL): the rack on the main mix
add_subdirectory(${BWFX_DIR} bwfx)

# ---------------------------------------------------------------------------
# The three drums, compiled from THEIR OWN source folders - no copies. A fix to
# Kickstart reaches Beetmachine on its next build. Each slot holds a real
# instance of the drum's processor and shows the drum's own editor.
# ---------------------------------------------------------------------------
set(DRUMS "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(KS "${DRUMS}/kickstart/plugin")
set(ST "${DRUMS}/snare-tactics/plugin")
set(HO "${DRUMS}/hats-off/plugin")

# Their build ids, read from their own CMakeLists so they never go stale here.
foreach(pair "KS;${KS}" "ST;${ST}" "HO;${HO}")
    list(GET pair 0 tag)
    list(GET pair 1 dir)
    file(STRINGS "${dir}/CMakeLists.txt" line REGEX "set\\(${tag}_BUILD_ID")
    string(REGEX MATCH "\"([0-9.]+)\"" _ "${line}")
    set(${tag}_BUILD_ID "${CMAKE_MATCH_1}")
    message(STATUS "Beetmachine hosts ${tag} build ${${tag}_BUILD_ID}")
endforeach()

set(DRUM_SOURCES
    ${KS}/engine/ks_engine.cpp  ${KS}/engine/ks_presets.cpp
    ${KS}/src/PluginProcessor.cpp ${KS}/src/PluginEditor.cpp
    ${ST}/engine/st_engine.cpp  ${ST}/engine/st_presets.cpp
    ${ST}/src/PluginProcessor.cpp ${ST}/src/PluginEditor.cpp
    ${HO}/engine/ho_engine.cpp  ${HO}/engine/ho_presets.cpp
    ${HO}/src/PluginProcessor.cpp ${HO}/src/PluginEditor.cpp)

# Three things are named the same in all three drums and would collide at
# link: the plug-in entry point, and two panel helper classes (HitPad,
# HitDisplay) that are DIFFERENT classes under one name - which the linker
# refuses, and which would otherwise be a silent one-definition violation.
# Each is renamed per drum, in that drum's own files only. Nothing in the
# drums' source changes, and the standalone drums build exactly as before.
# A new global name added to a drum's panel shows up here as a link error.
foreach(pair "ks;${KS}" "st;${ST}" "ho;${HO}")
    list(GET pair 0 tag)
    list(GET pair 1 dir)
    set_source_files_properties(${dir}/src/PluginProcessor.cpp ${dir}/src/PluginEditor.cpp PROPERTIES COMPILE_DEFINITIONS
        "createPluginFilter=beet_${tag}_createPluginFilter;HitPad=${tag}_HitPad;HitDisplay=${tag}_HitDisplay")
endforeach()

juce_add_plugin(Beetmachine
    COMPANY_NAME             "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE              Beet
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "Beetmachine"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # the BWFX rack is the standard web rack (bwfx-rack.js), as in Black Rider
    NEEDS_WEB_BROWSER        TRUE
    NEEDS_WEBVIEW2           TRUE
    # Smart App Control blocks the freshly built vst3_helper and fails the build
    # at the manifest step; the manifest is optional.
    VST3_AUTO_MANIFEST       FALSE
    VST3_CATEGORIES          "Instrument" "Drum")

juce_generate_juce_header(Beetmachine)

# The drum family's shared machine-style parts (vst3-apps/machine-art). The
# drums' panels are compiled in here and draw with them too: ONE set.
include("${CMAKE_CURRENT_SOURCE_DIR}/../../machine-art/machine-art.cmake")
brokild_add_machine_art(Beetmachine)

# the host page and the standard rack fragment, served from memory
juce_add_binary_data(BeetWeb HEADER_NAME BeetWebData.h NAMESPACE BeetWebData SOURCES
    src/web/bwfx-host.html
    "${BWFX_DIR}/ui/bwfx-rack.js")

target_sources(Beetmachine PRIVATE
    src/BeetProcessor.cpp
    src/BeetEditor.cpp
    src/BeetKits.cpp
    src/BeetLook.cpp
    src/BwfxPanel.cpp
    ${DRUM_SOURCES})

target_include_directories(Beetmachine PRIVATE "${BWFX_DIR}/adapter")

target_compile_definitions(Beetmachine PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    # link the WebView2 loader in, so no WebView2Loader.dll has to be on the PATH
    # (the Thin Walls lesson: it was only found here because of a dev kit)
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    _USE_MATH_DEFINES
    BEET_BUILD_ID="${BEET_BUILD_ID}"
    KS_BUILD_ID="${KS_BUILD_ID}"
    ST_BUILD_ID="${ST_BUILD_ID}"
    HO_BUILD_ID="${HO_BUILD_ID}")

target_link_libraries(Beetmachine
    PRIVATE
        bwfx
        BeetWeb
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(Beetmachine PRIVATE /fp:precise /bigobj)
endif()

# ---------------------------------------------------------------------------
# Gates. beettest drives the processor with no host; beetshot renders the
# editor to PNG, because a panel is checked by LOOKING at it.
# ---------------------------------------------------------------------------
foreach(t beettest beetshot beetrender)
    add_executable(${t} test/${t}.cpp)
    target_link_libraries(${t} PRIVATE Beetmachine bwfx)
    target_include_directories(${t} PRIVATE $<TARGET_PROPERTY:Beetmachine,INCLUDE_DIRECTORIES>)
    target_compile_definitions(${t} PRIVATE $<TARGET_PROPERTY:Beetmachine,COMPILE_DEFINITIONS>)
endforeach()
