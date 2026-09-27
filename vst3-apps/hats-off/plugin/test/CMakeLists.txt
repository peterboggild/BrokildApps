# HATS OFF offline bench - plain C++17, no JUCE.
#   cmake -S test -B <build> && cmake --build <build> --config Release
cmake_minimum_required(VERSION 3.22)
project(hotest LANGUAGES CXX)
add_executable(hotest bench.cpp ../engine/ho_engine.cpp ../engine/ho_presets.cpp)
target_compile_features(hotest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_definitions(hotest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(hotest PRIVATE /O2 /W4 /fp:precise)
else()
    target_compile_options(hotest PRIVATE -O2 -Wall -Wextra)
endif()

# the landing page demos, over a kick from Kickstart and a snare from Snare Tactics
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/render.cpp")
    add_executable(horender render.cpp ../engine/ho_engine.cpp ../engine/ho_presets.cpp
        ../../../kickstart/plugin/engine/ks_engine.cpp ../../../kickstart/plugin/engine/ks_presets.cpp
        ../../../snare-tactics/plugin/engine/st_engine.cpp ../../../snare-tactics/plugin/engine/st_presets.cpp)
    target_compile_features(horender PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_definitions(horender PRIVATE _USE_MATH_DEFINES)
        target_compile_options(horender PRIVATE /O2)
    endif()
endif()
