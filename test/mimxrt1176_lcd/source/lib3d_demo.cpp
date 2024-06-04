#include <cstdio>

#include "../../../src/timer.hpp"
#include "../../../src/render.hpp"
#include "../../../src/view.hpp"

#include "draw2d.h"

extern "C"
{

extern const uint8_t brick_lut[256][4];
extern const uint8_t brick_data[262144];
extern const uint32_t brick_width;
extern const uint32_t brick_height;

};

[[gnu::section(".bss.$SRAM_OC2")]]
uint32_t brick_lut_linear[256];

//#define SCREEN_WIDTH    720
//#define SCREEN_HEIGHT   1280
#define SCREEN_WIDTH    720/2
#define SCREEN_HEIGHT   1280/2
//#define SCREEN_WIDTH    1280/2
//#define SCREEN_HEIGHT   720/2

[[gnu::section(".bss.$BOARD_SDRAM")]]
uint32_t frame_texture[SCREEN_HEIGHT][SCREEN_WIDTH];

[[gnu::section(".bss.$BOARD_SDRAM")]]
float depth_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];

lib3d::render::renderer lib3d_renderer;

//------------------------------------------------------------------------------

uint32_t lightmap[16][16];

void draw_test_rect(float light_x, float light_y)
{
    constexpr int num_vertices{ 4 };
    constexpr int num_components{ 16 }; // x y z w r g b a sr sg sb ls lt

#if 0
    //constexpr uint32_t lightmap[4]{ 0xFF404040u, 0xFF404040u, 0xFFFFFF00u, 0xFFFFFF00u };
    constexpr uint32_t lightmap[16][16]
    {
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF808080u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF808080u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF808080u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF808080u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF808080u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF808080u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF808080u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF808080u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
        { 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u, 0xFF202020u },
    };
#endif
#if 1
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
#endif

    float d{ 20 };
    float s{ 16.f / 9.f };
    alignas(16) float vertices[num_vertices][num_components] =
    {
        { -s * d,  s * d, 3 * d, 1, 255,   0,   0, 255,  64,  64, 64, 0, 0, 0 , 0  },
        {  s * d,  s * d, 3 * d, 1,   0,   0, 255, 255,  64,  64, 64, 1, 0, 15, 0  },
        {  s * d, -s * d, 2 * d, 1,   0, 255, 255,   0, 255, 255,  0, 1, 1, 15, 15 },
        { -s * d, -s * d, 2 * d, 1, 255, 255,   0,   0, 255, 255,  0, 0, 1, 0 , 15 },
    };

    lib3d_renderer.set_geometry_coord(&vertices[0][0], num_components);
    lib3d_renderer.set_geometry_color(&vertices[0][4], num_components);
    lib3d_renderer.set_geometry_tex_coord(&vertices[0][11], num_components);
    lib3d_renderer.set_geometry_light_color(&vertices[0][8], num_components);
    lib3d_renderer.set_geometry_lmap_coord(&vertices[0][13], num_components);
    //lib3d_renderer.set_fill_texture(brick_width, brick_height, (lib3d::raster::ARGB*)brick_lut, (uint8_t*)brick_data);
    lib3d_renderer.set_fill_texture(brick_width, brick_height, (lib3d::raster::ARGB*)brick_lut_linear, (uint8_t*)brick_data);
    //lib3d_renderer.set_shade_lightmap(2, 2, (lib3d::raster::ARGB*)lightmap);
    lib3d_renderer.set_shade_lightmap(16, 16, (lib3d::raster::ARGB*)lightmap);

    lib3d::render::face faces[1];
    faces[0].count = num_vertices;
    faces[0].index = 0;
    lib3d_renderer.set_geometry_face(faces, 1);

    lib3d_renderer.set_fill_color({ 128, 128, 128, 64 });

    //lib3d_renderer.set_fill_type(lib3d::render::renderer::FILL_SOLID);
    //lib3d_renderer.set_fill_type(lib3d::render::renderer::FILL_VERTEX);
    lib3d_renderer.set_fill_type(lib3d::render::renderer::FILL_TEXTURE);

    //lib3d_renderer.set_shade_type(lib3d::render::renderer::SHADE_NONE);
    //lib3d_renderer.set_shade_type(lib3d::render::renderer::SHADE_VERTEX);
    lib3d_renderer.set_shade_type(lib3d::render::renderer::SHADE_LIGHTMAP);

    lib3d_renderer.set_blend_type(lib3d::render::renderer::BLEND_NONE);
    //lib3d_renderer.set_blend_type(lib3d::render::renderer::BLEND_ADD);
    //lib3d_renderer.set_blend_type(lib3d::render::renderer::BLEND_MUL);
    //lib3d_renderer.set_blend_type(lib3d::render::renderer::BLEND_ALPHA);

    lib3d_renderer.set_filter_type(lib3d::render::renderer::FILTER_LINEAR);

    lib3d_renderer.set_geometry_back_cull(false);

    lib3d_renderer.render_draw();
}

alignas(16) uint32_t correct_gamma_in_table[256];
alignas(16) uint32_t correct_gamma_out_table[256];

void correct_gamma_in_build_table()
{
    for (int n{}; n < 256; ++n)
        correct_gamma_in_table[n] = (uint32_t)(std::pow((float)n / 0xFF, 2.f) * 0xFF + 0.5f);
}

void correct_gamma_out_build_table()
{
    for (int n{}; n < 256; ++n)
        correct_gamma_out_table[n] = (uint32_t)(std::pow((float)n / 0xFF, 0.5f) * 0xFF + 0.5f);
}

inline uint32_t correct_gamma_in(uint32_t v)
{
    return
        0xFF000000u +
        (correct_gamma_in_table[(v & 0x00FF0000) >> 16u] << 16u) +
        (correct_gamma_in_table[(v & 0x0000FF00) >> 8u] << 8u) +
        (correct_gamma_in_table[(v & 0x000000FF)]);
}

inline uint32_t correct_gamma_out(uint32_t v)
{
    return
        0xFF000000u +
        (correct_gamma_out_table[(v & 0x00FF0000) >> 16u] << 16u) +
        (correct_gamma_out_table[(v & 0x0000FF00) >>  8u] <<  8u) +
        (correct_gamma_out_table[(v & 0x000000FF)       ]       );
    //uint32_t r{ (v & 0x00FF0000) >> 16u };
    //uint32_t g{ (v & 0x0000FF00) >>  8u };
    //uint32_t b{ (v & 0x000000FF)        };
    //return 0xFF000000u + (((r * r) >> 8u) << 16u) + ((g * g) & 0x0000FF00u) + ((b * b) >> 8u);
}

void correct_gamma_out(uint32_t* frame, int32_t width, int32_t height, int32_t stride)
{
    uint32_t* prow = frame;
    for (int y{}; y < height; ++y)
    {
        uint32_t* p = prow;
        for (int x{}; x < width; ++x)
        {
            *p = correct_gamma_out(*p);
            p++;
        }
        prow += stride;
    }
}

//------------------------------------------------------------------------------

//void filter_frame_2(uint32_t* data, uint32_t width, uint32_t height, uint32_t stride)
//{
//    // horizontal pass
//    for (uint32_t y{}; y < height; ++y)
//    {
//        uint32_t* row{ &data[y * stride] };
//        uint32_t v{ row[0] };
//        for (uint32_t x{}; x < width; ++x)
//        {
//            uint32_t r{ ((v & 0x00FEFEFEu) + (row[x] & 0x00FEFEFEu)) >> 1u };
//            v = row[x];
//            row[x] = r;
//        }
//    }
//    // vertical pass
//    for (uint32_t x{}; x < width; ++x)
//    {
//        uint32_t* col{ &data[x] };
//        uint32_t v{ col[0] };
//        for (uint32_t y{}; y < height; ++y)
//        {
//            uint32_t r{ ((v & 0x00FEFEFEu) + (col[y * stride] & 0x00FEFEFEu)) >> 1u };
//            v = col[y * stride];
//            col[y * stride] = r;
//        }
//    }
//}
//
//void filter_frame_4(uint32_t* data, uint32_t width, uint32_t height, uint32_t stride)
//{
//    // horizontal pass
//    for (uint32_t y{}; y < height; ++y)
//    {
//        uint32_t* row{ &data[y * stride] };
//        uint32_t v[3]{ row[0], row[0], row[0] };
//        for (uint32_t x{}; x < width; ++x)
//        {
//            uint32_t r{ ((v[0] & 0x00FCFCFCu) + (v[1] & 0x00FCFCFCu) + (v[2] & 0x00FCFCFCu) + (row[x] & 0x00FCFCFCu)) >> 2u };
//            v[0] = v[1];
//            v[1] = v[2];
//            v[2] = row[x];
//            row[x] = r;
//        }
//    }
//    // vertical pass
//    for (uint32_t x{}; x < width; ++x)
//    {
//        uint32_t* col{ &data[x] };
//        uint32_t v[3]{ col[0], col[stride], col[stride << 1u] };
//        for (uint32_t y{}; y < height; ++y)
//        {
//            uint32_t r{ ((v[0] & 0x00FCFCFCu) + (v[1] & 0x00FCFCFCu) + (v[2] & 0x00FCFCFCu) + (col[y * stride] & 0x00FCFCFCu)) >> 2u };
//            v[0] = v[1];
//            v[1] = v[2];
//            v[2] = col[y * stride];
//            col[y * stride] = r;
//        }
//    }
//}

//------------------------------------------------------------------------------

constexpr int lib3d_model_vert_stride{ 12 };
int lib3d_model_num_vert{};
[[gnu::section(".bss.$SRAM_OC1")]]
float lib3d_model_vertices[4096][lib3d_model_vert_stride];
int lib3d_model_num_face{};
lib3d::render::face lib3d_model_faces[1024];

void make_lib3d_model(float step, float depth)
{
    constexpr const char* str{ "lib3d" };
    constexpr uint32_t str_len{ 5 };
    constexpr uint32_t char_w{ 8 };
    constexpr uint32_t char_h{ 16 };
    constexpr uint32_t image_w{ char_w * str_len };
    constexpr uint32_t image_h{ char_h };
    uint32_t image[image_h][image_w];
    draw2d_rect_type rect;
    rect.data = &image[0][0];
    rect.height = image_h;
    rect.width = image_w;
    rect.stride = image_w;
    draw2d_fill_32(&rect, 0);
    draw2d_draw_string_32(&rect, 0, 0, str, 1);
    lib3d_model_num_vert = 0;
    lib3d_model_num_face = 0;
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
                    lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_num_vert;
                    lib3d_model_faces[lib3d_model_num_face].count = 4;
                    lib3d_model_num_face++;

                    lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                    lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                    lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                    lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                    lib3d_model_num_vert++;
                    lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                    lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                    lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                    lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                    lib3d_model_num_vert++;
                    lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                    lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                    lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                    lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                    lib3d_model_num_vert++;
                    lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                    lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                    lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                    lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                    lib3d_model_num_vert++;

                    lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_num_vert;
                    lib3d_model_faces[lib3d_model_num_face].count = 4;
                    lib3d_model_num_face++;

                    lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                    lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                    lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                    lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                    lib3d_model_num_vert++;
                    lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                    lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                    lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                    lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                    lib3d_model_num_vert++;
                    lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                    lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                    lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                    lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                    lib3d_model_num_vert++;
                    lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                    lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                    lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                    lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                    lib3d_model_num_vert++;

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
                        lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_num_vert;
                        lib3d_model_faces[lib3d_model_num_face].count = 4;
                        lib3d_model_num_face++;

                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                    }

                    if (enclose_right)
                    {
                        lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_num_vert;
                        lib3d_model_faces[lib3d_model_num_face].count = 4;
                        lib3d_model_num_face++;

                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                    }

                    if (enclose_top)
                    {
                        lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_num_vert;
                        lib3d_model_faces[lib3d_model_num_face].count = 4;
                        lib3d_model_num_face++;

                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * y);
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                    }

                    if (enclose_bottom)
                    {
                        lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_num_vert;
                        lib3d_model_faces[lib3d_model_num_face].count = 4;
                        lib3d_model_num_face++;

                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                        lib3d_model_vertices[lib3d_model_num_vert][0] = x_offset + step * x;
                        lib3d_model_vertices[lib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        lib3d_model_vertices[lib3d_model_num_vert][2] = z_offset + depth;
                        lib3d_model_vertices[lib3d_model_num_vert][3] = 1.f;
                        lib3d_model_num_vert++;
                    }
                }
            }
        }
    }
    for (int n{}; n < lib3d_model_num_face; ++n)
    {
        uint32_t v{ lib3d_model_faces[n].index };
        lib3d_model_vertices[v + 0][7] = 0.f;
        lib3d_model_vertices[v + 0][8] = 1.f / 5.25f;
        lib3d_model_vertices[v + 1][7] = 1.f / 5.25f;
        lib3d_model_vertices[v + 1][8] = 1.f / 5.25f;
        lib3d_model_vertices[v + 2][7] = 1.f / 5.25f;
        lib3d_model_vertices[v + 2][8] = 0.f;
        lib3d_model_vertices[v + 3][7] = 0.f;
        lib3d_model_vertices[v + 3][8] = 0.f;
    }
}

[[gnu::section(".bss.$SRAM_OC1")]]
float lib3d_model_vertices_transformed[4096][lib3d_model_vert_stride];
lib3d::raster::ARGB lib3d_model_lightmap[1024][4];

void draw_lib3d_model(float angle)
{
    lib3d::math::vec3 model_origin{ 0, 0, 25 };

    lib3d::math::mat3x3 model_rotation;
    lib3d::math::rotation_y(model_rotation, angle);
    lib3d::math::mat4x4 model_mat
    {
        model_rotation[0], model_rotation[1], model_rotation[2], model_origin[0],
        model_rotation[3], model_rotation[4], model_rotation[5], model_origin[1],
        model_rotation[6], model_rotation[7], model_rotation[8], model_origin[2],
        0.f, 0.f, 0.f, 1.f
    };

    // light
    {
        lib3d::math::vec4 light_pos{ 0, -8, 4, 1 };
        lib3d::math::vec3 light_intensity{ 255, 183, 76 };

        lib3d::math::vec4 light_pos_t;
        {
            lib3d::math::mat3x3 r;
            lib3d::math::vec3 t;
            lib3d::math::trn3x3(r, model_rotation);
            lib3d::math::mul3x3_3(t, r, model_origin);
            lib3d::math::mat4x4 m =
            {
                r[0], r[1], r[2], -t[0],
                r[3], r[4], r[5], -t[1],
                r[6], r[7], r[8], -t[2],
                 0.f,  0.f,  0.f,   1.f
            };
            lib3d::math::mat4x4 mt;
            lib3d::math::trn4x4(mt, m);
            lib3d::math::mul4x4t_4(light_pos_t, mt, light_pos);
            lib3d::math::mul4(light_pos_t, 1.f / light_pos_t[3]);
        }

        for (int nf{}; nf < lib3d_model_num_face; ++nf)
        {
            lib3d::render::face f{ lib3d_model_faces[nf] };
            uint32_t vert_index{ f.index };
            lib3d::math::vec3 v0, v1;
            lib3d::math::vec3 normal;
            lib3d::math::sub3(v0, lib3d_model_vertices[vert_index + 1], lib3d_model_vertices[vert_index + 0]);
            lib3d::math::sub3(v1, lib3d_model_vertices[vert_index + 2], lib3d_model_vertices[vert_index + 0]);
            lib3d::math::cross3(normal, v0, v1);
            lib3d::math::mul3(normal, lib3d::math::invsqrt(lib3d::math::dot3(normal, normal)));
#if 1
            for (uint32_t nv{}; nv < f.count; ++nv)
            {
                float* v{ lib3d_model_vertices[vert_index + nv] };
                lib3d::math::sub3(v0, light_pos_t, v);
                float dist2{ lib3d::math::dot3(v0, v0) };
                float scale{ lib3d::math::dot3(v0, normal) };
                if (scale < 0)
                    scale = 0;
                scale *= lib3d::math::invsqrt(dist2);
                scale /= dist2;
                lib3d::math::vec3 light;
                lib3d::math::mul3(light, light_intensity, scale);
                light[0] *= 255;
                light[1] *= 255;
                light[2] *= 255;
                lib3d::math::copy3(v + 4, light);
            }
#else
            for (uint32_t nv{}; nv < f.count; ++nv)
            {
                float* v{ lib3d_model_vertices[vert_index + nv] };
                lib3d::math::sub3(v0, light_pos_t, v);
                float dist2{ lib3d::math::dot3(v0, v0) };
                float scale{ lib3d::math::dot3(v0, normal) };
                if (scale < 0)
                    scale = 0;
                scale *= lib3d::math::invsqrt(dist2);
                scale /= dist2;
                lib3d::math::vec3 light;
                lib3d::math::mul3(light, light_intensity, scale);
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
                    lib3d_model_lightmap[nf][0].r = (uint8_t)light[0];
                    lib3d_model_lightmap[nf][0].g = (uint8_t)light[1];
                    lib3d_model_lightmap[nf][0].b = (uint8_t)light[2];
                    v[4] = 0.f;
                    v[5] = nf * 2 + 0.f;
                    break;
                case 2:
                    lib3d_model_lightmap[nf][1].r = (uint8_t)light[0];
                    lib3d_model_lightmap[nf][1].g = (uint8_t)light[1];
                    lib3d_model_lightmap[nf][1].b = (uint8_t)light[2];
                    v[4] = 1.f;
                    v[5] = nf * 2 + 0.f;
                    break;
                case 0:
                    lib3d_model_lightmap[nf][2].r = (uint8_t)light[0];
                    lib3d_model_lightmap[nf][2].g = (uint8_t)light[1];
                    lib3d_model_lightmap[nf][2].b = (uint8_t)light[2];
                    v[4] = 0.f;
                    v[5] = nf * 2 + 1.f;
                    break;
                case 1:
                    lib3d_model_lightmap[nf][3].r = (uint8_t)light[0];
                    lib3d_model_lightmap[nf][3].g = (uint8_t)light[1];
                    lib3d_model_lightmap[nf][3].b = (uint8_t)light[2];
                    v[4] = 1.f;
                    v[5] = nf * 2 + 1.f;
                    break;
                }
            }
#endif
        }
    }

    lib3d_renderer.set_geometry_light_color(&lib3d_model_vertices[0][4], lib3d_model_vert_stride);
    lib3d_renderer.set_geometry_lmap_coord(&lib3d_model_vertices[0][4], lib3d_model_vert_stride);

    lib3d_renderer.set_geometry_tex_coord(&lib3d_model_vertices[0][7], lib3d_model_vert_stride);

    {
        lib3d::math::mat4x4 mt;
        lib3d::math::trn4x4(mt, model_mat);
        for (int n{}; n < lib3d_model_num_vert; ++n)
            lib3d::math::mul4x4t_4(lib3d_model_vertices_transformed[n], mt, lib3d_model_vertices[n]);
    }

    lib3d_renderer.set_geometry_coord(&lib3d_model_vertices_transformed[0][0], lib3d_model_vert_stride);

    lib3d_renderer.set_geometry_face(lib3d_model_faces, lib3d_model_num_face);
    lib3d_renderer.set_geometry_face_index(nullptr);
    lib3d_renderer.set_geometry_back_cull(true);

    lib3d_renderer.set_fill_color({ 255, 255, 255, 128 });
    lib3d_renderer.set_fill_texture(brick_width, brick_height, (lib3d::raster::ARGB*)brick_lut_linear, (uint8_t*)brick_data);

    lib3d_renderer.set_shade_lightmap(2, 2 * 1024, (lib3d::raster::ARGB*)lib3d_model_lightmap);

    lib3d_renderer.set_fill_type(lib3d::render::renderer::FILL_SOLID);
    //lib3d_renderer.set_fill_type(lib3d::render::renderer::FILL_TEXTURE);

    //lib3d_renderer.set_shade_type(lib3d::render::renderer::SHADE_NONE);
    lib3d_renderer.set_shade_type(lib3d::render::renderer::SHADE_VERTEX);
    //lib3d_renderer.set_shade_type(lib3d::render::renderer::SHADE_LIGHTMAP);

    lib3d_renderer.set_blend_type(lib3d::render::renderer::BLEND_NONE);
    //lib3d_renderer.set_blend_type(lib3d::render::renderer::BLEND_ADD);
    //lib3d_renderer.set_blend_type(lib3d::render::renderer::BLEND_MUL);
    //lib3d_renderer.set_blend_type(lib3d::render::renderer::BLEND_ALPHA);

    //lib3d_renderer.set_fill_filter_type(lib3d::render::renderer::FILL_FILTER_LINEAR);

    lib3d_renderer.render_draw();
}

//------------------------------------------------------------------------------

bool get_event(uint32_t& event, uint32_t& info)
{
    return false;
}

void get_mouse(int32_t& dx, int32_t& dy)
{

}

//------------------------------------------------------------------------------

void lib3d_demo()
{
    int screen_width{ SCREEN_WIDTH };
    int screen_height{ SCREEN_HEIGHT };
//    int screen_width{ SCREEN_WIDTH / 2 };
//    int screen_height{ SCREEN_HEIGHT / 2 };

    lib3d::timer::interval interval;
    float fps{};
    float fps_count{};
    float ms{};

    correct_gamma_in_build_table();
    correct_gamma_out_build_table();

    for (int i{}; i < 256; ++i)
        brick_lut_linear[i] = correct_gamma_in(*(uint32_t*)brick_lut[i]);
        //brick_lut_linear[i] = *(uint32_t*)brick_lut[i];

    make_lib3d_model(1.f, 2.f);
    float lib3d_model_angle{ -3.1416f / 2 };
    float lib3d_model_angle_speed{ 3.1416f / 10.f };
    //float lib3d_model_angle{ 0 };
    //float lib3d_model_angle_speed{ 0 };

    float rect_light_angle{ 0.f };
    float rect_light_angle_speed{ 3.1416f / 10.f };

    interval.reset();

    lib3d_renderer.set_frame_clear_color({ 0xC0, 0xC0, 0xC0, 0xFF });
    lib3d_renderer.set_frame_clear_depth(0);

    lib3d::math::mat4x4 mat_proj;
    lib3d::math::mat4x4 mat_view;
    //float screen_ratio{ (float)screen_width / (float)screen_height };
    float screen_ratio{ (float)screen_height / (float)screen_width };
    lib3d::view::make_projection_perspective(
        mat_proj,
        screen_ratio,
        lib3d::math::pi / 2.f,
        lib3d::view::PROJECTION_Y);
    {
        float tmp;
        tmp = mat_proj[4];
        mat_proj[4] = -mat_proj[0];
        mat_proj[0] = tmp;
        tmp = mat_proj[5];
        mat_proj[5] = -mat_proj[1];
        mat_proj[1] = tmp;
        tmp = mat_proj[6];
        mat_proj[6] = -mat_proj[2];
        mat_proj[2] = tmp;
        tmp = mat_proj[7];
        mat_proj[7] = -mat_proj[3];
        mat_proj[3] = tmp;
    }
    lib3d::view::make_viewport(mat_view, (float)screen_width, (float)screen_height);

    lib3d_renderer.set_geometry_transform(mat_proj);
    lib3d_renderer.set_frame_transform(mat_view);

    lib3d::math::vec3 camera_pos{};
    lib3d::math::vec3 camera_ang{};
    lib3d::math::mat3x3 camera_A
    {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    };

    float camera_speed{ -1.f };
    camera_pos[2] = 52;

    enum
    {
        CONTROLLER_FORWARD,
        CONTROLLER_BACKWARD,
        CONTROLLER_UP,
        CONTROLLER_DOWN,
        CONTROLLER_LEFT,
        CONTROLLER_RIGHT,
        CONTROLLER_LOOK_UP,
        CONTROLLER_LOOK_DOWN,
        CONTROLLER_LOOK_LEFT,
        CONTROLLER_LOOK_RIGHT,

        CONTROLLER_COUNT
    };
    uint32_t controller{};
    uint32_t controller_keyboard_map[CONTROLLER_COUNT]
    {
        'w',
        's',
        ' ',
        'c',
        'a',
        'd',
        0xFF, // up
        0xFF, // down
        0xFF, // left
        0xFF // right
    };
    int32_t controller_move[2]{};

    bool running{ true };
    while (running)
    {
        uint32_t event{};
        uint32_t info{};

        while (get_event(event, info))
        {
            switch (event)
            {
            case 0: // quit
            {
                running = false;
            }
            break;
            case 1: // keydown
            {
                for (uint32_t n{}; n < CONTROLLER_COUNT; ++n)
                {
                    if (info == controller_keyboard_map[n])
                    {
                        controller |= 1 << n;
                        break;
                    }
                }
                if (info == 'R')
                {
                    fps_count = 0;
                }
            }
            break;
            case 2: // keyup
            {
                for (uint32_t n{}; n < CONTROLLER_COUNT; ++n)
                {
                    if (info == controller_keyboard_map[n])
                    {
                        controller &= ~(1 << n);
                        break;
                    }
                }
            }
            break;
            }
        }
        get_mouse(controller_move[0], controller_move[1]);

        float dt{ interval.get_s() };

        {
            lib3d::math::vec3 camera_pos_speed{};
            lib3d::math::vec3 camera_ang_speed{};

            if (controller & (1 << CONTROLLER_FORWARD))
                camera_pos_speed[2] += 1;
            if (controller & (1 << CONTROLLER_BACKWARD))
                camera_pos_speed[2] -= 1;
            if (controller & (1 << CONTROLLER_UP))
                camera_pos_speed[1] += 1;
            if (controller & (1 << CONTROLLER_DOWN))
                camera_pos_speed[1] -= 1;
            if (controller & (1 << CONTROLLER_LEFT))
                camera_pos_speed[0] -= 1;
            if (controller & (1 << CONTROLLER_RIGHT))
                camera_pos_speed[0] += 1;

            if (controller & (1 << CONTROLLER_LOOK_UP))
                camera_ang_speed[0] -= 1;
            if (controller & (1 << CONTROLLER_LOOK_DOWN))
                camera_ang_speed[0] += 1;
            if (controller & (1 << CONTROLLER_LOOK_LEFT))
                camera_ang_speed[1] -= 1;
            if (controller & (1 << CONTROLLER_LOOK_RIGHT))
                camera_ang_speed[1] += 1;

            camera_pos_speed[2] += camera_speed * dt;

            lib3d::math::vec3 camera_r_pos_speed;
            lib3d::math::mul3x3_3(camera_r_pos_speed, camera_A, camera_pos_speed);
            
            lib3d::math::mul3(camera_r_pos_speed, dt * 16.f); // step speed
            lib3d::math::mul3(camera_ang_speed, dt * lib3d::math::pi); // angle speed

            lib3d::math::add3(camera_pos, camera_r_pos_speed);
            lib3d::math::add3(camera_ang, camera_ang_speed);

            if (camera_pos[2] > 52)
                camera_speed = -camera_speed;
            if (camera_pos[2] < 0)
                camera_speed = -camera_speed;

            float fx{ lib3d::math::pi / (float)screen_width };
            float fy{ lib3d::math::pi / (float)screen_height };
            camera_ang[1] += (float)controller_move[0] * fx;
            camera_ang[0] += (float)controller_move[1] * fy;

            lib3d::math::mat3x3 Ax, Ay;
            lib3d::math::rotation_x(Ax, camera_ang[0]);
            lib3d::math::rotation_y(Ay, camera_ang[1]);
            lib3d::math::mul3x3_3x3(camera_A, Ay, Ax);

            lib3d::math::vec3 h;
            lib3d::math::mul3x3t_3(h, camera_A, camera_pos);
            lib3d::math::mat4x4 mat_rt
            {
                camera_A[0], camera_A[3], camera_A[6], -h[0],
                camera_A[1], camera_A[4], camera_A[7], -h[1],
                camera_A[2], camera_A[5], camera_A[8], -h[2],
                          0,           0,           0,     1,
            };

            lib3d::math::mat4x4 mat_pre;
            lib3d::math::mul4x4_4x4(mat_pre, mat_proj, mat_rt);

            lib3d_renderer.set_geometry_transform(mat_pre);
        }

        {
            uint32_t* pixels = &frame_texture[0][0];
//            extern uint32_t* APP_GetFrameBuffer();
//            uint32_t* pixels = APP_GetFrameBuffer();

            {
                draw2d_rect_type rect;
                rect.data = pixels;
                rect.height = screen_height;
                rect.width = screen_width;
                rect.stride = SCREEN_WIDTH;

                lib3d_renderer.set_frame_data(screen_width, screen_height, SCREEN_WIDTH, &depth_buffer[0][0], (lib3d::raster::ARGB*)pixels);
                lib3d_renderer.render_begin();

                lib3d_renderer.render_clear_frame();
                lib3d_renderer.render_clear_depth();

                draw_lib3d_model(lib3d_model_angle);
                lib3d_model_angle += lib3d_model_angle_speed * dt;

                float rect_light_x{ std::cos(rect_light_angle) * 0.25f + 0.5f };
                float rect_light_y{ std::sin(rect_light_angle) * -0.25f + 0.5f };
                draw_test_rect(rect_light_x, rect_light_y);
                rect_light_angle += rect_light_angle_speed * dt;

                correct_gamma_out(pixels, screen_width, screen_height, SCREEN_WIDTH);

                char string[128];
                if (dt != 0)
                {
                    float new_fps{ 1.f / dt };
                    float new_ms{ dt * 1000.f };
                    fps = (fps * fps_count + new_fps) / (fps_count + 1);
                    ms = (ms * fps_count + new_ms) / (fps_count + 1);
                    fps_count++;
                    //fps = 0.999f * fps + 0.001f * new_fps;
                    //ms = 0.999f * ms + 0.001f * new_ms;
                }
                snprintf(string, sizeof(string), "%ix%i\nfps %i\nms %i", screen_width, screen_height, (int)fps, (int)ms);

                draw2d_draw_string_32(&rect, 0, 0, string, 0xFF000000);
            }

            extern void APP_MemToScreen(uint32_t* buffer, int width, int height);
            APP_MemToScreen(&frame_texture[0][0], SCREEN_WIDTH, SCREEN_HEIGHT);
//            extern void APP_SetFrameBuffer(uint32_t* buffer);
//            APP_SetFrameBuffer(pixels);
        }

        interval.tick();
    }
}
