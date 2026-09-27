/* Copyright 2019 Alessio Ballotti <alessioballotti@tiscali.it> */

#pragma once
#include "raster_light.hpp"

namespace blib3d::raster
{

//------------------------------------------------------------------------------

static constexpr uint32_t num_max_vertices{ 12 };

// [x y] z w
// [x y] z w sr sg sb
// [x y] z w su sv
// [x y] z w px py pz nx ny nz
// [x y] z w fr fg fb fa
// [x y] z w fr fg fb fa sr sg sb
// [x y] z w fr fg fb fa su sv
// [x y] z w fr fg fb fa px py pz nx ny nz
// [x y] z w fs ft
// [x y] z w fr fg sr sg sb
// [x y] z w fr fg su sv
// [x y] z w fr fg px py pz nx ny nz

static constexpr uint32_t num_max_attributes{ 12 };

static constexpr int32_t mip_table_max_size{ 16 };

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
    DEPTH_SHIFT         = 0,
    DEPTH_MASK          = 0b00000000011,
    FILL_SHIFT          = 2,
    FILL_MASK           = 0b00000001100,
    SHADE_SHIFT         = 4,
    SHADE_MASK          = 0b00000110000,
    BLEND_SHIFT         = 6,
    BLEND_MASK          = 0b00011000000,
    TEXMASK_SHIFT       = 8,
    TEXMASK_MASK        = 0b00100000000,
    TEXFILTER_SHIFT     = 9,
    TEXFILTER_MASK      = 0b01000000000,
    TEXMIP_SHIFT        = 10,
    TEXMIP_MASK         = 0b10000000000,

    DEPTH_OFF           = 0,
    DEPTH_WRITE         = 1,
    DEPTH_TEST          = 2,
    DEPTH_TEST_WRITE    = 3,

    FILL_NONE           = 0 << FILL_SHIFT,
    FILL_SOLID          = 1 << FILL_SHIFT,
    FILL_VERTEX         = 2 << FILL_SHIFT,
    FILL_TEXTURE        = 3 << FILL_SHIFT,

    SHADE_NONE          = 0 << SHADE_SHIFT,
    SHADE_VERTEX        = 1 << SHADE_SHIFT,
    SHADE_LIGHTMAP      = 2 << SHADE_SHIFT,
    SHADE_LIGHT         = 3 << SHADE_SHIFT,

    BLEND_NONE          = 0 << BLEND_SHIFT,
    BLEND_ADD           = 1 << BLEND_SHIFT,
    BLEND_MUL           = 2 << BLEND_SHIFT,
    BLEND_ALPHA         = 3 << BLEND_SHIFT,

    TEXMASK_OFF         = 0 << TEXMASK_SHIFT,
    TEXMASK_ON          = 1 << TEXMASK_SHIFT,

    TEXFILTER_NONE      = 0 << TEXFILTER_SHIFT,
    TEXFILTER_LINEAR    = 1 << TEXFILTER_SHIFT,

    TEXMIP_NONE         = 0 << TEXMIP_SHIFT,
    TEXMIP_FACE         = 1 << TEXMIP_SHIFT,
};

struct config
{
    uint32_t flags;

    uint32_t num_faces;
    uint32_t* vertex_count_data;
    const float* vertex_data;
    uint32_t vertex_stride;

    bool back_cull;

    int32_t frame_width;
    int32_t frame_height;
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

    uint32_t num_lights;
    const light* light_data;
    const math::powfast_table* light_table;
};

void scan_faces_wireframe(const config* c);

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

//------------------------------------------------------------------------------

} // namespace blib3d::raster
