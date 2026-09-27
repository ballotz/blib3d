/* Copyright 2019 Alessio Ballotti <alessioballotti@tiscali.it> */

#pragma once
#include "math.hpp"

namespace blib3d::view
{

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

void make_pre_matrix_strip(
    math::mat4x4 pre_out,
    const math::mat4x4 pre_in,
    float strip_y_src,
    float strip_height,
    float frame_height);

void make_post_matrix_strip(
    math::mat4x4 post_out,
    const math::mat4x4 post_in,
    float strip_y_dst,
    float strip_height,
    float frame_height);

enum
{
    clip_plane_w,
    clip_plane_xpos,
    clip_plane_xneg,
    clip_plane_ypos,
    clip_plane_yneg,
#if defined(BLIB3D_CLIP_Z)
    clip_plane_zpos,
    clip_plane_zneg,
#endif
    clip_plane_count
};

void make_frustum_planes(
    math::vec4 planes_out[clip_plane_count],
    const math::mat4x4 pre_in);

} // namespace blib3d::view
