# Choose an implementation track

Use the shortest track that matches what you are building. Every track implements the same `approval/1` product behavior; none creates a second protocol.

## Before you start

Read [the foundation blueprint](foundation-development.md) and [public interface catalog](interfaces.md). Use [`SPEC.md`](../SPEC.md) and [the shared vectors](../protocol/vectors/approval-v1.json) as the behavior source.

## Track 1: Run a reference board

Choose this track when you want a working two-button developer reference before changing firmware.

### 1. Choose one exact target

| Board | Zephyr target |
| --- | --- |
| nRF52840 DK | `nrf52840dk/nrf52840` |
| XIAO nRF52840 / Sense | `xiao_ble/nrf52840/sense` |
| XIAO ESP32-C3 | `xiao_esp32c3/esp32c3` |
| XIAO ESP32-S3 | `xiao_esp32s3/esp32s3/procpu` |

### 2. Build from a configured Zephyr workspace

Run from the repository root:

```sh
nexting_devices_root="$(pwd -P)"
west build -p always -b nrf52840dk/nrf52840 "$nexting_devices_root/firmware/zephyr"
west build -p always -b xiao_ble/nrf52840/sense "$nexting_devices_root/firmware/zephyr"
west build -p always -b xiao_esp32c3/esp32c3 "$nexting_devices_root/firmware/zephyr"
west build -p always -b xiao_esp32s3/esp32s3/procpu "$nexting_devices_root/firmware/zephyr"
```

Build only the target you own. Espressif targets require `hal_espressif` blobs and the matching toolchains. The pinned CI uses Zephyr 4.3.0, Zephyr SDK 0.17.4, and west 1.5.0.

### 3. Wire and use it

Follow [the firmware wiring table](../firmware/zephyr/README.md), flash the generated image, remove any stale phone-side bond, and enroll the device in the official Nexting App when developer enrollment is available. Trigger one ordinary Allow/Deny request, verify Pending output, press each choice in separate requests, then hold both buttons for three seconds to verify local bond reset.

### 4. Record the right claim

A successful compiler artifact is Build verified. Use [the complete board checklist](board-verification.md) before recording Board verified.

## Track 2: Port a new MCU or RTOS

Choose this track when your platform is not one of the four Zephyr references.

### 1. Prove the platform contract

The platform must provide:

- BLE peripheral/GATT server support;
- encrypted bonding and local revocation;
- caller-owned receive and transmit buffers;
- a monotonic millisecond clock;
- two unambiguous local actions;
- a Pending output;
- volatile approval cleanup on disconnect and reboot.

### 2. Reuse the C core

Compile `sdk/c/src/nexting_device.c` and include `sdk/c/include/nexting_device.h`. Feed ordered Downlink fragments through `nexting_device_stream_push`. Route complete `present` and `resolved` messages into the state API.

On input, call `nexting_device_state_choose` once. Encode and notify the returned answer. Use `nexting_device_state_retry_answer` only for the locked choice. Call `nexting_device_state_tick` from the monotonic clock.

On disconnect or reboot, call `nexting_device_state_disconnect` and `nexting_device_stream_reset` before clearing outputs. Do not restore an approval from flash.

### 3. Keep the adapter thin

Your adapter owns BLE, GPIO, timers, bond lifecycle, and transport buffers. It does not implement JSON parsing, request replacement, TTL policy, choice locking, or retry semantics.

### 4. Verify before claiming support

Run the desktop C sanitizer suite, compile the exact target, then complete the real-board checklist. Add the board to [hardware support](hardware-support.md) only at the evidence level it earned.

## Track 3: Maintain protocol tooling

Choose this track for Rust, TypeScript, Python, or another protocol
implementation or conformance tool. It does not create an Agent adapter or a
custom App.

1. Load `protocol/vectors/approval-v1.json` directly; do not copy the cases.
2. Make every valid vector decode and canonically re-encode.
3. Make every invalid vector fail closed.
4. Implement the newline-inclusive 4096-byte frame ceiling and bounded oversize discard.
5. Keep product authorization and Agent actions in the official Nexting App; do not add them to the public package.
6. Add the language to public CI.
7. Update [public interfaces](interfaces.md), [conformance](conformance.md), and `CHANGELOG.md`.

A language SDK is not official because it compiles. It becomes maintained only after shared-vector parity, state tests, documented ownership, and CI.
