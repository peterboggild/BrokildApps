cmake_minimum_required(VERSION 3.22)
project(BrainScanBench CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bstest bench.cpp ../Source/Engine.cpp ../Source/Specimens.cpp ../Source/Anatomy.cpp ../Source/Import.cpp)
if(MSVC)
    target_compile_definitions(bstest PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bstest PRIVATE /O2 /fp:precise)
    # engines on the stack need room
    target_link_options(bstest PRIVATE /STACK:67108864)
endif()
add_executable(bsprobe probe-enamel.cpp ../Source/Engine.cpp ../Source/Specimens.cpp ../Source/Anatomy.cpp ../Source/Import.cpp)
target_compile_features(bsprobe PRIVATE cxx_std_17)
