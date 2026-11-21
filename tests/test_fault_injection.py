import unittest

from protocol import (
    Frame,
    FrameError,
    PacketType,
    Profile,
    TransferEvent,
    TransferPhase,
    TransferState,
    decode_frame,
    encode_frame,
)


class FaultInjectionSimulationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.target_id = 0x11223344
        self.session_id = 0xAABBCCDDEEFF0011
        self.state = TransferState(target_id=self.target_id, profile=Profile.MHZ_433)
        self.state.apply(TransferEvent.accept(session_id=self.session_id))

    def test_out_of_order_offset_is_rejected_and_does_not_corrupt_offset(self) -> None:
        """Catches out-of-order RF packets advancing flash pointer incorrectly."""
        # Chunk 0 arrives: 0..16
        res0 = self.state.apply(TransferEvent.data(self.session_id, sequence=0, offset=0, length=16))
        self.assertEqual(res0.confirmed_offset, 16)

        # Chunk 2 arrives prematurely: offset 32 instead of expected 16
        with self.assertRaisesRegex(FrameError, "unexpected offset"):
            self.state.apply(TransferEvent.data(self.session_id, sequence=2, offset=32, length=16))

        # Confirmed offset remains safely at 16
        self.assertEqual(self.state.confirmed_offset, 16)

    def test_reordered_late_duplicate_is_marked_duplicate(self) -> None:
        """Catches delayed duplicate RF frame triggering second write."""
        self.state.apply(TransferEvent.data(self.session_id, sequence=0, offset=0, length=16))
        res_dup = self.state.apply(TransferEvent.data(self.session_id, sequence=0, offset=0, length=16))
        self.assertTrue(res_dup.duplicate)
        self.assertEqual(res_dup.confirmed_offset, 16)

    def test_zero_length_payload_rejected(self) -> None:
        """Catches zero-length data packet causing infinite state loops."""
        with self.assertRaisesRegex(FrameError, "length"):
            self.state.apply(TransferEvent.data(self.session_id, sequence=0, offset=0, length=0))


if __name__ == "__main__":
    unittest.main()
