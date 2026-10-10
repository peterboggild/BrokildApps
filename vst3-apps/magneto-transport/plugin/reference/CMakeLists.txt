cmake_minimum_required(VERSION 3.22)
project(MagnetoTransportReference LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The offline reference: Preisach coating + Karlqvist head with explicit bias.
# Numbers from it are quoted, so no fast-math. OpenMP over tape cells.
find_package(OpenMP)
add_executable(mtref mtref.cpp)
if(MSVC)
    target_compile_definitions(mtref PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(mtref PRIVATE /O2 /fp:precise /W4)
else()
    target_compile_options(mtref PRIVATE -O2 -fno-fast-math -Wall -Wextra)
endif()
if(OpenMP_CXX_FOUND)
    target_link_libraries(mtref PRIVATE OpenMP::OpenMP_CXX)
endif()
