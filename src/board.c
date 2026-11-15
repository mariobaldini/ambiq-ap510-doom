#include "am_mcu_apollo.h"
#include "am_util.h"

#include "board.h"

#define STIMER_HZ 6000000u
#define PIN_SWO 28

static uint32_t last_count;
static uint64_t wrapped;

void board_init(void)
{
    am_hal_pwrctrl_temp_thresh_t thresholds;

    SCB->CCR |= SCB_CCR_LOB_Msk;
    __DSB();
    __ISB();

    am_hal_pwrctrl_low_power_init();
    am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_SIMOBUCK_INIT, NULL);
    am_hal_pwrctrl_temp_update(25.0f, &thresholds);

    am_hal_cachectrl_icache_enable();
    am_hal_cachectrl_dcache_enable(true);
}

void board_swo_init(void)
{
    am_hal_gpio_pincfg_t swo = {0};

    am_hal_itm_parameters_set(AM_HAL_TPIU_BAUD_1M);
    am_hal_itm_enable();
    swo.GP.cfg_b.uFuncSel = AM_HAL_PIN_28_SWO;
    swo.GP.cfg_b.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_0P5X;
    am_hal_gpio_pinconfig(PIN_SWO, swo);
    am_util_stdio_printf_init(am_hal_itm_print);
}

void board_timer_init(void)
{
    am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
    am_hal_stimer_config(AM_HAL_STIMER_HFRC_6MHZ | AM_HAL_STIMER_CFG_RUN | AM_HAL_STIMER_CFG_THAW);
    last_count = am_hal_stimer_counter_get();
}

// The 32-bit counter wraps every ~716 s at 6 MHz; Doom polls far more often than that, so one wrap check per read is enough.
static uint64_t ticks(void)
{
    uint32_t now = am_hal_stimer_counter_get();

    if (now < last_count)
    {
        wrapped += 1ull << 32;
    }
    last_count = now;
    return wrapped + now;
}

uint64_t board_ticks_us(void)
{
    return ticks() / (STIMER_HZ / 1000000u);
}

uint32_t board_ticks_ms(void)
{
    return (uint32_t)(ticks() / (STIMER_HZ / 1000u));
}

void board_delay_ms(uint32_t ms)
{
    uint64_t end = board_ticks_us() + (uint64_t)ms * 1000u;

    while (board_ticks_us() < end)
    {
    }
}
