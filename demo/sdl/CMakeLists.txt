cmake_minimum_required(VERSION 3.22)
project(sdl-demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(../../ blib3d)

add_subdirectory(../ demo)

set(SDL_STATIC ON CACHE BOOL "Build static library" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(../../../SDL SDL2)

add_executable(sdl-demo)

target_sources(sdl-demo PRIVATE
    source/main.cpp
)

target_include_directories(sdl-demo PRIVATE
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(sdl-demo PRIVATE
    blib3d
    demo
    SDL2::SDL2main
    SDL2::SDL2-static
)

target_compile_options(sdl-demo PRIVATE
    # -Wall -Wextra
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Debug>>:-O0>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O2>
)

if(WIN32)
    set_target_properties(sdl-demo PROPERTIES
        WIN32_EXECUTABLE TRUE)
endif()
