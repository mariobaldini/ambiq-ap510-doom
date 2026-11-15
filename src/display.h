#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define DISPLAY_SCREEN_W 320
#define DISPLAY_SCREEN_H 200

#define DISPLAY_TEXT_W 160
#define DISPLAY_TEXT_H 20

enum { DISPLAY_TEXT_TOP, DISPLAY_TEXT_BOTTOM, DISPLAY_TEXT_STRIPS };

typedef struct
{
    uint32_t gpu_us;
    uint32_t dc_wait_us;
} display_timing_t;

void display_init(void);
uint8_t *display_text_strip(int strip);
void display_set_palette(const uint32_t xrgb[256]);
void display_set_text_palette(const uint8_t rgb[256 * 3]);
void display_present(const uint8_t *screen, int text_dirty, display_timing_t *timing);

#endif
