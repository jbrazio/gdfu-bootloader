from pathlib import Path
import unittest

from protocol import MAGIC, MAX_PAYLOAD, VERSION, PacketType

ROOT = Path(__file__).parents[1]


class ProtocolSpecificationCoverageTests(unittest.TestCase):
    def test_protocol_constants_match_across_implementations(self) -> None:
        """Catches drift between docs/PROTOCOL_DFU_GFSK.md, the Python codec and the C headers."""
        self.assertEqual(MAGIC, b"GDFU")
        self.assertEqual(VERSION, 1)
        self.assertEqual(MAX_PAYLOAD, 192)
        self.assertEqual(PacketType.SET_TX_POWER, 12)

        config = (ROOT / "src" / "radio" / "gdfu_config.h").read_text()
        for define in (
            "GDFU_PROTOCOL_VERSION 1u",
            "GDFU_MAX_PAYLOAD 192u",
            "GDFU_FREQUENCY_HZ 433175000u",
            "GDFU_FREQUENCY_HZ 864100000u",
            "GDFU_RX_BANDWIDTH_433_HZ 187200u",
            "GDFU_RX_BANDWIDTH_868_HZ 187200u",
            "GDFU_BITRATE_BPS 80000u",
            "GDFU_DEVIATION_HZ 28000u",
            "GDFU_PREAMBLE_BITS 32u",
        ):
            self.assertIn(f"#define {define}", config)

        frame_h = (ROOT / "src" / "radio" / "gdfu_frame.h").read_text()
        self.assertIn("#define GDFU_HEADER_SIZE 30u", frame_h)
        self.assertIn("#define GDFU_CRC_SIZE 4u", frame_h)
        self.assertIn("GDFU_PKT_SET_TX_POWER = 12", frame_h)

        protocol = (ROOT / "docs" / "PROTOCOL_DFU_GFSK.md").read_text()
        for text in (
            "`SET_TX_POWER`",
            "10–22",
            "compiled profile",
            "30-second active-session timeout",
            "five total transmissions",
            "complete 34–226-byte GDFU frame",
            "GetRxBufferStatus",
            "80,000 bit/s",
            "±28 kHz",
            "187.2 kHz",
        ):
            self.assertIn(text, protocol)
        self.assertIn("five total transmissions", (ROOT / "docs" / "ARCHITECTURE.md").read_text())


if __name__ == "__main__":
    unittest.main()
