# KICKSTART offline bench - plain C++17, no JUCE.
#   cmake -S test -B <build> && cmake --build <build> --config Release
cmake_minimum_required(VERSION 3.22)
project(kstest LANGUAGES CXX)
add_executable(kstest bench.cpp ../engine/ks_engine.cpp ../engine/ks_presets.cpp)
target_compile_features(kstest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_definitions(kstest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(kstest PRIVATE /O2 /W4 /fp:precise)
else()
    target_compile_options(kstest PRIVATE -O2 -Wall -Wextra)
endif()

# the landing page demos
add_executable(ksrender render.cpp ../engine/ks_engine.cpp ../engine/ks_presets.cpp)
target_compile_features(ksrender PRIVATE cxx_std_17)
if(MSVC)
    target_compile_definitions(ksrender PRIVATE _USE_MATH_DEFINES)
endif()
