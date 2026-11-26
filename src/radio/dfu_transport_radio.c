#include "dfu_transport_radio.h"

#include <stdbool.h>
#include <string.h>

#include "app_error.h"
#include "boards.h"
#include "bootloader.h"
#include "dfu.h"
#include "dfu_init.h"
#include "dfu_transport.h"
#include "dfu_types.h"
#include "gdfu_frame.h"
#include "gdfu_radio_hal.h"
#include "gdfu_state.h"
#include "nrf_delay.h"
#include "nrf_error.h"
#include "nrf_ficr.h"
#include "nrf_nvic.h"

extern uint32_t board_millis(void);

static gdfu_session_t m_session;
static bool m_done = false;
static dfu_start_packet_t m_start_packet;

static void dfu_cb_handler(uint32_t packet, uint32_t result, uint8_t *p_data)
{
  (void)packet;
  (void)p_data;
  APP_ERROR_CHECK(result);
}

static uint32_t get_target_id(void)
{
  /* Stable per-device ID = low 32 bits of the BLE MAC (FICR DEVICEADDR[0]),
   * so it reads as the last 4 MAC bytes in scanners. It must survive the post-FINISH
   * reboot so the app can tell "booted the new application" (beacon gone)
   * from "fell back into the bootloader" (beacon returns with same ID).
   * Deliberate privacy trade-off (requested): the beacon is linkable to the
   * BLE MAC. Upgrade path if unlinkability is ever needed: generate one
   * random value, persist it in a spare UICR word on first boot and return
   * that instead (mind UICR words the MBR/bootloader settings already use). */
  uint32_t mac_lo = NRF_FICR->DEVICEADDR[0];
  return (mac_lo == 0u) ? 0x01020304u : mac_lo;
}

static uint32_t process_start_payload(const uint8_t *payload, uint16_t length)
{
  if (length != sizeof(dfu_start_packet_t))
  {
    return NRF_ERROR_INVALID_LENGTH;
  }
  memcpy(&m_start_packet, payload, sizeof(m_start_packet));

  // GDFU is deliberately application-only. Reject SD/bootloader package
  // modes before dfu_start_pkt_handle() reaches the shared prepare/erase path.
  if (m_start_packet.dfu_update_mode != DFU_UPDATE_APP)
  {
    return NRF_ERROR_NOT_SUPPORTED;
  }

  uint32_t flash_total = nrf_ficr_codepagesize_get(NRF_FICR) * nrf_ficr_codesize_get(NRF_FICR);
  uint32_t app_end = CODE_REGION_1_START + m_start_packet.app_image_size;
  if (app_end > flash_total)
  {
    return NRF_ERROR_DATA_SIZE;
  }

  dfu_update_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.packet_type = START_PACKET;
  pkt.params.start_packet = &m_start_packet;
  return dfu_start_pkt_handle(&pkt);
}

static uint32_t process_init_payload(const uint8_t *payload, uint16_t length)
{
  uint32_t aligned[(GDFU_MAX_PAYLOAD + 3) / 4];
  memset(aligned, 0, sizeof(aligned));
  memcpy(aligned, payload, length);

  dfu_update_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.packet_type = INIT_PACKET;
  pkt.params.data_packet.packet_length = (length + 3) / sizeof(uint32_t);
  pkt.params.data_packet.p_data_packet = aligned;

  uint32_t err = dfu_init_pkt_handle(&pkt);
  if (err != NRF_SUCCESS)
  {
    return err;
  }
  return dfu_init_pkt_complete();
}

static uint32_t process_data_payload(const uint8_t *payload, uint16_t length)
{
  if ((length % sizeof(uint32_t)) != 0u)
  {
    return NRF_ERROR_INVALID_LENGTH;
  }

  uint32_t aligned[(GDFU_MAX_PAYLOAD + 3) / 4];
  memcpy(aligned, payload, length);

  dfu_update_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.packet_type = DATA_PACKET;
  pkt.params.data_packet.packet_length = length / sizeof(uint32_t);
  pkt.params.data_packet.p_data_packet = aligned;
  return dfu_data_pkt_handle(&pkt);
}

static void send_ack(const gdfu_frame_t *rx, uint32_t confirmed_offset)
{
  gdfu_frame_t tx;
  memset(&tx, 0, sizeof(tx));
  tx.type = GDFU_PKT_ACK;
  tx.profile = GDFU_PROFILE;
  tx.target_id = rx->target_id;
  tx.session_id = rx->session_id;
  tx.sequence = rx->sequence;
  tx.offset = confirmed_offset;

  uint8_t raw[GDFU_MAX_FRAME_SIZE];
  size_t len = 0;
  if (gdfu_frame_encode(&tx, raw, sizeof(raw), &len))
  {
    gdfu_radio_send(raw, len, GDFU_DATA_ACK_TIMEOUT_MS);
  }
}

static void send_nack(const gdfu_frame_t *rx, gdfu_nack_code_t code)
{
  gdfu_frame_t tx;
  memset(&tx, 0, sizeof(tx));
  tx.type = GDFU_PKT_NACK;
  tx.profile = GDFU_PROFILE;
  tx.target_id = rx->target_id;
  tx.session_id = rx->session_id;
  tx.sequence = rx->sequence;
  tx.offset = code;

  uint8_t raw[GDFU_MAX_FRAME_SIZE];
  size_t len = 0;
  if (gdfu_frame_encode(&tx, raw, sizeof(raw), &len))
  {
    gdfu_radio_send(raw, len, GDFU_DATA_ACK_TIMEOUT_MS);
  }
}

static void finish_ack_window(const gdfu_frame_t *finish, uint32_t confirmed_offset)
{
  uint32_t remaining_ms = GDFU_FINISH_ACK_WINDOW_MS;
  while (remaining_ms > 0u)
  {
    uint8_t rx_raw[GDFU_MAX_FRAME_SIZE];
    size_t rx_len = 0;
    uint32_t const started_ms = board_millis();
    int rc = gdfu_radio_recv(rx_raw, sizeof(rx_raw), &rx_len, remaining_ms);
    uint32_t const elapsed_ms = board_millis() - started_ms;
    if (elapsed_ms >= remaining_ms)
    {
      return;
    }
    remaining_ms -= elapsed_ms;
    if (rc != 0 || rx_len == 0u)
    {
      continue;
    }

    gdfu_frame_t retry;
    if (gdfu_frame_decode(rx_raw, rx_len, &retry) &&
        retry.type == GDFU_PKT_FINISH &&
        retry.target_id == finish->target_id &&
        retry.session_id == finish->session_id &&
        retry.sequence == finish->sequence)
    {
      send_ack(&retry, confirmed_offset);
    }
  }
}

static void recover_session(void)
{
  /* Session overrides are volatile: every recovery returns to compiled power. */
  if (gdfu_radio_set_tx_power(GDFU_DEFAULT_TX_POWER_DBM) != 0)
  {
    /* A failed SPI write must not leave the raised session power latched:
     * a full radio init reapplies the compiled default. */
    (void)gdfu_radio_init();
  }
  gdfu_session_recover(&m_session);
  gdfu_session_advertise(&m_session);
}

static void send_beacon(uint32_t target_id)
{
  gdfu_frame_t tx;
  memset(&tx, 0, sizeof(tx));
  tx.type = GDFU_PKT_BEACON;
  tx.profile = GDFU_PROFILE;
  tx.target_id = target_id;
  tx.sequence = 0u;
  tx.offset = 0u;

  uint8_t raw[GDFU_MAX_FRAME_SIZE];
  size_t len = 0;
  if (gdfu_frame_encode(&tx, raw, sizeof(raw), &len))
  {
    gdfu_radio_send(raw, len, 100);
  }
}

uint32_t dfu_transport_radio_update_start(void)
{
  if (gdfu_radio_init() != 0)
  {
    return NRF_ERROR_INTERNAL;
  }

  uint32_t target_id = get_target_id();
  gdfu_session_init(&m_session, target_id);
  gdfu_session_advertise(&m_session);
  dfu_register_callback(dfu_cb_handler);
  m_done = false;

  uint8_t rx_raw[GDFU_MAX_FRAME_SIZE];
  uint8_t session_idle_ticks = 0;
  const uint8_t max_idle_ticks = GDFU_SESSION_TIMEOUT_MS / GDFU_DATA_ACK_TIMEOUT_MS;

  // START is the point of no return: it erases the app slot. Before it, the
  // slot is untouched, so an idle target can safely go back to the app.
  uint32_t const entered_ms = board_millis();
  bool upload_attempted = false;

  bool led_advertising = true; // main.c set STATE_GFSK_ADVERTISING before entry
  while (!m_done)
  {
    bool const advertising = (m_session.state == GDFU_STATE_ADVERTISING);
    if (advertising && !upload_attempted &&
        (board_millis() - entered_ms) >= GDFU_IDLE_TIMEOUT_MS &&
        bootloader_app_is_valid())
    {
      gdfu_radio_sleep();
      NRF_POWER->GPREGRET = DFU_MAGIC_SKIP;
      NVIC_SystemReset();
    }
    if (advertising != led_advertising)
    {
      // Beacon blink only while idling for a connection; otherwise stock LED.
      led_advertising = advertising;
      led_state(advertising ? STATE_GFSK_ADVERTISING : STATE_BLE_DISCONNECTED);
    }
    if (advertising)
    {
      send_beacon(target_id);
    }

    size_t rx_len = 0;
    int rc = gdfu_radio_recv(rx_raw, sizeof(rx_raw), &rx_len,
                             (m_session.state == GDFU_STATE_ADVERTISING) ? GDFU_BEACON_INTERVAL_MS : GDFU_DATA_ACK_TIMEOUT_MS);

    // Elapsed-time accounting: every receive window that ends without a
    // frame addressed to this session advances the idle clock, so co-channel
    // traffic (other targets' beacons, noise) cannot pin a session open.
    if (m_session.state == GDFU_STATE_RECEIVING && ++session_idle_ticks >= max_idle_ticks)
    {
      recover_session();
      session_idle_ticks = 0;
      continue;
    }

    if (rc != 0 || rx_len == 0)
    {
      continue;
    }

    gdfu_frame_t frame;
    if (!gdfu_frame_decode(rx_raw, rx_len, &frame))
    {
      continue;
    }

    // Not for us (other devices' beacons, START/DATA/FINISH for another
    // target): stay silent. A NACK echoes the frame's target_id/session_id,
    // so the proxy would take it as that other device's answer.
    if (frame.target_id != target_id)
    {
      continue;
    }

    if (frame.type == GDFU_PKT_START)
    {
      upload_attempted = true;
      // Retried START for the session we already opened (our ACK was lost):
      // re-ACK instead of reporting BUSY and killing a transfer that started.
      if (m_session.state == GDFU_STATE_RECEIVING && m_session.session_id != 0u &&
          frame.session_id == m_session.session_id)
      {
        send_ack(&frame, m_session.confirmed_offset);
        session_idle_ticks = 0;
        continue;
      }
      // A proxy reset loses its session but leaves this single-bank DFU core
      // in RX_DATA/RDY after an incomplete transfer. A new START at byte zero
      // is a fresh transaction, so reset the volatile core before validating
      // it. Only do this while advertising: START during an active session is
      // rejected below and cannot discard in-progress state.
      if (m_session.state == GDFU_STATE_ADVERTISING)
      {
        uint32_t const reset_err = dfu_init();
        if (reset_err != NRF_SUCCESS)
        {
          // dfu_init() fails only in flash storage setup; don't leak the raw
          // NRF_ERROR_* value into the NACK code space.
          send_nack(&frame, GDFU_NACK_FLASH);
          recover_session();
          continue;
        }
      }
      gdfu_result_t const accept_res =
          gdfu_session_accept(&m_session, frame.target_id, frame.profile, frame.session_id);
      if (accept_res != GDFU_OK)
      {
        send_nack(&frame, accept_res == GDFU_ERR_STATE ? GDFU_NACK_BUSY : GDFU_NACK_PROFILE);
        continue;
      }
      if (process_start_payload(frame.payload, frame.length) != NRF_SUCCESS)
      {
        send_nack(&frame, GDFU_NACK_IMAGE_META);
        recover_session();
        continue;
      }
      send_ack(&frame, 0u);
      session_idle_ticks = 0;
      continue;
    }

    if (frame.session_id != m_session.session_id || m_session.session_id == 0u)
    {
      send_nack(&frame, GDFU_NACK_SESSION);
      continue;
    }

    // Valid session traffic: any frame carrying our session ID resets the
    // idle clock (whether or not the command itself validates).
    session_idle_ticks = 0;

    if (frame.type == GDFU_PKT_SET_TX_POWER)
    {
      if (!gdfu_session_active(&m_session, frame.session_id))
      {
        send_nack(&frame, GDFU_NACK_SESSION);
        continue;
      }
      if (frame.length != 1u || !gdfu_tx_power_valid(frame.payload[0]))
      {
        send_nack(&frame, GDFU_NACK_MALFORMED);
        continue;
      }
      if (gdfu_radio_set_tx_power(frame.payload[0]) != 0)
      {
        send_nack(&frame, GDFU_NACK_RADIO);
        continue;
      }
      send_ack(&frame, m_session.confirmed_offset);
      continue;
    }

    if (frame.type == GDFU_PKT_DATA)
    {
      uint32_t const confirmed_offset = m_session.confirmed_offset;
      gdfu_data_result_t data_res = gdfu_session_data(&m_session, frame.session_id,
                                                      frame.sequence, frame.offset,
                                                      frame.length);
      if (data_res.result != GDFU_OK)
      {
        send_nack(&frame, GDFU_NACK_SEQUENCE);
        continue;
      }
      if (!data_res.duplicate)
      {
        uint32_t dfu_err;
        if (confirmed_offset == 0u && frame.offset == 0u)
        {
          /* First DATA frame carries the init packet (device metadata + CRC16). */
          dfu_err = process_init_payload(frame.payload, frame.length);
          if (dfu_err != NRF_SUCCESS)
          {
            send_nack(&frame, GDFU_NACK_FLASH);
            recover_session();
            continue;
          }
        }
        else
        {
          dfu_err = process_data_payload(frame.payload, frame.length);
          if (dfu_err != NRF_SUCCESS && dfu_err != NRF_ERROR_INVALID_LENGTH)
          {
            send_nack(&frame, GDFU_NACK_FLASH);
            recover_session();
            continue;
          }
        }
      }
      send_ack(&frame, data_res.confirmed_offset);
      continue;
    }

    if (frame.type == GDFU_PKT_FINISH)
    {
      if (dfu_image_validate() == NRF_SUCCESS && dfu_image_activate() == NRF_SUCCESS)
      {
        send_ack(&frame, m_session.confirmed_offset);
        finish_ack_window(&frame, m_session.confirmed_offset);
        m_done = true;
      }
      else
      {
        send_nack(&frame, GDFU_NACK_IMAGE_CRC);
        recover_session();
      }
      continue;
    }

    if (frame.type == GDFU_PKT_ABORT)
    {
      send_ack(&frame, 0u);
      recover_session();
      continue;
    }
  }

  gdfu_radio_sleep();
  NVIC_SystemReset();
  return NRF_SUCCESS;
}

uint32_t dfu_transport_radio_close(void)
{
  gdfu_radio_sleep();
  return NRF_SUCCESS;
}
