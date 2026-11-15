#ifndef PANEL_H
#define PANEL_H

#include <stdint.h>

#define PANEL_WIDTH 468
#define PANEL_HEIGHT 468

int panel_init(void);
void panel_start_frame(uintptr_t fb_phys);
void panel_wait_frame(void);

#endif
