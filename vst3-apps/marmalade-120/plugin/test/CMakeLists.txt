cmake_minimum_required(VERSION 3.22)
project(Marmalade120Test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The bench measures numbers that get quoted, and spicecheck compares the
# solver against ngspice to a fraction of a percent, so no fast-math anywhere.
set(MM_SOURCES ../Source/Circuit.cpp ../Source/Engine.cpp)

add_executable(mmtest bench.cpp ${MM_SOURCES})
target_include_directories(mmtest PRIVATE ../Source)

# Needs ngspice on the PATH; exits 1 if the solver and ngspice disagree.
add_executable(spicecheck spicecheck.cpp ../Source/Circuit.cpp)
target_include_directories(spicecheck PRIVATE ../Source)

# Runs a WAV (or a built-in plucked riff) through the engine, to listen.
add_executable(mmrender render.cpp ${MM_SOURCES})
target_include_directories(mmrender PRIVATE ../Source)

foreach(t mmtest spicecheck mmrender)
    if(MSVC)
        target_compile_options(${t} PRIVATE /O2 /fp:precise /W4)
    else()
        target_compile_options(${t} PRIVATE -O2 -fno-fast-math -Wall -Wextra)
    endif()
endforeach()
