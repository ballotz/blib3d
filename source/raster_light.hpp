/* Copyright 2019 Alessio Ballotti <alessioballotti@tiscali.it> */

#pragma once
#include "math.hpp"

namespace blib3d::raster
{

struct light
{
    enum
    {
        type_off,
        type_ambient,
        type_directional,
        type_point,
        type_spot
    };
    uint32_t type;
    float radius; // point, spot
    math::vec3 position; // point, spot
    math::vec3 direction; // directional, spot
    math::vec3 intensity; // all
    float damping[3]; // point, spot
    float spot_costh_min; // spot
    float spot_costh_range_inv; // spot
};

//------------------------------------------------------------------------------

blib3d_force_inline void light_ambient(
    const light& l,
    math::vec3 res)
{
    math::add3(res, l.intensity);
}

blib3d_force_inline void light_directional(
    const light& l,
    const math::vec3 surf_normal,
    math::vec3 res)
{
    float scale{ -math::dot3(l.direction, surf_normal) };
    if (scale > 0.f)
        math::muladd3(res, l.intensity, scale);
}

blib3d_force_inline void light_point(
    const light& l,
    const math::vec3 surf_position,
    const math::vec3 surf_normal,
    math::vec3 res)
{
    math::vec3 ray;
    math::sub3(ray, l.position, surf_position); // R = Lp - Sp
    float dist2{ math::dot3(ray, ray) }; // d2 = |R|^2
    if (dist2 < l.radius * l.radius)
    {
        float scale{ math::dot3(ray, surf_normal) }; // s = R . N = cos(t) * |R|
        if (scale > 0.f)
        {
            float dist1{ math::sqrt(dist2) }; // d1 = |R|
            scale /= 1e-3f + dist1 * (l.damping[0] + dist1 * l.damping[1] + dist2 * l.damping[2]); // s = cos(t) / (damp0 + damp1*|R| + damp2*|R|^2)
            math::muladd3(res, l.intensity, scale);
        }
    }
}

blib3d_force_inline void light_spot(
    const light& l,
    const math::vec3 surf_position,
    const math::vec3 surf_normal,
    math::vec3 res)
{
    math::vec3 ray;
    math::sub3(ray, l.position, surf_position); // R = Lp - Sp
    float dist2{ math::dot3(ray, ray) }; // d2 = |R|^2
    if (dist2 < l.radius * l.radius)
    {
        float scale_n{ math::dot3(ray, surf_normal) }; // sn = R . N = cos(t) * |R|
        if (scale_n > 0.f)
        {
            float dist1{ math::sqrt(dist2) }; // d1 = |R|
            float scale_l{ -math::dot3(ray, l.direction) }; // s2 = R . L = cos(p) * |R|
            float costh_min_scaled{ l.spot_costh_min * dist1 };
            if (scale_l > costh_min_scaled) // cos(p) * |R| > cos_min * |R|
            {
                scale_l = (scale_l - costh_min_scaled) * l.spot_costh_range_inv; // (cos(p) - cos_min) * cos_range_inv * |R|
                float scale{ scale_n * math::min(scale_l * scale_l, dist2) }; // s = cos(t) * min(scale_l, 1) * |R|^3
                scale /= 1e-3f + dist1 * dist2 * (l.damping[0] + dist1 * l.damping[1] + dist2 * l.damping[2]);
                math::muladd3(res, l.intensity, scale);
            }
        }
    }
}

} // namespace blib3d::raster
