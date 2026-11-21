from pathlib import Path
import subprocess
import tempfile
import unittest


class BootloaderStateTests(unittest.TestCase):
    def test_profile_and_session_guards_compile_and_run_on_host(self) -> None:
        """Catches accepting bad profile/session transitions before radio or DFU use."""
        root = Path(__file__).parents[1]
        source = root / "tests" / "test_gdfu_state.c"
        state = root / "src" / "radio" / "gdfu_state.c"
        include = root / "src" / "radio"

        with tempfile.TemporaryDirectory() as output:
            binary = Path(output) / "gdfu-state-test"
            subprocess.run(
                ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(include), str(source), str(state), "-o", str(binary)],
                check=True,
            )
            subprocess.run([str(binary)], check=True)

    def test_c_frame_codec_parses_canonical_wire_format(self) -> None:
        """Catches C framing decoder diverging from shared protocol vectors."""
        root = Path(__file__).parents[1]
        source = root / "tests" / "test_gdfu_frame.c"
        frame_c = root / "src" / "radio" / "gdfu_frame.c"
        include = root / "src" / "radio"

        with tempfile.TemporaryDirectory() as output:
            binary = Path(output) / "gdfu-frame-test"
            subprocess.run(
                ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(include), str(source), str(frame_c), "-o", str(binary)],
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
