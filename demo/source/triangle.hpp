#pragma once
#include <cstdint>

namespace triangle
{

void setup(int32_t width, int32_t height);

void draw(uint32_t* pixels, float* zbuffer, int32_t stride);

} // namespace triangle
