cmake_minimum_required(VERSION 3.22)
project(CWTest CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The engine is JUCE-free, so both benches build with a plain compiler.
#   cmake -S test -B test/build && cmake --build test/build --config Release
#   test/build/Release/cwtest.exe wavs     renders WAVs + asserts
#   test/build/Release/cwaudit.exe         aliasing / THD / clicks / headroom

add_executable(cwtest  render_test.cpp ../Source/Core/cw_core.cpp)
add_executable(cwaudit audit.cpp       ../Source/Core/cw_core.cpp)

foreach(t cwtest cwaudit)
    target_include_directories(${t} PRIVATE ../Source/Core)
    # MSVC does not define M_PI without this; the sources were written for the
    # Linux CI compiler. Defining it here keeps the shipped source untouched.
    target_compile_definitions(${t} PRIVATE _USE_MATH_DEFINES)
    # cw::Engine is a stack local in both benches and its delay lines alone are
    # ~512 KB, which overflows the 1 MB default stack on Windows (Linux CI gets 8 MB).
    if(MSVC)
        target_link_options(${t} PRIVATE /STACK:33554432)
    endif()
endforeach()

# scratch probe (safe to delete): does each filter temper actually render differently?
add_executable(cwtemper temperprobe.cpp ../Source/Core/cw_core.cpp)
target_include_directories(cwtemper PRIVATE ../Source/Core)
target_compile_definitions(cwtemper PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_link_options(cwtemper PRIVATE /STACK:33554432)
endif()

# scratch probe (safe to delete): does the CUT knob move the filter?
add_executable(cwcut cutprobe.cpp ../Source/Core/cw_core.cpp)
target_include_directories(cwcut PRIVATE ../Source/Core)
target_compile_definitions(cwcut PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_link_options(cwcut PRIVATE /STACK:33554432)
endif()

# scratch probe (safe to delete): where does LADDER lose its level?
add_executable(cwladder ladderprobe.cpp ../Source/Core/cw_core.cpp)
target_include_directories(cwladder PRIVATE ../Source/Core)
target_compile_definitions(cwladder PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_link_options(cwladder PRIVATE /STACK:33554432)
endif()

# scratch probe (safe to delete): does the resonance land on the cutoff?
add_executable(cwtune tuneprobe.cpp ../Source/Core/cw_core.cpp)
target_include_directories(cwtune PRIVATE ../Source/Core)
target_compile_definitions(cwtune PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_link_options(cwtune PRIVATE /STACK:33554432)
endif()

# scratch probe (safe to delete): does one army's temper duck the other?
add_executable(cwmix mixprobe.cpp ../Source/Core/cw_core.cpp)
target_include_directories(cwmix PRIVATE ../Source/Core)
target_compile_definitions(cwmix PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_link_options(cwmix PRIVATE /STACK:33554432)
endif()

# scratch probe (safe to delete): oscillator aliasing, isolated from the ensemble
add_executable(cwalias aliasprobe.cpp ../Source/Core/cw_core.cpp)
target_include_directories(cwalias PRIVATE ../Source/Core)
target_compile_definitions(cwalias PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_link_options(cwalias PRIVATE /STACK:33554432)
endif()
