#include <stdbool.h>
#include <stddef.h>

#include "am_mcu_apollo.h"
#include "nema_dc.h"
#include "nema_dc_hal.h"
#include "nema_dc_mipi.h"
#include "nema_sys_defs.h"

#include "board.h"
#include "panel.h"

#define PIN_VDD18_EN 58
#define PIN_PANEL_RESET 63

#define DSI_LANES 1
#define DSI_DBI_WIDTH 16

#define DCS_EXIT_SLEEP 0x11
#define DCS_DISPLAY_ON 0x29
#define DCS_COLUMN_ADDRESS 0x2a
#define DCS_PAGE_ADDRESS 0x2b
#define DCS_ADDRESS_MODE 0x36
#define DCS_PIXEL_FORMAT 0x3a
#define DCS_PIXEL_FORMAT_24BPP 0x77

static nemadc_layer_t layer;

static void pin_output(uint32_t pin, bool high)
{
    am_hal_gpio_pinconfig(pin, am_hal_gpio_pincfg_output);
    am_hal_gpio_state_write(pin, high ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
}

static void vdd18_switch(bool on)
{
    pin_output(PIN_VDD18_EN, on);
}

static void panel_reset(void)
{
    pin_output(PIN_PANEL_RESET, true);
    board_delay_ms(5);
    am_hal_gpio_state_write(PIN_PANEL_RESET, AM_HAL_GPIO_OUTPUT_CLEAR);
    board_delay_ms(20);
    am_hal_gpio_state_write(PIN_PANEL_RESET, AM_HAL_GPIO_OUTPUT_SET);
    board_delay_ms(150);
}

static uint32_t dcs(uint8_t cmd, uint8_t *params, uint8_t len)
{
    return nemadc_mipi_cmd_write(cmd, params, len, true, false);
}

static uint32_t generic(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};

    return nemadc_mipi_cmd_write(0, buf, sizeof(buf), false, false);
}

static uint32_t set_window(uint8_t cmd, uint16_t first, uint16_t last)
{
    uint8_t range[4] = {first >> 8, first & 0xff, last >> 8, last & 0xff};

    return dcs(cmd, range, sizeof(range));
}

// The CO5300 manufacturer pages (0xFE selects one) have no public register names.
static uint32_t co5300_configure(void)
{
    static const uint8_t vendor_page1[][2] = {{0xfe, 0x01}, {0x0a, 0xf8}, {0xfe, 0x00}};
    static const uint8_t vendor_page2[][2] = {{0xfe, 0x20}, {0xf4, 0x5a}, {0xf5, 0x59}, {0xfe, 0x80}, {0x00, 0xf8}, {0xfe, 0x00}};
    uint8_t format = DCS_PIXEL_FORMAT_24BPP;
    uint8_t address_mode = 0;
    uint32_t err = 0;

    err |= generic(0x51, 0xff);
    err |= dcs(DCS_PIXEL_FORMAT, &format, 1);
    for (size_t i = 0; i < sizeof(vendor_page1) / sizeof(vendor_page1[0]); i++)
    {
        err |= generic(vendor_page1[i][0], vendor_page1[i][1]);
    }
    err |= dcs(DCS_ADDRESS_MODE, &address_mode, 1);
    for (size_t i = 0; i < sizeof(vendor_page2) / sizeof(vendor_page2[0]); i++)
    {
        err |= generic(vendor_page2[i][0], vendor_page2[i][1]);
    }
    err |= dcs(DCS_EXIT_SLEEP, NULL, 0);
    board_delay_ms(120);
    err |= dcs(DCS_DISPLAY_ON, NULL, 0);
    err |= set_window(DCS_COLUMN_ADDRESS, 0, PANEL_WIDTH - 1);
    err |= set_window(DCS_PAGE_ADDRESS, 0, PANEL_HEIGHT - 1);
    return err;
}

int panel_init(void)
{
    nemadc_initial_config_t dc = {0};

    am_hal_dsi_register_external_vdd18_callback(vdd18_switch);
    am_hal_dsi_init();
    am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_DISP);
    if (nemadc_init() != 0)
    {
        return -1;
    }
    am_hal_dsi_para_config(DSI_LANES, DSI_DBI_WIDTH, AM_HAL_DSI_FREQ_TRIM_X20, true);

    dc.eInterface = DISP_INTERFACE_DBIDSI;
    dc.ui16ResX = PANEL_WIDTH;
    dc.ui16ResY = PANEL_HEIGHT;
    dc.ui32FrontPorchX = 1;
    dc.ui32FrontPorchY = 1;
    dc.ui32BackPorchX = 1;
    dc.ui32BackPorchY = 1;
    dc.ui32BlankingX = 1;
    dc.ui32BlankingY = 1;
    dc.ui32PixelFormat = MIPICFG_16RGB888_OPT0;
    dc.bTEEnable = false;
    nemadc_configure(&dc);

    panel_reset();
    if (co5300_configure() != 0)
    {
        return -2;
    }

    layer.resx = PANEL_WIDTH;
    layer.resy = PANEL_HEIGHT;
    layer.sizex = PANEL_WIDTH;
    layer.sizey = PANEL_HEIGHT;
    layer.format = NEMADC_RGB24;
    layer.blendmode = NEMADC_BL_SRC;
    layer.stride = nemadc_stride_size(NEMADC_RGB24, PANEL_WIDTH);
    layer.alpha = 0xff;
    return 0;
}

void panel_start_frame(uintptr_t fb_phys)
{
    layer.baseaddr_phys = fb_phys;
    layer.baseaddr_virt = (void *)fb_phys;
    nemadc_set_layer(0, &layer);
    nemadc_transfer_frame_prepare(false);
    nemadc_transfer_frame_launch();
}

void panel_wait_frame(void)
{
    nemadc_wait_vsync();
}
