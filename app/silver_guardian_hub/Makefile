# Silver Guardian Hub - Makefile
#
# Licensed under the Apache License, Version 2.0

include $(APPDIR)/Make.defs

ifeq ($(CONFIG_LVX_USE_DEMO_CONTEST2026_325_SILVER_GUARDIAN_HUB), y)
PROGNAME = silver_guardian_hub
PRIORITY = 100
STACKSIZE = 32768
MODULE = $(CONFIG_LVX_USE_DEMO_CONTEST2026_325_SILVER_GUARDIAN_HUB)

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_325_silver_guardian_hub

# 注意：本目录是通过 repo manifest 的 <linkfile> 映射到编译位置的，
# 改完源码要先跑 tools/sync_app.sh push 同步，再编译。

CSRCS  = src/event.c
CSRCS += src/audio.c
CSRCS += src/medication.c
CSRCS += src/cloud.c
CSRCS += src/lcd.c
CSRCS += src/ui.c
CSRCS += src/touch.c
CSRCS += src/sensors.c
CSRCS += src/storage.c

# 中文字库（由 tools/gen_lvgl_fonts.sh 生成，Noto Sans SC / SIL OFL 1.1）
CSRCS += src/fonts/sg_font_16.c
CSRCS += src/fonts/sg_font_24.c

MAINSRC = src/main.c
endif

include $(APPDIR)/Application.mk
