#pragma once
#include "raster.hpp"
#include "timer.hpp"

namespace lib3d::render
{

#if defined(CLIP_Z)
constexpr uint32_t num_clip_planes{ 7 };
#else
constexpr uint32_t num_clip_planes{ 5 };
#endif
using clip_interp_type = float;
constexpr clip_interp_type clip_w_min{ (clip_interp_type)1e-2 };
// this way input faces when clipped will not exceed raster max vertex count
constexpr uint32_t num_max_vertices{ raster::num_max_vertices - num_clip_planes };
constexpr uint32_t num_max_components{ 2 + raster::num_max_attributes };

class renderer
{
public:
    renderer();

    ~renderer();

    //----------------------------------

    void set_frame_data(
        uint32_t width,
        uint32_t height,
        uint32_t stride,
        float* depth,
        raster::ARGB* frame);

    void set_frame_clear_color(raster::ARGB color);

    void set_frame_clear_depth(float depth);

    void set_frame_transform(math::mat4x4 matrix); // 4x4

    //----------------------------------

    // vertex x y z coordinate
    void set_geometry_coord(float* data, uint32_t vertex_stride);

    // vertex r g b a color
    void set_geometry_color(float* data, uint32_t vertex_stride);

    // vertex u v texture coordinate
    void set_geometry_tex_coord(float* data, uint32_t vertex_stride);

    // vertex r g b light color coordinate
    void set_geometry_light_color(float* data, uint32_t vertex_stride);

    // vertex s t lightmap coordinate
    void set_geometry_lmap_coord(float* data, uint32_t vertex_stride);

    // face
    void set_geometry_face(raster::face* data, uint32_t count);

    // face index
    // if nullptr then all faces are rendered
    void set_geometry_face_index(uint32_t* data);

    void set_geometry_transform(math::mat4x4 matrix); // 4x4

    void set_geometry_back_cull(bool back_cull);

    //----------------------------------

    enum
    {
        FILL_DEPTH = 1,
        FILL_SOLID,
        FILL_VERTEX,
        FILL_TEXTURE
    };
    void set_fill_type(uint32_t setting);

    void set_fill_color(raster::ARGB color);

    void set_fill_texture(
        int32_t texture_width,
        int32_t texture_height,
        raster::ARGB texture_lut[256],
        uint8_t* texture_data);

    enum
    {
        SHADE_NONE,
        SHADE_SOLID,
        SHADE_VERTEX,
        SHADE_LIGHTMAP
    };
    void set_shade_type(uint32_t setting);

    void set_shade_color(raster::ARGB color);

    void set_shade_lightmap(
        int32_t lightmap_width,
        int32_t lightmap_height,
        raster::ARGB* lightmap);

    enum
    {
        BLEND_NONE,
        BLEND_MASK,
        BLEND_ADD,
        BLEND_MUL,
        BLEND_ALPHA
    };
    void set_blend_type(uint32_t setting);

    enum
    {
        MIP_NONE,
        MIP_FACE,
        MIP_SUBSPAN
    };
    void set_mip_type(uint32_t setting);

    enum
    {
        FILL_FILTER_NONE,
        FILL_FILTER_DITHER,
        FILL_FILTER_LINEAR
    };
    void set_fill_filter_type(uint32_t setting);

    //----------------------------------

    void render_begin();

    void render_clear_frame();

    void render_clear_depth();

    void render_draw();

    void render_end();

    //----------------------------------

    //timer::profile prof_clear;
    //timer::profile prof_geometry;
    //timer::profile prof_raster;

private:
    uint32_t frame_width{};
    uint32_t frame_height{};
    uint32_t frame_stride{};
    float* frame_depth{};
    raster::ARGB* frame_data{};
    raster::ARGB frame_clear_color{};
    float frame_clear_depth{ FLT_MAX };

    float* geometry_coord_data{};
    uint32_t geometry_coord_stride{};
    float* geometry_color_data{};
    uint32_t geometry_color_stride{};
    float* geometry_tex_coord_data{};
    uint32_t geometry_tex_coord_stride{};
    float* geometry_light_color_data{};
    uint32_t geometry_light_color_stride{};
    float* geometry_lmap_coord_data{};
    uint32_t geometry_lmap_coord_stride{};
    raster::face* geometry_face{};
    uint32_t geometry_face_count{};
    uint32_t* geometry_face_index{};

    math::mat4x4 pre_matrix_t;
    math::mat4x4 post_matrix_t;

    raster::config raster_config;

    float render_buffer[2][raster::num_max_vertices][num_max_components];

    static constexpr uint32_t raster_face_buffer_size{ 32 };
    static constexpr uint32_t raster_geometry_buffer_size{ raster_face_buffer_size * 4 * 8 };
    raster::face raster_face_buffer[raster_face_buffer_size];
    float raster_geometry_buffer[raster_geometry_buffer_size];
    uint32_t raster_face_buffer_index{};
    uint32_t raster_geometry_buffer_index{};
    uint32_t raster_geometry_vertex_index{};
};

} // namespace lib3d::render
