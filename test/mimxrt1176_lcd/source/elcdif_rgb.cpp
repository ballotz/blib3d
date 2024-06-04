/*
 * Copyright  2017-2019 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_common.h"
#include "fsl_elcdif.h"
#include "fsl_debug_console.h"

#include "fsl_soc_src.h"
#include "fsl_gpio.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "elcdif_support.h"

#include <algorithm>
#include "draw2d.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#ifndef APP_LCDIF_DATA_BUS
#define APP_LCDIF_DATA_BUS kELCDIF_DataBus24Bit
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

//#define PIXELFORMAT 32
#define PIXELFORMAT 16

#if PIXELFORMAT==32
typedef uint32_t frame_buffer_type;
#elif PIXELFORMAT==16
typedef uint16_t frame_buffer_type;
#endif

static volatile bool s_frameDone = false;

AT_NONCACHEABLE_SECTION_ALIGN(static frame_buffer_type s_frameBuffer[2][APP_IMG_HEIGHT][APP_IMG_WIDTH], FRAME_BUFFER_ALIGN);

uint32_t frameBufferIndex = 0;

/*******************************************************************************
 * Code
 ******************************************************************************/

static void BOARD_ResetDisplayMix(void)
{
    /*
     * Reset the displaymix, otherwise during debugging, the
     * debugger may not reset the display, then the behavior
     * is not right.
     */
    SRC_AssertSliceSoftwareReset(SRC, kSRC_DisplaySlice);
    while (kSRC_SliceResetInProcess == SRC_GetSliceResetState(SRC, kSRC_DisplaySlice))
    {
    }
}

extern "C"
{

void APP_LCDIF_IRQHandler(void)
{
    uint32_t intStatus;

    intStatus = ELCDIF_GetInterruptStatus(APP_ELCDIF);

    ELCDIF_ClearInterruptStatus(APP_ELCDIF, intStatus);

    if (intStatus & kELCDIF_CurFrameDone)
    {
        s_frameDone = true;
    }
    SDK_ISR_EXIT_BARRIER;
}

} // extern "C"

void APP_ELCDIF_Init(void)
{
    const elcdif_rgb_mode_config_t config = {
        .panelWidth    = APP_IMG_WIDTH,
        .panelHeight   = APP_IMG_HEIGHT,
        .hsw           = APP_HSW,
        .hfp           = APP_HFP,
        .hbp           = APP_HBP,
        .vsw           = APP_VSW,
        .vfp           = APP_VFP,
        .vbp           = APP_VBP,
        .polarityFlags = APP_POL_FLAGS,
        .bufferAddr    = (uint32_t)s_frameBuffer[0],
#if PIXELFORMAT==32
        .pixelFormat   = kELCDIF_PixelFormatXRGB8888,
#elif PIXELFORMAT==16
        .pixelFormat   = kELCDIF_PixelFormatRGB565,
#endif
        .dataBus       = APP_LCDIF_DATA_BUS,
    };

#if (defined(APP_ELCDIF_HAS_DISPLAY_INTERFACE) && APP_ELCDIF_HAS_DISPLAY_INTERFACE)
    BOARD_InitDisplayInterface();
#endif
    ELCDIF_RgbModeInit(APP_ELCDIF, &config);
}

void APP_FillFrameBuffer32(uint32_t frameBuffer[APP_IMG_HEIGHT][APP_IMG_WIDTH])
{
    /* Background color. */
    static const uint32_t bgColor = 0U;
    /* Foreground color. */
    static uint8_t fgColorIndex          = 0U;
    static const uint32_t fgColorTable[] = {0x000000FFU, 0x0000FF00U, 0x0000FFFFU, 0x00FF0000U,
                                            0x00FF00FFU, 0x00FFFF00U, 0x00FFFFFFU};
    uint32_t fgColor                     = fgColorTable[fgColorIndex];

    /* Position of the foreground rectangle. */
    static uint16_t upperLeftX  = 0U;
    static uint16_t upperLeftY  = 0U;
    static uint16_t lowerRightX = (APP_IMG_WIDTH - 1U) / 2U;
    static uint16_t lowerRightY = (APP_IMG_HEIGHT - 1U) / 2U;

    static int8_t incX = 1;
    static int8_t incY = 1;

    /* Change color in next forame or not. */
    static bool changeColor = false;

    uint32_t i, j;

    /* Background color. */
    for (i = 0; i < APP_IMG_HEIGHT; i++)
    {
        for (j = 0; j < APP_IMG_WIDTH; j++)
        {
            frameBuffer[i][j] = bgColor;
        }
    }

    /* Foreground color. */
    for (i = upperLeftY; i < lowerRightY; i++)
    {
        for (j = upperLeftX; j < lowerRightX; j++)
        {
            frameBuffer[i][j] = fgColor;
        }
    }

    /* Update the format: color and rectangle position. */
    upperLeftX += incX;
    upperLeftY += incY;
    lowerRightX += incX;
    lowerRightY += incY;

    changeColor = false;

    if (0U == upperLeftX)
    {
        incX        = 1;
        changeColor = true;
    }
    else if (APP_IMG_WIDTH - 1 == lowerRightX)
    {
        incX        = -1;
        changeColor = true;
    }

    if (0U == upperLeftY)
    {
        incY        = 1;
        changeColor = true;
    }
    else if (APP_IMG_HEIGHT - 1 == lowerRightY)
    {
        incY        = -1;
        changeColor = true;
    }

    if (changeColor)
    {
        fgColorIndex++;

        if (ARRAY_SIZE(fgColorTable) == fgColorIndex)
        {
            fgColorIndex = 0U;
        }
    }
}

void APP_FillFrameBuffer16(uint16_t frameBuffer[APP_IMG_HEIGHT][APP_IMG_WIDTH])
{
    /* Foreground color. */
    static uint8_t fgColorIndex          = 0U;
    static const uint16_t fgColorTable[] = {0x001FU, 0x07E0U, 0x07FFU, 0xF800U, 0xF81FU, 0xFFE0U, 0xFFFFU};
    uint16_t fgColor                     = fgColorTable[fgColorIndex];

    /* Position of the foreground rectangle. */
    static uint16_t upperLeftX  = 0;
    static uint16_t upperLeftY  = 0;
    static uint16_t lowerRightX = (APP_IMG_WIDTH - 1U) / 2U;
    static uint16_t lowerRightY = (APP_IMG_HEIGHT - 1U) / 2U;

    static int8_t incX = 1;
    static int8_t incY = 1;

    /* Change color in next forame or not. */
    static bool changeColor = false;

    uint32_t i, j;

    /* Set background color to black. */
    //memset(frameBuffer, 0, APP_IMG_WIDTH * APP_IMG_HEIGHT * 2);
    for (i = 0; i < APP_IMG_HEIGHT; i++)
    {
        for (j = 0; j < APP_IMG_WIDTH; j++)
        {
            frameBuffer[i][j] = 0;
        }
    }

    /* Foreground color. */
    for (i = upperLeftY; i < lowerRightY; i++)
    {
        for (j = upperLeftX; j < lowerRightX; j++)
        {
            frameBuffer[i][j] = fgColor;
        }
    }

    /* Update the format: color and rectangle position. */
    upperLeftX += incX;
    upperLeftY += incY;
    lowerRightX += incX;
    lowerRightY += incY;

    changeColor = false;

    if (0U == upperLeftX)
    {
        incX        = 1;
        changeColor = true;
    }
    else if (APP_IMG_WIDTH - 1 == lowerRightX)
    {
        incX        = -1;
        changeColor = true;
    }

    if (0U == upperLeftY)
    {
        incY        = 1;
        changeColor = true;
    }
    else if (APP_IMG_HEIGHT - 1 == lowerRightY)
    {
        incY        = -1;
        changeColor = true;
    }

    if (changeColor)
    {
        fgColorIndex++;

        if (ARRAY_SIZE(fgColorTable) == fgColorIndex)
        {
            fgColorIndex = 0U;
        }
    }
}

/*!
 * @brief Main function
 */
int main(void)
{
    //uint32_t frameBufferIndex = 0;

    draw2d_interval_type interval;
    float interval_dt, fps = 0, new_fps, ms = 0, new_ms, count = 0;
    draw2d_rect_type rect;
    char string[128];

    BOARD_ConfigMPU();
    BOARD_BootClockRUN();
    BOARD_ResetDisplayMix();
    BOARD_InitLpuartPins();
    BOARD_InitMipiPanelPins();
    BOARD_InitDebugConsole();
    BOARD_InitLcdifClock();

    draw2d_interval_init();
    draw2d_interval_reset(&interval);

    PRINTF("LCDIF RGB example start...\r\n");

    APP_ELCDIF_Init();

    BOARD_EnableLcdInterrupt();

    /* Clear the frame buffer. */
//    memset(s_frameBuffer, 0, sizeof(s_frameBuffer));
#if PIXELFORMAT==32
    std::fill(&s_frameBuffer[0][0][0], &s_frameBuffer[0][APP_IMG_HEIGHT][0], 0xFFFFFF00u);
    std::fill(&s_frameBuffer[1][0][0], &s_frameBuffer[1][APP_IMG_HEIGHT][0], 0xFF00FFFFu);
#elif PIXELFORMAT==16
    std::fill(&s_frameBuffer[0][0][0], &s_frameBuffer[0][APP_IMG_HEIGHT][0], 0xFFE0);
    std::fill(&s_frameBuffer[1][0][0], &s_frameBuffer[1][APP_IMG_HEIGHT][0], 0x07FF);
#endif

#if PIXELFORMAT==32
    APP_FillFrameBuffer32(s_frameBuffer[frameBufferIndex]);
#elif PIXELFORMAT==16
    APP_FillFrameBuffer16(s_frameBuffer[frameBufferIndex]);
#endif

    ELCDIF_EnableInterrupts(APP_ELCDIF, kELCDIF_CurFrameDoneInterruptEnable);
    ELCDIF_RgbModeStart(APP_ELCDIF);

#if 0
    while (1)
    {
        frameBufferIndex ^= 1U;

#if PIXELFORMAT==32
        APP_FillFrameBuffer32(s_frameBuffer[frameBufferIndex]);
#elif PIXELFORMAT==16
        APP_FillFrameBuffer16(s_frameBuffer[frameBufferIndex]);
#endif

        draw2d_interval_tick(&interval);
        interval_dt = draw2d_interval_get_s(&interval);
        if (interval_dt != 0)
        {
            new_fps = 1.f / interval_dt;
            new_ms = interval_dt * 1000.f;
            fps = (fps * count + new_fps) / (count + 1);
            ms = (ms * count + new_ms) / (count + 1);
            count++;
        }
        snprintf(string, sizeof(string), "%ix%i %ibpp\nfps %i\nms %i", APP_IMG_WIDTH, APP_IMG_HEIGHT, PIXELFORMAT, (int)fps, (int)ms);
        rect.data = &s_frameBuffer[frameBufferIndex][0][0];
        rect.height = APP_IMG_HEIGHT;
        rect.width = APP_IMG_WIDTH;
        rect.stride = APP_IMG_WIDTH;
#if PIXELFORMAT==32
        draw2d_draw_string_32(&rect, 0, 0, string, 0xFF808080);
#elif PIXELFORMAT==16
        draw2d_draw_string_16(&rect, 0, 0, string, 0x8410);
#endif

//        ELCDIF_SetNextBufferAddr(APP_ELCDIF, (uint32_t)s_frameBuffer[frameBufferIndex]);
        APP_ELCDIF->NEXT_BUF = (uint32_t)s_frameBuffer[frameBufferIndex];
        APP_ELCDIF->CUR_BUF = (uint32_t)s_frameBuffer[frameBufferIndex];

//        s_frameDone = false;
//        /* Wait for previous frame complete. */
//        while (!s_frameDone)
//        {
//        }
    }
#endif

    float tavg = 0.f;
    for (int i = 0; i < 32; ++i)
    {
        uint32_t t0 = DWT->CYCCNT;

        frameBufferIndex ^= 1U;

        for (int i = 0; i < APP_IMG_WIDTH * APP_IMG_HEIGHT; ++i)
            s_frameBuffer[frameBufferIndex][0][i] = 0;

        ELCDIF_SetNextBufferAddr(APP_ELCDIF, (uint32_t)s_frameBuffer[frameBufferIndex]);

        s_frameDone = false;
        /* Wait for previous frame complete. */
        while (!s_frameDone)
        {
        }

        uint32_t t1 = DWT->CYCCNT;
        uint32_t t = t1 - t0;
        tavg = (tavg * (float)i + (float)t) / (float)(i + 1);
    }
    PRINTF("Display min time: %dus\r\n", (int)(tavg / SystemCoreClock * 1e6f));

    extern void lib3d_demo();
    lib3d_demo();

    return 0;
}

void APP_MemToScreen(uint32_t* buffer, int width, int height)
{
    frameBufferIndex ^= 1U;

#if 0
    int jshift = 0;
    while ((height << jshift) < APP_IMG_WIDTH)
        jshift++;

    int ishift = 0;
    while ((width << ishift) < APP_IMG_HEIGHT)
        ishift++;

    int shift = jshift > ishift ? jshift : ishift;

    uint32_t* dest_buffer = &s_frameBuffer[frameBufferIndex][0][0];
    for (int j = 0; j < APP_IMG_WIDTH; j++)
    {
        int jj = APP_IMG_WIDTH - 1 - j;
        for (int i = 0; i < APP_IMG_HEIGHT; i++)
        {
            dest_buffer[jj + i * APP_IMG_WIDTH] = buffer[(i >> shift) + (j >> shift) * width];
        }
    }
//    uint32_t* dest_buffer = &s_frameBuffer[frameBufferIndex][0][0];
//    for (int i = 0; i < APP_IMG_HEIGHT; i++)
//    {
//        int ii = i * APP_IMG_WIDTH;
//        for (int j = 0; j < APP_IMG_WIDTH; j++)
//        {
//            int jj = APP_IMG_WIDTH - 1 - j;
//            dest_buffer[ii + j] = buffer[(i >> shift) + (jj >> shift) * width];
//        }
//    }
#endif
#if 0
#if PIXELFORMAT==32
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            s_frameBuffer[frameBufferIndex][i][j] = buffer[i * width + j];
#elif PIXELFORMAT==16
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            uint32_t c32 = buffer[i * width + j];
            uint16_t c16 = ((c32 & 0x000000F8) >> 3) + ((c32 & 0x0000FC00) >> 5) + ((c32 & 0x00F80000) >> 8);
            s_frameBuffer[frameBufferIndex][i][j] = c16;
        }
    }
#endif
#endif
#if 1
#if PIXELFORMAT==32
    uint32_t* dest = &s_frameBuffer[frameBufferIndex][0][0];
    uint32_t* src = buffer;
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            *dest++ = *src;
            *dest++ = *src;
            src++;
        }
        src -= width;
        for (int j = 0; j < width; j++)
        {
            *dest++ = *src;
            *dest++ = *src;
            src++;
        }
    }
#elif PIXELFORMAT==16
    uint32_t* dest = (uint32_t*)&s_frameBuffer[frameBufferIndex][0][0];
    uint32_t* src = buffer;
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            uint32_t c32 = *src;
            uint32_t c16 = ((c32 & 0x000000F8) >> 3) + ((c32 & 0x0000FC00) >> 5) + ((c32 & 0x00F80000) >> 8);
            uint32_t c1616 = c16 + (c16 << 16);
            *dest = c1616;
            *(dest + width) = c1616;
            src++;
            dest++;
        }
        dest += width;
    }
#endif
#endif

//    ELCDIF_SetNextBufferAddr(APP_ELCDIF, (uint32_t)s_frameBuffer[frameBufferIndex]);
    APP_ELCDIF->NEXT_BUF = (uint32_t)s_frameBuffer[frameBufferIndex];
    APP_ELCDIF->CUR_BUF = (uint32_t)s_frameBuffer[frameBufferIndex];

//    s_frameDone = false;
//    /* Wait for previous frame complete. */
//    while (!s_frameDone)
//    {
//    }
}

uint32_t* APP_GetFrameBuffer()
{
    frameBufferIndex ^= 1U;

#if PIXELFORMAT==16
#elif PIXELFORMAT==32
    return &s_frameBuffer[frameBufferIndex][0][0];
#endif
}

void APP_SetFrameBuffer(uint32_t* buffer)
{
//    ELCDIF_SetNextBufferAddr(APP_ELCDIF, (uint32_t)buffer);
    APP_ELCDIF->NEXT_BUF = (uint32_t)s_frameBuffer[frameBufferIndex];
    APP_ELCDIF->CUR_BUF = (uint32_t)s_frameBuffer[frameBufferIndex];

//    s_frameDone = false;
//    /* Wait for previous frame complete. */
//    while (!s_frameDone)
//    {
//    }
}
