#pragma once
#include <cstdint>

namespace demo
{

void setup(int32_t width, int32_t height, bool rotate = false);

enum
{
    EVENT_KEYDOWN,
    EVENT_KEYUP,

    EVENT_COUNT
};
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

    CONTROLLER_RESET,

    CONTROLLER_COUNT
};

void tick(uint32_t controller, int32_t dx, int32_t dy);

void draw(uint32_t* pixels, float* zbuffer, int32_t stride);

} // namespace demo
