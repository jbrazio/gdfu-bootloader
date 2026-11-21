MCU_SUB_VARIANT = nrf52840
CFLAGS += -DDEVICE_NAME='"4631_DFU"'

# GFSK radio transport is opt-in (profile selected compile-time).
ifeq ($(GFSK_DFU_ENABLED),1)
CFLAGS += -DGFSK_DFU_ENABLED=1
endif
