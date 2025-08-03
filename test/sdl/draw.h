#ifndef INCLUDE_DRAW

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct draw_rect_type
{
    uint32_t* data;
    int32_t height;
    int32_t width;
    int32_t stride;
};

void draw_fill(struct draw_rect_type* rect, uint32_t color);

void draw_string(struct draw_rect_type* rect, int32_t pos_x, int32_t pos_y, const char* str, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif INCLUDE_DRAW
