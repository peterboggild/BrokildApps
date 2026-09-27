cmake_minimum_required(VERSION 3.22)
project(BattlestarOverdriveTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# A distortion bench measures numbers that get quoted, so no fast-math: a
# reciprocal approximation has no business in something whose output is the
# figure in the manual.
add_executable(botest bench.cpp ../Source/Engine.cpp)
target_include_directories(botest PRIVATE ../Source)

if(MSVC)
    target_compile_options(botest PRIVATE /O2 /fp:precise /W4)
else()
    target_compile_options(botest PRIVATE -O2 -fno-fast-math -Wall -Wextra)
endif()

# Sound-quality measurements: aliasing, IMD, auto-gain behaviour, headroom,
# noise floor, oversampler transparency. Prints numbers, asserts nothing -
# the bench says whether it works, this says whether it is good.
add_executable(boquality quality.cpp ../Source/Engine.cpp)
target_include_directories(boquality PRIVATE ../Source)
if(MSVC)
    target_compile_options(boquality PRIVATE /O2 /fp:precise /W4)
else()
    target_compile_options(boquality PRIVATE -O2 -fno-fast-math -Wall -Wextra)
endif()
