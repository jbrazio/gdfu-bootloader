# Changelog

All notable changes to this project are documented in this file. Format inspired by [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Release versions use `YYYY.M.K` (candidates `YYYY.M.K-rcN`) and are maintained manually in `VERSION`.

## [2026.11.1]

### Added

- Each board's release includes separate 433 MHz and 868 MHz firmware files, identified by `_433` or `_868` in the filename.

- GFSK (GDFU v1) radio DFU transport in `src/radio/`: SX1262 SPI HAL, frame codec, session state machine, and `dfu_transport_radio.c` transport; enabled per board with `GFSK_DFU_ENABLED=1` and `SX1262_*` pins in `board.h`.

### Fixed

- Rejected START now NACKs with a specific code: 14 when a previous session is still open (was always 2, "profile mismatch"). Frames for another target (any type) are dropped silently instead of NACKed with code 4, so a nearby device in the bootloader no longer disrupts another device's transfer.
- A retried START for the already-open session is re-ACKed instead of NACKed as busy (lost ACK no longer aborts a started update).
- A `dfu_init()` failure on START NACKs with 11 (flash failure) instead of a raw `NRF_ERROR_*` value that collided with protocol codes.

### Changed

- Releases are labelled `gdfu-YYYY.M.K`; `dev` pre-releases are `gdfu-YYYY.M.K-rcN-<short SHA>`. Local changes add `-dirty`.

- gFSK link is now 80 kbps / 28 kHz deviation / 187.2 kHz RX bandwidth on both profiles (was 100 kbps / 50 kHz, 234.3 kHz RX on 433 MHz); bootloader and proxy must match.
- Beacon target ID is now the low 32 bits of the BLE MAC (FICR `DEVICEADDR[0]`, shown as the last 4 MAC bytes), the same ID lusofw prints in its `start ota` reply (`OK: 0x…`), so the app shows exactly which repeater is being flashed. It is stable across reboots (was random per boot), so the app can also confirm the new application booted after FINISH.
