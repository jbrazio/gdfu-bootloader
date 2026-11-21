# Bootloader specification (Heltec T114, nRF52840)

Implementation boundary of the GFSK DFU bootloader on the target. Wire framing and session rules: [GDFU RF protocol](PROTOCOL_DFU_GFSK.md). Lifecycle overview: [Architecture](ARCHITECTURE.md).

## Target and scope

- Board: Heltec T114 (nRF52840 with SX1262), `BOARD=heltec_t114`. `GFSK_DFU_ENABLED=1` compiles GFSK/USB; unset or `GFSK_DFU_ENABLED=0` compiles stock OTAFIX BLE/USB DFU. `heltec_t114` is the only hardware-validated GFSK board. `promicro_nrf52840`, `wiscore_rak4631_board`, `wiscore_rak3401`, `sensecap_solar_p1`, `xiao_nrf52840_ble` and `xiao_nrf52840_ble_sense` also define `SX1262_*` pins (from MeshCore variants) and build GFSK, but are untested on hardware. Other OTAFIX boards build stock BLE/USB only. Board notes: root README's supported-boards table. Optional `SX1262_RXEN_PIN` (high for RX, low for TX/sleep) and `SX1262_POWER_EN_PIN` (high before reset) cover non-T114 front ends; T114 defines neither.
- Application images only. SoftDevice and bootloader updates are rejected by mode and metadata checks; a GDFU session never writes the SoftDevice, bootloader, bootloader config, MBR parameters, bootloader settings, or application-data pages.
- Single bank: the new image overwrites the running application in place. An interrupted update leaves no valid application; there is no rollback. Recovery is a fresh update from byte zero.

## Profile selection

The RF profile is compile-time: `GDFU_PROFILE=1` (433.175 MHz) or `GDFU_PROFILE=2` (864.100 MHz). One profile per build; `gdfu_config.h` maps it to `GDFU_FREQUENCY_HZ` and rejects any other value at compile time. A `START` whose header profile differs from the compiled profile is refused. Radio framing for both profiles: [GDFU RF protocol](PROTOCOL_DFU_GFSK.md).

## Entry into GFSK DFU

On reset the bootloader enters GFSK radio DFU when an update is requested — no valid application, DFU button, or `GPREGRET` magic `0xA8` (`DFU_MAGIC_OTA_RESET`) / `0xB1` — unless USB DFU was selected. USB DFU stays available in every GFSK build and takes precedence when chosen: double-reset, `0x4E` (serial-only), or `0x57` (UF2) select it. These magics are the recovery entry when RF operation is impossible.

While advertising, the target beacons every 5 s with its stable target ID (low 32 bits of the BLE MAC, FICR `DEVICEADDR[0]`; zero remapped to `0x01020304`). The ID survives reboots. After a timeout, validation failure, or `ABORT` the target clears volatile session state and returns to advertising; on `FINISH` success it activates the image and reboots into the application.

## Radio ownership

The bootloader owns the SX1262 over SPI (pins in each board's `board.h`, e.g. `src/boards/heltec_t114/board.h`). Default TX power is 10 dBm; a session-bound `SET_TX_POWER` (10–22 dBm) overrides it for one session, and every recovery reapplies the compiled 10 dBm default. Regulatory compliance for either frequency is the deployer's responsibility.

## START acceptance and flash protection

Two checks bound every session:

1. Transport check (`dfu_transport_radio.c`): the update mode must be application-only (`DFU_UPDATE_APP`), and the image end (`0x00026000` + declared size) must not exceed total flash read from FICR.
2. DFU-core check (`dfu_single_bank.c`): image sizes must be word-aligned, and the total image size must not exceed `DFU_IMAGE_MAX_SIZE_FULL = (0xF4000 − 0x26000) − 0xA000 = 0xC4000` bytes (s140 6.1.1; the start follows the installed SoftDevice) (application region minus the 40 KiB application-data reserve).

Violations answer `NACK` 8 before anything is erased.

## Fresh-session erase

On every accepted `START`, before its ACK, the bootloader erases every 4 KiB page covering the declared image size, starting at `DFU_BANK_0_REGION_START` = `0x00026000`. It is not the whole application region — only the pages the declared image will occupy. At roughly 85 ms per page, the validated 444,052-byte image (109 pages) erases in about 9 s, within the proxy's configured 15 s `START` ACK window. The maximum allowed `0xC4000` image occupies `0xC4000 / 0x1000 = 196` pages and takes about 16.7 s at that estimate, exceeding the proxy's 15 s window. This is an existing operational incompatibility, not a code change. The up-front erase guarantees a failed session can leave no programmed bits under the next one (nRF52 flash cannot flip a programmed zero back to one).

## Metadata and postvalidation

- The first `DATA` frame carries the Nordic init packet. The target requires `device_type` 82 (`0x52`) and a matching SoftDevice FWID (`0xFFFE` matches any).
- Subsequent `DATA` frames carry word-aligned image bytes, accepted only at the confirmed offset.
- `FINISH` verifies the final Nordic CRC16 over the received image and the metadata, then activates the image in bootloader settings and reboots.

## Flash layout (nRF52840)

Application start is `CODE_REGION_1_START`, i.e. the installed SoftDevice end read at runtime; addresses below are for s140 6.1.1.

| Range | Content |
|---|---|
| `0x00000000–0x00001000` | MBR |
| `0x00001000–0x00026000` | SoftDevice (s140 6.1.1; 7.3.0 on XIAO, XIAO Sense and P1 ends at `0x00027000`) |
| `0x00026000–0x000EA000` | Application |
| `0x000EA000–0x000F4000` | Application data (reserved by DFU: `0xA000`) |
| `0x000F4000–0x000FD800` | Bootloader |
| `0x000FD800–0x000FE000` | Bootloader config |
| `0x000FE000–0x000FF000` | MBR parameters |
| `0x000FF000–0x00100000` | Bootloader settings |

Session timeouts mirror the RF contract: 30-second active-session timeout (fifteen 2 s idle windows), `DATA` ACK 2 s, control frames 10 s, `START` ACK covered by the proxy's 15 s window.

## Build commands and artifacts

```sh
# GFSK/USB DFU
make -C bootloader -B BOARD=heltec_t114 GFSK_DFU_ENABLED=1 GDFU_PROFILE=1 elf
make -C bootloader -B BOARD=heltec_t114 GFSK_DFU_ENABLED=1 GDFU_PROFILE=2 elf

# Stock OTAFIX BLE/USB DFU (default; explicit 0 is equivalent)
make -C bootloader -B BOARD=heltec_t114 elf
make -C bootloader -B BOARD=heltec_t114 GFSK_DFU_ENABLED=0 elf
```

Needs `gcc-arm-none-eabi`. Outputs under `_build/build-heltec_t114-profile<N>/`: `heltec_t114_bootloader-<git>.out` and `.hex`, `..._nosd.hex`, `update-..._nosd.uf2`, and the merged SoftDevice image `..._s140_6.1.1.hex` / `.zip` (MBR 2.4.1; the ZIP is built with adafruit-nrfutil, `--dev-type 0x0052 --dev-revision 52840`). CI builds both GFSK profiles and default BLE; it never flashes hardware.

## Source and test ownership

- `src/radio/`: `dfu_transport_radio.c` (GDFU transport), `gdfu_frame.c` (codec), `gdfu_state.c` (session rules), `gdfu_radio_hal.c` (SX1262 SPI driver), `gdfu_config.h` (profile and timing constants).
- `lib/sdk11/components/libraries/bootloader_dfu/`: SDK11 DFU core — `dfu_single_bank.c` (size checks, up-front erase), `dfu_types.h` (region constants).
- `src/dfu_init.c`: init-packet validation and CRC16 postvalidation.
- Host tests (`python3 -m unittest discover -s tests -p 'test_*.py'`): `test_protocol.py` and `test_fault_injection.py` (reference codec in `tests/protocol.py` against `tests/test-vectors.json`), `test_bootloader_state.py` (profile/session guards, C codec against the canonical vector), `test_bootloader_make.py`, `test_radio_source.py`, `test_protocol_coverage.py`. C unit tests: `tests/*.c`.
