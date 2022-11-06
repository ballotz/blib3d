#include "view.hpp"

namespace lib3d::view
{

void make_translation_rotation(
    math::mat4x4 out,
    math::vec3 translation,
    math::vec3 rotation_angle)
{
    math::mat3x3 r;
    math::make_rotation(r, rotation_angle);

    // V = R'(W - O)
    // V = R'W - R'O
    math::vec3 t;
    math::mul3x3t_3(t, r, translation);
    math::copy4x4(out,
        math::mat4x4
        {
            r[0], r[1], r[2], -t[0],
            r[3], r[4], r[5], -t[1],
            r[6], r[7], r[8], -t[2],
               0,    0,    0,     1
        });
}

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

    math::copy4x4(out,
        math::mat4x4
        {
            xs,  0,  0,  0,
             0, ys,  0,  0,
             0,  0,  1,  0,
             0,  0,  0,  1
        });
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

    math::copy4x4(out,
        math::mat4x4
        {
            xs,  0,  0,  0,
             0, ys,  0,  0,
             0,  0,  0, -1,
             0,  0,  1,  0
        });
}

void make_viewport(
    math::mat4x4 out,
    float screen_w,
    float screen_h)
{
    float hw{ screen_w / 2.f };
    float hh{ screen_h / 2.f };
    math::copy4x4(out,
        math::mat4x4
        {
            hw,   0,   0,  hw,
             0, -hh,   0,  hh,
             0,   0,   1,   0,
             0,   0,   0,   1
        });
}

} // namespace lib3d::view
