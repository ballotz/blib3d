/*
 * Copyright 2019 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_lcdifv2.h"
#include "lcdifv2_support.h"
#include "fsl_debug_console.h"

#include "fsl_soc_src.h"
#include "pin_mux.h"
#include "board.h"

#include "draw2d.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DEMO_CORE_ID 0

#define DEMO_BYTE_PER_PIXEL 2U

#define DEMO_IMG_HEIGHT         DEMO_PANEL_HEIGHT
#define DEMO_IMG_WIDTH          DEMO_PANEL_WIDTH
#define DEMO_IMG_BYTES_PER_LINE (DEMO_PANEL_WIDTH * DEMO_BYTE_PER_PIXEL)

/* Use layer 0 in this example. */
#define DEMO_LCDIFV2_LAYER 0

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

static uint16_t* s_frameBufferAddr[2] = {(uint16_t*)DEMO_FB0_ADDR, (uint16_t*)DEMO_FB1_ADDR};

static volatile bool s_frameDone = false;

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

void DEMO_LCDIFV2_IRQHandler(void)
{
    uint32_t intStatus;

    intStatus = LCDIFV2_GetInterruptStatus(DEMO_LCDIFV2, DEMO_CORE_ID);
    LCDIFV2_ClearInterruptStatus(DEMO_LCDIFV2, DEMO_CORE_ID, intStatus);

    if (0 != (intStatus & kLCDIFV2_VerticalBlankingInterrupt))
    {
        s_frameDone = true;
    }
    SDK_ISR_EXIT_BARRIER;
}

}

void DEMO_FillFrameBuffer(uint16_t* frameBuffer)
{
    /* Foreground color. */
    static uint8_t fgColorIndex          = 0U;
    static const uint16_t fgColorTable[] = {0x001FU, 0x07E0U, 0x07FFU, 0xF800U, 0xF81FU, 0xFFE0U, 0xFFFFU};
    uint16_t fgColor                     = fgColorTable[fgColorIndex];

    /* Position of the foreground rectangle. */
    static uint16_t upperLeftX  = 0;
    static uint16_t upperLeftY  = 0;
    static uint16_t lowerRightX = (DEMO_IMG_WIDTH - 1U) / 2U;
    static uint16_t lowerRightY = (DEMO_IMG_HEIGHT - 1U) / 2U;

    static int8_t incX = 1;
    static int8_t incY = 1;

    /* Change color in next forame or not. */
    static bool changeColor = false;

    uint32_t i, j;

    /* Set background color to black. */
    //memset(frameBuffer, 0, DEMO_IMG_WIDTH * DEMO_IMG_HEIGHT * DEMO_BYTE_PER_PIXEL);
    for (i = 0; i < DEMO_IMG_HEIGHT; i++)
    {
        for (j = 0; j < DEMO_IMG_WIDTH; j++)
        {
            frameBuffer[i * DEMO_IMG_WIDTH + j] = 0;
        }
    }

    /* Foreground color. */
    for (i = upperLeftY; i < lowerRightY; i++)
    {
        for (j = upperLeftX; j < lowerRightX; j++)
        {
            frameBuffer[i * DEMO_IMG_WIDTH + j] = fgColor;
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
    else if (DEMO_IMG_WIDTH - 1 == lowerRightX)
    {
        incX        = -1;
        changeColor = true;
    }

    if (0U == upperLeftY)
    {
        incY        = 1;
        changeColor = true;
    }
    else if (DEMO_IMG_HEIGHT - 1 == lowerRightY)
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

void DEMO_LCDIFV2_Init(void)
{
    const lcdifv2_display_config_t lcdifv2Config = {
        .panelWidth    = DEMO_PANEL_WIDTH,
        .panelHeight   = DEMO_PANEL_HEIGHT,
        .hsw           = DEMO_HSW,
        .hfp           = DEMO_HFP,
        .hbp           = DEMO_HBP,
        .vsw           = DEMO_VSW,
        .vfp           = DEMO_VFP,
        .vbp           = DEMO_VBP,
        .polarityFlags = DEMO_POL_FLAGS,
        .lineOrder     = kLCDIFV2_LineOrderRGB,
    };

    const lcdifv2_buffer_config_t fbConfig = {
        .strideBytes = DEMO_IMG_WIDTH * DEMO_BYTE_PER_PIXEL,
        .pixelFormat = kLCDIFV2_PixelFormatRGB565,
    };

    if (kStatus_Success != BOARD_InitDisplayInterface())
    {
        PRINTF("Display interface initialize failed\r\n");

        while (1)
        {
        }
    }

    LCDIFV2_Init(DEMO_LCDIFV2);

    LCDIFV2_SetDisplayConfig(DEMO_LCDIFV2, &lcdifv2Config);

    LCDIFV2_EnableDisplay(DEMO_LCDIFV2, true);

    LCDIFV2_SetLayerBufferConfig(DEMO_LCDIFV2, DEMO_LCDIFV2_LAYER, &fbConfig);

    LCDIFV2_SetLayerSize(DEMO_LCDIFV2, DEMO_LCDIFV2_LAYER, DEMO_IMG_WIDTH, DEMO_IMG_HEIGHT);

    LCDIFV2_SetLayerOffset(DEMO_LCDIFV2, DEMO_LCDIFV2_LAYER, 0, 0);

    NVIC_EnableIRQ(DEMO_LCDIFV2_IRQn);

    LCDIFV2_EnableInterrupts(DEMO_LCDIFV2, DEMO_CORE_ID, kLCDIFV2_VerticalBlankingInterrupt);
}

void DEMO_LCDIFV2_RGB(void)
{
    uint32_t frameBufferIndex = 0;

    draw2d_interval_type interval;
    float interval_dt, fps = 0, new_fps, ms = 0, new_ms, count = 0;
    draw2d_rect_type rect;
    char string[128];

    draw2d_interval_init();
    draw2d_interval_reset(&interval);

    DEMO_FillFrameBuffer(s_frameBufferAddr[frameBufferIndex]);

    LCDIFV2_EnableLayer(DEMO_LCDIFV2, DEMO_LCDIFV2_LAYER, true);

    while (1)
    {
        LCDIFV2_SetLayerBufferAddr(DEMO_LCDIFV2, DEMO_LCDIFV2_LAYER, (uint32_t)s_frameBufferAddr[frameBufferIndex]);
        LCDIFV2_TriggerLayerShadowLoad(DEMO_LCDIFV2, DEMO_LCDIFV2_LAYER);

        /*
         * Wait for previous frame complete.
         * New frame buffer configuration load at the next VSYNC.
         */
//        s_frameDone = false;
//        while (!s_frameDone)
//        {
//        }

        frameBufferIndex ^= 1U;
        DEMO_FillFrameBuffer(s_frameBufferAddr[frameBufferIndex]);

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
        snprintf(string, sizeof(string), "%ix%i\nfps %i\nms %i", DEMO_IMG_WIDTH, DEMO_IMG_HEIGHT, (int)fps, (int)ms);
        rect.data = (uint16_t*)s_frameBuffer[frameBufferIndex];
        rect.height = DEMO_IMG_HEIGHT;
        rect.width = DEMO_IMG_WIDTH;
        rect.stride = DEMO_IMG_WIDTH;
        draw2d_draw_string_16(&rect, 0, 0, string, 0x8410);
    }
}

/*!
 * @brief Main function
 */
int main(void)
{
    BOARD_ConfigMPU();
    BOARD_BootClockRUN();
    BOARD_ResetDisplayMix();
    BOARD_InitLpuartPins();
    BOARD_InitMipiPanelPins();
    BOARD_InitDebugConsole();
    BOARD_InitLcdifClock();

    PRINTF("LCDIF v2 RGB565 example start...\r\n");

    DEMO_LCDIFV2_Init();

    DEMO_LCDIFV2_RGB();

    while (1)
    {
    }
}
