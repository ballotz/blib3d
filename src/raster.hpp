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
    FILL_SHIFT              = 0,
    FILL_BIT_MASK           = 0b0000000011,
    SHADE_SHIFT             = 2,
    SHADE_BIT_MASK          = 0b0000001100,
    BLEND_SHIFT             = 4,
    BLEND_BIT_MASK          = 0b0001110000,
    MIP_SHIFT               = 7,
    MIP_BIT_MASK            = 0b0110000000,
    FILL_FILTER_SHIFT       = 9,
    FILL_FILTER_BIT_MASK    = 0b1000000000,

    FILL_DEPTH              = 0,
    FILL_SOLID              = 1,
    FILL_VERTEX             = 2,
    FILL_TEXTURE            = 3,

    SHADE_NONE              = 0 << SHADE_SHIFT,
    SHADE_VERTEX            = 1 << SHADE_SHIFT,
    SHADE_LIGHTMAP          = 2 << SHADE_SHIFT,

    BLEND_NONE              = 0 << BLEND_SHIFT,
    BLEND_MASK              = 1 << BLEND_SHIFT,
    BLEND_ADD               = 2 << BLEND_SHIFT,
    BLEND_MUL               = 3 << BLEND_SHIFT,
    BLEND_ALPHA             = 4 << BLEND_SHIFT,

    MIP_NONE                = 0 << MIP_SHIFT,
    MIP_FACE                = 1 << MIP_SHIFT,
    MIP_PIXEL               = 2 << MIP_SHIFT,

    FILL_FILTER_NONE        = 0 << FILL_FILTER_SHIFT,
    FILL_FILTER_LINEAR      = 1 << FILL_FILTER_SHIFT,
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
