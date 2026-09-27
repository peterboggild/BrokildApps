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
