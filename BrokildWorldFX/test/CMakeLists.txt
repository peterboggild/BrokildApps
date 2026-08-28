# BWFX offline bench — plain C++17, no JUCE.
#   cmake -S test -B test/build
#   cmake --build test/build --config Release
#   test/build/Release/bwfxtest.exe
cmake_minimum_required(VERSION 3.22)
project(bwfxtest LANGUAGES CXX)

add_executable(bwfxtest
    bench.cpp
    ../src/bwfx_json.cpp
    ../src/bwfx_rack.cpp
    ../src/bwfx_macros.cpp
    ../modules/bwfx_conv.cpp
    ../modules/bwfx_modules.cpp
    ../modules/bwfx_spectra.cpp)

target_compile_features(bwfxtest PRIVATE cxx_std_17)
target_include_directories(bwfxtest PRIVATE ../src ../modules)
if(MSVC)
    target_compile_definitions(bwfxtest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(bwfxtest PRIVATE /O2)
else()
    target_compile_options(bwfxtest PRIVATE -O2 -Wall -Wextra)
endif()

add_executable(dbgclick
    dbgclick.cpp
    ../src/bwfx_json.cpp
    ../src/bwfx_rack.cpp
    ../src/bwfx_macros.cpp
    ../modules/bwfx_conv.cpp
    ../modules/bwfx_modules.cpp
    ../modules/bwfx_spectra.cpp)
target_compile_features(dbgclick PRIVATE cxx_std_17)
target_include_directories(dbgclick PRIVATE ../src ../modules)
