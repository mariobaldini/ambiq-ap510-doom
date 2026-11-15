#include <stdio.h>

#include "doomgeneric.h"
#include "i_video.h"

#include "board.h"
#include "display.h"
#include "metrics.h"

extern uint8_t __zone_start[];
extern uint8_t __zone_end[];

void DG_Init(void)
{
    display_init();
}

void DG_DrawFrame(void)
{
    uint64_t t0 = board_ticks_us();
    display_timing_t timing;

    if (palette_changed)
    {
        uint32_t xrgb[256];

        for (int i = 0; i < 256; i++)
        {
            xrgb[i] = ((uint32_t)colors[i].r << 16) | ((uint32_t)colors[i].g << 8) | colors[i].b;
        }
        display_set_palette(xrgb);
        palette_changed = false;
    }

    int text_dirty = metrics_update(t0);
    display_present((const uint8_t *)DG_ScreenBuffer, text_dirty, &timing);
    metrics_frame(&timing, (uint32_t)(board_ticks_us() - t0));
}

void DG_SleepMs(uint32_t ms)
{
    uint64_t start = board_ticks_us();

    while (board_ticks_us() - start < (uint64_t)ms * 1000)
    {
    }
    metrics_sleep((uint32_t)(board_ticks_us() - start));
}

uint32_t DG_GetTicksMs(void)
{
    return board_ticks_ms();
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    (void)pressed;
    (void)key;
    return 0;
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}

unsigned char *DG_ZoneBase(int *size)
{
    *size = (int)(__zone_end - __zone_start);
    return __zone_start;
}

void platform_exit(int code, const char *last_line)
{
    char top[24], bot0[24], bot1[24];
    int tics, realtics;
    display_timing_t timing;

    if (sscanf(last_line, "timed %d gametics in %d realtics", &tics, &realtics) == 2 && tics > 0 && realtics > 0)
    {
        unsigned fps_x10 = (unsigned)(((uint64_t)tics * 350 + realtics / 2) / realtics);
        unsigned secs_x10 = (unsigned)(((uint64_t)realtics * 10 + 17) / 35);

        snprintf(top, sizeof(top), "%u.%u FPS", fps_x10 / 10, fps_x10 % 10);
        snprintf(bot0, sizeof(bot0), "%d TICS", tics);
        snprintf(bot1, sizeof(bot1), "IN %u.%u S", secs_x10 / 10, secs_x10 % 10);
        metrics_show_message("TIMEDEMO", top, bot0, bot1);
    }
    else
    {
        snprintf(top, sizeof(top), "EXIT %d", code);
        metrics_show_message("", top, last_line, "");
    }
    display_present((const uint8_t *)DG_ScreenBuffer, 1, &timing);
}
