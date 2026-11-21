# GDFU v1 RF protocol

Canonical contract for the GFSK radio link between the ESP32 proxy and the nRF52840 bootloader. BLE transport side: [BLE protocol](https://github.com/jbrazio/gdfu-proxy/blob/master/docs/PROTOCOL_BLE.md). Component behavior: [Bootloader](BOOTLOADER_SPECS.md) and [Proxy](https://github.com/jbrazio/gdfu-proxy/blob/master/docs/PROXY_SPECS.md).

## Scope and security

GDFU transfers **application images only**. The sender supplies application metadata in `START` and the init packet with the final image CRC16; the target rejects non-application modes, invalid metadata, unsafe flash ranges, and a wrong CRC before activation.

The link is unauthenticated: any device in range can observe beacons and send frames. The frame CRC32, the final image CRC16, and the target and random session IDs provide integrity and session isolation. They are **not sender authentication** and do not prove firmware origin. There is no encryption and no rollback protection.

## Profiles

| ID | Name | Frequency | Evidence |
|---:|---|---|---|
| 1 | `GDFU_PROFILE_ID_433` | 433.175 MHz | Live-validated end to end on hardware. |
| 2 | `GDFU_PROFILE_ID_868` | 864.100 MHz | Buildable and selectable at compile time. No end-to-end or regulatory evidence; parity with 433.175 MHz is not claimed. |

Both profiles use the same modulation. A target accepts `START` only on its compiled profile (header byte 6 must equal it). Regional regulatory compliance for either frequency is the deployer's responsibility; no band-specific claim is made here.

| Parameter | Profile 1 | Profile 2 |
|---|---|---|
| Modulation | GFSK, 80,000 bit/s, ±28 kHz deviation, Gaussian BT 0.5 | GFSK, 80,000 bit/s, ±28 kHz deviation, Gaussian BT 0.5 |
| RX bandwidth | 187.2 kHz | 187.2 kHz |
| Preamble | 32 bits, 16-bit detector |
| Sync word | 16 bits, `0x13 0x37` |
| Packet mode | Variable length |
| PHY CRC | 2 bytes, non-inverted (SX126x register defaults: init `0x1D0F`, polynomial `0x1021`; not overridden in software) |
| Whitening | Enabled (`0x01`), SX126x default seed (not overridden) |
| Default TX power | 10 dBm (compiled into both ends) |
| PA | SX1262 `SetPaConfig 0x04 0x07 0x00 0x01`, power via `SetTxParams` |

Both ends apply the mandatory SX126x TX modulation errata workaround (register `0x0889` bit 2) and RX gain boost (`0x08AC = 0x96`). `CalibrateImage` uses the 430–440 MHz coefficients for profile 1 and the 863–870 MHz coefficients for profile 2.

TX power is session-bound: the proxy sends `SET_TX_POWER` with one unsigned byte, 10–22 inclusive, after the `START` ACK. The target applies it only while a receiving session is active and reverts to the compiled 10 dBm default on every recovery. It changes TX power only, never profile or frequency.

## PHY packet versus GDFU frame

The SX1262 runs in variable-length packet mode. Before each transmit, both endpoint drivers configure the PHY payload length to the encoded GDFU frame length, then write that raw GDFU frame at FIFO offset zero. The SX1262 therefore carries the complete 34–226-byte GDFU frame; it does not consume the leading `G` (`0x47`) as a length byte.

On receive, both drivers obtain the received payload length from `GetRxBufferStatus`, read exactly that many FIFO bytes, then pass them to the GDFU decoder. The decoder requires the received byte count to equal the header-declared frame length and verifies its independent CRC32.

The PHY packet length and PHY CRC are radio machinery, not GDFU fields. The PHY CRC protects each RF packet; GDFU CRC32 protects the decoded GDFU frame.

## GDFU frame

30-byte header, 0–192 payload bytes, 4-byte CRC32: 34–226 bytes total. All multibyte fields are big-endian.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic `GDFU` |
| 4 | 1 | protocol version, `1` |
| 5 | 1 | packet type |
| 6 | 1 | profile ID |
| 7 | 1 | flags, must be zero; nonzero is rejected |
| 8 | 4 | target ID |
| 12 | 8 | session ID |
| 20 | 4 | sequence |
| 24 | 4 | offset |
| 28 | 2 | payload length |
| 30 | 0–192 | payload |
| last 4 | 4 | CRC32 (IEEE 802.3 / zlib, big-endian, over all preceding bytes) |

The decoder rejects frames shorter than 34 bytes, wrong magic, wrong version, nonzero flags, a declared length above 192, a length that does not match the actual byte count, trailing data, and CRC32 mismatch.

ID rules:

- **Target ID**: stable unsigned 32-bit value: the low 32 bits of the BLE MAC (FICR `DEVICEADDR[0]`; zero is remapped to `0x01020304`). It survives reboots, so a target that falls back into the bootloader after `FINISH` reappears with the same ID. Zero is invalid. The beacon is therefore linkable to the BLE MAC (deliberate trade-off).
- **Session ID**: proxy-generated random unsigned 64-bit value, nonzero, unique per update.
- **Sequence**: unsigned 32-bit RF packet number, `0` for `START`, incremented by the proxy per transmitted frame. `ACK`/`NACK` echo it.
- **Offset**: image byte offset for `DATA`; confirmed image offset for `ACK`; error code for `NACK`.

## Packet types

| Value | Type | Payload / behavior |
|---:|---|---|
| 1 | `BEACON` | Empty payload. Sent every 5 s while the target is advertising. Carries the target ID and the compiled profile in the header. |
| 2 | `START` | 16-byte packed Nordic `dfu_start_packet_t` (little-endian struct; see Application metadata). Target validates target ID, profile, mode, and sizes, then erases before acknowledging. |
| 3 | `READY` | Reserved value. No current sender: `START` acceptance is the `ACK`. |
| 4 | `DATA` | Image byte range at exact `offset`. Offset zero carries the init packet. The receiver writes only fresh in-order data. |
| 5 | `ACK` | Echoed sequence; offset field is the confirmed image offset (init packet included). |
| 6 | `NACK` | Echoed sequence; offset field is the error code. The sender retries only the permitted frame. |
| 7 | `STATUS` | Reserved value. Not currently sent. |
| 8 | `FINISH` | Empty payload. Requests postvalidation and activation. |
| 9 | `COMPLETE` | Reserved value. No current sender: `FINISH` success is an `ACK`, after which the target boots the application. |
| 10 | `ABORT` | Empty payload. Target acknowledges, clears volatile session state, and returns to advertising. |
| 11 | `ERROR` | Reserved value. Failures use `NACK`. |
| 12 | `SET_TX_POWER` | One unsigned dBm byte, 10–22 inclusive, session-bound. Proxy sends it after the `START` ACK; the target ACKs only after the SX1262 accepts the new power. Only after this ACK does the proxy report `STREAMING` on BLE. |

Decoders accept all twelve values (the reference codec in `tests/protocol.py` and both C implementations), but current firmware emits only `BEACON`, `START`, `DATA`, `FINISH`, `ABORT`, `SET_TX_POWER`, `ACK`, and `NACK`.

## Sessions, sequence, and offset rules

- `START` is accepted only while the target is advertising, only for the current target ID and the compiled profile, and only with a nonzero session ID. A wrong profile answers `NACK` 2; a `START` while another session is still open answers `NACK` 14 (a retried `START` for the already-open session is re-ACKed instead). Frames of any type for another target ID are dropped silently. A `START` received while advertising first resets the single-bank DFU core, so a stale half-written session cannot corrupt the fresh one.
- Every frame after `START` must carry the active session ID; mismatch answers `NACK` 4. Any frame carrying the active session ID resets the target's idle clock.
- `DATA` is accepted only at `offset == confirmed_offset`. A duplicate of the most recent `DATA` (same sequence and offset) is re-ACKed without a flash write or progress advance. Other unexpected offsets or sequences answer `NACK` 6 and never alter flash state.
- The first `DATA` at offset zero carries the Nordic init packet. Later `DATA` carries application bytes and must have a payload length that is a multiple of 4; senders pad the final chunk with `0xFF`.
- `FINISH`: the target verifies the final image CRC16 and the metadata. Success answers `ACK`, activates the image, and reboots into the application. Failure answers `NACK` 9 and recovers.
- `ABORT`: the target answers `ACK` and recovers. Recovery clears volatile session state, invalidates image markers, reapplies the compiled 10 dBm default, and returns to advertising for a fresh offset-zero `START`.

## Timing and retries

| Stage | Value |
|---|---|
| Beacon interval | 5 s, advertising only |
| `DATA` ACK deadline | 2 s per stage (transmit window and ACK window) |
| `SET_TX_POWER` ACK deadline | 10 s per attempt, up to `GDFU_MAX_RETRIES = 5` total transmissions. The target re-applies the same power and re-ACKs a duplicate. |
| `FINISH` ACK deadline | 2 s per attempt (`GDFU_FINISH_ACK_TIMEOUT_MS`), up to `GDFU_MAX_RETRIES = 5` total transmissions. The target re-ACKs a duplicate `FINISH` for 12 s after the first (`GDFU_FINISH_ACK_WINDOW_MS`). |
| `ABORT` ACK deadline | 10 s. A BLE-initiated `ABORT` is a single best-effort transmission; the disconnect-recovery `ABORT` uses up to `GDFU_MAX_RETRIES` total transmissions. |
| `START` ACK deadline | 15 s; proxy code starts at attempt 1 and re-arms only while `attempt < GDFU_MAX_RETRIES`, so `GDFU_MAX_RETRIES = 5` permits five total transmissions, 200 ms apart after failed attempts. The target ACKs only after flash erase; this cannot accommodate the estimated 16.7 s erase for a maximum `0xC4000` image. |
| Active-session timeout | 30-second active-session timeout without valid peer traffic, enforced on both proxy and target |
| Retry budget | `GDFU_MAX_RETRIES = 5` is the proxy's maximum-attempt limit: `DATA` and `START` get five total transmissions per frame, including the initial transmission (four retransmissions). |

The RF in-flight window is one frame: the proxy sends one `DATA` frame and waits for its `ACK` before the next. The 8-chunk proxy queue measured in chunks is BLE-side buffering, separate from this window ([BLE protocol](https://github.com/jbrazio/gdfu-proxy/blob/master/docs/PROTOCOL_BLE.md)).

## Application metadata and init/data mapping

`START` payload: the Nordic `dfu_start_packet_t` packed little-endian, 16 bytes: `[update_mode:1][padding:3][sd_size:4 LE][bl_size:4 LE][app_size:4 LE]`. `update_mode` is `0x04` (application only); SD and bootloader sizes are zero. The proxy substitutes its own application-only probe payload when a client omits the field.

Init packet: the first `DATA` frame (offset zero) carries the Nordic init packet — device type, device revision, application version, SoftDevice FWID list, then extended data whose first two bytes are the final image CRC16 (Nordic CRC-16/CCITT-FALSE over the exact application image). Shipped packages carry a 14-byte init packet. The target requires device type 82 (`0x52`) and a matching SoftDevice FWID (`0xFFFE` matches any); the CRC16 is verified at `FINISH`.

Application data: every `DATA` after the first carries word-aligned image bytes; the final chunk is padded with `0xFF`.

## Errors

Error codes travel in the `NACK` offset field on RF and in `STATUS.last_error` on BLE. `FILE_STATUS.last_error` is a separate file-management domain ([BLE protocol](https://github.com/jbrazio/gdfu-proxy/blob/master/docs/PROTOCOL_BLE.md)).

| Code | Meaning | Emitted by |
|---:|---|---|
| 1 | unsupported protocol version | contract value; bad versions are dropped by the frame decoder |
| 2 | profile mismatch | target: any `START` target/profile mismatch |
| 3 | wrong target | proxy: `SELECT_TARGET` for an unknown or expired target |
| 4 | wrong or expired session | target: wrong session ID; proxy: 30 s session expiry |
| 5 | malformed frame or CRC mismatch | target: malformed `SET_TX_POWER`; proxy: oversized `START_DFU` payload |
| 6 | sequence or offset mismatch | target: out-of-order `DATA` |
| 7 | retry budget exhausted or timeout | contract value |
| 8 | image metadata/board/range invalid | target: `START` payload or size checks |
| 9 | image hash/CRC invalid | target: `FINISH` validation failure |
| 10 | image metadata or board mismatch | contract value |
| 11 | flash failure | target: erase/init/data flash errors |
| 12 | reserved | scoped to file management (`PACKAGE`); never used by the RF/DFU status domain |
| 13 | radio/BLE transport failure | target: `SET_TX_POWER` radio failure; proxy: RF op failure |
| 14 | target busy (session still open) | target: `START` while another session is open; clears after the 30 s session timeout |

## Test vector

Canonical vectors: `tests/test-vectors.json` (copies in gdfu-proxy `tests/` and gdfu-application `lib/codec.test.ts` must match). The valid frame is a v1 `DATA` packet, profile 1, target `0x01020304`, session `0x0102030405060708`, sequence `0x0A0B0C0D`, offset `0x10111213`, payload `AABB`, CRC32 `0x67F3218B`:

```text
47444655 01 04 01 00 01020304 0102030405060708 0A0B0C0D 10111213 0002 AABB 67F3218B
```

The second vector flips the CRC32 to `...8A` and must be rejected with a CRC error. The app codec (gdfu-application `lib/`), both C codecs, the proxy codec, and the Python codec all check against this file.
