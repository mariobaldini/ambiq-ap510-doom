#include <stdio.h>
#include <string.h>

#include "am_mcu_apollo.h"
#include "nema_core.h"
#include "nema_utils.h"

#include "board.h"
#include "display.h"
#include "panel.h"

// nema_sys_defs.h defines printf away to nothing.
#undef printf

#define FB_COUNT 2
#define TEXT_SCALE 2

static nema_buffer_t fb[FB_COUNT];
static nema_buffer_t palette;
static nema_buffer_t text_palette;
static nema_buffer_t text[DISPLAY_TEXT_STRIPS];
static nema_cmdlist_t cl;
static const int fb_w = PANEL_WIDTH, fb_h = PANEL_HEIGHT;
static int back;
static int fb_cleared[FB_COUNT];
static int transfer_pending;

static nema_buffer_t gpu_alloc(int size)
{
    nema_buffer_t bo = nema_buffer_create(size);

    if (bo.base_virt == NULL)
    {
        printf("display: GPU pool exhausted allocating %d bytes\n", size);
        for (;;)
        {
        }
    }
    memset(bo.base_virt, 0, size);
    nema_buffer_flush(&bo);
    return bo;
}

void display_init(void)
{
    int err;

    am_hal_interrupt_master_enable();
    err = panel_init();
    if (err != 0)
    {
        printf("display: panel init failed (%d)\n", err);
    }
    am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_GFX);
    if (nema_init() != 0)
    {
        printf("display: nema_init failed\n");
    }

    for (int i = 0; i < FB_COUNT; i++)
    {
        fb[i] = gpu_alloc(fb_w * fb_h * 3);
    }
    palette = gpu_alloc(256 * 4);
    text_palette = gpu_alloc(256 * 4);
    for (int i = 0; i < DISPLAY_TEXT_STRIPS; i++)
    {
        text[i] = gpu_alloc(DISPLAY_TEXT_W * DISPLAY_TEXT_H);
    }
    cl = nema_cl_create();
    printf("display: %dx%d RGB888, %d framebuffers\n", fb_w, fb_h, FB_COUNT);
}

uint8_t *display_text_strip(int strip)
{
    return text[strip].base_virt;
}

// The GPU reads the 256-entry LUT as a 16x16 texture, transposed: index i is fetched from entry (i & 15) * 16 + (i >> 4).
static int lut_slot(int i)
{
    return ((i & 15) << 4) | (i >> 4);
}

void display_set_palette(const uint32_t xrgb[256])
{
    uint32_t *out = palette.base_virt;

    for (int i = 0; i < 256; i++)
    {
        out[lut_slot(i)] = nema_rgba((xrgb[i] >> 16) & 0xff, (xrgb[i] >> 8) & 0xff, xrgb[i] & 0xff, 0xff);
    }
    nema_buffer_flush(&palette);
}

void display_set_text_palette(const uint8_t rgb[256 * 3])
{
    uint32_t *out = text_palette.base_virt;

    for (int i = 0; i < 256; i++)
    {
        out[lut_slot(i)] = nema_rgba(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2], 0xff);
    }
    nema_buffer_flush(&text_palette);
}

static void blit_lut(uintptr_t src, int w, int h, uintptr_t pal, int x, int y, int dw, int dh)
{
    nema_bind_lut_tex(src, w, h, NEMA_L8, -1, NEMA_FILTER_PS, pal, NEMA_RGBA8888);
    nema_blit_quad_fit(x, y, x + dw, y, x + dw, y + dh, x, y + dh);
}

void display_present(const uint8_t *screen, int text_dirty, display_timing_t *timing)
{
    // 4:3 like the original monitors, and small enough that its corners stay inside the round panel.
    const int game_w = 360, game_h = 270;
    const int game_x = (fb_w - game_w) / 2, game_y = (fb_h - game_h) / 2;
    const int text_w = DISPLAY_TEXT_W * TEXT_SCALE, text_h = DISPLAY_TEXT_H * TEXT_SCALE;
    const int text_x = (fb_w - text_w) / 2;
    am_hal_cachectrl_range_t range = {.ui32StartAddr = (uint32_t)screen,
                                      .ui32Size = DISPLAY_SCREEN_W * DISPLAY_SCREEN_H};
    uint64_t t0 = board_ticks_us();

    am_hal_cachectrl_dcache_clean(&range);
    if (text_dirty)
    {
        for (int i = 0; i < DISPLAY_TEXT_STRIPS; i++)
        {
            nema_buffer_flush(&text[i]);
        }
    }

    nema_cl_rewind(&cl);
    nema_cl_bind(&cl);
    nema_bind_dst_tex(fb[back].base_phys, fb_w, fb_h, NEMA_RGB24, -1);
    nema_set_clip(0, 0, fb_w, fb_h);
    if (!fb_cleared[back])
    {
        nema_clear(nema_rgba(0, 0, 0, 0xff));
        fb_cleared[back] = 1;
    }
    nema_set_blend_blit(NEMA_BL_SRC | NEMA_BLOP_LUT);
    blit_lut((uintptr_t)screen, DISPLAY_SCREEN_W, DISPLAY_SCREEN_H, palette.base_phys, game_x, game_y, game_w, game_h);
    blit_lut(text[DISPLAY_TEXT_TOP].base_phys, DISPLAY_TEXT_W, DISPLAY_TEXT_H, text_palette.base_phys,
             text_x, game_y - text_h - 4, text_w, text_h);
    blit_lut(text[DISPLAY_TEXT_BOTTOM].base_phys, DISPLAY_TEXT_W, DISPLAY_TEXT_H, text_palette.base_phys,
             text_x, game_y + game_h + 4, text_w, text_h);
    nema_cl_submit(&cl);
    nema_cl_wait(&cl);

    uint64_t t1 = board_ticks_us();
    if (transfer_pending)
    {
        panel_wait_frame();
    }
    uint64_t t2 = board_ticks_us();

    panel_start_frame(fb[back].base_phys);
    transfer_pending = 1;
    back = (back + 1) % FB_COUNT;

    timing->gpu_us = (uint32_t)(t1 - t0);
    timing->dc_wait_us = (uint32_t)(t2 - t1);
}
