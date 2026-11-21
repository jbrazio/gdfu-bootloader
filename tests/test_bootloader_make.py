from pathlib import Path
import subprocess
import unittest


GFSK_BOARDS = (
    "heltec_t114",
    "promicro_nrf52840",
    "wiscore_rak4631_board",
    "wiscore_rak3401",
    "sensecap_solar_p1",
    "xiao_nrf52840_ble",
    "xiao_nrf52840_ble_sense",
)
SX1262_PINS = ("SPI_INSTANCE", "CS_PIN", "RESET_PIN", "BUSY_PIN", "DIO1_PIN", "SCK_PIN", "MOSI_PIN", "MISO_PIN")


class BootloaderMakeTests(unittest.TestCase):
    def make_dry_run(self, *variables: str, board: str = "heltec_t114") -> str:
        bootloader = Path(__file__).parents[1]
        result = subprocess.run(
            ["make", "-Bn", f"BOARD={board}", *variables, "elf"],
            cwd=bootloader,
            text=True,
            capture_output=True,
            check=True,
        )
        return result.stdout

    def test_radio_profile_artifact_names(self) -> None:
        for profile, frequency in ((1, 433), (2, 868)):
            with self.subTest(profile=profile):
                output = self.make_dry_run("GFSK_DFU_ENABLED=1", f"GDFU_PROFILE={profile}", "BUILD_VERSION=gdfu-abcdef1")
                self.assertIn(f"heltec_t114_bootloader-gdfu-abcdef1_{frequency}.out", output)
                merged = subprocess.check_output(
                    ["make", "-Bn", "BOARD=heltec_t114", "GFSK_DFU_ENABLED=1", f"GDFU_PROFILE={profile}",
                     "BUILD_VERSION=gdfu-abcdef1", "copy-artifact"],
                    cwd=Path(__file__).parents[1], text=True,
                )
                self.assertIn(f"heltec_t114_bootloader-gdfu-abcdef1_{frequency}_s140_6.1.1.hex", merged)

    def test_calendar_release_emits_numeric_version_macro(self) -> None:
        """Calendar release fields remain valid C integers."""
        output = self.make_dry_run("BUILD_VERSION=2026.11.1")

        self.assertRegex(output, r"-DMK_BOOTLOADER_VERSION='\(\d+ << 16\) \+ \(\d+ << 8\) \+ \d+'")
        self.assertIn("-DUF2_VERSION_BASE='\"20", output)

    def test_release_candidate_numeric_version_ignores_rc_suffix(self) -> None:
        output = self.make_dry_run("CALVER_VERSION=2026.11.1-rc2", "BUILD_VERSION=2026.11.1-rc2-abcdef1", "GFSK_DFU_ENABLED=1")

        self.assertIn("-DMK_BOOTLOADER_VERSION='(2026 << 16) + (11 << 8) + 1'", output)
        self.assertIn("-DUF2_VERSION_BASE='\"gdfu-2026.11.1-rc2-abcdef1\"'", output)

    def test_debug_diagnostics_require_explicit_debug_one(self) -> None:
        output = self.make_dry_run()
        debug_output = self.make_dry_run("DEBUG=1")
        headers = (Path(__file__).parents[1] / "src" / "boards" / "boards.h").read_text()

        self.assertNotIn("-DDEBUG=1", output)
        self.assertIn("-DDEBUG=1", debug_output)
        self.assertIn("#ifdef CFG_DEBUG", headers)

    def test_cmake_debug_linker_omits_nosys_write_stub(self) -> None:
        root = Path(__file__).parents[1]
        cmake = (root / "CMakeLists.txt").read_text()
        debug_linker = (root / "linker" / "nrf52840_debug.ld").read_text()

        main = (root / "src" / "main.c").read_text()

        self.assertIn("--specs=nano.specs", cmake)
        self.assertIn('if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")\n  target_link_options(bootloader PUBLIC --specs=nosys.specs)', cmake)
        self.assertNotIn("-lnosys", debug_linker)
        for syscall in ("_close", "_exit", "_fstat", "_isatty", "_lseek", "_read", "_sbrk"):
            self.assertIn(syscall, main)

    def test_t114_defaults_to_stock_ble_transport(self) -> None:
        output = self.make_dry_run()

        self.assertIn("dfu_transport_ble.c", output)
        self.assertNotIn("dfu_transport_radio.c", output)
        self.assertNotIn("gdfu_radio_hal.c", output)
        self.assertNotIn("nrfx_spim.c", output)
        self.assertNotIn("-DGFSK_DFU_ENABLED", output)

    def test_t114_gfsk_transport_requires_explicit_one(self) -> None:
        output = self.make_dry_run("GFSK_DFU_ENABLED=1")

        self.assertIn("dfu_transport_radio.c", output)
        self.assertIn("gdfu_radio_hal.c", output)
        self.assertIn("nrfx_spim.c", output)
        self.assertNotIn("dfu_transport_ble.c", output)
        self.assertIn("-DGFSK_DFU_ENABLED=1", output)

    def test_gfsk_bootloader_version_has_gdfu_prefix(self) -> None:
        output = self.make_dry_run("GFSK_DFU_ENABLED=1")

        self.assertIn("-DUF2_VERSION_BASE='\"gdfu-", output)
        self.assertIn("-DUF2_VERSION='\"gdfu-", output)
        self.assertIn("-DBLEDIS_FW_VERSION='\"gdfu-", output)

    def test_t114_zero_selects_stock_ble_transport(self) -> None:
        output = self.make_dry_run("GFSK_DFU_ENABLED=0")

        self.assertIn("dfu_transport_ble.c", output)
        self.assertNotIn("dfu_transport_radio.c", output)
        self.assertNotIn("gdfu_radio_hal.c", output)
        self.assertNotIn("nrfx_spim.c", output)
        self.assertNotIn("-DGFSK_DFU_ENABLED", output)

    def test_every_gfsk_board_selects_one_transport_by_exact_value(self) -> None:
        for board in GFSK_BOARDS:
            for value in (None, "0", "1"):
                with self.subTest(board=board, value=value):
                    output = self.make_dry_run(*([] if value is None else [f"GFSK_DFU_ENABLED={value}"]), board=board)
                    gfsk = value == "1"
                    self.assertEqual("dfu_transport_radio.c" in output, gfsk)
                    self.assertEqual("gdfu_radio_hal.c" in output, gfsk)
                    self.assertEqual("nrfx_spim.c" in output, gfsk)
                    self.assertEqual("dfu_transport_ble.c" in output, not gfsk)
                    self.assertEqual("-DGFSK_DFU_ENABLED=1" in output, gfsk)
                    self.assertNotIn("-DGFSK_DFU_ENABLED=0", output)

    def test_every_gfsk_board_defines_sx1262_pins(self) -> None:
        boards = Path(__file__).parents[1] / "src/boards"
        for board in GFSK_BOARDS:
            with self.subTest(board=board):
                header = (boards / board / "board.h").read_text()
                for pin in SX1262_PINS:
                    self.assertRegex(header, rf"#define\s+SX1262_{pin}\s")

    def test_cmake_boards_with_gfsk_option_use_exact_value(self) -> None:
        boards = Path(__file__).parents[1] / "src/boards"
        for board in GFSK_BOARDS:
            cmake = boards / board / "board.cmake"
            if cmake.exists():
                with self.subTest(board=board):
                    text = cmake.read_text()
                    self.assertIn('if (GFSK_DFU_ENABLED STREQUAL "1")', text)

    def test_gfsk_guards_test_feature_value(self) -> None:
        root = Path(__file__).parents[1]
        guarded_files = (
            root / "src/main.c",
            root / "src/screen.c",
            root / "src/nrfx_config.h",
            root / "lib/sdk11/components/libraries/bootloader_dfu/bootloader.c",
            root / "lib/sdk11/components/libraries/bootloader_dfu/dfu_single_bank.c",
            root / "lib/sdk11/components/libraries/bootloader_dfu/dfu_transport.h",
        )

        for path in guarded_files:
            with self.subTest(path=path.relative_to(root)):
                source = path.read_text()
                self.assertNotIn("#ifdef GFSK_DFU_ENABLED", source)
                self.assertNotIn("#ifndef GFSK_DFU_ENABLED", source)


if __name__ == "__main__":
    unittest.main()
