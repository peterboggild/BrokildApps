cmake_minimum_required(VERSION 3.22)
project(GravityWellBench)

#  The engine is plain C++ and knows nothing about JUCE, so the bench builds
#  it directly: what the bench measures is exactly what the plug-in runs.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gwtest
    bench.cpp
    ../Source/Engine.cpp
    ../Source/Presets.cpp)

target_include_directories(gwtest PRIVATE ../Source)

if(MSVC)
    #  _USE_MATH_DEFINES: MSVC has no M_PI.
    #  /fp:precise: this is an instrument, not a game - reciprocal
    #  approximations have no business in a filter's tuning.
    target_compile_definitions(gwtest PRIVATE _USE_MATH_DEFINES)
    target_compile_options(gwtest PRIVATE /fp:precise /O2)
    #  cw::Engine-sized objects live on the stack in benches across this
    #  fleet and overflow Windows' 1 MB default.
    target_link_options(gwtest PRIVATE /STACK:33554432)
else()
    target_compile_options(gwtest PRIVATE -O2 -fno-fast-math)
endif()

# The probe: engine internals, printed rather than asserted. Its own target so
# a failing bench never blocks a look inside.
add_executable(gwprobe
    probe.cpp
    ../Source/Engine.cpp
    ../Source/Presets.cpp)
target_include_directories(gwprobe PRIVATE ../Source)
if(MSVC)
    target_compile_definitions(gwprobe PRIVATE _USE_MATH_DEFINES)
    target_compile_options(gwprobe PRIVATE /fp:precise /O2)
    target_link_options(gwprobe PRIVATE /STACK:33554432)
else()
    target_compile_options(gwprobe PRIVATE -O2 -fno-fast-math)
endif()

# The click A/B: the same patch with one thing changed per file, for Peter
# to listen to.  Four proxy metrics disagreed with his ears; he does not.
add_executable(gwvariants variants.cpp ../Source/Engine.cpp ../Source/Presets.cpp)
target_include_directories(gwvariants PRIVATE ../Source)
if(MSVC)
    target_compile_definitions(gwvariants PRIVATE _USE_MATH_DEFINES)
    target_compile_options(gwvariants PRIVATE /fp:precise /O2)
    target_link_options(gwvariants PRIVATE /STACK:33554432)
endif()
