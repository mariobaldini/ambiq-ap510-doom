#include <stdio.h>
#include <string.h>

#include "am_mcu_apollo.h"
#include "am_util.h"

#include "board.h"
#include "build_config.h"
#include "wad_mem.h"

extern const uint8_t __wad_start[];
extern const uint8_t __wad_end[];
extern uint8_t __gpu_pool_start[];
extern uint8_t __gpu_pool_end[];

void doomgeneric_Create(int argc, char **argv);
void doomgeneric_Tick(void);

#define MPU_ATTR_CACHED 0
#define MPU_ATTR_UNCACHED 1

static void mpu_setup(void)
{
    static am_hal_mpu_attr_t attrs[] =
    {
        {
            .ui8AttrIndex = MPU_ATTR_CACHED,
            .bNormalMem = true,
            .sOuterAttr = {.bNonTransient = false, .bWriteBack = true, .bReadAllocate = true, .bWriteAllocate = false},
            .sInnerAttr = {.bNonTransient = false, .bWriteBack = true, .bReadAllocate = true, .bWriteAllocate = false},
            .eDeviceAttr = DEV_nGnRnE,
        },
        // Bits 0b0100 (write-back set, no allocation) are how ARMv8-M encodes Normal non-cacheable memory.
        {
            .ui8AttrIndex = MPU_ATTR_UNCACHED,
            .bNormalMem = true,
            .sOuterAttr = {.bNonTransient = false, .bWriteBack = true, .bReadAllocate = false, .bWriteAllocate = false},
            .sInnerAttr = {.bNonTransient = false, .bWriteBack = true, .bReadAllocate = false, .bWriteAllocate = false},
            .eDeviceAttr = DEV_nGnRnE,
        },
    };
    am_hal_mpu_region_config_t regions[] =
    {
        // A stray engine write into a mapped lump must fault loudly instead of corrupting or silently missing MRAM.
        {
            .ui32RegionNumber = 0,
            .ui32BaseAddress = (uint32_t)__wad_start,
            .ui32LimitAddress = (uint32_t)__wad_end - 1,
            .eShareable = NON_SHARE,
            .eAccessPermission = RO_NONPRIV,
            .bExecuteNever = true,
            .ui32AttrIndex = MPU_ATTR_CACHED,
            .bEnable = true,
        },
        // Command lists, the ring buffer and textures are written by the CPU and read by the GPU with no cache maintenance.
        {
            .ui32RegionNumber = 1,
            .ui32BaseAddress = (uint32_t)__gpu_pool_start,
            .ui32LimitAddress = (uint32_t)__gpu_pool_end - 1,
            .eShareable = NON_SHARE,
            .eAccessPermission = RW_NONPRIV,
            .bExecuteNever = true,
            .ui32AttrIndex = MPU_ATTR_UNCACHED,
            .bEnable = true,
        },
    };

    am_hal_mpu_attr_configure(attrs, sizeof(attrs) / sizeof(attrs[0]));
    am_hal_mpu_region_clear();
    am_hal_mpu_region_configure(regions, sizeof(regions) / sizeof(regions[0]));
    am_hal_cachectrl_dcache_invalidate(NULL, true);
    am_hal_mpu_enable(true, true);
}

static void check_mram_banks(void)
{
    am_hal_pwrctrl_mcu_memory_config_t cfg;

    am_hal_pwrctrl_mcu_memory_config_get(&cfg);
    if (cfg.eNVMCfg != AM_HAL_PWRCTRL_NVM0_AND_NVM1)
    {
        cfg.eNVMCfg = AM_HAL_PWRCTRL_NVM0_AND_NVM1;
        am_hal_pwrctrl_mcu_memory_config(&cfg);
        printf("MRAM bank 1 was off, powered on\n");
    }
}

static int split_args(char *line, char **argv, int max)
{
    int argc = 0;
    char *tok = strtok(line, " ");

    while (tok != NULL && argc < max)
    {
        argv[argc++] = tok;
        tok = strtok(NULL, " ");
    }
    return argc;
}

int main(void)
{
    static char extra[] = DOOM_ARGS;
    static char *argv[32] = {"doom", "-iwad", "doom1.wad", "-mmap", "-nogui"};
    int argc = 5;

    board_init();
    board_swo_init();

    printf("\nDoom on Apollo510 EVB, build %s\n", BUILD_ID);
    printf("CPU HP mode: %s\n",
           am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE) == AM_HAL_STATUS_SUCCESS ? "on" : "FAILED");
    printf("GPU HP mode: %s\n",
           am_hal_pwrctrl_gpu_mode_select(AM_HAL_PWRCTRL_GPU_MODE_HIGH_PERFORMANCE) == AM_HAL_STATUS_SUCCESS ? "on" : "FAILED");

    check_mram_banks();
    mpu_setup();
    board_timer_init();

    if (wad_check() != 0)
    {
        for (;;)
        {
        }
    }

    argc += split_args(extra, argv + argc, (int)(sizeof(argv) / sizeof(argv[0])) - argc - 1);
    argv[argc] = NULL;

    doomgeneric_Create(argc, argv);
    for (;;)
    {
        doomgeneric_Tick();
    }
}
