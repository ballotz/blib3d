#include "render.hpp"
#include <cassert>

namespace blib3d::render
{

// input expected on buffer 0
// num_out_vertices has the number of output vertices
// returns the number of the buffer with the output face
uint32_t clip_face(
    float face_buffer[2][raster::num_max_vertices][num_max_components],
    uint32_t num_in_vertices,
    uint32_t num_components,
    uint32_t& num_out_vertices)
{
    struct util
    {
        static uint32_t clip_flag(const float* p)
        {
            enum
            {
                INSIDE  = 0b00000000,
                WMIN    = 0b00000001,
                LEFT    = 0b00000010,
                RIGHT   = 0b00000100,
                TOP     = 0b00001000,
                BOTTOM  = 0b00010000,
#if defined(CLIP_Z)
                NEAR    = 0b00100000,
                FAR     = 0b01000000,
#endif
            };

            uint32_t flag{ INSIDE };

            if (p[3] < (float)clip_w_min) // w < w_min
                flag |= WMIN;

            if (p[0] < -p[3]) // x < -w
                flag |= LEFT;
            else if (p[0] > p[3]) // x > w
                flag |= RIGHT;

            if (p[1] < -p[3]) // y < -w
                flag |= TOP;
            else if (p[1] > p[3]) // y > w
                flag |= BOTTOM;

#if defined(CLIP_Z)
            if (p[2] < -p[3]) // z < -w
                flag |= NEAR;
            else if (p[2] > p[3]) // z > w
                flag |= FAR;
#endif

            return flag;
        }
    };

    assert(num_in_vertices <= num_max_vertices);
    assert(num_components <= num_max_components);

    uint32_t out_flag[num_max_vertices];
    uint32_t any_out{ 0 };
    uint32_t all_out{ UINT32_MAX };

    for (uint32_t n{ 0 }; n < num_in_vertices; ++n)
    {
        out_flag[n] = util::clip_flag(face_buffer[0][n]);
        any_out |= out_flag[n];
        all_out &= out_flag[n];
    }

    if (!any_out) // all inside, do nothing
    {
        num_out_vertices = num_in_vertices;
        return 0;
    }

    if (all_out) // all outside, do nothing
    {
        num_out_vertices = 0;
        return 0;
    }

    // some in and some out, clip

    uint32_t clip_vertex_count[2];
    uint32_t clip_flag[2][raster::num_max_vertices];
    uint32_t clip_face_in{ 0 };
    uint32_t clip_face_out{ 1 };

    clip_vertex_count[0] = num_in_vertices;
    for (uint32_t n{ 0 }; n < num_in_vertices; ++n)
        clip_flag[0][n] = ~out_flag[n];

    for (uint32_t nplane{ 0 }; nplane < num_clip_planes; ++nplane)
    {
        uint32_t plane_bit{ 1u << nplane };

        uint32_t plane_in[raster::num_max_vertices];
        uint32_t plane_all_in{ plane_bit };
        for (uint32_t n{ 0 }; n < clip_vertex_count[clip_face_in]; ++n)
        {
            uint32_t v{ clip_flag[clip_face_in][n] & plane_bit };
            plane_in[n] = v;
            plane_all_in &= v;
        }

        if (plane_all_in)
            continue;

        clip_vertex_count[clip_face_out] = 0;

        for (uint32_t nv0{ 0 }; nv0 < clip_vertex_count[clip_face_in]; ++nv0)
        {
            uint32_t nv1{ nv0 + 1 };
            if (nv1 == clip_vertex_count[clip_face_in])
                nv1 = 0;

            float* v0{ face_buffer[clip_face_in][nv0] };
            float* v1{ face_buffer[clip_face_in][nv1] };

            if (plane_in[nv0])
            {
                uint32_t nv{ clip_vertex_count[clip_face_out] };
                float* out{ face_buffer[clip_face_out][nv] };
                for (uint32_t nc{ 0 }; nc < num_components; ++nc)
                    out[nc] = v0[nc];
                clip_flag[clip_face_out][nv] = clip_flag[clip_face_in][nv0];
                clip_vertex_count[clip_face_out]++;
                assert(clip_vertex_count[clip_face_out] <= raster::num_max_vertices);
            }
            if (plane_in[nv0] != plane_in[nv1])
            {
                constexpr clip_interp_type plane_gamma[num_clip_planes]
                {
                    clip_w_min,
                    (clip_interp_type)-1.0,
                    (clip_interp_type)1.0,
                    (clip_interp_type)-1.0,
                    (clip_interp_type)1.0,
#if defined(CLIP_Z)
                    (clip_interp_type)-1.0,
                    (clip_interp_type)1.0
#endif
                };
                constexpr uint32_t plane_component[num_clip_planes]
                {
                    0, // unused
                    0,
                    0,
                    1,
                    1,
#if defined(CLIP_Z)
                    2,
                    2
#endif
                };

                const clip_interp_type w0{ v0[3] };
                const clip_interp_type w1{ v1[3] };
                const clip_interp_type gamma{ plane_gamma[nplane] };
                clip_interp_type alpha;
                if (nplane == 0)
                {
                    alpha = (gamma - w0) / (w1 - w0);
                }
                else
                {
                    const uint32_t nc = plane_component[nplane];
                    const clip_interp_type c0 = v0[nc];
                    const clip_interp_type c1 = v1[nc];
                    alpha = (gamma * w0 - c0) / (c1 - c0 - gamma * (w1 - w0));
                }

                if (alpha > (clip_interp_type)0.0 && alpha < (clip_interp_type)1.0)
                {
                    uint32_t nv{ clip_vertex_count[clip_face_out] };
                    float* out{ face_buffer[clip_face_out][nv] };
                    const clip_interp_type k0{ (clip_interp_type)1.0 - alpha };
                    const clip_interp_type k1{ alpha };
                    for (uint32_t nc{ 0 }; nc < num_components; ++nc)
                        out[nc] = k0 * v0[nc] + k1 * v1[nc];
                    clip_flag[clip_face_out][nv] = ~util::clip_flag(out);
                    clip_vertex_count[clip_face_out]++;
                    assert(clip_vertex_count[clip_face_out] <= raster::num_max_vertices);
                }
            }
        }

        clip_face_in = !clip_face_in;
        clip_face_out = !clip_face_out;

        //// if vertex_count < 3 all inside vertices lies exactly on one clip plane
        //if (clip_vertex_count[clip_face_in] < 3)
        //    break;
    }

    num_out_vertices = clip_vertex_count[clip_face_in];
    return clip_face_in;
}

//------------------------------------------------------------------------------

renderer::renderer()
{
    raster_config.flags = 0;
    raster_config.vertex_count_data = raster_vertex_count_buffer;
    raster_config.vertex_data = raster_geometry_buffer;
    raster_config.back_cull = true;
}

renderer::~renderer()
{
}

//------------------------------------------------------------------------------

void renderer::set_frame_data(
    uint32_t width,
    uint32_t height,
    uint32_t stride,
    float* depth,
    raster::ARGB* frame)
{
    frame_width = width;
    frame_height = height;
    frame_stride = stride;
    frame_depth = depth;
    frame_data = frame;

    raster_config.frame_stride = frame_stride;
    raster_config.depth_buffer = frame_depth;
    raster_config.frame_buffer = frame_data;

    occlusion_config.frame_width = width;
    occlusion_config.frame_height = height;
    occlusion_config.depth_buffer = depth;
}

void renderer::set_frame_clear_color(raster::ARGB color)
{
    frame_clear_color = color;
}

void renderer::set_frame_clear_depth(float depth)
{
    frame_clear_depth = depth;
}

void renderer::set_frame_transform(math::mat4x4 matrix)
{
    math::trn4x4(post_matrix_t, matrix);
}

//------------------------------------------------------------------------------

void renderer::set_geometry_coord(float* data, uint32_t vertex_stride)
{
    geometry_coord_data = data;
    geometry_coord_stride = vertex_stride;
}

void renderer::set_geometry_color(float* data, uint32_t vertex_stride)
{
    geometry_color_data = data;
    geometry_color_stride = vertex_stride;
}

void renderer::set_geometry_tex_coord(float* data, uint32_t vertex_stride)
{
    geometry_tex_coord_data = data;
    geometry_tex_coord_stride = vertex_stride;
}

void renderer::set_geometry_light_color(float* data, uint32_t vertex_stride)
{
    geometry_light_color_data = data;
    geometry_light_color_stride = vertex_stride;
}

void renderer::set_geometry_lmap_coord(float* data, uint32_t vertex_stride)
{
    geometry_lmap_coord_data = data;
    geometry_lmap_coord_stride = vertex_stride;
}

void renderer::set_geometry_face(face* data, uint32_t count)
{
    geometry_face = data;
    geometry_face_count = count;
}

void renderer::set_geometry_face_index(uint32_t* data)
{
    geometry_face_index = data;
}

void renderer::set_geometry_transform(math::mat4x4 matrix)
{
    math::trn4x4(pre_matrix_t, matrix);
}

void renderer::set_geometry_back_cull(bool back_cull)
{
    raster_config.back_cull = back_cull;
}

//------------------------------------------------------------------------------

void renderer::set_fill_type(uint32_t setting)
{
    raster_config.flags &= ~raster::FILL_BIT_MASK;
    raster_config.flags |= (setting << raster::FILL_SHIFT) & raster::FILL_BIT_MASK;
}

void renderer::set_shade_type(uint32_t setting)
{
    raster_config.flags &= ~raster::SHADE_BIT_MASK;
    raster_config.flags |= (setting << raster::SHADE_SHIFT) & raster::SHADE_BIT_MASK;
}

void renderer::set_blend_type(uint32_t setting)
{
    raster_config.flags &= ~raster::BLEND_BIT_MASK;
    raster_config.flags |= (setting << raster::BLEND_SHIFT) & raster::BLEND_BIT_MASK;
}

void renderer::set_mip_type(uint32_t setting)
{
    raster_config.flags &= ~raster::MIP_BIT_MASK;
    raster_config.flags |= (setting << raster::MIP_SHIFT) & raster::MIP_BIT_MASK;
}

void renderer::set_filter_type(uint32_t setting)
{
    raster_config.flags &= ~raster::FILTER_BIT_MASK;
    raster_config.flags |= (setting << raster::FILTER_SHIFT) & raster::FILTER_BIT_MASK;
}

void renderer::set_fill_color(raster::ARGB color)
{
    raster_config.fill_color = color;
}

void renderer::set_fill_texture(
    int32_t texture_width,
    int32_t texture_height,
    const raster::ARGB texture_lut[256],
    const uint8_t* texture_data)
{
    raster_config.texture_width = texture_width;
    raster_config.texture_height = texture_height;
    raster_config.texture_lut = texture_lut;
    raster_config.texture_data = texture_data;
}

void renderer::set_shade_color(raster::ARGB color)
{
    raster_config.shade_color = color;
}

void renderer::set_shade_lightmap(
    int32_t lightmap_width,
    int32_t lightmap_height,
    raster::ARGB* lightmap)
{
    raster_config.lightmap_width = lightmap_width;
    raster_config.lightmap_height = lightmap_height;
    raster_config.lightmap = lightmap;
}

//------------------------------------------------------------------------------

void renderer::render_begin()
{

}

void renderer::render_end()
{

}

void renderer::render_clear_frame()
{
    prof_raster.start();

    raster::ARGB color{ frame_clear_color };
    raster::ARGB* p{ frame_data };
    uint32_t width{ frame_width };
    uint32_t jump{ frame_stride - frame_width };
    for (uint32_t row{ 0 }; row < frame_height; ++row)
    {
        uint32_t n{ width };
        while (n--)
            *p++ = color;
        p += jump;
    }

    prof_raster.stop();
}

void renderer::render_clear_depth()
{
    prof_raster.start();

    float depth{ frame_clear_depth };
    float* p{ frame_depth };
    uint32_t width{ frame_width };
    uint32_t jump{ frame_stride - frame_width };
    for (uint32_t row{ 0 }; row < frame_height; ++row)
    {
        uint32_t n{ width };
        while (n--)
            *p++ = depth;
        p += jump;
    }

    prof_raster.stop();
}

void renderer::render_draw()
{
    prof_geometry.start();

    // config data sources

    struct data_source
    {
        float* data; // counter
        uint32_t count; // sequential elements to read
        uint32_t stride; // stride
    };
    data_source geometry_source[3];
    uint32_t geometry_source_count{ 1 };
    uint32_t attribute_count{ 0 };

    geometry_source[0].data = geometry_coord_data;
    geometry_source[0].count = 3;
    geometry_source[0].stride = geometry_coord_stride;

    if ((raster_config.flags & raster::FILL_BIT_MASK) == raster::FILL_VERTEX)
    {
        assert(geometry_color_data);
        assert(geometry_color_stride >= 4);
        data_source& source{ geometry_source[geometry_source_count++] };
        source.data = geometry_color_data;
        source.count = 4;
        source.stride = geometry_color_stride;
        attribute_count += 4;
    }
    else
    if ((raster_config.flags & raster::FILL_BIT_MASK) == raster::FILL_TEXTURE)
    {
        assert(geometry_tex_coord_data);
        assert(geometry_tex_coord_stride >= 2);
        data_source& source{ geometry_source[geometry_source_count++] };
        source.data = geometry_tex_coord_data;
        source.count = 2;
        source.stride = geometry_tex_coord_stride;
        attribute_count += 2;
    }

    if ((raster_config.flags & raster::SHADE_BIT_MASK) == raster::SHADE_VERTEX)
    {
        assert(geometry_light_color_data);
        assert(geometry_light_color_stride >= 3);
        data_source& source{ geometry_source[geometry_source_count++] };
        source.data = geometry_light_color_data;
        source.count = 3;
        source.stride = geometry_light_color_stride;
        attribute_count += 3;
    }
    else
    if ((raster_config.flags & raster::SHADE_BIT_MASK) == raster::SHADE_LIGHTMAP)
    {
        assert(geometry_lmap_coord_data);
        assert(geometry_lmap_coord_stride >= 2);
        data_source& source{ geometry_source[geometry_source_count++] };
        source.data = geometry_lmap_coord_data;
        source.count = 2;
        source.stride = geometry_lmap_coord_stride;
        attribute_count += 2;
    }

    uint32_t component_count{ 4 + attribute_count };

    // pre transform, clip, post transform

    uint32_t face_count{ geometry_face_count };
    face* faces{ geometry_face };
    uint32_t* face_index{ geometry_face_index };

    raster_config.vertex_stride = component_count;

    raster_face_buffer_index = 0;
    raster_geometry_buffer_index = 0;
    //raster_geometry_vertex_index = 0;

    for (uint32_t nf{ 0 }; nf < face_count; ++nf)
    {
        face face_in{ faces[face_index ? face_index[nf] : nf] };

        // check if there is enough free space in buffers

        uint32_t raster_face_buffer_free{ raster_face_buffer_size - raster_face_buffer_index };
        uint32_t raster_geometry_buffer_free{ raster_geometry_buffer_size - raster_geometry_buffer_index };
        if (raster_face_buffer_free < 1 || raster_geometry_buffer_free < (raster::num_max_vertices * component_count))
        {
            // draw current buffer content and reset buffers

            raster_config.num_faces = raster_face_buffer_index;

            prof_geometry.stop();
            prof_raster.start();
            raster::scan_faces(&raster_config);
            prof_raster.stop();
            prof_geometry.start();

            raster_face_buffer_index = 0;
            raster_geometry_buffer_index = 0;
            //raster_geometry_vertex_index = 0;
        }

        // pre transform face to render buffer

#if 0
        uint32_t nc{ 0 };
        {
            data_source& source{ geometry_source[0] };
            float* in{ &source.data[source.stride * face_in.index] };
            float* out{ &render_buffer[0][0][nc] };
            for (uint32_t nv{ 0 }; nv < face_in.count; ++nv)
            {
                math::mul4x4t_3(out, pre_matrix_t, in);
                in += source.stride;
                out += num_max_components;
            }
            nc += 4;
        }
        for (uint32_t ns{ 1 }; ns < geometry_source_count; ++ns)
        {
            data_source& source{ geometry_source[ns] };
            float* in{ &source.data[source.stride * face_in.index] };
            float* out{ &render_buffer[0][0][nc] };
            for (uint32_t nv{ 0 }; nv < face_in.count; ++nv)
            {
                for (uint32_t n{ 0 }; n < source.count; ++n)
                    out[n] = in[n];
                in += source.stride;
                out += num_max_components;
            }
            nc += source.count;
        }
#else
        float* in[3]
        {
            &geometry_source[0].data[geometry_source[0].stride * face_in.index],
            &geometry_source[1].data[geometry_source[1].stride * face_in.index],
            &geometry_source[2].data[geometry_source[2].stride * face_in.index],
        };
        for (uint32_t nv{ 0 }; nv < face_in.count; ++nv)
        {
            float* out{ render_buffer[0][nv] };
            math::mul4x4t_3(out, pre_matrix_t, in[0]);
            in[0] += geometry_source[0].stride;
            out += 4;
            for (uint32_t ns{ 1 }; ns < geometry_source_count; ++ns)
            {
                for (uint32_t n{ 0 }; n < geometry_source[ns].count; ++n)
                    out[n] = in[ns][n];
                in[ns] += geometry_source[ns].stride;
                out += geometry_source[ns].count;
            }
        }
#endif

        // clip

        uint32_t num_out_vertices;
        uint32_t buffer_index{ clip_face(render_buffer, face_in.count, component_count, num_out_vertices) };
        if (num_out_vertices < 3)
            continue;

        float (&clip_buffer)[raster::num_max_vertices][num_max_components]{ render_buffer[buffer_index] };

        // post transform

        math::vec4 res;
        for (uint32_t nv{ 0 }; nv < num_out_vertices; ++nv)
        {
            float* out{ clip_buffer[nv] };
            math::mul4x4t_4(res, post_matrix_t, out);
            assert(res[3] != 0.f);
            float winv{ 1.f / res[3] };
            out[0] = res[0] * winv;
            out[1] = res[1] * winv;
            out[2] = res[2] * winv;
            out[3] = winv;
            for (uint32_t n{ 4 }; n < component_count; ++n)
                out[n] *= winv;
        }

        // add to raster buffer

        raster_vertex_count_buffer[raster_face_buffer_index++] = num_out_vertices;

        float* out{ &raster_geometry_buffer[raster_geometry_buffer_index] };
        for (uint32_t nv{ 0 }; nv < num_out_vertices; ++nv)
            for (uint32_t nc{ 0 }; nc < component_count; ++nc)
                *out++ = clip_buffer[nv][nc];
        raster_geometry_buffer_index += num_out_vertices * component_count;
    }

    // draw remaining buffer content

    if (raster_face_buffer_index)
    {
        raster_config.num_faces = raster_face_buffer_index;

        prof_geometry.stop();
        prof_raster.start();
        raster::scan_faces(&raster_config);
        prof_raster.stop();
        prof_geometry.start();
    }

    prof_geometry.stop();
}

//------------------------------------------------------------------------------

void renderer::occlusion_build_mipchain()
{
    raster::occlusion_build_mipchain(
        occlusion_config,
        occlusion_data);
}

bool renderer::occlusion_test_rect(float screen_min[2], float screen_max[2], float depth_min)
{
    return raster::occlusion_test_rect(
        occlusion_config,
        occlusion_data,
        screen_min, screen_max, depth_min);
}

//------------------------------------------------------------------------------

const raster::occlusion_data& renderer::debug_get_occlusion_data()
{
    return occlusion_data;
}

} // namespace blib3d::render
