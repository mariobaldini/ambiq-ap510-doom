# Firmware build. Runs inside the build container (see Makefile); every path is relative to the repo root.

BUILD := build
OBJ   := $(BUILD)/obj
GEN   := $(BUILD)/gen
DL    := $(BUILD)/dl

TOOLCHAIN_DIR    := tools/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi
TOOLCHAIN_TAR    := tools/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz
TOOLCHAIN_URL    := https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz
TOOLCHAIN_SHA256 := 6cd1bbc1d9ae57312bcd169ae283153a9572bd6a8e4eeae2fedfbc33b115fdbb

WAD_TGZ_URLS   := https://deb.debian.org/debian/pool/non-free/d/doom-wad-shareware/doom-wad-shareware_1.9.fixed.orig.tar.gz \
                  https://snapshot.debian.org/file/a2d5964a84235eb60119f213b2cdf7f748672744
WAD_TGZ_SHA256 := e02c8b5e01be7373d4c53f82556118e2aaaf8f83fa2af5eee1efadf9c55c4eb1
WAD_SHA256     := 1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771

CROSS   := $(TOOLCHAIN_DIR)/bin/arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
NM      := $(CROSS)nm
SIZE    := $(CROSS)size

HAL    := ambiq/hal_ambiq
CMSIS  := ambiq/cmsis
NEMA   := ambiq/ambiqhal_ambiq/components/graphics/NemaGFX_SDK
DOOM   := doom/doomgeneric

DOOM_ARGS ?=
JLINK_SN  ?=
JLINK_DEVICE := AP510NFA-CBR
APP_ADDR     := 0x00410000
SWO_CPUFREQ  := 48000000
SWO_FREQ     := 1000000

DOOM_SRC := $(addprefix $(DOOM)/,$(addsuffix .c,\
    dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game \
    hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat \
    m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map \
    p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data \
    r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video \
    wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc i_input i_video doomgeneric))

PORT_SRC := $(wildcard src/*.c)

HAL_SRC := $(addprefix $(HAL)/mcu/apollo510/hal/,\
    am_hal_global.c am_hal_pwrctrl.c am_hal_clkmgr.c am_hal_security.c am_hal_spotmgr.c am_hal_spotmgr_pcm0_7.c \
    am_hal_spotmgr_pcm2_0.c am_hal_spotmgr_pcm2_1.c am_hal_spotmgr_pcm2_2.c am_hal_stimer.c am_hal_queue.c \
    am_hal_info.c am_hal_gpio.c am_hal_dcu.c) \
  $(addprefix $(HAL)/mcu/apollo510/hal/mcu/,\
    am_hal_bootrom_helper.c am_hal_cachectrl.c am_hal_clkgen.c am_hal_debug.c am_hal_interrupt.c am_hal_rtc.c \
    am_hal_utils.c am_hal_mcuctrl.c am_hal_mram.c am_hal_sysctrl.c am_hal_syspll.c am_hal_itm.c am_hal_tpiu.c \
    am_hal_mpu.c am_hal_dsi.c) \
  $(addprefix $(HAL)/utils/,am_util_stdio.c am_util_delay.c am_util_faultisr.c)

NEMA_PORT_SRC := $(NEMA)/port/nema_dc_hal.c

LIBS := $(NEMA)/libraries/lib_nema_apollo5x_nemagfx.a

ARCH := -mthumb -mcpu=cortex-m55 -mfpu=auto -mfloat-abi=hard
BASE_CFLAGS := $(ARCH) -O2 -g -std=gnu99 -ffunction-sections -fdata-sections -fno-common -MMD -MP

AMBIQ_CFLAGS := -DPART_apollo510 -DAM_PACKAGE_BGA -DAM_PART_APOLLO510 -DBAREMETAL -DNEMA_PLATFORM=apollo510_nemagfx -Dgcc \
    -I$(HAL)/mcu -I$(HAL)/mcu/apollo510 -I$(HAL)/mcu/apollo510/hal -I$(HAL)/mcu/apollo510/hal/mcu \
    -I$(HAL)/CMSIS/AmbiqMicro/Include -I$(HAL)/utils -I$(CMSIS)/CMSIS/Core/Include \
    -I$(NEMA)/headers/include/tsi/NemaGFX -I$(NEMA)/headers/include/tsi/NemaDC -I$(NEMA)/headers/include/tsi/NemaVG \
    -I$(NEMA)/headers/include/tsi/common -I$(NEMA)/headers/NemaDC -I$(NEMA)/headers/NemaGFX \
    -I$(NEMA)/headers/NemaGFX/Nema -I$(NEMA)/headers/common/mem -I$(NEMA)/port

# am_hal_global.c declares its weak newlib stubs as int f(void), which gcc flags against the _exit builtin.
VENDOR_CFLAGS := $(AMBIQ_CFLAGS) -Wno-builtin-declaration-mismatch

DOOM_CFLAGS := -DCMAP256 -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -DDG_STATIC_ZONE -DDG_MEM_WAD -I$(DOOM)

PORT_CFLAGS := -Wall -Wextra -Wno-unused-parameter -Isrc -I$(GEN) $(DOOM_CFLAGS) $(AMBIQ_CFLAGS)

LDFLAGS := $(ARCH) -nostartfiles -static -T src/linker.ld --specs=nano.specs --specs=nosys.specs -u _printf_float \
    -Wl,--gc-sections,--entry,Reset_Handler,-Map,$(BUILD)/doom.map,--print-memory-usage,--no-warn-rwx-segments
LDLIBS := -Wl,--start-group $(LIBS) -lm -lc -lgcc -Wl,--end-group

obj = $(patsubst %.c,$(OBJ)/%.o,$(patsubst /%,%,$(1)))
DOOM_OBJ  := $(call obj,$(DOOM_SRC))
PORT_OBJ  := $(call obj,$(PORT_SRC))
AMBIQ_OBJ := $(call obj,$(HAL_SRC) $(NEMA_PORT_SRC))
OBJS := $(DOOM_OBJ) $(PORT_OBJ) $(AMBIQ_OBJ)

.PHONY: all submodules toolchain flash reset swo
.DELETE_ON_ERROR:

all: $(BUILD)/doom.bin $(BUILD)/wad.bin $(BUILD)/flash.jlink $(BUILD)/reset.jlink
	@$(SIZE) $(BUILD)/doom.elf

submodules:
	@test -f $(HAL)/mcu/apollo510/am_mcu_apollo.h -a -f $(CMSIS)/CMSIS/Core/Include/core_cm55.h -a -f $(LIBS) || \
		git submodule update --init --depth 1

toolchain: $(CC)

$(CC):
	@echo "toolchain: downloading Arm GNU Toolchain 13.2.rel1"
	@mkdir -p tools
	curl -fSL --retry 3 --connect-timeout 30 -o $(TOOLCHAIN_TAR).part $(TOOLCHAIN_URL)
	echo "$(TOOLCHAIN_SHA256)  $(TOOLCHAIN_TAR).part" | sha256sum -c -
	tar -xJf $(TOOLCHAIN_TAR).part -C tools
	rm -f $(TOOLCHAIN_TAR).part
	touch $@

$(GEN)/build_config.h: FORCE
	@mkdir -p $(@D)
	@printf '#define DOOM_ARGS "%s"\n#define BUILD_ID "%s"\n' '$(DOOM_ARGS)' \
		"$$(git describe --always --dirty 2>/dev/null || echo unknown)" > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

FORCE:

$(DOOM_OBJ): $(OBJ)/%.o: %.c | toolchain submodules
	@mkdir -p $(@D)
	@echo "cc  $<"
	@$(CC) $(BASE_CFLAGS) $(DOOM_CFLAGS) -c $< -o $@

$(PORT_OBJ): $(OBJ)/%.o: %.c $(GEN)/build_config.h | toolchain submodules
	@mkdir -p $(@D)
	@echo "cc  $<"
	@$(CC) $(BASE_CFLAGS) $(PORT_CFLAGS) -c $< -o $@

# The display controller port is written for Zephyr; src/zephyr_compat stands in for the few kernel headers it uses.
$(call obj,$(NEMA_PORT_SRC)): VENDOR_CFLAGS += -Isrc/zephyr_compat -DCONFIG_MIPI_DSI_AMBIQ -DSOC_SERIES_APOLLO5X \
    -DCONFIG_NEMADC_WAIT_DC_VSYNC_TIMEOUT_MS=1000

$(AMBIQ_OBJ): $(OBJ)/%.o: %.c | toolchain submodules
	@mkdir -p $(@D)
	@echo "cc  $<"
	@$(CC) $(BASE_CFLAGS) $(VENDOR_CFLAGS) -c $< -o $@

$(BUILD)/doom.elf: $(OBJS) src/linker.ld
	@echo "ld  $@"
	@$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

$(BUILD)/doom.bin: $(BUILD)/doom.elf
	$(OBJCOPY) -O binary $< $@

$(DL)/doom1.wad:
	@mkdir -p $(DL)
	@for url in $(WAD_TGZ_URLS); do \
		echo "wad: downloading $$url"; \
		curl -fSL --retry 3 --connect-timeout 30 -o $(DL)/wad.tar.gz.part "$$url" && \
		echo "$(WAD_TGZ_SHA256)  $(DL)/wad.tar.gz.part" | sha256sum -c - && break; \
		rm -f $(DL)/wad.tar.gz.part; \
	done; test -f $(DL)/wad.tar.gz.part
	tar -xzOf $(DL)/wad.tar.gz.part doom-wad-shareware-1.9.fixed/doom1.wad > $@.part
	echo "$(WAD_SHA256)  $@.part" | sha256sum -c -
	mv $@.part $@
	rm -f $(DL)/wad.tar.gz.part

wad_symbol = $$($(NM) $(BUILD)/doom.elf | awk '$$3 == "$(1)" { print "0x" $$1 }')

$(BUILD)/wad.bin: $(DL)/doom1.wad tools/wadprep.py $(BUILD)/doom.elf
	python3 -I tools/wadprep.py --in $< --out $@ --max-size $$(( $(call wad_symbol,__wad_end) - $(call wad_symbol,__wad_start) ))

JLINK_CONNECT := device $(JLINK_DEVICE)\nif swd\nspeed 4000\nconnect\n
# The secure bootloader can park the CPU on a stale debugger-halt request right before jumping to the app. Clearing C_HALT in
# DHCSR releases it and, unlike a second g, is not an error when the CPU is already running.
JLINK_RUN := r\ng\nsleep 500\nw4 0xE000EDF0 0xA05F0001\nq\n

$(BUILD)/flash.jlink: $(BUILD)/doom.elf
	@printf '$(JLINK_CONNECT)loadbin $(BUILD)/doom.bin $(APP_ADDR)\nloadbin $(BUILD)/wad.bin %s\n$(JLINK_RUN)' \
		"$(call wad_symbol,__wad_start)" > $@

$(BUILD)/reset.jlink:
	@mkdir -p $(@D)
	@printf '$(JLINK_CONNECT)$(JLINK_RUN)' > $@

JLINK := JLinkExe -NoGui 1 -ExitOnError 1 $(if $(JLINK_SN),-SelectEmuBySN $(JLINK_SN))

flash: all
	$(JLINK) -CommandFile $(BUILD)/flash.jlink

reset: $(BUILD)/reset.jlink
	$(JLINK) -CommandFile $<

swo:
	JLinkSWOViewerCLExe -device $(JLINK_DEVICE) -cpufreq $(SWO_CPUFREQ) -swofreq $(SWO_FREQ) -itmport 0

-include $(OBJS:.o=.d)
