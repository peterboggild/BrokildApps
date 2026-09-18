# RITE OF PASSAGE offline bench — plain C++17, no JUCE, builds anywhere.
#   cmake -S test -B test/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build test/build
#   ./test/build/roptest
cmake_minimum_required(VERSION 3.22)
project(roptest LANGUAGES CXX)

add_executable(roptest
    bench.cpp
    ../engine/rop_loudness.cpp
    ../engine/rop_effects.cpp
    ../engine/rop_effects_more.cpp
    ../engine/rop_effects_third.cpp
    ../engine/rop_rack.cpp)

target_compile_features(roptest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_definitions(roptest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(roptest PRIVATE /O2 /W4)
else()
    target_compile_options(roptest PRIVATE -O2 -Wall -Wextra)
endif()
