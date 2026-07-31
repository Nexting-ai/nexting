# Choose an implementation track

Use the shortest track that matches what you are building. Every track implements the same `approval/1` product behavior; none creates a second protocol.

## Before you start

Read [the foundation blueprint](foundation-development.md) and [public interface catalog](interfaces.md). Use [`SPEC.md`](../SPEC.md) and [the shared vectors](../protocol/vectors/approval-v1.json) as the behavior source.

## Track 1: Run a reference board

Choose this track when you want a working two-button developer reference before changing firmware.

### 1. Choose one exact target

| Board                 | Zephyr target                 |
| --------------------- | ----------------------------- |
| nRF52840 DK           | `nrf52840dk/nrf52840`         |
| XIAO nRF52840 / Sense | `xiao_ble/nrf52840/sense`     |
| XIAO ESP32-C3         | `xiao_esp32c3/esp32c3`        |
| XIAO ESP32-S3         | `xiao_esp32s3/esp32s3/procpu` |

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

Follow [the firmware wiring table](../firmware/zephyr/README.md), flash the generated image, remove any stale phone-side bond, and enroll the device in the Host/App. Trigger one ordinary Allow/Deny request, verify Pending output, press each choice in separate requests, then hold both buttons for three seconds to verify local bond reset.

### 4. Record the right claim

A successful compiler artifact is Build verified. Use [the complete board checklist](board-verification.md) before recording Board verified.

## Track 2: Integrate a Host or App

Choose this track when an App, desktop Host, or trusted gateway already owns an Agent approval and needs a physical control surface.

### 1. Add the public package

Consume `sdk/swift` as a Swift package and import `NextingDeviceKit`. Do not copy Swift files into the product target.

### 2. Supply product-owned policy

The Host owns:

- enrollment and revocation UI;
- a stable authorization decision;
- internal prompt context;
- the minimum public summary;
- the final Agent answer operation;
- risk policy and phone-side UI.

The SDK owns BLE transport, public request state, TTL, races, dedupe, framing, and fail-closed parsing.

### 3. Create the transport and coordinator

Create a `NextingDeviceAuthorizationPolicy` from the product's enrollment store. Pass `authorizationPolicy.isAuthorized` to both `NextingDeviceCentral` and `NextingDeviceRelayCoordinator<Context>`. Use the product's real prompt model as `Context`. The coordinator's `answerPrompt` closure must call the product's existing authoritative Agent answer path with that context and the returned `NextingDeviceChoice`.

The product calls `present(context:summary:ttlMs:)` with its real prompt context, a UTF-8-safe minimum summary, and a TTL from 1 through 300000 milliseconds. It stores the optional request ID returned by that call beside the same internal prompt; a nil result means the public request was rejected and must not be shown as delivered.

When the Agent operation succeeds, call `answerSucceeded(requestId:)`. When it fails, call `answerFailed(requestId:)`. A phone-first path calls `phoneAnswerStarted()` before the same success/failure completion. Cancellation calls `cancel()`. A monotonic timer calls `tick()` while work is pending.

Never treat the `answerPrompt` callback as success by itself. The physical choice becomes authoritative only after the existing Agent answer path succeeds.

### 4. Test without a board

Run the [macOS BLE simulator](../examples/macos-device-simulator/README.md) with a real iPhone. The iOS Simulator cannot exercise the Bluetooth Host path.

## Track 3: Port a new MCU or RTOS

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

## Track 4: Integrate the ILX MultiPad over USB CDC

Choose this track when the hardware is the open-source ILX MultiPad. It is a
USB CDC binding for the same public JSON frames, not a second approval protocol
and not automatic Nexting App USB enrollment.

1. Read the [MultiPad USB guide](multipad-usb.md) and identify the PCB variant.
2. Build `firmware/multipad` and run `npm run test:multipad` before touching the
   board.
3. Add `nexting_multipad_adapter.c/.h` and `sdk/c/src/nexting_device.c` to the
   upstream STM32 project. Preserve HID and the legacy `AA BB xx` commands.
4. Verify the original flash backup and boot path. Module boards can use the
   serial bootloader; FPC boards require SWD/J-Link.
5. Record CDC echo, `present → answer → resolved`, expiry, and disconnect
   evidence before claiming a physical integration.

The Host still owns USB authorization, Agent routing, and final action sinks.
The device sees only bounded public frames and never receives credentials.

The [first MultiPad case](cases/multipad-first-case.md) is the reproducible
walkthrough for this track. It pins the upstream commit, separates the original
HEX from the Nexting build, maps the first two keys to Allow/Deny, and records
the evidence required before calling the board compatible.

## Track 5: Maintain a new language SDK

This is the language SDK route formerly listed as **Track 4: Maintain a new language SDK**;
the number moved only to make the MultiPad hardware path visible.

Choose this track for Kotlin, Rust, TypeScript, Python, or another Host/device implementation.

1. Load `protocol/vectors/approval-v1.json` directly; do not copy the cases.
2. Make every valid vector decode and canonically re-encode.
3. Make every invalid vector fail closed.
4. Implement the newline-inclusive 4096-byte frame ceiling and bounded oversize discard.
5. Implement the one-prompt relay with monotonic TTL, replacement, duplicate rejection, choice locking, phone/device races, and two-phase Agent completion.
6. Add the language to public CI.
7. Update [public interfaces](interfaces.md), [conformance](conformance.md), and `CHANGELOG.md`.

A language SDK is not official because it compiles. It becomes maintained only after shared-vector parity, state tests, documented ownership, and CI.
