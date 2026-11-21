#include <assert.h>

#include "gdfu_state.h"

int main(void)
{
  gdfu_session_t session;
  gdfu_data_result_t data;

  gdfu_session_init(&session, 0x01020304u);
  assert(gdfu_session_advertise(&session) == GDFU_OK);
  assert(gdfu_session_accept(&session, 0x01020305u, GDFU_PROFILE, 1u) == GDFU_ERR_TARGET);
  assert(gdfu_session_accept(&session, 0x01020304u, GDFU_PROFILE, 1u) == GDFU_OK);
  // A START while a session is open is a state error (NACKed as "busy"), not a profile one.
  assert(gdfu_session_accept(&session, 0x01020304u, GDFU_PROFILE, 2u) == GDFU_ERR_STATE);

  data = gdfu_session_data(&session, 2u, 0u, 0u, 16u);
  assert(data.result == GDFU_ERR_SESSION);
  data = gdfu_session_data(&session, 1u, 0u, 0u, 16u);
  assert(data.result == GDFU_OK);
  assert(data.confirmed_offset == 16u);
  data = gdfu_session_data(&session, 1u, 0u, 0u, 16u);
  assert(data.result == GDFU_OK);
  assert(data.duplicate);
  assert(data.confirmed_offset == 16u);
  data = gdfu_session_data(&session, 1u, 1u, 32u, 16u);
  assert(data.result == GDFU_ERR_OFFSET);

  assert(!gdfu_tx_power_valid(9u));
  assert(gdfu_tx_power_valid(10u));
  assert(gdfu_tx_power_valid(22u));
  assert(!gdfu_tx_power_valid(23u));
  assert(gdfu_session_active(&session, 1u));
  assert(!gdfu_session_active(&session, 2u));

  gdfu_session_recover(&session);
  assert(session.state == GDFU_STATE_IDLE);
  assert(session.confirmed_offset == 0u);
  return 0;
}
