# Brokild World FX — compiled INTO every host synth (never a DLL: Smart App
# Control blocks fresh DLL hashes per file, and patches must stay a promise).
#
# Host usage:
#   add_subdirectory(${BWFX_DIR} bwfx)
#   target_link_libraries(<host> PRIVATE bwfx)
#
# The rack UI fragment (ui/bwfx-rack.js) is NOT built here — hosts embed it
# in their BinaryData and serve it beside their panel page.

cmake_minimum_required(VERSION 3.22)
project(BrokildWorldFX VERSION 1.0.0 LANGUAGES CXX)

add_library(bwfx STATIC
    src/bwfx_json.cpp
    src/bwfx_rack.cpp
    modules/bwfx_conv.cpp
    modules/bwfx_modules.cpp)

target_compile_features(bwfx PUBLIC cxx_std_17)
target_include_directories(bwfx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
    # adapter/ is header-only JUCE glue compiled by HOSTS, never by this lib
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter)
set_target_properties(bwfx PROPERTIES POSITION_INDEPENDENT_CODE ON)
