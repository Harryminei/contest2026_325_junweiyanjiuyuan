include $(APPDIR)/Make.defs

# 应用名称

PROGNAME = silver_guardian_hub
PRIORITY = $(CONFIG_SILVER_GUARDIAN_HUB_PRIORITY)
STACKSIZE = $(CONFIG_SILVER_GUARDIAN_HUB_STACKSIZE)

# 源文件

CSRCS = src/main.c \
        src/event.c \
        src/audio.c \
        src/medication.c \
        src/cloud.c

include $(APPDIR)/Application.mk
