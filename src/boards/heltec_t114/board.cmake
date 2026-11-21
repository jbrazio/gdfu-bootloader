set(MCU_VARIANT nrf52840)

# GFSK radio transport is opt-in (profile selected compile-time).
set(GFSK_DFU_ENABLED 0 CACHE STRING "Enable GFSK radio DFU")
if (GFSK_DFU_ENABLED STREQUAL "1")
  add_compile_definitions(GFSK_DFU_ENABLED=1)
endif ()
