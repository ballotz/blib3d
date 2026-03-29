cmake_minimum_required(VERSION 3.22)
project(demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(demo)
target_compile_features(demo PUBLIC c_std_17 cxx_std_17)
set_target_properties(demo PROPERTIES
    C_EXTENSIONS OFF
    CXX_EXTENSIONS OFF    
)

target_sources(demo PUBLIC
    source/brick.c
    source/draw2d.cpp
    source/rect_model.cpp
    source/text_model.cpp
    source/demo.cpp
    source/triangle.cpp
)

# target_include_directories(demo
#     PRIVATE
# )

# target_link_libraries(demo
#     PRIVATE
# )

# target_compile_definitions(demo
#     PRIVATE
# )

# target_compile_options(demo
#     PRIVATE
# )
