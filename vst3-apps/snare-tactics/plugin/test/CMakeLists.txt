# SNARE TACTICS offline bench - plain C++17, no JUCE.
#   cmake -S test -B <build> && cmake --build <build> --config Release
cmake_minimum_required(VERSION 3.22)
project(sttest LANGUAGES CXX)
add_executable(sttest bench.cpp ../engine/st_engine.cpp ../engine/st_presets.cpp)
target_compile_features(sttest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_definitions(sttest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(sttest PRIVATE /O2 /W4 /fp:precise)
else()
    target_compile_options(sttest PRIVATE -O2 -Wall -Wextra)
endif()

# the landing page demos
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/render.cpp")
    # over a kick from its sibling, Kickstart (separate namespace, no clash)
    add_executable(strender render.cpp ../engine/st_engine.cpp ../engine/st_presets.cpp
        ../../../kickstart/plugin/engine/ks_engine.cpp ../../../kickstart/plugin/engine/ks_presets.cpp)
    target_compile_features(strender PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_definitions(strender PRIVATE _USE_MATH_DEFINES)
        target_compile_options(strender PRIVATE /O2)
    endif()
endif()
