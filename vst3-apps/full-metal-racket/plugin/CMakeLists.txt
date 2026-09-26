cmake_minimum_required(VERSION 3.22)

project(FullMetalRacket VERSION 0.1.0)

# Build id: date (YYMMDD) plus the build number that day. Bump it AND re-run
# cmake -S . -B build, or the compiled-in value does not change.
set(FM_BUILD_ID "260926.1")

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

# Brokild World FX — the shared global rack, compiled in (BWFX-DESIGN.md).
if(NOT DEFINED BWFX_DIR)
    set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")
endif()
add_subdirectory("${BWFX_DIR}" bwfx)

juce_add_plugin(FullMetalRacket
    COMPANY_NAME "Brokild"
    PLUGIN_MANUFACTURER_CODE Brkd
    PLUGIN_CODE FmRk
    FORMATS VST3 Standalone
    PRODUCT_NAME "Full Metal Racket"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Smart App Control on this machine blocks the freshly built, unsigned
    # vst3_helper.exe, which fails an otherwise good build at the manifest
    # step (and silently yields a 0-byte moduleinfo.json when it half-works).
    # moduleinfo.json is optional; hosts scan the binary without it.
    VST3_AUTO_MANIFEST FALSE
    VST3_CATEGORIES "Instrument" "Drum")

juce_generate_juce_header(FullMetalRacket)

# ── panel decals ────────────────────────────────────────────────────────────
# Whatever PNGs exist at CONFIGURE time get compiled in; none is required. Add
# or replace files and re-run "cmake -S . -B build", or the glob is stale.
# Two locations are searched, local first, and a duplicate basename is skipped
# so the same part cannot be compiled in twice.
set(FMR_ART_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/art"
    "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/assets/fmr-panel-decals")
set(FMR_ART_FILES "")
set(_fmr_seen "")
foreach(_dir ${FMR_ART_DIRS})
    file(GLOB _found "${_dir}/fmr-*.png")   # only parts; source sheets are not compiled in
    foreach(_f ${_found})
        get_filename_component(_base "${_f}" NAME)
        list(FIND _fmr_seen "${_base}" _at)
        if(_at EQUAL -1)
            list(APPEND _fmr_seen "${_base}")
            list(APPEND FMR_ART_FILES "${_f}")
        endif()
    endforeach()
endforeach()
list(LENGTH FMR_ART_FILES _fmr_count)
message(STATUS "Full Metal Racket: ${_fmr_count} panel decal(s) compiled in")

juce_add_binary_data(FullMetalRacketAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "${BWFX_DIR}/ui/bwfx-rack.js"
        ${FMR_ART_FILES})

target_sources(FullMetalRacket
    PRIVATE
        Source/Engine.cpp
        Source/Kits.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(FullMetalRacket
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        _USE_MATH_DEFINES=1
        FM_BUILD_ID="${FM_BUILD_ID}")

target_link_libraries(FullMetalRacket
    PRIVATE
        FullMetalRacketAssets
        bwfx
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
