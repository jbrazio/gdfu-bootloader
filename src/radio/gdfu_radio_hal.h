#ifndef GDFU_RADIO_HAL_H
#define GDFU_RADIO_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gdfu_config.h"

int gdfu_radio_init(void);
int gdfu_radio_set_tx_power(uint8_t dbm);
int gdfu_radio_send(const uint8_t *buf, size_t len, uint32_t timeout_ms);
int gdfu_radio_recv(uint8_t *buf, size_t cap, size_t *out_len, uint32_t timeout_ms);
void gdfu_radio_recover(void);
void gdfu_radio_sleep(void);

#endif
