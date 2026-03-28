#pragma once
#include <cstdint>

template<typename T>
struct draw2d_rect
{
    T* data;
    int32_t height;
    int32_t width;
    int32_t stride;
};

template<typename T>
void draw2d_fill(draw2d_rect<T>* rect, T color);

template<typename T>
void draw2d_draw_char(draw2d_rect<T>* rect, int32_t pos_x, int32_t pos_y, char c, T color);

template<typename T>
void draw2d_draw_string(draw2d_rect<T>* rect, int32_t pos_x, int32_t pos_y, const char* str, T color);
