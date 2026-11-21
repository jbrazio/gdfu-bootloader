#ifndef GDFU_STATE_H
#define GDFU_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "gdfu_config.h"

typedef enum
{
  GDFU_STATE_IDLE,
  GDFU_STATE_ADVERTISING,
  GDFU_STATE_NEGOTIATING,
  GDFU_STATE_RECEIVING,
  GDFU_STATE_VERIFYING,
  GDFU_STATE_SUCCESS,
  GDFU_STATE_FAILURE,
  GDFU_STATE_RECOVERY,
} gdfu_state_id_t;

typedef enum
{
  GDFU_OK,
  GDFU_ERR_STATE,
  GDFU_ERR_PROFILE,
  GDFU_ERR_TARGET,
  GDFU_ERR_SESSION,
  GDFU_ERR_SEQUENCE,
  GDFU_ERR_OFFSET,
  GDFU_ERR_LENGTH,
} gdfu_result_t;

typedef struct
{
  gdfu_state_id_t state;
  uint32_t target_id;
  uint64_t session_id;
  uint32_t confirmed_offset;
  uint32_t last_sequence;
  uint32_t last_offset;
  bool has_last_data;
} gdfu_session_t;

typedef struct
{
  gdfu_result_t result;
  bool duplicate;
  uint32_t confirmed_offset;
} gdfu_data_result_t;

void gdfu_session_init(gdfu_session_t *session, uint32_t target_id);
gdfu_result_t gdfu_session_advertise(gdfu_session_t *session);
gdfu_result_t gdfu_session_accept(gdfu_session_t *session, uint32_t target_id,
                                  gdfu_profile_id_t profile, uint64_t session_id);
gdfu_data_result_t gdfu_session_data(gdfu_session_t *session, uint64_t session_id,
                                     uint32_t sequence, uint32_t offset,
                                     uint16_t length);
void gdfu_session_recover(gdfu_session_t *session);
bool gdfu_session_active(const gdfu_session_t *session, uint64_t session_id);
bool gdfu_tx_power_valid(uint8_t dbm);

#endif
