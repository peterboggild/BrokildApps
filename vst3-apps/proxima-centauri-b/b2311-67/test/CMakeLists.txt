cmake_minimum_required(VERSION 3.22)
project(AB67Bench CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ab67test
    bench.cpp
    ../Source/Lattice.cpp
    ../Source/Engine.cpp ../Source/Modulation.cpp)

if(MSVC)
    # M_PI is not defined by default in MSVC; the sources use their own PI but
    # the bench holds a large Engine on the stack, so give it room.
    target_compile_definitions(ab67test PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab67test PRIVATE /O2 /fp:precise)
    target_link_options(ab67test PRIVATE /STACK:67108864)
endif()
add_executable(t6 t6.cpp ../Source/Lattice.cpp ../Source/Engine.cpp ../Source/Modulation.cpp)
target_link_options(t6 PRIVATE /STACK:67108864)
target_compile_options(t6 PRIVATE $<$<CONFIG:Release>:/O2>)
add_executable(ab67probe probe.cpp ../Source/Lattice.cpp ../Source/Engine.cpp ../Source/Modulation.cpp)
target_link_options(ab67probe PRIVATE /STACK:67108864)
target_compile_options(ab67probe PRIVATE $<$<CONFIG:Release>:/O2>)
add_executable(t6asan t6.cpp ../Source/Lattice.cpp ../Source/Engine.cpp ../Source/Modulation.cpp)
target_compile_options(t6asan PRIVATE /fsanitize=address /Zi /Od)
target_link_options(t6asan PRIVATE /STACK:67108864 /DEBUG)

add_executable(ab67gaincal gaincal.cpp ../Source/Lattice.cpp ../Source/Engine.cpp ../Source/Modulation.cpp)
target_link_options(ab67gaincal PRIVATE /STACK:67108864)
target_compile_options(ab67gaincal PRIVATE $<$<CONFIG:Release>:/O2>)

add_executable(ab67void voidtest.cpp ../Source/Lattice.cpp ../Source/Engine.cpp ../Source/Modulation.cpp)
if(MSVC)
    target_compile_definitions(ab67void PRIVATE _USE_MATH_DEFINES)
    target_compile_options(ab67void PRIVATE /O2 /fp:fast)
    target_link_options(ab67void PRIVATE /STACK:33554432)
endif()
