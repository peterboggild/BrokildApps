cmake_minimum_required(VERSION 3.22)
project(Marmalade50Test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The solver is the Marmalade 120's, shared, not copied: one circuit solver,
# checked against ngspice once, for both amps.
set(MM_SOLVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../marmalade-120/plugin/Source)
set(MM_INCLUDES ../Source ${MM_SOLVER_DIR})

add_executable(mm50bias biasset.cpp ${MM_SOLVER_DIR}/Circuit.cpp)
target_include_directories(mm50bias PRIVATE ${MM_INCLUDES})

# The split solve against the whole one; exits 1 if they differ.
add_executable(mm50split splitcheck.cpp ${MM_SOLVER_DIR}/Circuit.cpp ${MM_SOLVER_DIR}/Partition.cpp)
target_include_directories(mm50split PRIVATE ${MM_INCLUDES})

# Runs a WAV (or a built-in plucked riff) through the engine, to listen.
add_executable(mm50render render.cpp ../Source/Engine50.cpp ${MM_SOLVER_DIR}/Circuit.cpp ${MM_SOLVER_DIR}/Partition.cpp)
target_include_directories(mm50render PRIVATE ${MM_INCLUDES})

# The bench: the amp measured against its sheets and its rating; exits 1 on a failed check.
add_executable(mm50test bench.cpp ../Source/Engine50.cpp ${MM_SOLVER_DIR}/Circuit.cpp ${MM_SOLVER_DIR}/Partition.cpp)
target_include_directories(mm50test PRIVATE ${MM_INCLUDES})

# The whole netlist against ngspice (on the PATH); exits 1 if they disagree.
add_executable(mm50spice spicecheck.cpp ${MM_SOLVER_DIR}/Circuit.cpp)
target_include_directories(mm50spice PRIVATE ${MM_INCLUDES})

foreach(t mm50bias mm50split mm50render mm50test mm50spice)
    if(MSVC)
        target_compile_options(${t} PRIVATE /O2 /fp:precise /W4)
    else()
        target_compile_options(${t} PRIVATE -O2 -fno-fast-math -Wall -Wextra)
    endif()
endforeach()
