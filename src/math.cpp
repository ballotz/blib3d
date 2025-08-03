#include "math.hpp"
#include <algorithm>

namespace blib3d::math
{

int32_t test3_aab_plane(const vec3 box_min, const vec3 box_max, const vec4 plane)
{
    vec3 v0, v1;
    copy3(v0, box_min);
    copy3(v1, box_max);
    if (plane[0] > 0.f)
        std::swap(v0[0], v1[0]);
    if (plane[1] > 0.f)
        std::swap(v0[1], v1[1]);
    if (plane[2] > 0.f)
        std::swap(v0[2], v1[2]);
    float d0{ v0[0] * plane[0] + v0[1] * plane[1] + v0[2] * plane[2] + plane[3] };
    float d1{ v1[0] * plane[0] + v1[1] * plane[1] + v1[2] * plane[2] + plane[3] };
    if (d0 < 0.f && d1 < 0.f)
        return test_outside;
    else if (d0 > 0.f && d1 > 0.f)
        return test_inside;
    else
        return test_intersect;
}

int32_t test3_sphere_plane(const vec3 pos, const float radius, const vec4 plane)
{
    float d{ pos[0] * plane[0] + pos[1] * plane[1] + pos[2] * plane[2] + plane[3] };
    if (d < -radius)
        return test_outside;
    else if (d > radius)
        return test_inside;
    else
        return test_intersect;
}

int32_t test3_sphere_aab(const vec3 pos, const float radius, const vec3 box_min, const vec3 box_max)
{
    int32_t test[3]
    {
        test1_segment_segment(pos[0] - radius, pos[0] + radius, box_min[0], box_max[0]),
        test1_segment_segment(pos[1] - radius, pos[1] + radius, box_min[1], box_max[1]),
        test1_segment_segment(pos[2] - radius, pos[2] + radius, box_min[2], box_max[2])
    };
    if (test[0] == test_outside || test[1] == test_outside || test[2] == test_outside)
        return test_outside;
    else if (test[0] == test_inside && test[1] == test_inside && test[2] == test_inside)
        return test_inside;
    else
    {
        vec3 mindist;
        clamp3(mindist, pos, box_min, box_max);
        sub3(mindist, pos);
        float dist2{ dot3(mindist, mindist) };
        if (dist2 > radius * radius)
            return test_outside;
        else
            return test_intersect;
    }
}

int32_t test2_segment_line(const vec2 sa, const vec2 sb, const vec2 la, const vec2 lb)
{
    vec2 v0, v1, v2;
    sub2(v0, lb, la); // la -> lb
    sub2(v1, sa, la); // la -> sa
    sub2(v2, sb, la); // la -> sb
    float s0{ cross2(v0, v1) };
    float s1{ cross2(v0, v2) };
    if (s0 < 0.f && s1 < 0.f)
        return test_outside;
    else if (s0 > 0.f && s1 > 0.f)
        return test_inside;
    else
        return test_intersect;
}

int32_t test1_segment_segment(const float s1min, const float s1max, const float s2min, const float s2max)
{
    if (s1min > s2max || s1max < s2min)
        return test_outside;
    else if (s1min > s2min && s1max < s2max)
        return test_inside;
    else
        return test_intersect;
}

} // namespace blib3d::math
