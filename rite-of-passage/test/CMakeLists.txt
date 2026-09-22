# RITE OF PASSAGE offline bench — plain C++17, no JUCE, builds anywhere.
#   cmake -S test -B test/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build test/build
#   ./test/build/roptest
cmake_minimum_required(VERSION 3.22)
project(roptest LANGUAGES CXX)

# BWFX, because the registry now carries the wrapped World modules and a
# bench that could not build them would be testing a different plugin from
# the one that ships. BWFX is plain C++17 with no JUCE, so the bench stays a
# JUCE-free project that builds anywhere — it just has one more directory to
# find, and -DBWFX_DIR=<path> moves it.
set(BWFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../BrokildWorldFX" CACHE PATH "Brokild World FX")
add_subdirectory(${BWFX_DIR} bwfx)

add_executable(roptest
    bench.cpp
    ../engine/rop_loudness.cpp
    ../engine/rop_effects.cpp
    ../engine/rop_effects_bwfx.cpp
    ../engine/rop_effects_more.cpp
    ../engine/rop_effects_third.cpp
    ../engine/rop_rack.cpp)

target_link_libraries(roptest PRIVATE bwfx)

target_compile_features(roptest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_definitions(roptest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(roptest PRIVATE /O2 /W4)
else()
    target_compile_options(roptest PRIVATE -O2 -Wall -Wextra)
endif()
