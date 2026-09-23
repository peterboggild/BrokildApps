cmake_minimum_required(VERSION 3.22)
project(ab2311test CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(abtest
    bench.cpp
    ../Source/Specimen.cpp
    ../Source/Engine.cpp)
target_compile_definitions(abtest PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_compile_options(abtest PRIVATE /O2 /fp:fast)
    # cw-lesson: big engine objects on the stack need headroom on Windows
    target_link_options(abtest PRIVATE /STACK:33554432)
endif()

# a direct probe of engine internals — the bench answers "does it work",
# this answers "what exactly is it doing"
add_executable(abprobe
    probe.cpp
    ../Source/Specimen.cpp
    ../Source/Engine.cpp)
target_compile_definitions(abprobe PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_compile_options(abprobe PRIVATE /O2 /fp:fast)
    target_link_options(abprobe PRIVATE /STACK:33554432)
endif()

# how much does it evolve, and does playing leave it changed?
add_executable(abevolve
    evolve.cpp
    ../Source/Specimen.cpp
    ../Source/Engine.cpp)
target_compile_definitions(abevolve PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_compile_options(abevolve PRIVATE /O2 /fp:fast)
    target_link_options(abevolve PRIVATE /STACK:33554432)
endif()
