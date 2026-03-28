#pragma once
#include "../../source/render.hpp"

namespace demo
{

void rect_model_setup();

void rect_model_tick(float light_x, float light_y);

void rect_model_draw(blib3d::render::renderer& renderer);

} // namespace demo
