#pragma once
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

typedef struct
{
    void* data;
    int32_t height;
    int32_t width;
    int32_t stride;
} draw2d_rect_type;

void draw2d_fill_16(draw2d_rect_type* rect, uint16_t color);

void draw2d_fill_32(draw2d_rect_type* rect, uint32_t color);

void draw2d_draw_char_16(draw2d_rect_type* rect, int32_t pos_x, int32_t pos_y, char c, uint16_t color);

void draw2d_draw_char_32(draw2d_rect_type* rect, int32_t pos_x, int32_t pos_y, char c, uint32_t color);

void draw2d_draw_string_16(draw2d_rect_type* rect, int32_t pos_x, int32_t pos_y, const char* str, uint16_t color);

void draw2d_draw_string_32(draw2d_rect_type* rect, int32_t pos_x, int32_t pos_y, const char* str, uint32_t color);

//------------------------------------------------------------------------------

#include "MIMXRT1176_cm7.h"
#include "MIMXRT1176_cm7_features.h"

typedef struct
{
    uint32_t t;
    uint32_t us;
} draw2d_interval_type;

static inline void draw2d_interval_init()
{
    // Enable trace globally (required for DWT)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Enable cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void draw2d_interval_tick(draw2d_interval_type* instance)
{
    uint32_t new_t = DWT->CYCCNT;
    instance->us = (uint32_t)((float)(new_t - instance->t) * (1e6f / (float)SystemCoreClock));
    instance->t = new_t;
}

static inline void draw2d_interval_reset(draw2d_interval_type* instance)
{
    instance->t = DWT->CYCCNT;
    instance->us = 0;
}

static inline uint32_t draw2d_interval_get_us(draw2d_interval_type* instance)
{
    return instance->us;
}

static inline float draw2d_interval_get_s(draw2d_interval_type* instance)
{
    return (float)instance->us * 1e-6f;
}

#if defined __cplusplus
}
#endif
