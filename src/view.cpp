#include "view.hpp"

namespace blib3d::view
{

void make_projection_ortho(
    math::mat4x4 out,
    float screen_ratio,
    float span,
    uint32_t flag)
{
    float span_x;
    float span_y;

    switch (flag)
    {
    case PROJECTION_X:
        span_x = span;
        span_y = span / screen_ratio;
        break;
    case PROJECTION_Y:
        span_x = span * screen_ratio;
        span_y = span;
        break;
    default:
        span_x = span;
        span_y = span;
    }

    float xs{ 2.f / span_x };
    float ys{ 2.f / span_y };

    out[ 0] = xs;
    out[ 1] = 0;
    out[ 2] = 0;
    out[ 3] = 0;

    out[ 4] = 0;
    out[ 5] = ys;
    out[ 6] = 0;
    out[ 7] = 0;

    out[ 8] = 0;
    out[ 9] = 0;
    out[10] = 1;
    out[11] = 0;

    out[12] = 0;
    out[13] = 0;
    out[14] = 0;
    out[15] = 1;
}

void make_projection_perspective(
    math::mat4x4 out,
    float screen_ratio,
    float field_of_view,
    uint32_t flag)
{
    float span{ std::tan(field_of_view / 2.f) };

    float span_x;
    float span_y;

    switch (flag)
    {
    case PROJECTION_X:
        span_x = span;
        span_y = span / screen_ratio;
        break;
    case PROJECTION_Y:
        span_x = span * screen_ratio;
        span_y = span;
        break;
    default:
        span_x = span;
        span_y = span;
    }

    float xs{ 1.f / span_x };
    float ys{ 1.f / span_y };

    out[ 0] = xs;
    out[ 1] = 0;
    out[ 2] = 0;
    out[ 3] = 0;

    out[ 4] = 0;
    out[ 5] = ys;
    out[ 6] = 0;
    out[ 7] = 0;

    out[ 8] = 0;
    out[ 9] = 0;
    out[10] = 0;
    out[11] = -1;

    out[12] = 0;
    out[13] = 0;
    out[14] = 1;
    out[15] = 0;
}

void make_viewport(
    math::mat4x4 out,
    float screen_w,
    float screen_h)
{
    float hw{ screen_w / 2.f };
    float hh{ screen_h / 2.f };

    out[ 0] = hw;
    out[ 1] = 0;
    out[ 2] = 0;
    out[ 3] = hw;

    out[ 4] = 0;
    out[ 5] = -hh;
    out[ 6] = 0;
    out[ 7] = hh;

    out[ 8] = 0;
    out[ 9] = 0;
    out[10] = 1;
    out[11] = 0;

    out[12] = 0;
    out[13] = 0;
    out[14] = 0;
    out[15] = 1;
}

} // namespace blib3d::view
