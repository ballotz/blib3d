#include <SDL.h>

#include <cstdio>

#include "../src/timer.hpp"
#include "../src/raster.hpp"

SDL_Window* window{};
SDL_Renderer* renderer{};
SDL_Texture* frame_texture{};
float* depth_buffer{};

int init(int width, int height)
{
    // attempt to initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        return 1; // SDL init fail

    // init the window
    window = SDL_CreateWindow("lib3d test",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN
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

    return 0;
}

void deinit()
{
    // clean up SDL
    delete[] depth_buffer;
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

//------------------------------------------------------------------------------

struct draw_rect_type
{
    uint32_t* data;
    int32_t height;
    int32_t width;
    int32_t stride;
};

void draw_fill(draw_rect_type* rect, uint32_t color)
{
    uint32_t* prow = rect->data;
    for (int y{}; y < rect->height; ++y)
    {
        uint32_t* p = prow;
        for (int x{}; x < rect->width; ++x)
            *p++ = color;
        prow += rect->stride;
    }
}

inline int32_t draw_clip(int32_t v, int32_t lim)
{
    if (v < 0)
        return 0;
    if (v > lim)
        return lim;
    return v;
}

const uint8_t draw_font_data[1536] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3,
    0xFF, 0xC3, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x83,
    0xFF, 0xFF, 0x87, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x9F, 0xF9, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xE7, 0xFF, 0xFF, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xFF, 0xFF, 0xFF, 0xF3, 0xCF, 0xFF, 0xFF,
    0xE7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xE7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xCF,
    0xFF, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF9,
    0xFF, 0xFF, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x9F, 0xF9, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xFF, 0xF3, 0xE7, 0xCF, 0xFF, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xF1, 0xFF, 0xFF, 0xE7, 0xE7, 0xFF, 0xFF,
    0xF3, 0xFF, 0xFF, 0x9F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xCF,
    0xF9, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF9,
    0xFF, 0xFF, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x9F, 0xF9, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF3, 0xFF, 0xE7, 0xE7, 0xE7, 0xFF, 0x00,
    0xFF, 0xE7, 0xFF, 0xC9, 0xC3, 0xE4, 0xC4, 0xFF, 0xE7, 0xE7, 0xFF, 0xFF,
    0xE3, 0xFF, 0xE3, 0x9F, 0xE1, 0xF3, 0x81, 0xC3, 0xF9, 0x87, 0xC3, 0xCF,
    0xC3, 0xC7, 0xE3, 0xE3, 0xF9, 0xFF, 0x9F, 0xE7, 0x80, 0x99, 0x83, 0xC3,
    0x87, 0x81, 0x9F, 0xC1, 0x99, 0xC3, 0xC3, 0x99, 0x81, 0x9C, 0x9C, 0xC3,
    0x9F, 0xC3, 0x99, 0xC3, 0xE7, 0xC3, 0xE7, 0xC9, 0x99, 0xE7, 0x81, 0xCF,
    0xF9, 0xF3, 0xFF, 0xFF, 0xFF, 0xC1, 0x83, 0xC3, 0xC1, 0xC3, 0xCF, 0xC1,
    0x99, 0x81, 0xF3, 0x99, 0x81, 0x9C, 0x99, 0xC3, 0x83, 0xC1, 0x9F, 0x83,
    0xE1, 0xC1, 0xE7, 0xC9, 0x99, 0xC3, 0x81, 0xE7, 0xE7, 0xE7, 0xFF, 0x00,
    0xFF, 0xE7, 0xFF, 0xC9, 0x99, 0xA4, 0x99, 0xFF, 0xCF, 0xF3, 0xFF, 0xFF,
    0xE3, 0xFF, 0xE3, 0xCF, 0xCC, 0xF3, 0x9F, 0x99, 0xF9, 0xF3, 0x99, 0xCF,
    0x99, 0xE7, 0xE3, 0xE3, 0xF3, 0xFF, 0xCF, 0xE7, 0x3F, 0x99, 0x99, 0x99,
    0x93, 0x9F, 0x9F, 0x99, 0x99, 0xE7, 0x99, 0x99, 0x9F, 0x9C, 0x9C, 0x99,
    0x9F, 0x99, 0x99, 0x99, 0xE7, 0x99, 0xC3, 0xC9, 0x99, 0xE7, 0x9F, 0xCF,
    0xF3, 0xF3, 0xFF, 0xFF, 0xFF, 0x99, 0x99, 0x99, 0x99, 0x9F, 0xCF, 0x99,
    0x99, 0xE7, 0xF3, 0x99, 0xE7, 0x94, 0x99, 0x99, 0x99, 0x99, 0x9F, 0xF9,
    0xCF, 0x99, 0xC3, 0xC9, 0x99, 0x99, 0x9F, 0xE7, 0xE7, 0xE7, 0xFF, 0x00,
    0xFF, 0xFF, 0xFF, 0x80, 0xF9, 0x91, 0x99, 0xFF, 0xCF, 0xF3, 0xC9, 0xE7,
    0xFF, 0xFF, 0xFF, 0xCF, 0xC4, 0xF3, 0xCF, 0x99, 0x80, 0xF9, 0x99, 0xCF,
    0x99, 0xF3, 0xFF, 0xFF, 0xE7, 0xFF, 0xE7, 0xFF, 0x30, 0x99, 0x99, 0x99,
    0x99, 0x9F, 0x9F, 0x99, 0x99, 0xE7, 0x99, 0x93, 0x9F, 0x9C, 0x9C, 0x99,
    0x9F, 0x99, 0x99, 0xF9, 0xE7, 0x99, 0x99, 0xC9, 0x99, 0xE7, 0x9F, 0xCF,
    0xF3, 0xF3, 0xFF, 0xFF, 0xFF, 0x99, 0x99, 0x9F, 0x99, 0x9F, 0xCF, 0x99,
    0x99, 0xE7, 0xF3, 0x93, 0xE7, 0x94, 0x99, 0x99, 0x99, 0x99, 0x9F, 0xF9,
    0xCF, 0x99, 0x99, 0x94, 0xC3, 0x99, 0xCF, 0xCF, 0xE7, 0xF3, 0xFF, 0x00,
    0xFF, 0xE7, 0xFF, 0xC9, 0xF3, 0xCF, 0x90, 0xFF, 0xCF, 0xF3, 0xE3, 0xE7,
    0xFF, 0xFF, 0xFF, 0xE7, 0xC4, 0xF3, 0xE7, 0xF9, 0x99, 0xF9, 0x99, 0xE7,
    0x91, 0xC1, 0xFF, 0xFF, 0xCF, 0x81, 0xF3, 0xE7, 0x24, 0x81, 0x99, 0x9F,
    0x99, 0x9F, 0x9F, 0x91, 0x99, 0xE7, 0xF9, 0x93, 0x9F, 0x94, 0x98, 0x99,
    0x9F, 0x99, 0x93, 0xF3, 0xE7, 0x99, 0x99, 0x94, 0xD3, 0xE7, 0xCF, 0xCF,
    0xE7, 0xF3, 0xFF, 0xFF, 0xFF, 0xC1, 0x99, 0x9F, 0x99, 0x81, 0xCF, 0x99,
    0x99, 0xE7, 0xF3, 0x87, 0xE7, 0x94, 0x99, 0x99, 0x99, 0x99, 0x9F, 0xC3,
    0xCF, 0x99, 0x99, 0x94, 0xE7, 0x99, 0xE7, 0x9F, 0xE7, 0xF9, 0xFF, 0x00,
    0xFF, 0xE7, 0xFF, 0xC9, 0xE7, 0xE7, 0x9F, 0xFF, 0xCF, 0xF3, 0x80, 0x81,
    0xFF, 0x81, 0xFF, 0xE7, 0xCC, 0xF3, 0xF3, 0xE3, 0xC9, 0x83, 0x99, 0xE7,
    0xC3, 0x99, 0xFF, 0xFF, 0x9F, 0xFF, 0xF9, 0xE7, 0x24, 0x99, 0x83, 0x9F,
    0x99, 0x83, 0x83, 0x9F, 0x81, 0xE7, 0xF9, 0x87, 0x9F, 0x94, 0x90, 0x99,
    0x83, 0x99, 0x83, 0xE7, 0xE7, 0x99, 0x99, 0x94, 0xE7, 0xC3, 0xE7, 0xCF,
    0xE7, 0xF3, 0xFF, 0xFF, 0xFF, 0xF9, 0x99, 0x9F, 0x99, 0x99, 0x81, 0x99,
    0x99, 0xE7, 0xF3, 0x93, 0xE7, 0x94, 0x99, 0x99, 0x99, 0x99, 0x8F, 0x9F,
    0xCF, 0x99, 0x99, 0x94, 0xC3, 0x99, 0xF3, 0xCF, 0xE7, 0xF3, 0xFF, 0x00,
    0xFF, 0xC3, 0xFF, 0xC9, 0xCF, 0xF3, 0xC7, 0xFF, 0xCF, 0xF3, 0xE3, 0xE7,
    0xFF, 0xFF, 0xFF, 0xF3, 0xC8, 0xF3, 0xF9, 0xF9, 0xC9, 0x9F, 0x83, 0xF3,
    0x89, 0x99, 0xE3, 0xE3, 0xCF, 0x81, 0xF3, 0xF3, 0x30, 0x99, 0x99, 0x9F,
    0x99, 0x9F, 0x9F, 0x9F, 0x99, 0xE7, 0xF9, 0x93, 0x9F, 0x94, 0x84, 0x99,
    0x99, 0x99, 0x99, 0xCF, 0xE7, 0x99, 0x99, 0x94, 0xE7, 0x99, 0xF3, 0xCF,
    0xCF, 0xF3, 0xFF, 0xFF, 0xFF, 0xF9, 0x99, 0x99, 0x99, 0x99, 0xCF, 0x99,
    0x99, 0xE7, 0xF3, 0x99, 0xE7, 0x94, 0x99, 0x99, 0x99, 0x99, 0x91, 0x9F,
    0xCF, 0x99, 0x99, 0x94, 0x99, 0x99, 0xF9, 0xE7, 0xE7, 0xE7, 0xFF, 0x00,
    0xFF, 0xC3, 0x99, 0x80, 0x9F, 0x89, 0x93, 0xE7, 0xE7, 0xE7, 0xC9, 0xE7,
    0xFF, 0xFF, 0xFF, 0xF3, 0xC8, 0x83, 0x99, 0x99, 0xC9, 0x9F, 0xCF, 0xF3,
    0x99, 0x99, 0xE3, 0xE3, 0xE7, 0xFF, 0xE7, 0x99, 0x3C, 0x99, 0x99, 0x99,
    0x99, 0x9F, 0x9F, 0x99, 0x99, 0xE7, 0xF9, 0x93, 0x9F, 0x88, 0x8C, 0x99,
    0x99, 0x99, 0x99, 0x9F, 0xE7, 0x99, 0x99, 0x9C, 0xCB, 0x99, 0xF9, 0xCF,
    0xCF, 0xF3, 0xFF, 0xFF, 0xFF, 0xC3, 0x83, 0xC3, 0xC1, 0xC3, 0xCF, 0xC1,
    0x83, 0x87, 0xC3, 0x99, 0xE7, 0x81, 0x83, 0xC3, 0x83, 0xC1, 0x99, 0xC1,
    0x81, 0x99, 0x99, 0x9C, 0x99, 0x99, 0x81, 0xE7, 0xE7, 0xE7, 0x71, 0x00,
    0xFF, 0xC3, 0x99, 0xC9, 0x99, 0x25, 0x93, 0xE7, 0xE7, 0xE7, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xF9, 0xCC, 0xE3, 0x99, 0x99, 0xCF, 0x9F, 0xE7, 0xF9,
    0x99, 0x99, 0xFF, 0xFF, 0xF3, 0xFF, 0xCF, 0x99, 0x3C, 0xC3, 0x99, 0x99,
    0x93, 0x9F, 0x9F, 0x99, 0x99, 0xE7, 0xF9, 0x99, 0x9F, 0x9C, 0x9C, 0x99,
    0x99, 0x99, 0x99, 0x99, 0xE7, 0x99, 0x99, 0x9C, 0x99, 0x99, 0xF9, 0xCF,
    0x9F, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0x9F, 0xFF, 0xF9, 0xFF, 0xCF, 0xFF,
    0x9F, 0xFF, 0xFF, 0x9F, 0xE7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xCF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xE7, 0xE7, 0x24, 0x00,
    0xFF, 0xE7, 0x99, 0xC9, 0xC3, 0x27, 0xC7, 0xE7, 0xF3, 0xCF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xF9, 0xE1, 0xF3, 0xC3, 0xC3, 0xCF, 0x81, 0xE3, 0x81,
    0xC3, 0xC3, 0xFF, 0xFF, 0xF9, 0xFF, 0x9F, 0xC3, 0x81, 0xE7, 0x83, 0xC3,
    0x87, 0x81, 0x81, 0xC3, 0x99, 0xC3, 0xF9, 0x99, 0x9F, 0x9C, 0x9C, 0xC3,
    0x83, 0xC3, 0x83, 0xC3, 0x81, 0x99, 0x99, 0x9C, 0x99, 0x99, 0x81, 0xC3,
    0x9F, 0xC3, 0x99, 0xFF, 0xF3, 0xFF, 0x9F, 0xFF, 0xF9, 0xFF, 0xE1, 0xFF,
    0x9F, 0xE7, 0xF3, 0x9F, 0x87, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xCF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF3, 0xE7, 0xCF, 0x8E, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0x8F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xC3, 0xFF, 0xE7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xE7, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xE7, 0xFF, 0xC7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

void draw_char(draw_rect_type* rect, int32_t pos_x, int32_t pos_y, char c, uint32_t color)
{
    if (c < 32 || c > 126)
        return;
    int32_t x0 = draw_clip(pos_x, rect->width);
    int32_t x1 = draw_clip(pos_x + 8, rect->width);
    int32_t y0 = draw_clip(pos_y, rect->height);
    int32_t y1 = draw_clip(pos_y + 16, rect->height);
    int32_t w = x1 - x0;
    int32_t h = y1 - y0;
    if (w == 0 || h == 0)
        return;
    int32_t offx = x0 - pos_x;
    int32_t offy = y0 - pos_y;
    int32_t data_col = c - 32;
    int32_t data_row = 15 - offy;
    const uint8_t* pbmp = &draw_font_data[data_col + 96 * data_row];
    uint32_t* prow = (uint32_t*)rect->data;
    prow = &prow[x0 + rect->stride * y0];
    while (h--)
    {
        uint8_t bmp = ~*pbmp;
        uint32_t bit = 1 << (7 - offx);
        uint32_t* p = prow;
        int32_t c = w;
        while (c--)
        {
            if (bmp & bit)
                *p = color;
            bit >>= 1;
            ++p;
        }
        pbmp -= 96;
        prow += rect->stride;
    }
}

void draw_string(draw_rect_type* rect, int32_t pos_x, int32_t pos_y, const char* str, uint32_t color)
{
    int32_t pos_x_init = pos_x;
    while (*str)
    {
        if (*str >= 32 && *str <= 126)
        {
            draw_char(rect, pos_x, pos_y, *str, color);
            pos_x += 8;
        }
        else if (*str == '\r')
        {
            pos_x = pos_x_init;
        }
        else if (*str == '\n')
        {
            pos_x = pos_x_init;
            pos_y += 16;
        }
        else
        {
            draw_char(rect, pos_x, pos_y, '_', color);
            pos_x += 8;
        }
        ++str;
    }
}

//------------------------------------------------------------------------------

void draw_test_rect(uint32_t* frame, float* depth, int32_t width, int32_t height, int32_t stride)
{
    constexpr int num_vertices{ 4 };
    constexpr int num_components{ 12 }; // x y z w r g b a sr sg sb x

    uint32_t lightmap[4]{ 0xFFFF0000u, 0xFF0000FFu, 0xFFFFFF00u, 0xFF00FFFFu };

    float d{ 20 };
    alignas(16) float vertices[num_vertices][num_components] =
    {
        { -1 * d,  1 * d, 3 * d, 1, 255,   0,   0, 255,  64,  64, 64 },
        { -1 * d, -1 * d, 2 * d, 1, 255, 255,   0,   0, 255, 255,  0 },
        {  1 * d, -1 * d, 2 * d, 1,   0, 255, 255,   0, 255, 255,  0 },
        {  1 * d,  1 * d, 3 * d, 1,   0,   0, 255, 255,  64,  64, 64 },
    };
#if 0
    vertices[0][4] = 0;
    vertices[0][5] = 0;
    vertices[1][4] = 0;
    vertices[1][5] = 1;
    vertices[2][4] = 1;
    vertices[2][5] = 1;
    vertices[3][4] = 1;
    vertices[3][5] = 0;
#endif
    float ws{ lib3d::math::min(width, height) / (float)height };
    float hs{ lib3d::math::min(width, height) / (float)width };
    for (int n{}; n < num_vertices; ++n)
    {
        float x{ vertices[n][0] * (width / (ws * 2.f)) + vertices[n][2] * (width / 2.f) };
        float y{ vertices[n][1] * (-height / (hs * 2.f)) + vertices[n][2] * (height / 2.f) };
        float z{ -vertices[n][3] };
        float w{ vertices[n][2] };
        float winv{ 1.f / w };
        vertices[n][0] = x * winv;
        vertices[n][1] = y * winv;
        vertices[n][2] = z * winv;
        vertices[n][3] = winv;
        for (int a{ 4 }; a < num_components; ++a)
            vertices[n][a] *= winv;
    }
    lib3d::raster::face faces[1];
    faces[0].count = num_vertices;
    faces[0].index = 0;

    lib3d::raster::config cfg;

    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_NONE;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_ALPHA;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_VERTEX;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_ALPHA;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_LIGHTMAP;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_LIGHTMAP | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_LIGHTMAP | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_LIGHTMAP | lib3d::raster::BLEND_ALPHA;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_NONE;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_ALPHA;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_VERTEX;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_MUL;
    cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_ALPHA;

    cfg.num_faces = 1;
    cfg.faces = faces;
    cfg.vertex_data = &vertices[0][0];
    cfg.vertex_stride = num_components;
    cfg.back_cull = false;

    cfg.frame_stride = stride;
    cfg.depth_buffer = depth;
    cfg.frame_buffer = (lib3d::raster::ARGB*)frame;

    cfg.fill_color = { 128, 128, 128, 64 };

    cfg.lightmap_width = 2;
    cfg.lightmap_height = 2;
    cfg.lightmap = (lib3d::raster::ARGB*)lightmap;

    lib3d::raster::scan_faces(&cfg);
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

constexpr int lib3d_model_vert_stride{ 12 };
int lib3d_model_num_vert{};
float lib3d_model_vertices[4096][lib3d_model_vert_stride];
int lib3d_model_num_face{};
lib3d::raster::face lib3d_model_faces[1024];

void make_lib3d_model(float step, float depth)
{
    constexpr const char* str{ "lib3d" };
    constexpr uint32_t str_len{ 5 };
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
                    lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_vert_stride * lib3d_model_num_vert;
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

                    lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_vert_stride * lib3d_model_num_vert;
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
                        lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_vert_stride * lib3d_model_num_vert;
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
                        lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_vert_stride * lib3d_model_num_vert;
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
                        lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_vert_stride * lib3d_model_num_vert;
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
                        lib3d_model_faces[lib3d_model_num_face].index = lib3d_model_vert_stride * lib3d_model_num_vert;
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
}

float lib3d_model_vertices_transformed[4096][lib3d_model_vert_stride];
lib3d::raster::ARGB lib3d_model_lightmap[1024][4];

void draw_lib3d_model(
    uint32_t* frame_buffer,
    float* depth_buffer,
    int32_t width,
    int32_t height,
    int32_t stride,
    float angle)
{
    lib3d::math::vec3 model_origin{ 0, 0, 25 };
    lib3d::math::vec3 model_angle{ 0, angle, 0 };

    lib3d::math::mat3x3 model_rotation;
    lib3d::math::make_rotation(model_rotation, model_angle);
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
            lib3d::raster::face f{ lib3d_model_faces[nf] };
            uint32_t vert_index{ f.index / lib3d_model_vert_stride };
            lib3d::math::vec3 v0, v1;
            lib3d::math::vec3 normal;
            lib3d::math::sub3(v0, lib3d_model_vertices[vert_index + 1], lib3d_model_vertices[vert_index + 0]);
            lib3d::math::sub3(v1, lib3d_model_vertices[vert_index + 2], lib3d_model_vertices[vert_index + 0]);
            lib3d::math::cross3(normal, v0, v1);
            lib3d::math::mul3(normal, lib3d::math::invsqrt(lib3d::math::dot3(normal, normal)));
#if 0
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
                    v[4] =          0.f;
                    v[5] = nf * 2 + 0.f;
                    break;
                case 2:
                    lib3d_model_lightmap[nf][1].r = (uint8_t)light[0];
                    lib3d_model_lightmap[nf][1].g = (uint8_t)light[1];
                    lib3d_model_lightmap[nf][1].b = (uint8_t)light[2];
                    v[4] =          1.f;
                    v[5] = nf * 2 + 0.f;
                    break;
                case 0:
                    lib3d_model_lightmap[nf][2].r = (uint8_t)light[0];
                    lib3d_model_lightmap[nf][2].g = (uint8_t)light[1];
                    lib3d_model_lightmap[nf][2].b = (uint8_t)light[2];
                    v[4] =          0.f;
                    v[5] = nf * 2 + 1.f;
                    break;
                case 1:
                    lib3d_model_lightmap[nf][3].r = (uint8_t)light[0];
                    lib3d_model_lightmap[nf][3].g = (uint8_t)light[1];
                    lib3d_model_lightmap[nf][3].b = (uint8_t)light[2];
                    v[4] =          1.f;
                    v[5] = nf * 2 + 1.f;
                    break;
                }
            }
        }
#endif
    }

    lib3d::math::vec3 view_origin{ 0, 0, 0 };
    lib3d::math::vec3 view_angle{ 0, 0, 0 };

    lib3d::math::mat3x3 view_rotation;
    lib3d::math::make_rotation(view_rotation, view_angle);
    lib3d::math::mat4x4 view_proj{};
    {
        float wspan = lib3d::math::max(width, height) / (float)height;
        float hspan = lib3d::math::max(width, height) / (float)width;
        view_proj[0] = 1.f / wspan;
        view_proj[5] = 1.f / hspan;
        view_proj[11] = -1.f;
        view_proj[14] = 1.f;
    }
    lib3d::math::mat4x4 view_pre;
    {
        lib3d::math::mat3x3 r;
        lib3d::math::vec3 t;
        lib3d::math::trn3x3(r, view_rotation);
        lib3d::math::mul3x3_3(t, r, view_origin);
        lib3d::math::mat4x4 m =
        {
            r[0], r[1], r[2], -t[0],
            r[3], r[4], r[5], -t[1],
            r[6], r[7], r[8], -t[2],
             0.f,  0.f,  0.f,   1.f
        };
        lib3d::math::mul4x4_4x4(view_pre, view_proj, m);
    }
    lib3d::math::mat4x4 view_post{};
    {
        view_post[0] = width / 2.f;
        view_post[3] = width / 2.f;
        view_post[5] = -height / 2.f;
        view_post[7] = height / 2.f;
        view_post[10] = 1.f;
        view_post[15] = 1.f;
    }

    {
        lib3d::math::mat4x4 view_mat;
        lib3d::math::mat4x4 mat;
        lib3d::math::mul4x4_4x4(view_mat, view_post, view_pre);
        lib3d::math::mul4x4_4x4(mat, view_mat, model_mat);
        lib3d::math::mat4x4 mt;
        lib3d::math::trn4x4(mt, mat);
        for (int n{}; n < lib3d_model_num_vert; ++n)
        {
            lib3d::math::vec4 vert;
            const float* vert_in{ lib3d_model_vertices[n] };
            float* vert_out{ lib3d_model_vertices_transformed[n] };
            lib3d::math::mul4x4t_4(vert, mt, vert_in);
            float winv{ 1.f / vert[3] };
            vert_out[0] = vert[0] * winv;
            vert_out[1] = vert[1] * winv;
            vert_out[2] = vert[2] * winv;
            vert_out[3] = winv;
            for (int a{ 4 }; a < lib3d_model_vert_stride; ++a)
                vert_out[a] = vert_in[a] * winv;
        }
    }

    lib3d::raster::config cfg;

    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_NONE;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_ALPHA;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_VERTEX;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_ALPHA;
    cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_LIGHTMAP;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_LIGHTMAP | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_LIGHTMAP | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_SOLID | lib3d::raster::SHADE_LIGHTMAP | lib3d::raster::BLEND_ALPHA;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_NONE;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_NONE | lib3d::raster::BLEND_ALPHA;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_VERTEX;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_ADD;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_MUL;
    //cfg.flags = lib3d::raster::FILL_VERTEX | lib3d::raster::SHADE_VERTEX | lib3d::raster::BLEND_ALPHA;

    cfg.num_faces = lib3d_model_num_face;
    cfg.faces = lib3d_model_faces;
    cfg.vertex_data = &lib3d_model_vertices_transformed[0][0];
    cfg.vertex_stride = lib3d_model_vert_stride;
    cfg.back_cull = true;

    cfg.frame_stride = stride;
    cfg.depth_buffer = depth_buffer;
    cfg.frame_buffer = (lib3d::raster::ARGB*)frame_buffer;

    cfg.fill_color = { 255, 255, 255, 128 };

    cfg.lightmap_width = 2;
    cfg.lightmap_height = 2 * 1024;
    cfg.lightmap = (lib3d::raster::ARGB*)lib3d_model_lightmap;

    lib3d::raster::scan_faces(&cfg);
}

//------------------------------------------------------------------------------

#define SCREEN_WIDTH  720
#define SCREEN_HEIGHT 480
//#define SCREEN_WIDTH  (2 * 1280)
//#define SCREEN_HEIGHT (2 *  720)

int SDL_main(int argc, char* argv[])
{
    if (init(SCREEN_WIDTH, SCREEN_HEIGHT) != 0)
        return 1;

    lib3d::timer::interval interval;
    float fps{};
    float fps_count{};

    correct_gamma_out_build_table();
    make_lib3d_model(1.f, 2.f);
    float lib3d_model_angle{ -3.1416f / 2 };
    float lib3d_model_angle_speed{ 3.1416f / 10.f };

    interval.init();

    bool running{ true };
    while (running)
    {
        SDL_Event event;
        if (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            default:
                break;
            }
        }

        {
            void* pixels;
            int pitch;
            SDL_LockTexture(frame_texture, 0, &pixels, &pitch);            

            {
                draw_rect_type rect;
                rect.data = (uint32_t*)pixels;
                rect.height = SCREEN_HEIGHT;
                rect.width = SCREEN_WIDTH;
                rect.stride = pitch / sizeof(uint32_t);
                draw_fill(&rect, 0xFFC0C0C0);

                for (int y{}; y < rect.height; ++y)
                    for (int x{}; x < rect.width; ++x)
                        depth_buffer[x + y * rect.stride] = 0.f;

                draw_test_rect((uint32_t*)pixels, depth_buffer, SCREEN_WIDTH, SCREEN_HEIGHT, pitch / sizeof(uint32_t));

                draw_lib3d_model((uint32_t*)pixels, depth_buffer, SCREEN_WIDTH, SCREEN_HEIGHT, pitch / sizeof(uint32_t), lib3d_model_angle);
                lib3d_model_angle += lib3d_model_angle_speed * interval.get_s();

                correct_gamma_out((uint32_t*)pixels, SCREEN_WIDTH, SCREEN_HEIGHT, pitch / sizeof(uint32_t));

                char string[128];
                if (interval.get_s() != 0)
                {
                    float new_fps{ 1.f / interval.get_s() };
                    fps = (fps * fps_count + new_fps) / (fps_count + 1);
                    fps_count++;
                }
                snprintf(string, sizeof(string), "FPS %i", (int)fps);

                draw_string(&rect, 0, 0, string, 0xFFFFFFFF);
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
