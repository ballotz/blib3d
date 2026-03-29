#include "../../source/render.hpp"
#include "../../source/view.hpp"

namespace triangle
{

blib3d::render::renderer renderer;

//------------------------------------------------------------------------------

// --- Geometry: one triangle, vertices in world space ---
//     x       y      z
float coords[] =
{
    0.0f,  0.5f,  1.0f,   // top center
   -0.5f, -0.5f,  1.0f,   // bottom left
    0.5f, -0.5f,  1.0f,   // bottom right
};
blib3d::render::face faces[] = { {0, 3} }; // index=0, count=3

//------------------------------------------------------------------------------

int32_t screen_width{ 0 };
int32_t screen_height{ 0 };

void setup(int32_t width, int32_t height)
{
    screen_width = width;
    screen_height = height;

    renderer.set_frame_clear_color({ 0x00, 0x00, 0x00, 0xFF });
    renderer.set_frame_clear_depth(0);

    blib3d::math::mat4x4 mat_proj;
    blib3d::math::mat4x4 mat_view;

    float screen_ratio{ (float)screen_width / (float)screen_height };
    blib3d::view::make_projection_perspective(
        mat_proj,
        screen_ratio,
        blib3d::math::pi / 2.f,
        blib3d::view::PROJECTION_Y);
    blib3d::view::make_viewport(mat_view, (float)screen_width, (float)screen_height);

    renderer.set_geometry_transform(mat_proj);
    renderer.set_frame_transform(mat_view);

    renderer.set_geometry_coord(coords, 3);
    renderer.set_geometry_face(faces, 1);
    renderer.set_geometry_back_cull(false);

    renderer.set_fill_type(blib3d::render::renderer::FILL_SOLID);
    renderer.set_fill_color({ 64, 128, 255, 255 });
    renderer.set_shade_type(blib3d::render::renderer::SHADE_NONE);
    renderer.set_blend_type(blib3d::render::renderer::BLEND_NONE);
}

//------------------------------------------------------------------------------

void draw(uint32_t* pixels, float* zbuffer, int32_t stride)
{
    renderer.set_frame_data(screen_width, screen_height, stride, zbuffer, (blib3d::raster::ARGB*)pixels);
    renderer.render_begin();

    renderer.render_clear_frame();
    renderer.render_clear_depth();

    renderer.render_draw();

    renderer.render_end();
}

} // namespace demo
