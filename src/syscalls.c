#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "am_mcu_apollo.h"

#include "wad_mem.h"

#define WAD_FD 3

extern uint8_t __heap_start[];
extern uint8_t __heap_end[];

void platform_exit(int code, const char *last_line);

static uint8_t *heap_top = __heap_start;
static uint32_t wad_pos;
static int wad_open;
static char line[64];
static char last_line[64];
static size_t line_len;

void *_sbrk(ptrdiff_t incr)
{
    uint8_t *prev = heap_top;

    if (incr > __heap_end - heap_top || incr < __heap_start - heap_top)
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_top += incr;
    return prev;
}

static void keep_line(char c)
{
    if (c == '\n')
    {
        if (line_len > 0)
        {
            line[line_len] = '\0';
            memcpy(last_line, line, line_len + 1);
            line_len = 0;
        }
    }
    else if (line_len < sizeof(line) - 1)
    {
        line[line_len++] = c;
    }
}

int _write(int fd, const char *buf, int len)
{
    if (fd != 1 && fd != 2)
    {
        errno = EBADF;
        return -1;
    }
    for (int i = 0; i < len; i++)
    {
        if (buf[i] == '\n')
        {
            ITM_SendChar('\r');
        }
        ITM_SendChar((uint32_t)buf[i]);
        keep_line(buf[i]);
    }
    return len;
}

int _open(const char *path, int flags, ...)
{
    if (wad_is_name(path) && (flags & O_ACCMODE) == O_RDONLY && !wad_open)
    {
        wad_open = 1;
        wad_pos = 0;
        return WAD_FD;
    }
    errno = ENOENT;
    return -1;
}

int _close(int fd)
{
    if (fd == WAD_FD && wad_open)
    {
        wad_open = 0;
        return 0;
    }
    return 0;
}

int _read(int fd, char *buf, int len)
{
    if (fd != WAD_FD || !wad_open)
    {
        errno = EBADF;
        return -1;
    }
    uint32_t left = wad_size() - wad_pos;
    if ((uint32_t)len > left)
    {
        len = (int)left;
    }
    memcpy(buf, wad_data() + wad_pos, (size_t)len);
    wad_pos += (uint32_t)len;
    return len;
}

int _lseek(int fd, int offset, int whence)
{
    if (fd != WAD_FD || !wad_open)
    {
        errno = ESPIPE;
        return -1;
    }
    int64_t pos = whence == SEEK_SET ? offset : whence == SEEK_CUR ? (int64_t)wad_pos + offset : (int64_t)wad_size() + offset;
    if (pos < 0 || pos > wad_size())
    {
        errno = EINVAL;
        return -1;
    }
    wad_pos = (uint32_t)pos;
    return (int)pos;
}

int _fstat(int fd, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    if (fd == WAD_FD && wad_open)
    {
        st->st_mode = S_IFREG | 0444;
        st->st_size = wad_size();
        return 0;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd)
{
    return fd >= 0 && fd <= 2;
}

int mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    errno = EROFS;
    return -1;
}

int _unlink(const char *path)
{
    (void)path;
    errno = EROFS;
    return -1;
}

int _link(const char *old, const char *new)
{
    (void)old;
    (void)new;
    errno = EROFS;
    return -1;
}

void _exit(int code)
{
    keep_line('\n');
    platform_exit(code, last_line);
    for (;;)
    {
        __WFI();
    }
}
