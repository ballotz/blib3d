#include "demo.hpp"
#include "brick.h"
#include "draw2d.hpp"
#include "rect_model.hpp"
#include "text_model.hpp"
#include "../../source/timer.hpp"
#include "../../source/render.hpp"
#include "../../source/view.hpp"
#include <cstdio>

namespace demo
{

blib3d::render::renderer renderer;

//------------------------------------------------------------------------------

void surface_normal(float* vertices, int32_t vert_stride, int32_t vert_count, blib3d::math::vec3 normal)
{
    // (v1 - v0) x (v2 - v0) =
    // v1 x v2 - v1 x v0 - v0 x v2 + v0 x v0 =
    // v0 x v1 + v1 x v2 + v2 x v0

    blib3d::math::vec3 t, norm{};
    int32_t vl{ vert_stride * vert_count };
    for (int32_t v0{ 0 }, v1{ vert_stride }; v0 < vl; v0 += vert_stride, v1 += vert_stride)
    {
        if (v1 >= vl)
            v1 = 0;
        blib3d::math::cross3(t, &vertices[v0], &vertices[v1]);
        blib3d::math::add3(norm, t);
    }
    float k{ blib3d::math::sqrt(blib3d::math::dot3(norm, norm)) };
    if (k > 0.f)
    {
        k = 1.f / k;
        blib3d::math::mul3(normal, norm, k);
    }
}

//------------------------------------------------------------------------------

int32_t screen_width{ 0 };
int32_t screen_height{ 0 };

blib3d::math::mat4x4 mat_proj;
blib3d::math::vec3 camera_pos{};
blib3d::math::vec3 camera_ang{};
blib3d::math::mat3x3 camera_A
{
    1, 0, 0,
    0, 1, 0,
    0, 0, 1
};

blib3d::timer::interval interval;
blib3d::timer::profile profile;

blib3d::raster::light lights[2];

void setup(int32_t width, int32_t height, bool rotate)
{
    screen_width = width;
    screen_height = height;

    interval.reset();

    renderer.set_frame_clear_color({ 0xC0, 0xC0, 0xC0, 0xFF });
    renderer.set_frame_clear_depth(0);

    blib3d::math::mat4x4 mat_view;

    if (rotate == false)
    {
        float screen_ratio{ (float)screen_width / (float)screen_height };
        blib3d::view::make_projection_perspective(
            mat_proj,
            screen_ratio,
            blib3d::math::pi / 2.f,
            blib3d::view::PROJECTION_Y);
        blib3d::view::make_viewport(mat_view, (float)screen_width, (float)screen_height);
    }
    else
    {
        float screen_ratio{ (float)screen_height / (float)screen_width };
        blib3d::view::make_projection_perspective(
            mat_proj,
            screen_ratio,
            blib3d::math::pi / 2.f,
            blib3d::view::PROJECTION_Y);
        {
            float tmp;
            tmp = mat_proj[4];
            mat_proj[4] = -mat_proj[0];
            mat_proj[0] = tmp;
            tmp = mat_proj[5];
            mat_proj[5] = -mat_proj[1];
            mat_proj[1] = tmp;
            tmp = mat_proj[6];
            mat_proj[6] = -mat_proj[2];
            mat_proj[2] = tmp;
            tmp = mat_proj[7];
            mat_proj[7] = -mat_proj[3];
            mat_proj[3] = tmp;
        }
        blib3d::view::make_viewport(mat_view, (float)screen_width, (float)screen_height);
    }

    renderer.set_geometry_transform(mat_proj);
    renderer.set_frame_transform(mat_view);

    lights[0].type = blib3d::raster::light::light::type_point;
    lights[0].position[0] = 0.f;
    lights[0].position[1] = 32.f;
    lights[0].position[2] = 0.f;
    lights[0].intensity[0] = 1.0f;
    lights[0].intensity[1] = 0.5f;
    lights[0].intensity[2] = 0.1f;
    lights[0].damping[0] = 0.f;
    lights[0].damping[1] = 0.f;
    lights[0].damping[2] = 1.f / (16.f * 16.f);
    lights[0].radius = 1024.f;
    lights[1].type = blib3d::raster::light::light::type_spot;
    lights[1].position[0] = 0.f;
    lights[1].position[1] = 0.f;
    lights[1].position[2] = 0.f;
    lights[1].direction[0] = 0.f;
    lights[1].direction[1] = 0.f;
    lights[1].direction[2] = 1.f;
    lights[1].intensity[0] = 1.0f;
    lights[1].intensity[1] = 0.5f;
    lights[1].intensity[2] = 0.1f;
    lights[1].damping[0] = 0.f;
    lights[1].damping[1] = 0.5f / (16.f);
    lights[1].damping[2] = 0.5f / (16.f * 16.f);
    float spot_costh_min = std::cos(0.50f);
    float spot_costh_max = std::cos(0.25f);
    lights[1].spot_costh_min = spot_costh_min;
    lights[1].spot_costh_range_inv = 1.f / (spot_costh_max - spot_costh_min);
    lights[1].radius = 1024.f;
    renderer.set_shade_lights(lights, 2);

    renderer.gamma_set(2.2f);

    rect_model_setup();
    text_model_setup(1.f, 2.f);
}

//------------------------------------------------------------------------------

float rect_light_angle{ 0.f };
float rect_light_angle_speed{ 3.1416f / 10.f };

float text_model_angle{ -3.1416f / 2 };
float text_model_angle_speed{ 3.1416f / 10.f };
//float text_model_angle{ 0 };
//float text_model_angle_speed{ 0 };

float fps{};
float fps_count{};
float ms{};

uint32_t profile_us;
float profile_fps{};
float profile_fps_count{};
float profile_ms{};

char string[128];

void tick(uint32_t controller, int32_t dx, int32_t dy)
{
    float dt{ interval.get_s() };
    interval.tick();

    profile.start();

    if (controller & (1 << CONTROLLER_RESET))
    {
        fps_count = 0;
    }

    {
        blib3d::math::vec3 camera_pos_speed{};
        blib3d::math::vec3 camera_ang_speed{};

        if (controller & (1 << CONTROLLER_FORWARD))
            camera_pos_speed[2] += 1;
        if (controller & (1 << CONTROLLER_BACKWARD))
            camera_pos_speed[2] -= 1;
        if (controller & (1 << CONTROLLER_UP))
            camera_pos_speed[1] += 1;
        if (controller & (1 << CONTROLLER_DOWN))
            camera_pos_speed[1] -= 1;
        if (controller & (1 << CONTROLLER_LEFT))
            camera_pos_speed[0] -= 1;
        if (controller & (1 << CONTROLLER_RIGHT))
            camera_pos_speed[0] += 1;

        if (controller & (1 << CONTROLLER_LOOK_UP))
            camera_ang_speed[0] -= 1;
        if (controller & (1 << CONTROLLER_LOOK_DOWN))
            camera_ang_speed[0] += 1;
        if (controller & (1 << CONTROLLER_LOOK_LEFT))
            camera_ang_speed[1] -= 1;
        if (controller & (1 << CONTROLLER_LOOK_RIGHT))
            camera_ang_speed[1] += 1;

        blib3d::math::vec3 camera_r_pos_speed;
        blib3d::math::mul3x3_3(camera_r_pos_speed, camera_A, camera_pos_speed);
        
        blib3d::math::mul3(camera_r_pos_speed, dt * 16.f); // step speed
        blib3d::math::mul3(camera_ang_speed, dt * blib3d::math::pi); // angle speed

        blib3d::math::add3(camera_pos, camera_r_pos_speed);
        blib3d::math::add3(camera_ang, camera_ang_speed);

        float fx{ blib3d::math::pi / (float)screen_width };
        float fy{ blib3d::math::pi / (float)screen_height };
        camera_ang[1] += (float)dx * fx;
        camera_ang[0] += (float)dy * fy;

        blib3d::math::mat3x3 Ax, Ay;
        blib3d::math::rotation_x(Ax, camera_ang[0]);
        blib3d::math::rotation_y(Ay, camera_ang[1]);
        blib3d::math::mul3x3_3x3(camera_A, Ay, Ax);

        blib3d::math::vec3 h;
        blib3d::math::mul3x3t_3(h, camera_A, camera_pos);
        blib3d::math::mat4x4 mat_rt
        {
            camera_A[0], camera_A[3], camera_A[6], -h[0],
            camera_A[1], camera_A[4], camera_A[7], -h[1],
            camera_A[2], camera_A[5], camera_A[8], -h[2],
                        0,           0,           0,     1,
        };

        blib3d::math::mat4x4 mat_pre;
        blib3d::math::mul4x4_4x4(mat_pre, mat_proj, mat_rt);

        renderer.set_geometry_transform(mat_pre);
    }

    rect_light_angle += rect_light_angle_speed * dt;
    float rect_light_x{ std::cos(rect_light_angle) * 0.25f + 0.5f };
    float rect_light_y{ std::sin(rect_light_angle) * -0.25f + 0.5f };
    rect_model_tick(rect_light_x, rect_light_y);

    if (text_model_angle > +(blib3d::math::pi / 2.f))
    {
    	text_model_angle = +(blib3d::math::pi / 2.f);
    	text_model_angle_speed = -text_model_angle_speed;
    }
    if (text_model_angle < -(blib3d::math::pi / 2.f))
    {
    	text_model_angle = -(blib3d::math::pi / 2.f);
    	text_model_angle_speed = -text_model_angle_speed;
    }
    text_model_angle += text_model_angle_speed * dt;
    if (text_model_angle >= blib3d::math::pi)
    	text_model_angle -= (blib3d::math::pi * 2.f);
    if (text_model_angle < -blib3d::math::pi)
    	text_model_angle += (blib3d::math::pi * 2.f);
    text_model_tick(text_model_angle);

    if (dt != 0)
    {
        float new_fps{ 1.f / dt };
        float new_ms{ dt * 1000.f };
        fps = (fps * fps_count + new_fps) / (fps_count + 1);
        ms = (ms * fps_count + new_ms) / (fps_count + 1);
        fps_count++;
    }
    if (profile_us != 0)
    {
        float new_fps{ 1e6f / (float)profile_us };
        float new_ms{ (float)profile_us / 1000.f };
        profile_fps = (profile_fps * profile_fps_count + new_fps) / (profile_fps_count + 1);
        profile_ms = (profile_ms * profile_fps_count + new_ms) / (profile_fps_count + 1);
        profile_fps_count++;
    }

    snprintf(string, sizeof(string), "%ix%i\nfps %i\nms %i\nprof fps %i\nprof ms %i",
		screen_width, screen_height,
		(int)fps, (int)ms,
		(int)profile_fps, (int)profile_ms);

    profile.stop();
}

void draw(uint32_t* pixels, float* zbuffer, int32_t stride)
{
	profile.start();

    draw2d_rect<uint32_t> rect;
    rect.data = (uint32_t*)pixels;
    rect.height = screen_height;
    rect.width = screen_width;
    rect.stride = stride;

    renderer.set_frame_data(screen_width, screen_height, stride, zbuffer, (blib3d::raster::ARGB*)pixels);
    renderer.render_begin();

    renderer.render_clear_frame();
    renderer.render_clear_depth();

    rect_model_draw(renderer);

    text_model_draw(renderer);    

    draw2d_draw_string<uint32_t>(&rect, 0, 0, string, 0xFF888888);

    renderer.render_end();

    profile.stop();
    profile.update();
    profile_us = profile.max();
}

} // namespace demo
