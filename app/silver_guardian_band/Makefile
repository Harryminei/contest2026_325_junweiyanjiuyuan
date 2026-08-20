include $(APPDIR)/Make.defs

# 应用名称

PROGNAME = silver_guardian_band
PRIORITY = $(CONFIG_SILVER_GUARDIAN_BAND_PRIORITY)
STACKSIZE = $(CONFIG_SILVER_GUARDIAN_BAND_STACKSIZE)

# 源文件

CSRCS = src/main.c \
        src/sos.c \
        src/imu.c \
        src/ble.c \
        src/ui.c

include $(APPDIR)/Application.mk
