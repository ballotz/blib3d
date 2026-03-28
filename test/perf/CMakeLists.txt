cmake_minimum_required(VERSION 3.22)
project(perf)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(../../ blib3d)

add_executable(perf main.cpp)

target_link_libraries(perf PRIVATE
    blib3d
)

target_compile_options(perf PRIVATE
    # -Wall -Wextra
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Debug>>:-O0>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O2>
)

if (WIN32)
    message(STATUS "Configuring for Windows")
elseif (APPLE)
    message(STATUS "Configuring for macOS")
elseif (UNIX)
    message(STATUS "Configuring for Linux")
endif()
