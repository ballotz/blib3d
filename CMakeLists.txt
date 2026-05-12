cmake_minimum_required(VERSION 3.22)
project(blib3d)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(blib3d)
target_compile_features(blib3d PUBLIC c_std_17 cxx_std_17)
set_target_properties(blib3d PROPERTIES
    C_EXTENSIONS OFF
    CXX_EXTENSIONS OFF    
)

target_sources(blib3d PUBLIC
    source/math.cpp
    source/raster.cpp
    source/render.cpp
    source/timer.cpp
    source/view.cpp
)

# target_include_directories(blib3d
#     PRIVATE
# )

# target_link_libraries(blib3d
#     PRIVATE
# )

target_compile_definitions(blib3d
    PUBLIC
    BLIB3D_USE_SIMD
)

# target_compile_options(blib3d
#     PRIVATE
# )
