#include <stdbool.h>
#include <stdlib.h>

#include "am_mcu_apollo.h"
#include "nema_cmdlist.h"
#include "nema_core.h"
#include "nema_hal.h"
#include "nema_regs.h"

#include "board.h"

#define MAX_PENDING_CL 8
#define RING_BYTES ((MAX_PENDING_CL * 12 + 16) * 4)
#define WAIT_TIMEOUT_MS 1000

extern uint8_t __gpu_pool_start[];
extern uint8_t __gpu_pool_end[];

static uint8_t *pool_next = __gpu_pool_start;
static nema_ringbuffer_t ring;
static volatile int last_cl_id = -1;

// CL ids are 24-bit and wrap, so "reached" is a serial-number comparison, not a plain >=.
static bool cl_reached(int done, int cl_id)
{
    if (cl_id < 0)
    {
        return true;
    }
    if (done < 0)
    {
        return false;
    }
    uint32_t dist = ((uint32_t)done - (uint32_t)cl_id) & SUBMISSION_ID_MASK;
    return dist < (SUBMISSION_ID_MASK + 1u) / 2u;
}

void am_gpu_isr(void)
{
    int id = (int)nema_reg_read(NEMA_CLID);
    int prev;

    do
    {
        nema_reg_write(NEMA_INTERRUPT, 0);
        NVIC_ClearPendingIRQ(GPU_IRQn);
        prev = id;
        id = (int)nema_reg_read(NEMA_CLID);
    } while (id != prev);
    last_cl_id = id;
}

int32_t nema_sys_init(void)
{
    ring.bo = nema_buffer_create(RING_BYTES);
    if (ring.bo.base_virt == NULL)
    {
        return -1;
    }
    ring.offset = 0;
    ring.last_submission_id = -1;

    last_cl_id = -1;
    nema_reg_write(NEMA_INTERRUPT, 0);
    NVIC_ClearPendingIRQ(GPU_IRQn);
    NVIC_EnableIRQ(GPU_IRQn);

    return nema_rb_init(&ring, 1);
}

uint32_t nema_reg_read(uint32_t reg)
{
    return *(volatile uint32_t *)(GPU_BASE + reg);
}

void nema_reg_write(uint32_t reg, uint32_t value)
{
    *(volatile uint32_t *)(GPU_BASE + reg) = value;
}

int nema_wait_irq(void)
{
    int seen = last_cl_id;
    uint32_t start = board_ticks_ms();

    while (last_cl_id == seen && nema_reg_read(NEMA_STATUS) != 0)
    {
        if (board_ticks_ms() - start > WAIT_TIMEOUT_MS)
        {
            return -1;
        }
    }
    return 0;
}

int nema_wait_irq_cl(int cl_id)
{
    uint32_t start = board_ticks_ms();

    while (!cl_reached((int)nema_reg_read(NEMA_CLID), cl_id))
    {
        if (board_ticks_ms() - start > WAIT_TIMEOUT_MS)
        {
            return -1;
        }
    }
    return 0;
}

am_hal_status_e nema_get_cl_status(int32_t cl_id)
{
    if (nema_reg_read(NEMA_STATUS) == 0 || cl_reached((int)nema_reg_read(NEMA_CLID), cl_id))
    {
        return AM_HAL_STATUS_SUCCESS;
    }
    return AM_HAL_STATUS_IN_USE;
}

int nema_get_last_cl_id(void)
{
    return last_cl_id;
}

int nema_get_last_submission_id(void)
{
    return ring.last_submission_id;
}

bool nema_rb_check_full(void)
{
    if (ring.last_submission_id < 0 || nema_reg_read(NEMA_STATUS) == 0)
    {
        return false;
    }
    uint32_t done = (uint32_t)nema_reg_read(NEMA_CLID);
    uint32_t pending = ((uint32_t)ring.last_submission_id - done) & SUBMISSION_ID_MASK;
    return pending >= MAX_PENDING_CL;
}

nema_buffer_t nema_buffer_create_pool(int pool, int size)
{
    nema_buffer_t bo = {.size = 0, .fd = -1, .base_virt = NULL, .base_phys = 0};
    uint32_t span = ((uint32_t)size + 31u) & ~31u;

    (void)pool;
    if (size <= 0 || span > (uint32_t)(__gpu_pool_end - pool_next))
    {
        return bo;
    }
    bo.base_virt = pool_next;
    bo.base_phys = (uintptr_t)pool_next;
    bo.size = size;
    pool_next += span;
    return bo;
}

nema_buffer_t nema_buffer_create(int size)
{
    return nema_buffer_create_pool(NEMA_MEM_POOL_CL, size);
}

// A bump pool: only the most recent buffer can be given back. The port allocates everything once at start-up.
void nema_buffer_destroy(nema_buffer_t *bo)
{
    uint8_t *start = bo->base_virt;

    if (start != NULL && start + ((bo->size + 31) & ~31) == pool_next)
    {
        pool_next = start;
    }
    bo->base_virt = NULL;
    bo->base_phys = 0;
    bo->size = 0;
}

uint32_t nema_pool_used(void)
{
    return (uint32_t)(pool_next - __gpu_pool_start);
}

void *nema_buffer_map(nema_buffer_t *bo)
{
    return bo->base_virt;
}

void nema_buffer_unmap(nema_buffer_t *bo)
{
    (void)bo;
}

uintptr_t nema_buffer_phys(nema_buffer_t *bo)
{
    return bo->base_phys;
}

void nema_buffer_flush(nema_buffer_t *bo)
{
    am_hal_cachectrl_range_t range = {.ui32StartAddr = (uint32_t)bo->base_virt, .ui32Size = (uint32_t)bo->size};

    am_hal_cachectrl_dcache_clean(&range);
}

void nema_buffer_invalidate(nema_buffer_t *bo)
{
    am_hal_cachectrl_range_t range = {.ui32StartAddr = (uint32_t)bo->base_virt, .ui32Size = (uint32_t)bo->size};

    am_hal_cachectrl_dcache_invalidate(&range, false);
}

void *nema_host_malloc(size_t size)
{
    return malloc(size);
}

void nema_host_free(void *ptr)
{
    free(ptr);
}

int nema_mutex_lock(int mutex_id)
{
    (void)mutex_id;
    return 0;
}

int nema_mutex_unlock(int mutex_id)
{
    (void)mutex_id;
    return 0;
}
