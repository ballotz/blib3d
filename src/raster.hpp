#pragma once
#include "math.hpp"

namespace blib3d::raster
{

//------------------------------------------------------------------------------

static constexpr uint32_t num_max_vertices{ 12 };
static constexpr uint32_t num_max_attributes{ 9 }; // [x y] z w cr cg cb ca sr sg sb

//------------------------------------------------------------------------------

struct alignas(4) ARGB
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
};

//------------------------------------------------------------------------------

enum
{
    FILL_SHIFT      = 0,
    FILL_BIT_MASK   = 0b00000000111,
    SHADE_SHIFT     = 3,
    SHADE_BIT_MASK  = 0b00000011000,
    BLEND_SHIFT     = 5,
    BLEND_BIT_MASK  = 0b00011100000,
    MIP_SHIFT       = 8,
    MIP_BIT_MASK    = 0b01100000000,
    FILTER_SHIFT    = 10,
    FILTER_BIT_MASK = 0b10000000000,

    FILL_DEPTH      = 0,
    FILL_OUTLINE    = 1,
    FILL_SOLID      = 2,
    FILL_VERTEX     = 3,
    FILL_TEXTURE    = 4,

    SHADE_NONE      = 0 << SHADE_SHIFT,
    SHADE_VERTEX    = 1 << SHADE_SHIFT,
    SHADE_LIGHTMAP  = 2 << SHADE_SHIFT,

    BLEND_NONE      = 0 << BLEND_SHIFT,
    BLEND_MASK      = 1 << BLEND_SHIFT,
    BLEND_ADD       = 2 << BLEND_SHIFT,
    BLEND_MUL       = 3 << BLEND_SHIFT,
    BLEND_ALPHA     = 4 << BLEND_SHIFT,

    MIP_NONE        = 0 << MIP_SHIFT,
    MIP_FACE        = 1 << MIP_SHIFT,
    MIP_PIXEL       = 2 << MIP_SHIFT,

    FILTER_NONE     = 0 << FILTER_SHIFT,
    FILTER_LINEAR   = 1 << FILTER_SHIFT,
};

struct config
{
    uint32_t flags;

    uint32_t num_faces;
    uint32_t* vertex_count_data;
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
    const ARGB* texture_lut;
    const uint8_t* texture_data;

    int32_t lightmap_width;
    int32_t lightmap_height;
    const ARGB* lightmap;
};

void scan_faces(const config* c);

//------------------------------------------------------------------------------

struct occlusion_config
{
    int32_t frame_width;
    int32_t frame_height;
    //int32_t frame_stride; // TODO
    float* depth_buffer;
};

struct occlusion_data
{
    uint32_t level_count;
    static constexpr uint32_t level_max_count{ 16 };
    struct level
    {
        float* depth;
        int32_t w;
        int32_t h;
    };
    level levels[level_max_count];
};

void occlusion_build_mipchain(
    occlusion_config& cfg,
    occlusion_data& data);

// return true if occluded
bool occlusion_test_rect(
    occlusion_config& cfg,
    occlusion_data& data,
    float screen_min[2], float screen_max[2], float depth_min);

} // namespace blib3d::raster
