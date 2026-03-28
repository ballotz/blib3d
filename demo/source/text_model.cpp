#include "rect_model.hpp"
#include "draw2d.hpp"
#include "brick.h"

namespace demo
{

static constexpr int vert_stride{ 12 };
static int num_vertices{};
static float vertex_data[4096][vert_stride];

static int num_faces{};
static blib3d::render::face faces[1024];

static float vertex_data_transformed[4096][vert_stride];
static blib3d::raster::ARGB lightmap[1024][4];

extern void surface_normal(float* vertices, int32_t vert_stride, int32_t vert_count, blib3d::math::vec3 normal);

void text_model_setup(float step, float depth)
{
    constexpr const char* str{ "blib3d" };
    constexpr uint32_t str_len{ 6 };
    constexpr uint32_t char_w{ 8 };
    constexpr uint32_t char_h{ 16 };
    constexpr uint32_t image_w{ char_w * str_len };
    constexpr uint32_t image_h{ char_h };
    uint32_t image[image_h][image_w];
    draw2d_rect<uint32_t> rect;
    rect.data = &image[0][0];
    rect.height = image_h;
    rect.width = image_w;
    rect.stride = image_w;
    draw2d_fill(&rect, 0u);
    draw2d_draw_string(&rect, 0, 0, str, 1u);
    num_vertices = 0;
    num_faces = 0;
    float x_offset{ -step * image_w / 2 };
    float y_offset{ -step * image_h / 2 };
    float z_offset{ -depth / 2 };
    for (uint32_t n{}; n < str_len; ++n)
    {
        for (uint32_t y{}; y < char_h; ++y)
        {
            for (uint32_t x{ n * char_w }; x < (n + 1) * char_w; ++x)
            {
                if (image[y][x] == 1)
                {
                    faces[num_faces].index = num_vertices;
                    faces[num_faces].count = 4;
                    num_faces++;

                    vertex_data[num_vertices][0] = x_offset + step * x;
                    vertex_data[num_vertices][1] = -(y_offset + step * y);
                    vertex_data[num_vertices][2] = z_offset;
                    vertex_data[num_vertices][3] = 1.f;
                    num_vertices++;
                    vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                    vertex_data[num_vertices][1] = -(y_offset + step * y);
                    vertex_data[num_vertices][2] = z_offset;
                    vertex_data[num_vertices][3] = 1.f;
                    num_vertices++;
                    vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                    vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                    vertex_data[num_vertices][2] = z_offset;
                    vertex_data[num_vertices][3] = 1.f;
                    num_vertices++;
                    vertex_data[num_vertices][0] = x_offset + step * x;
                    vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                    vertex_data[num_vertices][2] = z_offset;
                    vertex_data[num_vertices][3] = 1.f;
                    num_vertices++;

                    faces[num_faces].index = num_vertices;
                    faces[num_faces].count = 4;
                    num_faces++;

                    vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                    vertex_data[num_vertices][1] = -(y_offset + step * y);
                    vertex_data[num_vertices][2] = z_offset + depth;
                    vertex_data[num_vertices][3] = 1.f;
                    num_vertices++;
                    vertex_data[num_vertices][0] = x_offset + step * x;
                    vertex_data[num_vertices][1] = -(y_offset + step * y);
                    vertex_data[num_vertices][2] = z_offset + depth;
                    vertex_data[num_vertices][3] = 1.f;
                    num_vertices++;
                    vertex_data[num_vertices][0] = x_offset + step * x;
                    vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                    vertex_data[num_vertices][2] = z_offset + depth;
                    vertex_data[num_vertices][3] = 1.f;
                    num_vertices++;
                    vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                    vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                    vertex_data[num_vertices][2] = z_offset + depth;
                    vertex_data[num_vertices][3] = 1.f;
                    num_vertices++;

                    bool enclose_left{};
                    bool enclose_right{};
                    bool enclose_top{};
                    bool enclose_bottom{};

                    if (x >= 1)
                    {
                        if (image[y][x - 1] == 0)
                            enclose_left = true;
                    }
                    else
                        enclose_left = true;

                    if (x < char_w - 1)
                    {
                        if (image[y][x + 1] == 0)
                            enclose_right = true;
                    }
                    else
                        enclose_right = true;

                    if (y >= 1)
                    {
                        if (image[y - 1][x] == 0)
                            enclose_top = true;
                    }
                    else
                        enclose_top = true;

                    if (y < char_h - 1)
                    {
                        if (image[y + 1][x] == 0)
                            enclose_bottom = true;
                    }
                    else
                        enclose_bottom = true;

                    if (enclose_left)
                    {
                        faces[num_faces].index = num_vertices;
                        faces[num_faces].count = 4;
                        num_faces++;

                        vertex_data[num_vertices][0] = x_offset + step * x;
                        vertex_data[num_vertices][1] = -(y_offset + step * y);
                        vertex_data[num_vertices][2] = z_offset + depth;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * x;
                        vertex_data[num_vertices][1] = -(y_offset + step * y);
                        vertex_data[num_vertices][2] = z_offset;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * x;
                        vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                        vertex_data[num_vertices][2] = z_offset;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * x;
                        vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                        vertex_data[num_vertices][2] = z_offset + depth;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                    }

                    if (enclose_right)
                    {
                        faces[num_faces].index = num_vertices;
                        faces[num_faces].count = 4;
                        num_faces++;

                        vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                        vertex_data[num_vertices][1] = -(y_offset + step * y);
                        vertex_data[num_vertices][2] = z_offset;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                        vertex_data[num_vertices][1] = -(y_offset + step * y);
                        vertex_data[num_vertices][2] = z_offset + depth;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                        vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                        vertex_data[num_vertices][2] = z_offset + depth;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                        vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                        vertex_data[num_vertices][2] = z_offset;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                    }

                    if (enclose_top)
                    {
                        faces[num_faces].index = num_vertices;
                        faces[num_faces].count = 4;
                        num_faces++;

                        vertex_data[num_vertices][0] = x_offset + step * x;
                        vertex_data[num_vertices][1] = -(y_offset + step * y);
                        vertex_data[num_vertices][2] = z_offset + depth;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                        vertex_data[num_vertices][1] = -(y_offset + step * y);
                        vertex_data[num_vertices][2] = z_offset + depth;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                        vertex_data[num_vertices][1] = -(y_offset + step * y);
                        vertex_data[num_vertices][2] = z_offset;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * x;
                        vertex_data[num_vertices][1] = -(y_offset + step * y);
                        vertex_data[num_vertices][2] = z_offset;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                    }

                    if (enclose_bottom)
                    {
                        faces[num_faces].index = num_vertices;
                        faces[num_faces].count = 4;
                        num_faces++;

                        vertex_data[num_vertices][0] = x_offset + step * x;
                        vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                        vertex_data[num_vertices][2] = z_offset;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                        vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                        vertex_data[num_vertices][2] = z_offset;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * (x + 1);
                        vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                        vertex_data[num_vertices][2] = z_offset + depth;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                        vertex_data[num_vertices][0] = x_offset + step * x;
                        vertex_data[num_vertices][1] = -(y_offset + step * (y + 1));
                        vertex_data[num_vertices][2] = z_offset + depth;
                        vertex_data[num_vertices][3] = 1.f;
                        num_vertices++;
                    }
                }
            }
        }
    }
    for (int n{}; n < num_faces; ++n)
    {
        uint32_t v{ faces[n].index };
        vertex_data[v + 0][7] = 0.f;
        vertex_data[v + 0][8] = 1.f / 5.25f;
        vertex_data[v + 1][7] = 1.f / 5.25f;
        vertex_data[v + 1][8] = 1.f / 5.25f;
        vertex_data[v + 2][7] = 1.f / 5.25f;
        vertex_data[v + 2][8] = 0.f;
        vertex_data[v + 3][7] = 0.f;
        vertex_data[v + 3][8] = 0.f;

        blib3d::math::vec3 normal;
        surface_normal(&vertex_data[v + 0][0], vert_stride, 4, normal);
        blib3d::math::copy3(&vertex_data[v + 0][9], normal);
        blib3d::math::copy3(&vertex_data[v + 1][9], normal);
        blib3d::math::copy3(&vertex_data[v + 2][9], normal);
        blib3d::math::copy3(&vertex_data[v + 3][9], normal);
    }
}

void text_model_tick(float angle)
{
    blib3d::math::vec3 model_origin{ 0, 0, 25 };

    blib3d::math::mat3x3 model_rotation;
    blib3d::math::rotation_y(model_rotation, angle);
    blib3d::math::mat4x4 model_mat
    {
        model_rotation[0], model_rotation[1], model_rotation[2], model_origin[0],
        model_rotation[3], model_rotation[4], model_rotation[5], model_origin[1],
        model_rotation[6], model_rotation[7], model_rotation[8], model_origin[2],
        0.f, 0.f, 0.f, 1.f
    };

    // light
    {
        blib3d::math::vec4 light_pos{ 0, -8, 4, 1 };
        blib3d::math::vec3 light_intensity{ 255, 183, 76 };

        blib3d::math::vec4 light_pos_t;
        {
            blib3d::math::mat3x3 r;
            blib3d::math::vec3 t;
            blib3d::math::trn3x3(r, model_rotation);
            blib3d::math::mul3x3_3(t, r, model_origin);
            blib3d::math::mat4x4 m =
            {
                r[0], r[1], r[2], -t[0],
                r[3], r[4], r[5], -t[1],
                r[6], r[7], r[8], -t[2],
                 0.f,  0.f,  0.f,   1.f
            };
            blib3d::math::mat4x4 mt;
            blib3d::math::trn4x4(mt, m);
            blib3d::math::mul4x4t_4(light_pos_t, mt, light_pos);
            blib3d::math::mul4(light_pos_t, 1.f / light_pos_t[3]);
        }

        for (int nf{}; nf < num_faces; ++nf)
        {
            blib3d::render::face f{ faces[nf] };
            uint32_t vert_index{ f.index };
            blib3d::math::vec3 v0, v1;
            blib3d::math::vec3 normal;
            blib3d::math::sub3(v0, vertex_data[vert_index + 1], vertex_data[vert_index + 0]);
            blib3d::math::sub3(v1, vertex_data[vert_index + 2], vertex_data[vert_index + 0]);
            blib3d::math::cross3(normal, v0, v1);
            blib3d::math::mul3(normal, blib3d::math::invsqrt(blib3d::math::dot3(normal, normal)));
#if 0
            for (uint32_t nv{}; nv < f.count; ++nv)
            {
                float* v{ vertex_data[vert_index + nv] };
                blib3d::math::sub3(v0, light_pos_t, v);
                float dist2{ blib3d::math::dot3(v0, v0) };
                float scale{ blib3d::math::dot3(v0, normal) };
                if (scale < 0)
                    scale = 0;
                scale *= blib3d::math::invsqrt(dist2);
                scale /= dist2;
                blib3d::math::vec3 light;
                blib3d::math::mul3(light, light_intensity, scale);
                light[0] *= 255;
                light[1] *= 255;
                light[2] *= 255;
                blib3d::math::copy3(v + 4, light);
            }
#else
            for (uint32_t nv{}; nv < f.count; ++nv)
            {
                float* v{ vertex_data[vert_index + nv] };
                blib3d::math::sub3(v0, light_pos_t, v);
                float dist2{ blib3d::math::dot3(v0, v0) };
                float scale{ blib3d::math::dot3(v0, normal) };
                if (scale < 0)
                    scale = 0;
                scale *= blib3d::math::invsqrt(dist2);
                scale /= dist2;
                blib3d::math::vec3 light;
                blib3d::math::mul3(light, light_intensity, scale);
                light[0] *= 255;
                light[1] *= 255;
                light[2] *= 255;
                if (light[0] > 255)
                    light[0] = 255;
                if (light[1] > 255)
                    light[1] = 255;
                if (light[2] > 255)
                    light[2] = 255;
                switch (nv)
                {
                case 3:
                    lightmap[nf][0].r = (uint8_t)light[0];
                    lightmap[nf][0].g = (uint8_t)light[1];
                    lightmap[nf][0].b = (uint8_t)light[2];
                    v[4] = 0.f;
                    v[5] = nf * 2 + 0.f;
                    break;
                case 2:
                    lightmap[nf][1].r = (uint8_t)light[0];
                    lightmap[nf][1].g = (uint8_t)light[1];
                    lightmap[nf][1].b = (uint8_t)light[2];
                    v[4] = 1.f;
                    v[5] = nf * 2 + 0.f;
                    break;
                case 0:
                    lightmap[nf][2].r = (uint8_t)light[0];
                    lightmap[nf][2].g = (uint8_t)light[1];
                    lightmap[nf][2].b = (uint8_t)light[2];
                    v[4] = 0.f;
                    v[5] = nf * 2 + 1.f;
                    break;
                case 1:
                    lightmap[nf][3].r = (uint8_t)light[0];
                    lightmap[nf][3].g = (uint8_t)light[1];
                    lightmap[nf][3].b = (uint8_t)light[2];
                    v[4] = 1.f;
                    v[5] = nf * 2 + 1.f;
                    break;
                }
            }
#endif
        }
    }

    {
        blib3d::math::mat4x4 mt;
        blib3d::math::trn4x4(mt, model_mat);
        for (int n{}; n < num_vertices; ++n)
            blib3d::math::mul4x4t_4(vertex_data_transformed[n], mt, vertex_data[n]);
        for (int n{}; n < num_vertices; ++n)
            blib3d::math::mul3x3_3(&vertex_data_transformed[n][9], model_rotation, &vertex_data[n][9]);
    }
}

void text_model_draw(blib3d::render::renderer& renderer)
{
    renderer.set_geometry_light_color(&vertex_data[0][4], vert_stride);
    renderer.set_geometry_lmap_coord(&vertex_data[0][4], vert_stride);

    renderer.set_geometry_tex_coord(&vertex_data[0][7], vert_stride);

    renderer.set_geometry_coord(&vertex_data_transformed[0][0], vert_stride);
    renderer.set_geometry_norm(&vertex_data_transformed[0][9], vert_stride);

    renderer.set_geometry_face(faces, num_faces);
    renderer.set_geometry_face_index(nullptr, 0);
    renderer.set_geometry_back_cull(true);

    renderer.set_fill_color({ 255, 255, 255, 128 });
    renderer.set_fill_texture(brick_width, brick_height, (blib3d::raster::ARGB*)brick_lut, (uint8_t*)brick_data);

    renderer.set_shade_lightmap(2, 2 * 1024, (blib3d::raster::ARGB*)lightmap);

    renderer.set_fill_type(blib3d::render::renderer::FILL_SOLID);
    //renderer.set_fill_type(blib3d::render::renderer::FILL_TEXTURE);

    //renderer.set_shade_type(blib3d::render::renderer::SHADE_NONE);
    //renderer.set_shade_type(blib3d::render::renderer::SHADE_VERTEX);
    renderer.set_shade_type(blib3d::render::renderer::SHADE_LIGHTMAP);
    //renderer.set_shade_type(blib3d::render::renderer::SHADE_LIGHT);

    renderer.set_blend_type(blib3d::render::renderer::BLEND_NONE);
    //renderer.set_blend_type(blib3d::render::renderer::BLEND_ADD);
    //renderer.set_blend_type(blib3d::render::renderer::BLEND_MUL);
    //renderer.set_blend_type(blib3d::render::renderer::BLEND_ALPHA);

    renderer.set_filter_type(blib3d::render::renderer::FILTER_LINEAR);

    renderer.render_draw();
}

} // namespace demo
