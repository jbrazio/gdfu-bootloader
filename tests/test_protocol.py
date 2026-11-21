import json
from pathlib import Path
import unittest

from protocol import (
    Frame,
    FrameError,
    PacketType,
    Profile,
    TransferEvent,
    TransferState,
    decode_frame,
    encode_frame,
)


class FrameTests(unittest.TestCase):
    def test_encode_frame_emits_hand_checked_wire_bytes(self) -> None:
        """Catches endian, length, and CRC regressions in RF framing."""
        frame = Frame(
            packet_type=PacketType.DATA,
            profile=Profile.MHZ_433,
            target_id=0x01020304,
            session_id=0x0102030405060708,
            sequence=0x0A0B0C0D,
            offset=0x10111213,
            payload=b"\xAA\xBB",
        )

        self.assertEqual(
            encode_frame(frame),
            bytes.fromhex(
                "47444655"  # magic: GDFU
                "01"        # protocol version
                "04"        # DATA
                "01"        # 433 MHz profile
                "00"        # flags reserved
                "01020304"  # target ID
                "0102030405060708"  # session ID
                "0A0B0C0D"  # sequence
                "10111213"  # image offset
                "0002"      # payload length
                "AABB"      # payload
                "67F3218B"  # IEEE CRC32 over preceding bytes
            ),
        )

    def test_set_tx_power_frame_round_trips(self) -> None:
        """Catches packet-type drift for target power synchronization."""
        frame = Frame(
            packet_type=PacketType.SET_TX_POWER,
            profile=Profile.MHZ_433,
            target_id=0x01020304,
            session_id=1,
            sequence=1,
            offset=0,
            payload=b"\x16",
        )
        self.assertEqual(decode_frame(encode_frame(frame)), frame)

    def test_decode_frame_rejects_crc_corruption(self) -> None:
        """Catches accepting radio corruption as a valid DFU command."""
        raw = bytes.fromhex(
            "47444655010401000102030401020304050607080A0B0C0D101112130002AABB"
            "67F3218A"
        )

        with self.assertRaisesRegex(FrameError, "CRC"):
            decode_frame(raw)

    def test_decode_frame_rejects_declared_length_larger_than_payload(self) -> None:
        """Catches malformed lengths reaching packet or flash processing."""
        raw = bytes.fromhex(
            "47444655010401000102030401020304050607080A0B0C0D101112130003AABB"
            "66314BBC"
        )

        with self.assertRaisesRegex(FrameError, "length"):
            decode_frame(raw)


class VectorTests(unittest.TestCase):
    def test_json_vectors_match_decoder_contract(self) -> None:
        """Catches test vector drift from actual interoperable framing."""
        vector_path = Path(__file__).parents[1] / "tests" / "test-vectors.json"
        vectors = json.loads(vector_path.read_text())["frames"]

        for vector in vectors:
            with self.subTest(vector=vector["name"]):
                raw = bytes.fromhex(vector["hex"])
                if vector["valid"]:
                    self.assertEqual(decode_frame(raw).payload, b"\xAA\xBB")
                else:
                    with self.assertRaisesRegex(FrameError, vector["error"]):
                        decode_frame(raw)


class TransferStateTests(unittest.TestCase):
    def test_receiving_duplicate_data_does_not_advance_confirmed_offset(self) -> None:
        """Catches duplicate RF DATA being treated as fresh flash input."""
        state = TransferState(target_id=0x01020304, profile=Profile.MHZ_433)
        state.apply(TransferEvent.accept(session_id=1))
        state.apply(TransferEvent.data(session_id=1, sequence=0, offset=0, length=16))

        result = state.apply(
            TransferEvent.data(session_id=1, sequence=0, offset=0, length=16)
        )

        self.assertEqual(result.confirmed_offset, 16)
        self.assertTrue(result.duplicate)

    def test_wrong_session_cannot_change_active_transfer(self) -> None:
        """Catches nearby radio traffic changing a selected target session."""
        state = TransferState(target_id=0x01020304, profile=Profile.MHZ_433)
        state.apply(TransferEvent.accept(session_id=1))

        with self.assertRaisesRegex(FrameError, "session"):
            state.apply(TransferEvent.data(session_id=2, sequence=0, offset=0, length=16))

        self.assertEqual(state.confirmed_offset, 0)


if __name__ == "__main__":
    unittest.main()
