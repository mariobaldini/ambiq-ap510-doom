#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>

#include "display.h"

int metrics_update(uint64_t now_us);
void metrics_frame(const display_timing_t *timing, uint32_t draw_us);
void metrics_sleep(uint32_t us);
void metrics_show_message(const char *top0, const char *top1, const char *bot0, const char *bot1);

#endif
