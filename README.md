# GDFU Bootloader

## GDFU ecosystem

### About GDFU

GDFU updates firmware on nRF52840 targets over a GFSK radio link. The application connects to an ESP32 proxy over BLE. The proxy sends the firmware to the target bootloader, so the phone or computer stays near the proxy rather than the target.

### Projects

| Project | Role |
| --- | --- |
| [GDFU Application](https://github.com/jbrazio/gdfu-application) | Web and mobile client for discovering targets and managing firmware updates |
| [GDFU Proxy](https://github.com/jbrazio/gdfu-proxy) | ESP32 firmware bridging BLE to the GFSK radio link |
| [GDFU Bootloader](https://github.com/jbrazio/gdfu-bootloader) | nRF52840 bootloader receiving and installing target firmware |

### How it works

```text
Application ── BLE ── Proxy ── GFSK radio ── Target bootloader
```

1. Install the GDFU bootloader on a supported target and the proxy firmware on a supported ESP32 board.
2. Set the proxy and target to the same radio profile. Use hardware suited to that frequency and follow local radio regulations.
3. Put the target into DFU mode, open the application, and connect to the proxy.
4. Select the discovered target and a compatible firmware package, then start the update and monitor its progress.

## About this module

GDFU Bootloader adds an SX1262 GFSK radio transport to an nRF52840 DFU bootloader. A target in DFU mode beacons over the radio and receives firmware from a GDFU proxy.

It is a fork of [OTAFIX](https://github.com/oltaco/Adafruit_nRF52_Bootloader_OTAFIX) by [Huw Duddy, oltaco](https://github.com/oltaco), based on the [Adafruit nRF52 Bootloader](https://github.com/adafruit/Adafruit_nRF52_Bootloader) by Adafruit Industries. Imported from OTAFIX commit `3a75482e17680169edd86760d122a154b1620c4d`; see [IMPORT.md](IMPORT.md).

### Key features

- Target discovery beacons and GFSK firmware reception through a GDFU proxy.
- Framed radio protocol with CRC checks, acknowledgements, retries, and session validation.
- USB DFU with either GFSK or stock OTAFIX BLE. Each image includes one OTA transport.
- Bootloader HEX, UF2 update, and SoftDevice-merged HEX and DFU ZIP artifacts.

### Supported hardware

- Hardware-tested with GFSK: `heltec_t114`.
- Build-tested with GFSK: `promicro_nrf52840`, `wiscore_rak4631_board`, `wiscore_rak3401`, `sensecap_solar_p1`, `xiao_nrf52840_ble`, `xiao_nrf52840_ble_sense`.
- Other boards build stock BLE/USB DFU with `GFSK_DFU_ENABLED=0`; see [board definitions](src/boards/).

GFSK requires an SX1262 connected using the board's `SX1262_*` pin definitions. Optional `SX1262_RXEN_PIN` and `SX1262_POWER_EN_PIN` support board-specific radio front ends. A successful build does not confirm that the board has a radio or that DFU works on the hardware.

#### Radio profiles

| `GDFU_PROFILE` | Frequency | Default |
| --- | --- | --- |
| `1` | 433.175 MHz | Yes |
| `2` | 864.1 MHz | No |

Both profiles use GFSK at 80 kbps, 28 kHz deviation, 187.2 kHz RX bandwidth, and a 32-bit preamble. The proxy and bootloader must use matching settings. Select the bootloader profile at build time, for example:

```bash
make BOARD=heltec_t114 GFSK_DFU_ENABLED=1 GDFU_PROFILE=2 all
```

### Documentation

- [GDFU RF protocol](docs/PROTOCOL_DFU_GFSK.md).
- [Architecture](docs/ARCHITECTURE.md).
- [Bootloader specification](docs/BOOTLOADER_SPECS.md).

- [Frame definitions and NACK codes](src/radio/gdfu_frame.h) and [wire codec](src/radio/gdfu_frame.c).
- [Session rules](src/radio/gdfu_state.h) and [state implementation](src/radio/gdfu_state.c).
- [Radio profiles, limits, and timeouts](src/radio/gdfu_config.h).
- [Nordic DFU transport integration](src/radio/dfu_transport_radio.c) and [SX1262 HAL](src/radio/gdfu_radio_hal.c).
- [Upstream import details](IMPORT.md).

### Build and flash

Initialise submodules after cloning:

```bash
git submodule update --init --recursive
```

Requires `arm-none-eabi-gcc`, Python 3 with `intelhex`, and `adafruit-nrfutil` for DFU packages. CI uses GCC 12.3.Rel1.

```bash
pip3 install adafruit-nrfutil intelhex
make BOARD=heltec_t114 GFSK_DFU_ENABLED=1 all   # GFSK + USB DFU
make BOARD=heltec_t114 GFSK_DFU_ENABLED=0 all   # stock OTAFIX BLE + USB DFU
```

Build output goes in `_build/build-<board>[-gfsk][-profile<N>]-<version>/`. `make ... copy-artifact` copies them to `_bin/<board>/`.

Choose the flashing method supported by your device:

```bash
make BOARD=heltec_t114 GFSK_DFU_ENABLED=1 flash-uf2                     # mounted bootloader drive
make BOARD=heltec_t114 GFSK_DFU_ENABLED=1 SERIAL=/dev/ttyACM0 flash-dfu # USB CDC serial DFU
make BOARD=heltec_t114 GFSK_DFU_ENABLED=1 flash                         # SWD via nrfjprog
```

SWD also supports `FLASHER=pyocd`. A blank target needs an initial SWD installation; UF2 and serial DFU updates require a compatible bootloader already installed.

### Testing

Run host-side protocol and session tests without target hardware:

```bash
cc -Isrc/radio tests/test_gdfu_frame.c src/radio/gdfu_frame.c -o /tmp/gdfu-frame-test && /tmp/gdfu-frame-test
cc -Isrc/radio tests/test_gdfu_state.c src/radio/gdfu_state.c -o /tmp/gdfu-state-test && /tmp/gdfu-state-test
python3 -m unittest discover -s tests -p 'test_*.py'
```

To test the complete update path, flash the bootloader and a matching proxy, then discover the target and transfer a compatible firmware package using GDFU Application. Host tests do not validate radio wiring or flash operations on hardware.

## Versioning

The root `VERSION` file holds the release version, in the form `YYYY.M.K` or `YYYY.M.K-rcN`. Nothing increments it; edit it by hand.

| Push to | `VERSION` | Result |
|---|---|---|
| `dev` | `2026.11.1-rc2` | Pre-release `v2026.11.1-rc2-<short SHA>`, if no tag starts with `v2026.11.1-rc2` yet |
| `dev` | `2026.11.1` | Skipped |
| `master` | `2026.11.1` | Release `v2026.11.1`, if that exact tag does not exist yet. Earlier RC tags do not block it |
| `master` | `2026.11.1-rc2` | Skipped |

A skipped push still runs the host tests, but builds and publishes nothing. To publish another candidate, change `rc2` to `rc3`. To publish the final release, remove the `-rcN` suffix and merge to `master`.

**Run workflow** (`workflow_dispatch`) skips these checks. It deletes any existing release with the computed tag, force-moves the tag to the current commit and publishes again. On `master` this replaces a published production release.

The firmware label adds the `gdfu-` prefix, for example `gdfu-2026.11.1` or `gdfu-2026.11.1-rc2-a1b2c3d`. MeshCore firmware uses this prefix to recognise the bootloader. A dirty working tree adds `-dirty`; staged, unstaged and untracked files count, ignored build output does not. The numeric version stored for applications (`MK_BOOTLOADER_VERSION`) ignores the RC suffix.

For detached local checkouts, set `GDFU_BUILD_BRANCH=dev` to select the development format. Record changes in [CHANGELOG.md](CHANGELOG.md): pre-releases use the `[Unreleased]` section, releases the section named after the version.

## Licensing

Copyright (c) 2026 João Brázio. Licensed under the MIT license. See [LICENSE](LICENSE). Vendored libraries keep their own licenses:

- `lib/nrfx/`, Nordic Semiconductor, BSD-3-Clause.
- `lib/tinyusb/`, MIT.
- `lib/uf2/`, Microsoft, MIT.
- `lib/SEGGER_RTT/`, SEGGER license.
- `lib/sdk*/`, Nordic Semiconductor SDK license.
