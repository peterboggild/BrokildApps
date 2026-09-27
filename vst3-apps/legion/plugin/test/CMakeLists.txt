# LEGION offline bench — plain C++17, no JUCE, builds and runs anywhere.
#   cmake -S test -B test/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build test/build
#   ./test/build/legiontest
cmake_minimum_required(VERSION 3.22)
project(legiontest LANGUAGES CXX)

add_executable(legiontest
    bench.cpp
    ../engine/legion_fft.cpp
    ../engine/legion_analysis.cpp
    ../engine/legion_shifter.cpp
    ../engine/legion_harmonizer.cpp
    ../engine/legion_leveller.cpp)

target_compile_features(legiontest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_definitions(legiontest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(legiontest PRIVATE /O2 /W4)
else()
    target_compile_options(legiontest PRIVATE -O2 -Wall -Wextra)
endif()
