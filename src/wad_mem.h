#ifndef WAD_MEM_H
#define WAD_MEM_H

#include <stdint.h>

int wad_check(void);
int wad_is_name(const char *path);
const uint8_t *wad_data(void);
uint32_t wad_size(void);

#endif
