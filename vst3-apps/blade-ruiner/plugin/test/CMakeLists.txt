cmake_minimum_required(VERSION 3.22)
project(brtest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Plain C++, no JUCE: the engine has no dependencies, which is the point.
add_executable(brtest test.cpp ../Source/Engine.cpp ../Source/Mood.cpp)
target_include_directories(brtest PRIVATE ../Source)
