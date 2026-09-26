cmake_minimum_required(VERSION 3.22)
project(BKTest CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(bktest test.cpp ../Source/Engine.cpp ../Source/Seed.cpp)
target_include_directories(bktest PRIVATE ../Source)

add_executable(bklevel levelprobe.cpp ../Source/Engine.cpp ../Source/Seed.cpp)
target_include_directories(bklevel PRIVATE ../Source)
