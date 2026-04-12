#include "rect_model.hpp"
#include "brick.h"

namespace demo
{

static constexpr int num_vertices{ 4 };
static constexpr int num_components{ 18 }; // x y z w r g b a sr sg sb ls lt tu tv nx ny nz

static float d{ 20 };
static float s{ 16.f / 9.f };
static float vertices[num_vertices][num_components] =
{
    { -s * d,  s * d, 3 * d, 1, 255,   0,   0, 255,  64,  64, 64, 0, 0, 0 , 0  },
    {  s * d,  s * d, 3 * d, 1,   0,   0, 255, 255,  64,  64, 64, 1, 0, 15, 0  },
    {  s * d, -s * d, 2 * d, 1,   0, 255, 255,   0, 255, 255,  0, 1, 1, 15, 15 },
    { -s * d, -s * d, 2 * d, 1, 255, 255,   0,   0, 255, 255,  0, 0, 1, 0 , 15 },
};

static blib3d::render::face faces[1];

static uint32_t lightmap[16][16];

extern void surface_normal(float* vertices, int32_t vert_stride, int32_t vert_count, blib3d::math::vec3 normal);

void rect_model_setup()
{
    faces[0].count = num_vertices;
    faces[0].index = 0;
}

void rect_model_tick(float light_x, float light_y)
{
    int light_w = 16;
    int light_h = 16;
    int light_os = 4;
    int light_shift = 2;
    float light_pos_x = light_x * light_w;
    float light_pos_y = light_y * light_h;
    float light_radius = 12;
    float light_offset = 0.1;
    for (int i = 0; i < light_h; ++i)
        for (int j = 0; j < light_w; ++j)
            lightmap[i][j] = 0xFF000000u;
    for (int i = 0; i < light_h * light_os; ++i)
    {
        for (int j = 0; j < light_w * light_os; ++j)
        {
            float pos_x = (float)j / (float)light_os;
            float pos_y = (float)i / (float)light_os;
            float dist_x = pos_x - light_pos_x;
            float dist_y = pos_y - light_pos_y;
            float light_dist = dist_x * dist_x + dist_y * dist_y;
            float light_val = 1.f - light_dist / light_radius;
            if (light_val < 0.f)
                light_val = 0.f;
            light_val += light_offset;
            if (light_val > 1.f)
                light_val = 1.f;
            uint32_t color = (uint32_t)(light_val * 255.f / (float)(light_os * light_os));
            lightmap[i >> light_shift][j >> light_shift] += color + (color << 8u) + (color << 16u);
        }
    }

    blib3d::math::vec3 normal;
    surface_normal(&vertices[0][0], num_components, 4, normal);
    blib3d::math::copy3(&vertices[0][15], normal);
    blib3d::math::copy3(&vertices[1][15], normal);
    blib3d::math::copy3(&vertices[2][15], normal);
    blib3d::math::copy3(&vertices[3][15], normal);
}

void rect_model_draw(blib3d::render::renderer& renderer)
{
    renderer.set_geometry_coord(&vertices[0][0], num_components);
    renderer.set_geometry_norm(&vertices[0][15], num_components);
    renderer.set_geometry_color(&vertices[0][4], num_components);
    renderer.set_geometry_tex_coord(&vertices[0][11], num_components);
    renderer.set_geometry_light_color(&vertices[0][8], num_components);
    renderer.set_geometry_lmap_coord(&vertices[0][13], num_components);
    renderer.set_fill_texture(brick_width, brick_height, (blib3d::raster::ARGB*)brick_lut, (uint8_t*)brick_data);
    renderer.set_shade_lightmap(16, 16, (blib3d::raster::ARGB*)lightmap);
    renderer.set_geometry_face(faces, 1);
    renderer.set_geometry_back_cull(false);

    renderer.set_fill_color({ 128, 128, 128, 64 });

    //renderer.set_fill_type(blib3d::render::renderer::FILL_SOLID);
    //renderer.set_fill_type(blib3d::render::renderer::FILL_VERTEX);
    renderer.set_fill_type(blib3d::render::renderer::FILL_TEXTURE);

    //renderer.set_shade_type(blib3d::render::renderer::SHADE_NONE);
    //renderer.set_shade_type(blib3d::render::renderer::SHADE_VERTEX);
    renderer.set_shade_type(blib3d::render::renderer::SHADE_LIGHTMAP);
    //renderer.set_shade_type(blib3d::render::renderer::SHADE_LIGHT);

    renderer.set_blend_type(blib3d::render::renderer::BLEND_NONE);
    //renderer.set_blend_type(blib3d::render::renderer::BLEND_ADD);
    //renderer.set_blend_type(blib3d::render::renderer::BLEND_MUL);
    //renderer.set_blend_type(blib3d::render::renderer::BLEND_ALPHA);

    renderer.set_filter_type(blib3d::render::renderer::FILTER_NONE);
    //renderer.set_filter_type(blib3d::render::renderer::FILTER_LINEAR);

    renderer.render_draw();
}

} // namespace demo
