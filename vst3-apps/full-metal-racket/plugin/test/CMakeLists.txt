cmake_minimum_required(VERSION 3.22)
project(fmrtest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(fmrtest test.cpp ../Source/Engine.cpp ../Source/Kits.cpp)

if(MSVC)
    # M_PI (the engine is written for both toolchains), and a big stack:
    # fmr::Engine is a stack local in the bench and its voice array alone
    # is well over Windows' 1 MB default. Symptom without it is 0xC00000FD
    # before a single line prints. (The Clone Wars lesson.)
    target_compile_definitions(fmrtest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(fmrtest PRIVATE /O2 /fp:precise)
    target_link_options(fmrtest PRIVATE /STACK:33554432)
else()
    target_compile_options(fmrtest PRIVATE -O2 -fno-fast-math)
endif()
