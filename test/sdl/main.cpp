#include <SDL.h>

#include <cstdio>

#include "../../src/timer.hpp"
#include "../../src/render.hpp"
#include "../../src/view.hpp"

extern "C"
{

extern const uint8_t brick_lut[256][4];
extern const uint8_t brick_data[262144];
extern const uint32_t brick_width;
extern const uint32_t brick_height;

};

#include "draw.h"

SDL_Window* window{};
SDL_Renderer* renderer{};
SDL_Texture* frame_texture{};
float* depth_buffer{};

blib3d::render::renderer blib3d_renderer;

int init(int& width, int& height)
{
    // attempt to initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        return 1; // SDL init fail

    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) != 0)
        return 1;
    
    //enum
    //{
    //    ratio_5_4,
    //    ratio_4_3,
    //    ratio_3_2,
    //    ratio_16_10,
    //    ratio_16_9,
    //    ratio_unknown
    //};
    //int ratio{ ratio_unknown };
    //if (mode.h * 5 == mode.w * 4)
    //    ratio = ratio_5_4;
    //else if (mode.h * 4 == mode.w * 3)
    //    ratio = ratio_4_3;
    //else if (mode.h * 3 == mode.w * 2)
    //    ratio = ratio_3_2;
    //else if (mode.h * 16 == mode.w * 10)
    //    ratio = ratio_16_10;
    //else if (mode.h * 16 == mode.w * 9)
    //    ratio = ratio_16_9;

    //if (ratio == ratio_4_3)
    //{
    //    width = 640;
    //    height = 480;
    //}
    //else if (ratio == ratio_3_2)
    //{
    //    width = 720;
    //    height = 480;
    //}
    //else if (ratio == ratio_16_9)
    //{
    //    width = 960;
    //    height = 540;
    //}

    //width = mode.w;
    //height = mode.h;
    width = mode.w / 2;
    height = mode.h / 2;
    //width = mode.w / 4;
    //height = mode.h / 4;

    // init the window
    window = SDL_CreateWindow("blib3d test",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        //SDL_WINDOW_SHOWN
        SDL_WINDOW_FULLSCREEN_DESKTOP
        //SDL_WINDOW_FULLSCREEN
    );
    if (window == 0)
        return 1; // window init fail

    renderer = SDL_CreateRenderer(
        window,
        -1,
        0);
        //SDL_RENDERER_PRESENTVSYNC);
    if (renderer == 0)
        return 1; // renderer init fail

    frame_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height);
    if (frame_texture == 0)
        return 1;

    void* pixels;
    int pitch;
    SDL_LockTexture(frame_texture, 0, &pixels, &pitch);
    SDL_UnlockTexture(frame_texture);
    depth_buffer = new float[width * (pitch / sizeof(uint32_t))];

    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_GetRelativeMouseState(0, 0); // flush first
    
    SDL_ShowCursor(SDL_DISABLE);

    return 0;
}

void deinit()
{
    SDL_ShowCursor(SDL_ENABLE);

    // clean up SDL
    delete[] depth_buffer;
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

//------------------------------------------------------------------------------

void surface_normal(float* vertices, int32_t vert_stride, int32_t vert_count, blib3d::math::vec3 normal)
{
    // (v1 - v0) x (v2 - v0) =
    // v1 x v2 - v1 x v0 - v0 x v2 + v0 x v0 =
    // v0 x v1 + v1 x v2 + v2 x v0

    blib3d::math::vec3 t, norm{};
    int32_t vl{ vert_stride * vert_count };
    for (int32_t v0{ 0 }, v1{ vert_stride }; v0 < vl; v0 += vert_stride, v1 += vert_stride)
    {
        if (v1 >= vl)
            v1 = 0;
        blib3d::math::cross3(t, &vertices[v0], &vertices[v1]);
        blib3d::math::add3(norm, t);
    }
    float k{ blib3d::math::sqrt(blib3d::math::dot3(norm, norm)) };
    if (k > 0.f)
    {
        k = 1.f / k;
        blib3d::math::mul3(normal, norm, k);
    }
}

//------------------------------------------------------------------------------

uint32_t lightmap[16][16];

void draw_test_rect(float light_x, float light_y)
{
    constexpr int num_vertices{ 4 };
    constexpr int num_components{ 18 }; // x y z w r g b a sr sg sb ls lt tu tv nx ny nz

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

    float d{ 20 };
    float s{ 16.f / 9.f };
    alignas(16) float vertices[num_vertices][num_components] =
    {
        { -s * d,  s * d, 3 * d, 1, 255,   0,   0, 255,  64,  64, 64, 0, 0, 0 , 0  },
        {  s * d,  s * d, 3 * d, 1,   0,   0, 255, 255,  64,  64, 64, 1, 0, 15, 0  },
        {  s * d, -s * d, 2 * d, 1,   0, 255, 255,   0, 255, 255,  0, 1, 1, 15, 15 },
        { -s * d, -s * d, 2 * d, 1, 255, 255,   0,   0, 255, 255,  0, 0, 1, 0 , 15 },
    };
    blib3d::math::vec3 normal;
    surface_normal(&vertices[0][0], num_components, 4, normal);
    blib3d::math::copy3(&vertices[0][15], normal);
    blib3d::math::copy3(&vertices[1][15], normal);
    blib3d::math::copy3(&vertices[2][15], normal);
    blib3d::math::copy3(&vertices[3][15], normal);

    blib3d_renderer.set_geometry_coord(&vertices[0][0], num_components);
    blib3d_renderer.set_geometry_norm(&vertices[0][15], num_components);
    blib3d_renderer.set_geometry_color(&vertices[0][4], num_components);
    blib3d_renderer.set_geometry_tex_coord(&vertices[0][11], num_components);
    blib3d_renderer.set_geometry_light_color(&vertices[0][8], num_components);
    blib3d_renderer.set_geometry_lmap_coord(&vertices[0][13], num_components);
    blib3d_renderer.set_fill_texture(brick_width, brick_height, (blib3d::raster::ARGB*)brick_lut, (uint8_t*)brick_data);
    blib3d_renderer.set_shade_lightmap(16, 16, (blib3d::raster::ARGB*)lightmap);

    blib3d::render::face faces[1];
    faces[0].count = num_vertices;
    faces[0].index = 0;
    blib3d_renderer.set_geometry_face(faces, 1);

    blib3d_renderer.set_fill_color({ 128, 128, 128, 64 });

    //blib3d_renderer.set_fill_type(blib3d::render::renderer::FILL_SOLID);
    //blib3d_renderer.set_fill_type(blib3d::render::renderer::FILL_VERTEX);
    blib3d_renderer.set_fill_type(blib3d::render::renderer::FILL_TEXTURE);

    //blib3d_renderer.set_shade_type(blib3d::render::renderer::SHADE_NONE);
    //blib3d_renderer.set_shade_type(blib3d::render::renderer::SHADE_VERTEX);
    //blib3d_renderer.set_shade_type(blib3d::render::renderer::SHADE_LIGHTMAP);
    blib3d_renderer.set_shade_type(blib3d::render::renderer::SHADE_LIGHT);

    blib3d_renderer.set_blend_type(blib3d::render::renderer::BLEND_NONE);
    //blib3d_renderer.set_blend_type(blib3d::render::renderer::BLEND_ADD);
    //blib3d_renderer.set_blend_type(blib3d::render::renderer::BLEND_MUL);
    //blib3d_renderer.set_blend_type(blib3d::render::renderer::BLEND_ALPHA);

    //blib3d_renderer.set_filter_type(blib3d::render::renderer::FILTER_LINEAR);

    blib3d_renderer.set_geometry_back_cull(false);

    blib3d_renderer.render_draw();
}

//------------------------------------------------------------------------------

constexpr int blib3d_model_vert_stride{ 12 };
int blib3d_model_num_vert{};
float blib3d_model_vertices[4096][blib3d_model_vert_stride];
int blib3d_model_num_face{};
blib3d::render::face blib3d_model_faces[1024];

void make_blib3d_model(float step, float depth)
{
    constexpr const char* str{ "blib3d" };
    constexpr uint32_t str_len{ 6 };
    constexpr uint32_t char_w{ 8 };
    constexpr uint32_t char_h{ 16 };
    constexpr uint32_t image_w{ char_w * str_len };
    constexpr uint32_t image_h{ char_h };
    uint32_t image[image_h][image_w];
    draw_rect_type rect;
    rect.data = &image[0][0];
    rect.height = image_h;
    rect.width = image_w;
    rect.stride = image_w;
    draw_fill(&rect, 0);
    draw_string(&rect, 0, 0, str, 1);
    blib3d_model_num_vert = 0;
    blib3d_model_num_face = 0;
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
                    blib3d_model_faces[blib3d_model_num_face].index = blib3d_model_num_vert;
                    blib3d_model_faces[blib3d_model_num_face].count = 4;
                    blib3d_model_num_face++;

                    blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                    blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                    blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                    blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                    blib3d_model_num_vert++;
                    blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                    blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                    blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                    blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                    blib3d_model_num_vert++;
                    blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                    blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                    blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                    blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                    blib3d_model_num_vert++;
                    blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                    blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                    blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                    blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                    blib3d_model_num_vert++;

                    blib3d_model_faces[blib3d_model_num_face].index = blib3d_model_num_vert;
                    blib3d_model_faces[blib3d_model_num_face].count = 4;
                    blib3d_model_num_face++;

                    blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                    blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                    blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                    blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                    blib3d_model_num_vert++;
                    blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                    blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                    blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                    blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                    blib3d_model_num_vert++;
                    blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                    blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                    blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                    blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                    blib3d_model_num_vert++;
                    blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                    blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                    blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                    blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                    blib3d_model_num_vert++;

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
                        blib3d_model_faces[blib3d_model_num_face].index = blib3d_model_num_vert;
                        blib3d_model_faces[blib3d_model_num_face].count = 4;
                        blib3d_model_num_face++;

                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                    }

                    if (enclose_right)
                    {
                        blib3d_model_faces[blib3d_model_num_face].index = blib3d_model_num_vert;
                        blib3d_model_faces[blib3d_model_num_face].count = 4;
                        blib3d_model_num_face++;

                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                    }

                    if (enclose_top)
                    {
                        blib3d_model_faces[blib3d_model_num_face].index = blib3d_model_num_vert;
                        blib3d_model_faces[blib3d_model_num_face].count = 4;
                        blib3d_model_num_face++;

                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * y);
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                    }

                    if (enclose_bottom)
                    {
                        blib3d_model_faces[blib3d_model_num_face].index = blib3d_model_num_vert;
                        blib3d_model_faces[blib3d_model_num_face].count = 4;
                        blib3d_model_num_face++;

                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * (x + 1);
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                        blib3d_model_vertices[blib3d_model_num_vert][0] = x_offset + step * x;
                        blib3d_model_vertices[blib3d_model_num_vert][1] = -(y_offset + step * (y + 1));
                        blib3d_model_vertices[blib3d_model_num_vert][2] = z_offset + depth;
                        blib3d_model_vertices[blib3d_model_num_vert][3] = 1.f;
                        blib3d_model_num_vert++;
                    }
                }
            }
        }
    }
    for (int n{}; n < blib3d_model_num_face; ++n)
    {
        uint32_t v{ blib3d_model_faces[n].index };
        blib3d_model_vertices[v + 0][7] = 0.f;
        blib3d_model_vertices[v + 0][8] = 1.f / 5.25f;
        blib3d_model_vertices[v + 1][7] = 1.f / 5.25f;
        blib3d_model_vertices[v + 1][8] = 1.f / 5.25f;
        blib3d_model_vertices[v + 2][7] = 1.f / 5.25f;
        blib3d_model_vertices[v + 2][8] = 0.f;
        blib3d_model_vertices[v + 3][7] = 0.f;
        blib3d_model_vertices[v + 3][8] = 0.f;

        blib3d::math::vec3 normal;
        surface_normal(&blib3d_model_vertices[v + 0][0], blib3d_model_vert_stride, 4, normal);
        blib3d::math::copy3(&blib3d_model_vertices[v + 0][9], normal);
        blib3d::math::copy3(&blib3d_model_vertices[v + 1][9], normal);
        blib3d::math::copy3(&blib3d_model_vertices[v + 2][9], normal);
        blib3d::math::copy3(&blib3d_model_vertices[v + 3][9], normal);
    }
}

float blib3d_model_vertices_transformed[4096][blib3d_model_vert_stride];
blib3d::raster::ARGB blib3d_model_lightmap[1024][4];

void draw_blib3d_model(float angle)
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

        for (int nf{}; nf < blib3d_model_num_face; ++nf)
        {
            blib3d::render::face f{ blib3d_model_faces[nf] };
            uint32_t vert_index{ f.index };
            blib3d::math::vec3 v0, v1;
            blib3d::math::vec3 normal;
            blib3d::math::sub3(v0, blib3d_model_vertices[vert_index + 1], blib3d_model_vertices[vert_index + 0]);
            blib3d::math::sub3(v1, blib3d_model_vertices[vert_index + 2], blib3d_model_vertices[vert_index + 0]);
            blib3d::math::cross3(normal, v0, v1);
            blib3d::math::mul3(normal, blib3d::math::invsqrt(blib3d::math::dot3(normal, normal)));
#if 0
            for (uint32_t nv{}; nv < f.count; ++nv)
            {
                float* v{ blib3d_model_vertices[vert_index + nv] };
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
                float* v{ blib3d_model_vertices[vert_index + nv] };
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
                    blib3d_model_lightmap[nf][0].r = (uint8_t)light[0];
                    blib3d_model_lightmap[nf][0].g = (uint8_t)light[1];
                    blib3d_model_lightmap[nf][0].b = (uint8_t)light[2];
                    v[4] = 0.f;
                    v[5] = nf * 2 + 0.f;
                    break;
                case 2:
                    blib3d_model_lightmap[nf][1].r = (uint8_t)light[0];
                    blib3d_model_lightmap[nf][1].g = (uint8_t)light[1];
                    blib3d_model_lightmap[nf][1].b = (uint8_t)light[2];
                    v[4] = 1.f;
                    v[5] = nf * 2 + 0.f;
                    break;
                case 0:
                    blib3d_model_lightmap[nf][2].r = (uint8_t)light[0];
                    blib3d_model_lightmap[nf][2].g = (uint8_t)light[1];
                    blib3d_model_lightmap[nf][2].b = (uint8_t)light[2];
                    v[4] = 0.f;
                    v[5] = nf * 2 + 1.f;
                    break;
                case 1:
                    blib3d_model_lightmap[nf][3].r = (uint8_t)light[0];
                    blib3d_model_lightmap[nf][3].g = (uint8_t)light[1];
                    blib3d_model_lightmap[nf][3].b = (uint8_t)light[2];
                    v[4] = 1.f;
                    v[5] = nf * 2 + 1.f;
                    break;
                }
            }
#endif
        }
    }

    blib3d_renderer.set_geometry_light_color(&blib3d_model_vertices[0][4], blib3d_model_vert_stride);
    blib3d_renderer.set_geometry_lmap_coord(&blib3d_model_vertices[0][4], blib3d_model_vert_stride);

    blib3d_renderer.set_geometry_tex_coord(&blib3d_model_vertices[0][7], blib3d_model_vert_stride);

    {
        blib3d::math::mat4x4 mt;
        blib3d::math::trn4x4(mt, model_mat);
        for (int n{}; n < blib3d_model_num_vert; ++n)
            blib3d::math::mul4x4t_4(blib3d_model_vertices_transformed[n], mt, blib3d_model_vertices[n]);
        for (int n{}; n < blib3d_model_num_vert; ++n)
            blib3d::math::mul3x3_3(&blib3d_model_vertices_transformed[n][9], model_rotation, &blib3d_model_vertices[n][9]);
    }

    blib3d_renderer.set_geometry_coord(&blib3d_model_vertices_transformed[0][0], blib3d_model_vert_stride);
    blib3d_renderer.set_geometry_norm(&blib3d_model_vertices_transformed[0][9], blib3d_model_vert_stride);

    blib3d_renderer.set_geometry_face(blib3d_model_faces, blib3d_model_num_face);
    blib3d_renderer.set_geometry_face_index(nullptr, 0);
    blib3d_renderer.set_geometry_back_cull(true);

    blib3d_renderer.set_fill_color({ 255, 255, 255, 128 });
    blib3d_renderer.set_fill_texture(brick_width, brick_height, (blib3d::raster::ARGB*)brick_lut, (uint8_t*)brick_data);

    blib3d_renderer.set_shade_lightmap(2, 2 * 1024, (blib3d::raster::ARGB*)blib3d_model_lightmap);

    blib3d_renderer.set_fill_type(blib3d::render::renderer::FILL_SOLID);
    //blib3d_renderer.set_fill_type(blib3d::render::renderer::FILL_TEXTURE);

    //blib3d_renderer.set_shade_type(blib3d::render::renderer::SHADE_NONE);
    //blib3d_renderer.set_shade_type(blib3d::render::renderer::SHADE_VERTEX);
    //blib3d_renderer.set_shade_type(blib3d::render::renderer::SHADE_LIGHTMAP);
    blib3d_renderer.set_shade_type(blib3d::render::renderer::SHADE_LIGHT);

    blib3d_renderer.set_blend_type(blib3d::render::renderer::BLEND_NONE);
    //blib3d_renderer.set_blend_type(blib3d::render::renderer::BLEND_ADD);
    //blib3d_renderer.set_blend_type(blib3d::render::renderer::BLEND_MUL);
    //blib3d_renderer.set_blend_type(blib3d::render::renderer::BLEND_ALPHA);

    blib3d_renderer.set_filter_type(blib3d::render::renderer::FILTER_LINEAR);

    blib3d_renderer.render_draw();
}

//------------------------------------------------------------------------------

int SDL_main(int argc, char* argv[])
{
    int screen_width{ 720 };
    int screen_height{ 480 };

    if (init(screen_width, screen_height) != 0)
        return 1;

    blib3d::timer::interval interval;
    float fps{};
    float fps_count{};
    float ms{};

    make_blib3d_model(1.f, 2.f);
    float blib3d_model_angle{ -3.1416f / 2 };
    float blib3d_model_angle_speed{ 3.1416f / 10.f };
    //float blib3d_model_angle{ 0 };
    //float blib3d_model_angle_speed{ 0 };

    float rect_light_angle{ 0.f };
    float rect_light_angle_speed{ 3.1416f / 10.f };

    interval.reset();

    blib3d_renderer.set_frame_clear_color({ 0xC0, 0xC0, 0xC0, 0xFF });
    blib3d_renderer.set_frame_clear_depth(0);

    blib3d::math::mat4x4 mat_proj;
    blib3d::math::mat4x4 mat_view;
    float screen_ratio{ (float)screen_width / (float)screen_height };
    blib3d::view::make_projection_perspective(
        mat_proj,
        screen_ratio,
        blib3d::math::pi / 2.f,
        blib3d::view::PROJECTION_Y);
    blib3d::view::make_viewport(mat_view, (float)screen_width, (float)screen_height);

    blib3d_renderer.set_geometry_transform(mat_proj);
    blib3d_renderer.set_frame_transform(mat_view);

    blib3d::math::vec3 camera_pos{};
    blib3d::math::vec3 camera_ang{};
    blib3d::math::mat3x3 camera_A
    {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    };

    blib3d::raster::light lights[2];
    lights[0].type = blib3d::raster::light::light::type_point;
    lights[0].pos[0] = 0.f;
    lights[0].pos[1] = 32.f;
    lights[0].pos[2] = 0.f;
    lights[0].intensity[0] = 1.0f;
    lights[0].intensity[1] = 0.5f;
    lights[0].intensity[2] = 0.1f;
    lights[0].damp[0] = 0.f;
    lights[0].damp[1] = 0.f;
    lights[0].damp[2] = 1.f / (16.f * 16.f);
    lights[0].radius = 1024.f;
    lights[1].type = blib3d::raster::light::light::type_spot;
    lights[1].pos[0] = 0.f;
    lights[1].pos[1] = 0.f;
    lights[1].pos[2] = 0.f;
    lights[1].dir[0] = 0.f;
    lights[1].dir[1] = 0.f;
    lights[1].dir[2] = 1.f;
    lights[1].intensity[0] = 1.0f;
    lights[1].intensity[1] = 0.5f;
    lights[1].intensity[2] = 0.1f;
    lights[1].damp[0] = 0.f;
    lights[1].damp[1] = 0.5f / (16.f);
    lights[1].damp[2] = 0.5f / (16.f * 16.f);
    lights[1].spotcosth[0] = std::cos(0.25f);
    lights[1].spotcosth[1] = std::cos(0.50f);
    lights[1].radius = 1024.f;
    blib3d_renderer.set_shade_lights(lights, 2);

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
        SDLK_w,
        SDLK_s,
        SDLK_SPACE,
        SDLK_c,
        SDLK_a,
        SDLK_d,
        SDLK_UP,
        SDLK_DOWN,
        SDLK_LEFT,
        SDLK_RIGHT
    };
    int32_t controller_move[2]{};

    bool running{ true };
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
            {
                running = false;
            }
            break;
            case SDL_KEYDOWN:
            {
                for (uint32_t n{}; n < CONTROLLER_COUNT; ++n)
                {
                    if (event.key.keysym.sym == controller_keyboard_map[n])
                    {
                        controller |= 1 << n;
                        break;
                    }
                }
                if (event.key.keysym.sym == SDLK_r)
                {
                    fps_count = 0;
                }
            }
            break;
            case SDL_KEYUP:
            {
                for (uint32_t n{}; n < CONTROLLER_COUNT; ++n)
                {
                    if (event.key.keysym.sym == controller_keyboard_map[n])
                    {
                        controller &= ~(1 << n);
                        break;
                    }
                }
            }
            break;
            }
        }
        SDL_GetRelativeMouseState(&controller_move[0], &controller_move[1]);

        float dt{ interval.get_s() };

        {
            blib3d::math::vec3 camera_pos_speed{};
            blib3d::math::vec3 camera_ang_speed{};

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

            blib3d::math::vec3 camera_r_pos_speed;
            blib3d::math::mul3x3_3(camera_r_pos_speed, camera_A, camera_pos_speed);
            
            blib3d::math::mul3(camera_r_pos_speed, dt * 16.f); // step speed
            blib3d::math::mul3(camera_ang_speed, dt * blib3d::math::pi); // angle speed

            blib3d::math::add3(camera_pos, camera_r_pos_speed);
            blib3d::math::add3(camera_ang, camera_ang_speed);

            float fx{ blib3d::math::pi / (float)screen_width };
            float fy{ blib3d::math::pi / (float)screen_height };
            camera_ang[1] += (float)controller_move[0] * fx;
            camera_ang[0] += (float)controller_move[1] * fy;

            blib3d::math::mat3x3 Ax, Ay;
            blib3d::math::rotation_x(Ax, camera_ang[0]);
            blib3d::math::rotation_y(Ay, camera_ang[1]);
            blib3d::math::mul3x3_3x3(camera_A, Ay, Ax);

            blib3d::math::vec3 h;
            blib3d::math::mul3x3t_3(h, camera_A, camera_pos);
            blib3d::math::mat4x4 mat_rt
            {
                camera_A[0], camera_A[3], camera_A[6], -h[0],
                camera_A[1], camera_A[4], camera_A[7], -h[1],
                camera_A[2], camera_A[5], camera_A[8], -h[2],
                          0,           0,           0,     1,
            };

            blib3d::math::mat4x4 mat_pre;
            blib3d::math::mul4x4_4x4(mat_pre, mat_proj, mat_rt);

            blib3d_renderer.set_geometry_transform(mat_pre);
        }

        {
            void* pixels;
            int pitch;
            SDL_LockTexture(frame_texture, 0, &pixels, &pitch);

            {
                draw_rect_type rect;
                rect.data = (uint32_t*)pixels;
                rect.height = screen_height;
                rect.width = screen_width;
                rect.stride = pitch / sizeof(uint32_t);

                blib3d_renderer.set_frame_data(screen_width, screen_height, pitch / sizeof(uint32_t), depth_buffer, (blib3d::raster::ARGB*)pixels);
                blib3d_renderer.render_begin();

                blib3d_renderer.render_clear_frame();
                blib3d_renderer.render_clear_depth();

                draw_blib3d_model(blib3d_model_angle);
                blib3d_model_angle += blib3d_model_angle_speed * dt;

                float rect_light_x{ std::cos(rect_light_angle) * 0.25f + 0.5f };
                float rect_light_y{ std::sin(rect_light_angle) * -0.25f + 0.5f };
                draw_test_rect(rect_light_x, rect_light_y);
                rect_light_angle += rect_light_angle_speed * dt;

                char string[128];
                if (dt != 0)
                {
                    float new_fps{ 1.f / dt };
                    float new_ms{ dt * 1000.f };
                    fps = (fps * fps_count + new_fps) / (fps_count + 1);
                    ms = (ms * fps_count + new_ms) / (fps_count + 1);
                    fps_count++;
                }
                snprintf(string, sizeof(string), "%ix%i\nfps %i\nms %i", screen_width, screen_height, (int)fps, (int)ms);

                draw_string(&rect, 0, 0, string, 0xFF888888);
            }

            SDL_UnlockTexture(frame_texture);

            //SDL_RenderClear(renderer); // clear the renderer to the draw color
            SDL_RenderCopy(renderer, frame_texture, NULL, NULL);
            SDL_RenderPresent(renderer); // draw to the screen
        }

        interval.tick();
    }

    deinit();

    return 0;
}
