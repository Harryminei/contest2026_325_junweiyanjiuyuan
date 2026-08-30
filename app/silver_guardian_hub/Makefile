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
CSRCS = src/event.c src/audio.c src/medication.c src/cloud.c src/lcd.c
MAINSRC = src/main.c
endif

include $(APPDIR)/Application.mk
