#include "../../source/demo.hpp"
#include <SDL.h>

SDL_Window* window{};
SDL_Renderer* renderer{};
SDL_Texture* frame_texture{};
float* depth_buffer{};

int init(int& width, int& height)
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        return 1;

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

    width = mode.w;
    height = mode.h;
    //width = mode.w / 2;
    //height = mode.h / 2;
    //width = mode.w / 4;
    //height = mode.h / 4;

    window = SDL_CreateWindow("blib3d demo",
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
        return 1;

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
    
    //SDL_ShowCursor(SDL_DISABLE);

    return 0;
}

void deinit()
{
    //SDL_ShowCursor(SDL_ENABLE);

    delete[] depth_buffer;
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

//------------------------------------------------------------------------------

int SDL_main(int argc, char* argv[])
{
    int screen_width{ 720 };
    int screen_height{ 480 };

    if (init(screen_width, screen_height) != 0)
        return 1;
    
    demo::setup(screen_width, screen_height, false);

    uint32_t controller{};
    uint32_t controller_keyboard_map[demo::CONTROLLER_COUNT]
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
        SDLK_RIGHT,

        SDLK_r,
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
                for (uint32_t n{}; n < demo::CONTROLLER_COUNT; ++n)
                {
                    if (event.key.keysym.sym == controller_keyboard_map[n])
                    {
                        controller |= 1 << n;
                        break;
                    }
                }
            }
            break;
            case SDL_KEYUP:
            {
                for (uint32_t n{}; n < demo::CONTROLLER_COUNT; ++n)
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

        demo::tick(controller, controller_move[0], controller_move[1]);

        {
            void* pixels;
            int pitch;
            SDL_LockTexture(frame_texture, 0, &pixels, &pitch);

            demo::draw((uint32_t*)pixels, depth_buffer, pitch / sizeof(uint32_t));

            SDL_UnlockTexture(frame_texture);

            //SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, frame_texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
    }

    deinit();

    return 0;
}
