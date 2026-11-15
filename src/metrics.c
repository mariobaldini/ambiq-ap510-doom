#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "doomtype.h"
#include "i_swap.h"
#include "v_patch.h"
#include "w_wad.h"
#include "z_zone.h"

#include "display.h"
#include "metrics.h"

#define FONT_FIRST '!'
#define FONT_LAST '_'
#define LINE_H 10
#define SPACE_W 4

static patch_t *font[FONT_LAST - FONT_FIRST + 1];
static int font_ready;

static uint64_t window_start;
static uint32_t frames, sleep_us, draw_us, gpu_us, dc_us;

static void font_load(void)
{
    char name[9];

    for (int c = FONT_FIRST; c <= FONT_LAST; c++)
    {
        snprintf(name, sizeof(name), "STCFN%.3d", c);
        int lump = W_CheckNumForName(name);
        font[c - FONT_FIRST] = lump >= 0 ? W_CacheLumpNum(lump, PU_STATIC) : NULL;
    }
    display_set_text_palette(W_CacheLumpName("PLAYPAL", PU_STATIC));
    font_ready = 1;
}

static patch_t *glyph(char c)
{
    c = (char)toupper((unsigned char)c);
    return (c >= FONT_FIRST && c <= FONT_LAST) ? font[c - FONT_FIRST] : NULL;
}

static int text_width(const char *s)
{
    int w = 0;

    for (; *s; s++)
    {
        patch_t *p = glyph(*s);
        w += p ? SHORT(p->width) : SPACE_W;
    }
    return w;
}

static void draw_glyph(uint8_t *dst, patch_t *p, int x0, int y0)
{
    int w = SHORT(p->width);

    for (int x = 0; x < w; x++)
    {
        const byte *col = (const byte *)p + LONG(p->columnofs[x]);

        while (col[0] != 0xff)
        {
            int top = col[0], len = col[1];
            const byte *src = col + 3;

            for (int i = 0; i < len; i++)
            {
                int px = x0 + x, py = y0 + top + i;
                if (px >= 0 && px < DISPLAY_TEXT_W && py >= 0 && py < DISPLAY_TEXT_H)
                {
                    dst[py * DISPLAY_TEXT_W + px] = src[i];
                }
            }
            col += len + 4;
        }
    }
}

static void draw_line(uint8_t *dst, int line, const char *s)
{
    int x = (DISPLAY_TEXT_W - text_width(s)) / 2;
    int y = 1 + line * LINE_H;

    for (; *s; s++)
    {
        patch_t *p = glyph(*s);
        if (p == NULL)
        {
            x += SPACE_W;
            continue;
        }
        draw_glyph(dst, p, x - SHORT(p->leftoffset), y - SHORT(p->topoffset));
        x += SHORT(p->width);
    }
}

static void draw_strip(int strip, const char *l0, const char *l1)
{
    uint8_t *dst = display_text_strip(strip);

    memset(dst, 0, DISPLAY_TEXT_W * DISPLAY_TEXT_H);
    draw_line(dst, 0, l0);
    draw_line(dst, 1, l1);
}

static void tenths(char *out, size_t n, const char *label, uint32_t value_x10, const char *unit)
{
    snprintf(out, n, "%s %lu.%lu%s", label, (unsigned long)(value_x10 / 10), (unsigned long)(value_x10 % 10), unit);
}

int metrics_update(uint64_t now_us)
{
    char top0[32], top1[32], bot0[32], bot1[32], tmp[32];

    if (!font_ready)
    {
        font_load();
        window_start = now_us;
        draw_strip(DISPLAY_TEXT_TOP, "AMBIQ APOLLO510", "DOOM");
        draw_strip(DISPLAY_TEXT_BOTTOM, "", "");
        return 1;
    }

    uint64_t window_us = now_us - window_start;
    if (window_us < 1000000 || frames == 0)
    {
        return 0;
    }

    uint32_t busy_us = window_us > sleep_us ? (uint32_t)window_us - sleep_us : 0;
    uint32_t game_us = busy_us > draw_us ? busy_us - draw_us : 0;
    uint32_t fps_x10 = (uint32_t)((uint64_t)frames * 10000000 / window_us);
    uint32_t load = (uint32_t)((uint64_t)busy_us * 100 / window_us);
    uint32_t game_x10 = game_us / frames / 100;
    uint32_t gpu_x10 = gpu_us / frames / 100;
    uint32_t dc_x10 = dc_us / frames / 100;
    uint32_t zone_free_k = (uint32_t)Z_FreeMemory() / 1024;

    tenths(top0, sizeof(top0), "FPS", fps_x10, "");
    snprintf(top1, sizeof(top1), "CPU LOAD %lu%%", (unsigned long)load);
    tenths(bot0, sizeof(bot0), "GAME", game_x10, "MS");
    tenths(tmp, sizeof(tmp), " GPU", gpu_x10, "MS");
    strncat(bot0, tmp, sizeof(bot0) - strlen(bot0) - 1);
    tenths(bot1, sizeof(bot1), "DSI", dc_x10, "MS");
    snprintf(tmp, sizeof(tmp), " ZONE %luK", (unsigned long)zone_free_k);
    strncat(bot1, tmp, sizeof(bot1) - strlen(bot1) - 1);

    draw_strip(DISPLAY_TEXT_TOP, top0, top1);
    draw_strip(DISPLAY_TEXT_BOTTOM, bot0, bot1);
    printf("%s | %s | %s | %s\n", top0, top1, bot0, bot1);

    window_start = now_us;
    frames = sleep_us = draw_us = gpu_us = dc_us = 0;
    return 1;
}

void metrics_frame(const display_timing_t *timing, uint32_t frame_draw_us)
{
    frames++;
    draw_us += frame_draw_us;
    gpu_us += timing->gpu_us;
    dc_us += timing->dc_wait_us;
}

void metrics_sleep(uint32_t us)
{
    sleep_us += us;
}

void metrics_show_message(const char *top0, const char *top1, const char *bot0, const char *bot1)
{
    if (!font_ready)
    {
        return;
    }
    draw_strip(DISPLAY_TEXT_TOP, top0, top1);
    draw_strip(DISPLAY_TEXT_BOTTOM, bot0, bot1);
}
