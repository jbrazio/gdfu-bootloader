MCU_SUB_VARIANT = nrf52840
SD_VERSION = 7.3.0
CFLAGS += -DDEVICE_NAME='"XIAO_DFU"'

# GFSK radio transport is opt-in (profile selected compile-time).
ifeq ($(GFSK_DFU_ENABLED),1)
CFLAGS += -DGFSK_DFU_ENABLED=1
endif
