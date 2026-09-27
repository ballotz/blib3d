/* Copyright 2019 Alessio Ballotti <alessioballotti@tiscali.it> */

#include "view.hpp"
#include "render.hpp"

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

    math::copy4x4(out, math::mat4x4
        {
            xs, 0, 0, 0,
            0, ys, 0, 0,
            0,  0, 1, 0,
            0,  0, 0, 1
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

    math::copy4x4(out, math::mat4x4
        {
            xs, 0, 0,  0,
            0, ys, 0,  0,
            0,  0, 0, -1,
            0,  0, 1,  0
        });
}

void make_viewport(
    math::mat4x4 out,
    float screen_w,
    float screen_h)
{
    float hw{ screen_w / 2.f };
    float hh{ screen_h / 2.f };

    math::copy4x4(out, math::mat4x4
        {
            hw,   0, 0, hw,
             0, -hh, 0, hh,
             0,   0, 1,  0,
             0,   0, 0,  1
        });
}

void make_pre_matrix_strip(
    math::mat4x4 pre_out,
    const math::mat4x4 pre_in,
    float strip_y_src,
    float strip_height,
    float frame_height)
{
    float s{ frame_height / strip_height };
    float t{ strip_y_src / frame_height };
    t = 1.f - s * (1.f - 2.f * t);
    math::mat4x4 m =
    {
        1, 0, 0, 0,
        0, s, 0, t,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    math::mul4x4_4x4(pre_out, m, pre_in);
}

void make_post_matrix_strip(
    math::mat4x4 post_out,
    const math::mat4x4 post_in,
    float strip_y_dst,
    float strip_height,
    float frame_height)
{
    float s{ frame_height / strip_height };
    float t{ strip_y_dst / frame_height };
    s = 1.f / s;
    t = (1.f - s) - 2.f * t;
    math::mat4x4 m
    {
        1, 0, 0, 0,
        0, s, 0, t,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    math::mul4x4_4x4(post_out, post_in, m);
}

void make_frustum_planes(
    math::vec4 planes_out[clip_plane_count],
    const math::mat4x4 pre_in)
{
    const float* mat{ pre_in };
    math::copy4(planes_out[0], math::vec4{ mat[12], mat[13], mat[14], mat[15] - render::clip_w_min });
    math::copy4(planes_out[1], math::vec4{ mat[12] + mat[0], mat[13] + mat[1], mat[14] + mat[2], mat[15] + mat[3] });
    math::copy4(planes_out[2], math::vec4{ mat[12] - mat[0], mat[13] - mat[1], mat[14] - mat[2], mat[15] - mat[3] });
    math::copy4(planes_out[3], math::vec4{ mat[12] + mat[4], mat[13] + mat[5], mat[14] + mat[6], mat[15] + mat[7] });
    math::copy4(planes_out[4], math::vec4{ mat[12] - mat[4], mat[13] - mat[5], mat[14] - mat[6], mat[15] - mat[7] });
#if defined(BLIB3D_CLIP_Z)
    math::copy4(planes_out[5], math::vec4{ mat[12] + mat[8], mat[13] + mat[9], mat[14] + mat[10], mat[15] + mat[11] });
    math::copy4(planes_out[6], math::vec4{ mat[12] - mat[8], mat[13] - mat[9], mat[14] - mat[10], mat[15] - mat[11] });
#endif
}

} // namespace blib3d::view
