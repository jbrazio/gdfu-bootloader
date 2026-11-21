#include "gdfu_state.h"

#include <string.h>

void gdfu_session_init(gdfu_session_t *session, uint32_t target_id)
{
  memset(session, 0, sizeof(*session));
  session->target_id = target_id;
  session->state = GDFU_STATE_IDLE;
}

gdfu_result_t gdfu_session_advertise(gdfu_session_t *session)
{
  if (session->target_id == 0u || session->state != GDFU_STATE_IDLE)
  {
    return GDFU_ERR_STATE;
  }

  session->state = GDFU_STATE_ADVERTISING;
  return GDFU_OK;
}

gdfu_result_t gdfu_session_accept(gdfu_session_t *session, uint32_t target_id,
                                  gdfu_profile_id_t profile, uint64_t session_id)
{
  if (session->state != GDFU_STATE_ADVERTISING)
  {
    return GDFU_ERR_STATE;
  }
  if (target_id != session->target_id)
  {
    return GDFU_ERR_TARGET;
  }
  if (profile != GDFU_PROFILE || session_id == 0u)
  {
    return GDFU_ERR_PROFILE;
  }

  session->session_id = session_id;
  session->state = GDFU_STATE_RECEIVING;
  return GDFU_OK;
}

gdfu_data_result_t gdfu_session_data(gdfu_session_t *session, uint64_t session_id,
                                     uint32_t sequence, uint32_t offset,
                                     uint16_t length)
{
  gdfu_data_result_t result = { .result = GDFU_ERR_STATE };

  if (session->state != GDFU_STATE_RECEIVING)
  {
    return result;
  }
  if (session_id != session->session_id)
  {
    result.result = GDFU_ERR_SESSION;
    return result;
  }
  if (length == 0u || length > GDFU_MAX_PAYLOAD)
  {
    result.result = GDFU_ERR_LENGTH;
    return result;
  }
  if (session->has_last_data && sequence == session->last_sequence &&
      offset == session->last_offset)
  {
    result.result = GDFU_OK;
    result.duplicate = true;
    result.confirmed_offset = session->confirmed_offset;
    return result;
  }
  if (offset != session->confirmed_offset)
  {
    result.result = GDFU_ERR_OFFSET;
    return result;
  }

  session->confirmed_offset += length;
  session->last_sequence = sequence;
  session->last_offset = offset;
  session->has_last_data = true;
  result.result = GDFU_OK;
  result.confirmed_offset = session->confirmed_offset;
  return result;
}

void gdfu_session_recover(gdfu_session_t *session)
{
  session->session_id = 0u;
  session->confirmed_offset = 0u;
  session->last_sequence = 0u;
  session->last_offset = 0u;
  session->has_last_data = false;
  session->state = GDFU_STATE_IDLE;
}

bool gdfu_session_active(const gdfu_session_t *session, uint64_t session_id)
{
  return session->state == GDFU_STATE_RECEIVING && session_id != 0u &&
         session_id == session->session_id;
}

bool gdfu_tx_power_valid(uint8_t dbm)
{
  return dbm >= GDFU_MIN_TX_POWER_DBM && dbm <= GDFU_MAX_TX_POWER_DBM;
}
