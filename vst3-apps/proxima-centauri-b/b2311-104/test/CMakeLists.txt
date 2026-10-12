cmake_minimum_required(VERSION 3.22)
project(AB104Bench CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ab104test bench.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104test PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104test PRIVATE /O2 /fp:precise)
    target_link_options(ab104test PRIVATE /STACK:67108864)
endif()

add_executable(ab104render render.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104render PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104render PRIVATE /O2 /fp:precise)
    target_link_options(ab104render PRIVATE /STACK:67108864)
endif()

# The three site checks include the shared site header. That used to be reached
# with a relative path from the source file, which was right when this tree sat
# at b\ArtefactB2311_104 and wrong the moment it moved. The hop belongs in ONE
# place, here, so the sources can name the header and nothing else.
set(AB104_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")

add_executable(ab104sitetest sitetest.cpp)
target_include_directories(ab104sitetest PRIVATE "${AB104_REPO_ROOT}")
if(MSVC)
    target_compile_definitions(ab104sitetest PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104sitetest PRIVATE /O2)
endif()

# the climate state machine with several findings in ONE process — the DAW
# configuration, which the standalone live check cannot reach
add_executable(ab104siteclimate siteclimate.cpp)
target_include_directories(ab104siteclimate PRIVATE "${AB104_REPO_ROOT}")
if(MSVC)
    target_compile_definitions(ab104siteclimate PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104siteclimate PRIVATE /O2)
    target_link_libraries(ab104siteclimate PRIVATE shell32)
endif()

# reads the shared site block from outside: who is on the bench, right now,
# and what each of them is publishing. Run it while a DAW is open.
add_executable(ab104sitewatch sitewatch.cpp)
target_include_directories(ab104sitewatch PRIVATE "${AB104_REPO_ROOT}")
if(MSVC)
    target_compile_definitions(ab104sitewatch PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104sitewatch PRIVATE /O2)
    target_link_libraries(ab104sitewatch PRIVATE shell32)
endif()

add_executable(ab104probe probe.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104probe PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104probe PRIVATE /O2 /fp:precise)
    target_link_options(ab104probe PRIVATE /STACK:67108864)
endif()

# how far apart the specimens actually sound (Peter, 2026-10-12: "the timbre
# of most presets is almost identical") - prints, asserts nothing
add_executable(ab104timbre timbre.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104timbre PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104timbre PRIVATE /O2 /fp:precise)
    target_link_options(ab104timbre PRIVATE /STACK:67108864)
endif()

# CHARACTER 0 must be the engine as it was before the material, sample for
# sample: renders through test/legacy (frozen from git) and compares
add_executable(ab104identity identity.cpp ../Source/Engine.cpp legacy/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104identity PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104identity PRIVATE /O2 /fp:precise)
    target_link_options(ab104identity PRIVATE /STACK:67108864)
endif()

add_executable(ab104matprobe matprobe.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104matprobe PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104matprobe PRIVATE /O2 /fp:precise)
endif()

add_executable(ab104songprobe songprobe.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104songprobe PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104songprobe PRIVATE /O2 /fp:precise)
endif()

add_executable(ab104tuneprobe tuneprobe.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104tuneprobe PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104tuneprobe PRIVATE /O2 /fp:precise)
endif()

add_executable(ab104aliasprobe aliasprobe.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104aliasprobe PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104aliasprobe PRIVATE /O2 /fp:precise)
endif()

# A/B listening set: one specimen per material family, CHARACTER 0 vs 0.8
add_executable(ab104listen listen.cpp ../Source/Engine.cpp)
if(MSVC)
    target_compile_definitions(ab104listen PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(ab104listen PRIVATE /O2 /fp:precise)
endif()
