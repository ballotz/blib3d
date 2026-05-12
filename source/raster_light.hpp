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
    math::sub3(ray, l.position, surf_position);
    float dist2{ math::dot3(ray, ray) };
    if (dist2 < l.radius * l.radius)
    {
        float scale1{ math::dot3(ray, surf_normal) };
        if (scale1 > 0.f)
        {
            float dist1{ math::sqrt(dist2) };
            float scale2{ -math::dot3(ray, l.direction) };
            float costh_min_scaled{ l.spot_costh_min * dist1 };
            if (scale2 > costh_min_scaled)
            {
                scale2 = (scale2 - costh_min_scaled) * l.spot_costh_range_inv;
                float scale{ scale1 * math::min(scale2, dist1) };
                scale /= 1e-3f + dist2 * (l.damping[0] + dist1 * l.damping[1] + dist2 * l.damping[2]);
                math::muladd3(res, l.intensity, scale);
            }
        }
    }
}

} // namespace blib3d::raster
