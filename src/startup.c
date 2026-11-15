#include <stdint.h>

#include "am_mcu_apollo.h"

extern uint32_t __StackTop[];
extern uint32_t __StackLimit[];
extern uint32_t __data_load[], __data_start[], __data_end[];
extern uint32_t __itcm_load[], __itcm_start[], __itcm_end[];
extern uint32_t __bss_start[], __bss_end[];

int main(void);

void Reset_Handler(void);
void HardFault_Handler(void);
void am_gpu_isr(void);
void am_disp_isr(void);
void am_timer15_isr(void);

void Default_Handler(void)
{
    for (;;)
    {
    }
}

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SecureFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
__attribute__((section(".vectors"), used, aligned(1024)))
void (*const vectors[16 + MAX_IRQn])(void) = {
    [0] = (void (*)(void))__StackTop,
    [1] = Reset_Handler,
    [2] = NMI_Handler,
    [3] = HardFault_Handler,
    [4] = MemManage_Handler,
    [5] = BusFault_Handler,
    [6] = UsageFault_Handler,
    [7] = SecureFault_Handler,
    [11] = SVC_Handler,
    [12] = DebugMon_Handler,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    [16 ... 16 + MAX_IRQn - 1] = Default_Handler,
    [16 + GPU_IRQn] = am_gpu_isr,
    [16 + DC_IRQn] = am_disp_isr,
    [16 + TIMER0_IRQn + AM_HAL_INTERNAL_TIMER_NUM_A] = am_timer15_isr,
};
#pragma GCC diagnostic pop

static void copy_words(uint32_t *dst, const uint32_t *src, const uint32_t *end)
{
    while (dst < end)
    {
        *dst++ = *src++;
    }
}

static void zero_words(uint32_t *dst, const uint32_t *end)
{
    while (dst < end)
    {
        *dst++ = 0;
    }
}

void Reset_Handler(void)
{
    __set_MSPLIM((uint32_t)__StackLimit);
    SCB->VTOR = (uint32_t)vectors;
    SCB->CPACR |= (3u << 20) | (3u << 22);
    __DSB();
    __ISB();

    copy_words(__data_start, __data_load, __data_end);
    copy_words(__itcm_start, __itcm_load, __itcm_end);
    zero_words(__bss_start, __bss_end);

    main();
    for (;;)
    {
    }
}
