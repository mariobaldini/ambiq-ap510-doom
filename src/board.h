#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

void board_init(void);
void board_swo_init(void);
void board_timer_init(void);
uint64_t board_ticks_us(void);
uint32_t board_ticks_ms(void);
void board_delay_ms(uint32_t ms);

#endif
