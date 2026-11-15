#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "w_file.h"

#include "wad_mem.h"

#define WAD_NAME "doom1.wad"
#define WAD_MAGIC 0x44415744u

typedef struct
{
    uint32_t magic;
    uint32_t length;
    uint32_t crc32;
    uint32_t reserved;
} wad_header_t;

extern const uint8_t __wad_start[];
extern const uint8_t __wad_end[];

extern wad_file_class_t mem_wad_file;

static int wad_ok;
static wad_file_t wad_file;

static uint32_t crc32_calc(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0xffffffffu;

    while (n--)
    {
        crc ^= *p++;
        for (int k = 0; k < 8; k++)
        {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

int wad_check(void)
{
    const wad_header_t *h = (const wad_header_t *)__wad_start;
    uint32_t room = (uint32_t)(__wad_end - __wad_start) - sizeof(*h);

    if (h->magic != WAD_MAGIC)
    {
        printf("WAD: no prepared WAD at %p (magic %08lx), run make flash\n", (void *)__wad_start, (unsigned long)h->magic);
        return -1;
    }
    if (h->length > room)
    {
        printf("WAD: length %lu exceeds region %lu\n", (unsigned long)h->length, (unsigned long)room);
        return -1;
    }
    uint32_t crc = crc32_calc(wad_data(), h->length);
    if (crc != h->crc32)
    {
        printf("WAD: CRC %08lx, expected %08lx\n", (unsigned long)crc, (unsigned long)h->crc32);
        return -1;
    }
    printf("WAD: %lu bytes at %p, CRC ok\n", (unsigned long)h->length, (void *)wad_data());
    wad_ok = 1;
    return 0;
}

int wad_is_name(const char *path)
{
    const char *base = strrchr(path, '/');

    return wad_ok && strcasecmp(base ? base + 1 : path, WAD_NAME) == 0;
}

const uint8_t *wad_data(void)
{
    return __wad_start + sizeof(wad_header_t);
}

uint32_t wad_size(void)
{
    return ((const wad_header_t *)__wad_start)->length;
}

static wad_file_t *mem_open(char *path)
{
    if (!wad_is_name(path))
    {
        return NULL;
    }
    wad_file.file_class = &mem_wad_file;
    wad_file.mapped = (byte *)wad_data();
    wad_file.length = wad_size();
    return &wad_file;
}

static void mem_close(wad_file_t *file)
{
    (void)file;
}

static size_t mem_read(wad_file_t *file, unsigned int offset, void *buffer, size_t len)
{
    if (offset >= file->length)
    {
        return 0;
    }
    if (len > file->length - offset)
    {
        len = file->length - offset;
    }
    memcpy(buffer, file->mapped + offset, len);
    return len;
}

wad_file_class_t mem_wad_file = {mem_open, mem_close, mem_read};
