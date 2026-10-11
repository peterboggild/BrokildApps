cmake_minimum_required(VERSION 3.22)
project(MagnetoTransportTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The bench measures numbers that get quoted, so no fast-math.
add_executable(mttest bench.cpp ../Source/Engine.cpp ../Source/Playback.cpp)
target_include_directories(mttest PRIVATE ../Source)
if(MSVC)
    target_compile_definitions(mttest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(mttest PRIVATE /O2 /fp:precise /W4)
else()
    target_compile_options(mttest PRIVATE -O2 -fno-fast-math -Wall -Wextra)
endif()

# Prints engine internals; asserts nothing. The tool that settles a question
# the bench raises.
add_executable(mtprobe probe.cpp ../Source/Engine.cpp ../Source/Playback.cpp)
target_include_directories(mtprobe PRIVATE ../Source)
if(MSVC)
    target_compile_definitions(mtprobe PRIVATE _USE_MATH_DEFINES)
    target_compile_options(mtprobe PRIVATE /O2 /fp:precise)
endif()
