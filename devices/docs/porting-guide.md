# Porting a device

The shortest route to a new Nexting device is to keep the protocol core unchanged and write a thin platform adapter.

Before adding a platform, read [the public foundation](foundation-development.md), [interface catalog](interfaces.md), and [MCU/RTOS implementation track](implementation-tracks.md#track-3-port-a-new-mcu-or-rtos).

## The five pieces

| Piece | Reuse or implement | Responsibility |
| --- | --- | --- |
| Protocol and state | Reuse `sdk/c` | JSON, UTF-8 bounds, newline framing, TTL, choice locking, retry, resolution. |
| BLE adapter | Implement | Four GATT UUIDs, encrypted writes, notifications, Device Info, connection lifecycle. |
| Inputs | Implement | Convert two trustworthy local actions into `ALLOW` or `DENY`; debounce before calling the core. |
| Output | Implement | Show Pending/Idle without treating an LED as authority. |
| Product security | Implement and document | Pairing UX, bond storage/revocation, update path, physical threat model. |

Do not copy the JavaScript reference into firmware and do not parse the protocol with substring searches. The C99 core exists to make ports behave identically.

## Runtime flow

1. Allocate a receive buffer of 512–4096 bytes and initialize `nexting_device_stream_t`.
2. Feed every downlink ATT fragment to `nexting_device_stream_push` in order.
3. Pass complete `present` and `resolved` messages to `nexting_device_state_t`.
4. On a debounced button press, call `nexting_device_state_choose`.
5. Encode its answer and notify the host. Keep that exact choice locked.
6. Call `nexting_device_state_retry_answer` from a monotonic timer and resend when it returns true.
7. Call `nexting_device_state_tick` to clear local expiry.
8. On disconnect, unsubscribe, or reboot, reset the stream and state. Never restore an approval from flash.

## BLE surface

Experimental 0.2 exposes one public connection surface: the BLE GATT service in `SPEC.md`. It opens no TCP, UDP, HTTP, MQTT, WebSocket, or cloud port.

- Downlink is host-to-device Write; Write With Response is the safe default.
- Uplink is device-to-host Notify.
- Device Info is Read and may be visible before authorization for compatibility discovery.
- Approval traffic and the notification subscription require encryption.

The device is not an Agent client. It receives an opaque request ID, bounded human summary, two fixed choices, and a relative TTL. Do not add account IDs, sessions, terminal IDs, cloud routes, API keys, or Agent credentials.

## Memory profile

The message limit is negotiated through Device Info. A constrained port may advertise 512 bytes. It must still accept the complete approval profile and a 240-byte UTF-8 summary. Buffers are caller-owned; the official core does not allocate heap memory.

## Verification before claiming support

Run the shared C tests, compile the exact board target, then complete `board-verification.md` on real hardware. Record:

- board revision and chip;
- toolchain and firmware commit;
- phone/App version;
- pairing method;
- every pass/fail result and serial log;
- measured current if the port claims battery suitability.

A GitHub pull request with code but no board evidence can be accepted as a community source port. It cannot receive a `Board verified` label.
