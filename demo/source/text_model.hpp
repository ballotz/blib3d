#pragma once
#include "../../source/render.hpp"

namespace demo
{

void text_model_setup(float step, float depth);

void text_model_tick(float angle);

void text_model_draw(blib3d::render::renderer& renderer);

} // namespace demo
