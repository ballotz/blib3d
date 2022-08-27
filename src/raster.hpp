#pragma once
#include "math.hpp"

namespace lib3d::raster
{

struct face
{
    uint32_t index; // index of face begin in vertex data array
    uint32_t count; // number of vertices
};

//------------------------------------------------------------------------------

force_inline int32_t real_to_raster(float v)
{
    return math::ceil(v - 0.5f);
}

force_inline float raster_to_real(int32_t v)
{
    return (float)v + 0.5f;
}

//------------------------------------------------------------------------------

static constexpr uint32_t num_max_vertices{ 12 };
static constexpr uint32_t num_max_attributes{ 10 };

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
    FILL_DEPTH   = 0b0001,
    FILL_SOLID   = 0b0010,
    FILL_VERTEX  = 0b0100,
    FILL_TEXTURE = 0b1000,

    SHADE_NONE     = 0b0001 << 4,
    SHADE_SOLID    = 0b0010 << 4,
    SHADE_VERTEX   = 0b0100 << 4,
    SHADE_LIGHTMAP = 0b1000 << 4,

    BLEND_MASK  = 0b0001 << 8,
    BLEND_ADD   = 0b0010 << 8,
    BLEND_MUL   = 0b0100 << 8,
    BLEND_ALPHA = 0b1000 << 8,

    MIP_FACE    = 0b01 << 12,
    MIP_SUBSPAN = 0b10 << 12,

    FILL_FILTER_DITHER = 0b01 << 14,
    FILL_FILTER_LINEAR = 0b10 << 14,
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

    int32_t lightmap_width;
    int32_t lightmap_height;
    ARGB* lightmap;
};

void scan_faces(const config* c);

//------------------------------------------------------------------------------

} // namespace lib3d::raster
