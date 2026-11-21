#include <assert.h>
#include <string.h>

#include "gdfu_frame.h"

int main(void)
{
  static const uint8_t valid_hex[] = {
    0x47, 0x44, 0x46, 0x55, 0x01, 0x04, 0x01, 0x00,
    0x01, 0x02, 0x03, 0x04,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x0A, 0x0B, 0x0C, 0x0D,
    0x10, 0x11, 0x12, 0x13,
    0x00, 0x02,
    0xAA, 0xBB,
    0x67, 0xF3, 0x21, 0x8B
  };

  gdfu_frame_t frame;
  assert(gdfu_frame_decode(valid_hex, sizeof(valid_hex), &frame));
  assert(frame.type == GDFU_PKT_DATA);
  assert(frame.profile == GDFU_PROFILE_433);
  assert(frame.target_id == 0x01020304u);
  assert(frame.session_id == 0x0102030405060708ULL);
  assert(frame.sequence == 0x0A0B0C0Du);
  assert(frame.offset == 0x10111213u);
  assert(frame.length == 2u);
  assert(frame.payload[0] == 0xAA && frame.payload[1] == 0xBB);

  uint8_t out_buf[GDFU_MAX_FRAME_SIZE];
  size_t out_len = 0;
  assert(gdfu_frame_encode(&frame, out_buf, sizeof(out_buf), &out_len));
  assert(out_len == sizeof(valid_hex));
  assert(memcmp(out_buf, valid_hex, sizeof(valid_hex)) == 0);

  gdfu_frame_t power = {
    .type = GDFU_PKT_SET_TX_POWER,
    .profile = GDFU_PROFILE_433,
    .target_id = 0x01020304u,
    .session_id = 1u,
    .sequence = 1u,
    .length = 1u,
    .payload = { 22u },
  };
  assert(gdfu_frame_encode(&power, out_buf, sizeof(out_buf), &out_len));
  assert(gdfu_frame_decode(out_buf, out_len, &frame));
  assert(frame.type == GDFU_PKT_SET_TX_POWER);
  assert(frame.length == 1u && frame.payload[0] == 22u);

  uint8_t corrupt[sizeof(valid_hex)];
  memcpy(corrupt, valid_hex, sizeof(valid_hex));
  corrupt[sizeof(valid_hex) - 1] ^= 0x01;
  assert(!gdfu_frame_decode(corrupt, sizeof(corrupt), &frame));

  return 0;
}
