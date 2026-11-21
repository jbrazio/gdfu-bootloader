# Architecture

Cross-component behavior reference. Byte-level formats live in [BLE protocol](https://github.com/jbrazio/gdfu-proxy/blob/master/docs/PROTOCOL_BLE.md) and [GDFU RF protocol](PROTOCOL_DFU_GFSK.md). Component detail lives in [Bootloader](BOOTLOADER_SPECS.md), [Proxy](https://github.com/jbrazio/gdfu-proxy/blob/master/docs/PROXY_SPECS.md) and [App](https://github.com/jbrazio/gdfu/blob/master/docs/APP_SPECS.md) specifications.

```text
App ──BLE GATT──▶ ESP32 proxy ──SX1262 GFSK, GDFU v1──▶ nRF52840 bootloader
```

## Responsibilities

| Component | Owns |
|---|---|
| App ([gdfu](https://github.com/jbrazio/gdfu)) | Target selection, firmware source and package validation, transfer UI, cancel/recovery, offline storage. Speaks only BLE. |
| Proxy ([gdfu-proxy](https://github.com/jbrazio/gdfu-proxy)) | BLE GATT server, SX1262 radio, target discovery, bounded queue, stop-and-wait RF forwarding, retries and deadlines, LittleFS package cache. Generates session IDs. |
| Bootloader (this repository) | Target beaconing, GDFU receive state machine, flash erase/write, image validation, activation. Generates target IDs. Based on [OTAFIX](https://github.com/oltaco/Adafruit_nRF52_Bootloader_OTAFIX) (Adafruit nRF52 Bootloader lineage); `heltec_t114` (validated) and six other boards (build-only) build GFSK/USB with `GFSK_DFU_ENABLED=1`, or stock BLE/USB when unset or `0`. Credits and licenses: root [README](../README.md#license). |
| `tests/test-vectors.json` | Canonical codec vectors. The app and proxy carry copies; keep them identical. |

Implementation and automated tests are authoritative where prose disagrees.

## Accepted versus confirmed progress

The proxy reports two byte counts. `acceptedBytes` counts bytes buffered by the proxy from BLE. `confirmedBytes` counts bytes the target has acknowledged over RF. Only `confirmedBytes` shows data the target has received. The app must not treat accepted bytes as delivered.

## Backpressure

The proxy queue holds 8 chunks (`MAX_QUEUE_CHUNKS`), 1,536 bytes at the 192-byte maximum payload. When full, the proxy rejects BLE writes with a busy response until space frees. The RF side is stop-and-wait: one unacknowledged DATA frame at a time. Queue capacity and RF in-flight window are separate.

## Update lifecycle

1. A target without a valid application, or entered into GFSK DFU, beacons every 5 s with its stable target ID.
2. The proxy discovers beacons (TTL 15 s, 16 entries). The app lists targets and selects one.
3. The proxy sends `START` with application-only metadata. The target rejects non-application modes and images larger than flash, then erases the application pages covering the declared image size before acknowledging. This erase runs on every fresh session, so a prior failed session cannot leave programmed bits behind. The proxy allows 15 s for the START ACK (erase takes about 9 s for 109 pages).
4. The proxy sends `SET_TX_POWER`. Only after its ACK does it report `STREAMING`.
5. First DATA at offset zero carries the Nordic init packet (device type, revision, SoftDevice list, final image CRC16). Later DATA carries application bytes.
6. The proxy sends `FINISH` (2 s ACK window per attempt, five total transmissions). The target verifies CRC16 and metadata, ACKs `FINISH`, then boots the application.

Timeouts: DATA ACK 2 s; control frames 10 s; START ACK 15 s; session expiry 30 s without valid peer traffic; the proxy's `GDFU_MAX_RETRIES = 5` allows five total transmissions for retryable frames, including the initial one. Error codes and state machine: [GDFU RF protocol](PROTOCOL_DFU_GFSK.md).

## Flash layout (nRF52840 target)

| Range | Content |
|---|---|
| `0x00000000–0x00001000` | MBR |
| `0x00001000–0x00026000` | SoftDevice |
| `0x00026000–0x000EA000` | Application |
| `0x000EA000–0x000F4000` | Application data / filesystem |
| `0x000F4000–0x000FD800` | Bootloader code |
| `0x000FD800–0x000FE000` | Bootloader config |
| `0x000FE000–0x000FF000` | MBR parameters |
| `0x000FF000–0x00100000` | Bootloader settings |

Addresses are for s140 6.1.1 (T114). The application starts at the installed SoftDevice end, read at runtime (`CODE_REGION_1_START`); on s140 7.3.0 boards (XIAO, XIAO Sense, P1) it is `0x00027000`.

DFU writes the application starting at that address (`0x00026000` on T114). A GDFU session never writes the SoftDevice or bootloader regions.

## Failure and recovery

- **Single bank.** The new image overwrites the running one. An interrupted or failed update, or power loss mid-transfer, leaves a partial or erased application. No rollback exists.
- **Target.** Without a valid application the bootloader re-enters GDFU advertising. Timeout, validation failure or abort clears volatile session state and returns to advertising. Recover by starting a fresh update from offset zero.
- **Proxy.** `ABORT` cancels an active transfer and returns the proxy to idle. A START chain (five total transmissions, including the initial one) in progress makes the proxy answer busy until it ends. If the proxy stops receiving target ACKs while beacons still arrive, power-cycle the proxy.
- **Stale target session.** A fresh `START` clears a stale target session. If the target is unresponsive, re-enter GFSK DFU on it before judging the link dead.

## CI gates

Each repository runs its own CI. Here: `python3 -m unittest discover -s tests -p 'test_*.py'` (protocol codec, C codec and session rules on host, Make checks) plus GFSK builds for every board that defines `SX1262_*` pins. Proxy and app CI build their own firmware/app. No CI runs hardware, BLE, SWD, SDR or RF tests.

## Security boundary

BLE and RF are unauthenticated. CRC32, CRC16, target IDs and session IDs provide integrity and isolation only. They are not sender authentication and do not establish firmware origin.

## Evidence and scope limits

- Application images only. No SoftDevice or bootloader update.
- 433.175 MHz is live-validated. 864.100 MHz builds and is selectable but lacks equivalent end-to-end or regulatory evidence.
- Not externally validated: multiple nearby targets, Android handset, RF/regulatory measurement.
- No sender authentication, encryption or rollback protection.
