from pathlib import Path
import unittest


class RadioSourceTests(unittest.TestCase):
    def test_bootloader_hal_matches_proxy_air_framing(self) -> None:
        """Catches bootloader HAL regressing from original air format."""
        source = (Path(__file__).parents[1] / "src" / "radio" / "gdfu_radio_hal.c").read_text(encoding="utf-8")
        # Original bootloader: whitening ON (0x01), CRC 2B (0x02), variable length (0x01)
        self.assertIn("0x02,\n    0x01,\n  };", source)
        # TX writes raw frame without length prefix
        self.assertIn("for (size_t i = 0; i < len; ++i) tx_buf[2 + i] = buf[i];", source)
        self.assertIn("cmd_write(tx_buf, len + 2)", source)
        # Both profiles use 187.2 kHz.
        self.assertIn("0x12,  /* rxBw = 187.2 kHz DSB", source)

    def test_target_applies_session_bound_tx_power_only(self) -> None:
        """Target must authenticate and validate power control before changing RF power."""
        root = Path(__file__).parents[1] / "src" / "radio"
        transport = (root / "dfu_transport_radio.c").read_text(encoding="utf-8")
        hal = (root / "gdfu_radio_hal.c").read_text(encoding="utf-8")
        for token in (
            "GDFU_PKT_SET_TX_POWER",
            "gdfu_session_active(&m_session, frame.session_id)",
            "frame.length != 1u",
            "gdfu_tx_power_valid(frame.payload[0])",
            "gdfu_radio_set_tx_power(frame.payload[0])",
            "gdfu_radio_set_tx_power(GDFU_DEFAULT_TX_POWER_DBM)",
            "session_idle_ticks",
        ):
            self.assertIn(token, transport)
        self.assertIn("int gdfu_radio_set_tx_power(uint8_t dbm)", hal)
        self.assertIn("SX1262_SET_TX_PARAMS, dbm, 0x04", hal)

    def test_first_data_frame_carries_init_packet(self) -> None:
        """Catches the target expecting image bytes before the init packet."""
        source = (Path(__file__).parents[1] / "src" / "radio" / "dfu_transport_radio.c").read_text(encoding="utf-8")
        self.assertIn("confirmed_offset == 0u && frame.offset == 0u", source)
        self.assertIn("process_init_payload", source)

    def test_target_nacks_flash_or_init_failures(self) -> None:
        """Catches acknowledging a DATA frame after its DFU handler failed."""
        source = (Path(__file__).parents[1] / "src" / "radio" / "dfu_transport_radio.c").read_text(encoding="utf-8")
        self.assertIn("uint32_t dfu_err", source)
        self.assertIn("if (dfu_err != NRF_SUCCESS)", source)
        self.assertIn("send_nack(&frame, GDFU_NACK_FLASH);", source)
        self.assertIn("gdfu_session_recover(&m_session)", source)

    def test_target_reinitializes_dfu_engine_before_fresh_start(self) -> None:
        """A proxy reset cannot leave the single-bank DFU core stuck in RX_DATA."""
        source = (Path(__file__).parents[1] / "src" / "radio" / "dfu_transport_radio.c").read_text(encoding="utf-8")
        start = source.index("if (frame.type == GDFU_PKT_START)")
        finish = source.index("if (frame.session_id != m_session.session_id", start)
        flow = source[start:finish]
        self.assertIn("dfu_init()", flow)
        self.assertLess(flow.index("dfu_init()"), flow.index("process_start_payload"))

    def test_target_rejects_non_application_start_before_erase(self) -> None:
        """Radio DFU is application-only; SD/bootloader modes must never enter prepare."""
        source = (Path(__file__).parents[1] / "src" / "radio" / "dfu_transport_radio.c").read_text(encoding="utf-8")
        start = source.index("static uint32_t process_start_payload")
        finish = source.index("static uint32_t process_init_payload", start)
        flow = source[start:finish]
        self.assertIn("DFU_UPDATE_APP", flow)
        self.assertIn("NRF_ERROR_NOT_SUPPORTED", flow)
        mode_check = flow.index("dfu_update_mode")
        engine = flow.rindex("dfu_start_pkt_handle")
        self.assertLess(mode_check, engine)
        self.assertIn("m_start_packet.dfu_update_mode", flow)

    def test_gfsk_status_led_repeats_a_a_b_c_pattern(self) -> None:
        """GFSK mode signals two fast pulses, one long pulse, then 3× off."""
        source = (Path(__file__).parents[1] / "src" / "boards" / "boards.c").read_text(encoding="utf-8")
        self.assertIn("#define GFSK_LED_UNIT_MS 100u", source)
        self.assertIn("#define GFSK_LED_CYCLE_MS (16u * GFSK_LED_UNIT_MS)", source)
        self.assertIn("millis % GFSK_LED_CYCLE_MS", source)
        self.assertIn("cycle < GFSK_LED_UNIT_MS", source)
        self.assertIn("cycle >= (2u * GFSK_LED_UNIT_MS) && cycle < (3u * GFSK_LED_UNIT_MS)", source)
        self.assertIn("cycle >= (4u * GFSK_LED_UNIT_MS) && cycle < (8u * GFSK_LED_UNIT_MS)", source)
        self.assertIn("GFSK_DFU_ENABLED == 1", source)

    def test_target_beacons_every_five_seconds_while_advertising(self) -> None:
        """Advertising receive windows define beacon cadence."""
        root = Path(__file__).parents[1] / "src" / "radio"
        config = (root / "gdfu_config.h").read_text(encoding="utf-8")
        transport = (root / "dfu_transport_radio.c").read_text(encoding="utf-8")
        self.assertIn("#define GDFU_BEACON_INTERVAL_MS 5000u", config)
        self.assertIn(
            "(m_session.state == GDFU_STATE_ADVERTISING) ? GDFU_BEACON_INTERVAL_MS : GDFU_DATA_ACK_TIMEOUT_MS",
            transport,
        )

    def test_gfsk_led_pattern_only_while_advertising(self) -> None:
        root = Path(__file__).parents[1] / "src"
        boards = (root / "boards" / "boards.c").read_text(encoding="utf-8")
        transport = (root / "radio" / "dfu_transport_radio.c").read_text(encoding="utf-8")
        self.assertIn("gfsk_led_pattern = (state == STATE_GFSK_ADVERTISING);", boards)
        self.assertIn("led_state(advertising ? STATE_GFSK_ADVERTISING : STATE_BLE_DISCONNECTED);", transport)

    def test_target_finish_ack_window_ignores_stray_frames(self) -> None:
        """A beacon/noise frame must not consume target's terminal FINISH retry window."""
        source = (Path(__file__).parents[1] / "src" / "radio" / "dfu_transport_radio.c").read_text(encoding="utf-8")
        start = source.index("static void finish_ack_window")
        window = source[start:source.index("static void recover_session", start)]
        self.assertIn("uint32_t remaining_ms = GDFU_FINISH_ACK_WINDOW_MS;", window)
        self.assertIn("while (remaining_ms > 0u)", window)
        self.assertIn("remaining_ms -= elapsed_ms;", window)
        self.assertIn("continue;", window)
        self.assertNotIn("send_ack(&retry, confirmed_offset);\n        return;", window)

    def test_dfu_init_registers_pstorage_only_once_per_boot(self) -> None:
        """Every fresh radio START resets DFU state but must not exhaust pstorage slots."""
        source = (Path(__file__).parents[1] / "lib" / "sdk11" / "components" /
                  "libraries" / "bootloader_dfu" / "dfu_single_bank.c").read_text(encoding="utf-8")
        init = source[source.index("uint32_t dfu_init(void)") : source.index("void dfu_register_callback", source.index("uint32_t dfu_init(void)"))]
        self.assertIn("m_storage_registered;", source)
        self.assertIn("if (!m_storage_registered)", init)
        self.assertIn("m_storage_registered = true;", init)

    def test_target_start_reports_dfu_init_error_code(self) -> None:
        """A target dfu_init failure maps to flash NACK 11, never a raw NRF_ERROR_* value."""
        source = (Path(__file__).parents[1] / "src" / "radio" / "dfu_transport_radio.c").read_text(encoding="utf-8")
        start = source.index("if (frame.type == GDFU_PKT_START)")
        accept = source.index("gdfu_session_accept(", start)
        failure = source[source.index("if (reset_err != NRF_SUCCESS)", start) : accept]
        self.assertIn("send_nack(&frame, GDFU_NACK_FLASH);", failure)

    def test_beacon_interval_is_five_seconds(self) -> None:
        """The proxy discovery TTL (15 s) assumes this 5 s beacon period."""
        cfg = (Path(__file__).parents[1] / "src" / "radio" / "gdfu_config.h").read_text(encoding="utf-8")
        self.assertIn("#define GDFU_BEACON_INTERVAL_MS 5000u", cfg)

    def test_target_replies_to_retried_finish_before_reset(self) -> None:
        """A lost terminal ACK must be recoverable before booting the new app."""
        root = Path(__file__).parents[1]
        source = (root / "src" / "radio" / "dfu_transport_radio.c").read_text(encoding="utf-8")
        self.assertIn("static void finish_ack_window", source)
        self.assertIn("gdfu_radio_recv(rx_raw, sizeof(rx_raw), &rx_len, remaining_ms)", source)
        self.assertIn("GDFU_FINISH_ACK_WINDOW_MS 12000u", (root / "src" / "radio" / "gdfu_config.h").read_text(encoding="utf-8"))
        success = source.index("if (dfu_image_validate() == NRF_SUCCESS && dfu_image_activate() == NRF_SUCCESS)")
        flow = source[success:source.index("else", success)]
        ack = flow.index("send_ack(&frame, m_session.confirmed_offset);")
        self.assertLess(ack, flow.index("finish_ack_window(&frame, m_session.confirmed_offset);"))


if __name__ == "__main__":
    unittest.main()
