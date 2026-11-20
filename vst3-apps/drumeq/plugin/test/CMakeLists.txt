# DRUMEQ offline bench - plain C++17, no JUCE.
#   cmake -S test -B <build> && cmake --build <build> --config Release
cmake_minimum_required(VERSION 3.22)
project(detest LANGUAGES CXX)
set(DE_ENGINE ../engine/de_engine.cpp ../engine/de_params.cpp ../engine/de_sources.cpp ../engine/de_find.cpp)
add_executable(detest bench.cpp ${DE_ENGINE})
target_compile_features(detest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_definitions(detest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(detest PRIVATE /O2 /W4 /fp:precise)
else()
    target_compile_options(detest PRIVATE -O2 -Wall -Wextra)
endif()
