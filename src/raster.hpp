#pragma once
#include "math.hpp"

namespace lib3d::raster
{

struct face
{
    uint32_t index; // vertex index
    uint32_t count; // vertex count
};

//------------------------------------------------------------------------------

static constexpr uint32_t num_max_vertices{ 12 };
static constexpr uint32_t num_max_attributes{ 9 }; // [x y] z w cr cg cb ca sr sg sb

//------------------------------------------------------------------------------

struct ARGB
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
};

//------------------------------------------------------------------------------

enum
{
    FILL_SHIFT    = 0,
    FILL_BIT_MASK = 0b000000000111,
    FILL_DEPTH    = 1,
    FILL_SOLID    = 2,
    FILL_VERTEX   = 3,
    FILL_TEXTURE  = 4,

    SHADE_SHIFT    = 3,
    SHADE_BIT_MASK = 0b000000011000,
    SHADE_SOLID    = 1 << SHADE_SHIFT,
    SHADE_VERTEX   = 2 << SHADE_SHIFT,
    SHADE_LIGHTMAP = 3 << SHADE_SHIFT,

    BLEND_SHIFT    = 5,
    BLEND_BIT_MASK = 0b000011100000,
    BLEND_MASK     = 1 << BLEND_SHIFT,
    BLEND_ADD      = 2 << BLEND_SHIFT,
    BLEND_MUL      = 3 << BLEND_SHIFT,
    BLEND_ALPHA    = 4 << BLEND_SHIFT,

    MIP_SHIFT    = 8,
    MIP_BIT_MASK = 0b001100000000,
    MIP_FACE     = 1 << MIP_SHIFT,
    MIP_SUBSPAN  = 2 << MIP_SHIFT,

    FILL_FILTER_SHIFT    = 10,
    FILL_FILTER_BIT_MASK = 0b110000000000,
    FILL_FILTER_DITHER   = 1 << FILL_FILTER_SHIFT,
    FILL_FILTER_LINEAR   = 2 << FILL_FILTER_SHIFT,
};

struct config
{
    uint32_t flags;

    uint32_t num_faces;
    const face* faces;
    const float* vertex_data;
    uint32_t vertex_stride;

    bool back_cull;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;

    ARGB fill_color;
    ARGB shade_color;

    int32_t texture_width;
    int32_t texture_height;
    ARGB* texture_lut;
    uint8_t* texture_data;

    int32_t lightmap_width;
    int32_t lightmap_height;
    ARGB* lightmap;
};

void scan_faces(const config* c);

//------------------------------------------------------------------------------

} // namespace lib3d::raster
