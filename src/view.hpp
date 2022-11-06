#pragma once
#include "math.hpp"

namespace lib3d::view
{

void make_translation_rotation(
    math::mat4x4 out,
    math::vec3 translation,
    math::vec3 rotation_angle);

enum
{
    PROJECTION_X,
    PROJECTION_Y
};

void make_projection_ortho(
    math::mat4x4 out,
    float screen_ratio,
    float span,
    uint32_t flag = PROJECTION_Y);

void make_projection_perspective(
    math::mat4x4 out,
    float screen_ratio,
    float field_of_view,
    uint32_t flag = PROJECTION_Y);

void make_viewport(
    math::mat4x4 out,
    float screen_w,
    float screen_h);

} // namespace lib3d::view
