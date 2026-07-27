# Public interfaces

Nexting Devices Experimental 0.2 exposes one small control surface between a trusted Host and an authorized nearby device. This page helps developers discover the interfaces. [`SPEC.md`](../SPEC.md) remains normative.

## What is public

- one Bluetooth LE GATT service;
- four newline-delimited message types for profile `approval/1` and one downlink message type for profile `status/1`;
- a Swift Host SDK;
- a Kotlin/JVM Host SDK for Android;
- a portable fixed-buffer C99 device SDK;
- shared vectors, reference implementations, simulator, firmware, and verification rules.

Experimental 0.2 does not expose a Nexting cloud API. There is no public TCP, UDP, HTTP, MQTT, or WebSocket endpoint, no device-to-Agent credential, and no account or session API in this repository. A device communicates with a user's authorized Host/App over BLE — and because the Host may reach the agent anywhere, every compatible device is remote-capable through its Host.

## BLE transport

The device is the BLE peripheral/GATT server. The Host/App is the central/GATT client.

| Surface | UUID | Direction | Product purpose | Required behavior | Implemented by |
| --- | --- | --- | --- | --- | --- |
| Primary Service | `6EADC0DE-0001-4A21-9C5E-1B7F3D9E42A0` | — | Discover a Nexting device | Advertise the service UUID | Host: `Central.swift`; device: `firmware/zephyr/src/main.c` |
| Downlink | `6EADC0DE-0002-4A21-9C5E-1B7F3D9E42A0` | Host → device | Present and resolve requests | Encrypted write; with-response is the default | Host: `Central.swift`; device: `main.c` + C stream |
| Uplink | `6EADC0DE-0003-4A21-9C5E-1B7F3D9E42A0` | device → Host | Report physical answers or errors | Encrypted notification subscription | Device: `main.c`; Host: `Central.swift` + Swift line decoder |
| Device Info | `6EADC0DE-0004-4A21-9C5E-1B7F3D9E42A0` | device → Host | Negotiate version, profiles, model, firmware, limits, and declared capabilities such as `statusSlots` | Read before approval traffic | Device: `main.c`; Host: `DeviceInfo.swift` / `Central.swift` |

One compact UTF-8 JSON object plus `\n` is one logical frame. BLE fragments preserve byte order and never interleave logical frames. The default complete-frame ceiling is 4096 bytes including the newline; every compatible implementation supports at least 512 bytes.

The Device Info characteristic is also where the [capability declaration](foundation-development.md#beyond-experimental-02-the-capability-roadmap) grows: future profiles add capability entries there before any new traffic flows.

## Wire messages

| Message | Direction | Product meaning | Fail-closed rule | Source and implementations |
| --- | --- | --- | --- | --- |
| `present` | Host → device | Show one bounded Allow/Deny request with a relative TTL | Invalid fields do not change current state | vectors; `reference/js/src/protocol.mjs`; `Protocol.swift`; `nexting_device.c` |
| `answer` | device → Host | Report the user's locked Allow or Deny choice | Unknown, stale, expired, unauthorized, or changed choices do not commit | vectors; relay/state tests; Swift and C state |
| `resolved` | Host → device | End the current request as answered, expired, cancelled, or replaced | A nonmatching ID does not clear another request | vectors; `relay.mjs`; `Relay.swift`; C state |
| `error` | either direction | Report a bounded protocol failure when useful | An error never carries authority to approve | vectors and all three strict codecs |
| `status` (profile `status/1`) | Host → device | Replace the full rendered state of up to 8 anonymous agent slots (idle, thinking, working, complete, needs_input, error, plus an optional 64-byte label) | A malformed frame is discarded whole; the previous rendered state stays; never touches approval state | `status-v1.json` vectors; all three strict codecs |

Exact fields, enums, byte limits, canonical JSON rules, nesting limits, and state transitions are in [the specification](../SPEC.md). Executable examples live in the shared vectors: [approval-v1.json](../protocol/vectors/approval-v1.json) and [status-v1.json](../protocol/vectors/status-v1.json). A Host sends `status` only to a device whose Device Info declares `statusSlots` of at least 1; status rendering is volatile and clears on disconnect, reboot, or a new bond.

## Swift Host SDK

| Public type | Use it for | Implementation |
| --- | --- | --- |
| `NextingDeviceMessage` / `NextingDeviceCodec` | Strict wire values and newline-terminated encoding/decoding | `sdk/swift/Sources/NextingDeviceKit/Protocol.swift` |
| `NextingDeviceAgentStatus` / `NextingDeviceAgentState` | One agent-status slot value for profile `status/1` | `Protocol.swift` |
| `NextingDeviceLineDecoder` | Bounded fragmented input | `Framing.swift` |
| `NextingDeviceInfo` | Capability negotiation, including the optional `statusSlots` declaration | `DeviceInfo.swift` |
| `NextingDeviceAuthorizationStore` / `NextingDeviceAuthorizationPolicy` | Explicit enrollment and deny-by-default authorization | `Authorization.swift` |
| `NextingDevicePromptRelay` | One-current-prompt state, TTL, races, retries, and two-phase completion | `Relay.swift` |
| `NextingDeviceRelayTransport` | Transport interface consumed by the coordinator | `Coordinator.swift` |
| `NextingDeviceRelayCoordinator<Context>` | Map product prompt context into public request state | `Coordinator.swift` |
| `NextingDeviceCentral` | CoreBluetooth discovery, setup, bounded writes, and notifications | `sdk/swift/Sources/NextingDeviceKit/Central.swift` |
| `NextingDeviceAnswerClaimGate` | Share single-consumption ownership between phone and hardware | `State.swift` |

The Host calls `present(context:summary:ttlMs:)`. A routed hardware answer invokes the Host-provided `answerPrompt` closure. The Host must call `answerSucceeded(requestId:)` only after its Agent action succeeds, or `answerFailed(requestId:)` after an authoritative failure. Phone-first answers call `phoneAnswerStarted()` before the same completion path. Cancellation calls `cancel()`. Agent-state indicators go through `publishStatus(_:)`, which sends a full-replacement status frame only when the connected device declared `statusSlots` and the link is authorized.

Production Hosts must supply enrollment and revocation. Peripheral-name matching is not a release identity mechanism.

## Kotlin Host SDK

The Kotlin/JVM module mirrors the bounded wire and Device Info models needed by
Android. `DeviceInfoCodec` decodes typed identity, buttons, rotary controls,
display, haptics, standard Battery Service support, and inert vendor facts.
`ProtocolCodec` implements the same canonical newline-delimited
`approval/1`/`status/1` values and limits. Android owns Bluetooth permission,
GATT lifecycle, encrypted authorization storage, account metadata, and UI.

## Portable C99 device SDK

| Public API | Use it for | Implementation |
| --- | --- | --- |
| `nexting_device_decode` / `nexting_device_encode` | Strict fixed-capacity messages | `sdk/c/src/nexting_device.c` |
| `nexting_device_stream_init` / `nexting_device_stream_push` / `nexting_device_stream_reset` | Caller-owned bounded receive storage | `nexting_device.c` stream section |
| `nexting_device_state_init` | Start in Idle with no actionable request | `nexting_device.c` state section |
| `nexting_device_state_on_present` | Enter or replace Pending using a monotonic deadline | `nexting_device.c` state section |
| `nexting_device_state_choose` | Lock the first local choice and create an answer | `nexting_device.c` state section |
| `nexting_device_state_retry_answer` | Retry the same locked answer after the interval | `nexting_device.c` state section |
| `nexting_device_state_on_resolved` | Clear the matching request | `nexting_device.c` state section |
| `nexting_device_state_tick` | Expire a request from a monotonic clock | `nexting_device.c` state section |
| `nexting_device_state_disconnect` | Clear volatile request and retry state | `nexting_device.c` state section |
| `nexting_device_status_init` / `nexting_device_status_on_message` / `nexting_device_status_disconnect` | Volatile eight-slot agent-status rendering with full replacement and disconnect clear | `nexting_device.c` status section |

The caller owns every buffer. The core does not initialize Bluetooth, allocate memory, read GPIO, drive an LED, persist an approval, or provide a wall clock. The Zephyr reference firmware does not declare `statusSlots` until per-slot indicator rendering is implemented and board-verified, so Hosts send it no status traffic today.

## Verify the interfaces

```sh
npm run test:reference
swift test --package-path sdk/swift
cmake -S sdk/c -B /tmp/nexting-device-c -DNEXTING_DEVICE_SANITIZE=ON
cmake --build /tmp/nexting-device-c
ctest --test-dir /tmp/nexting-device-c --output-on-failure
npm run test:firmware
```

The JavaScript reference verifies readable wire and relay behavior. Swift verifies Host APIs and transport policy. C sanitizers verify the fixed-buffer device API. The firmware contract verifies the Zephyr adapter's required security and transport structure. Physical behavior still requires the board checklist.

## Required limits and guarantees

| Contract | Experimental 0.2 |
| --- | --- |
| Wire version | `1` |
| Profiles | `approval/1` and `status/1` |
| Choices | exactly `allow` and `deny` |
| Request ID | 1–64 allowed ASCII bytes |
| Summary | at most 240 UTF-8 bytes |
| TTL | 1–300000 monotonic milliseconds |
| Logical frame | at most 4096 bytes including `\n` |
| Active prompts | one |
| Approval storage | volatile; clear on disconnect and reboot |
| Status slots | 0–8 per device, declared as `statusSlots` in Device Info |
| Status states | `idle`, `thinking`, `working`, `complete`, `needs_input`, `error` |
| Status label | optional, 1–64 UTF-8 bytes, no control characters |
| Status storage | volatile; full replacement per frame; clear on disconnect and reboot |

## Adding an interface

A new transport, profile, field, language SDK, or platform is not public merely because code exists. First update the product behavior and failure behavior, then `SPEC.md`, shared vectors, reference implementation, affected SDKs, tests, version/CHANGELOG, and this catalog.

Typed Device Info counts describe hardware only. Command keys, navigation,
rotary input, text content, voice, and configuration still require their own
versioned profiles, vectors, and evidence before devices may claim them. Wi-Fi,
HTTP, MQTT, USB, multiple prompts, and production identity also require
explicit future contracts. Nothing may silently reuse the Experimental 0.2
compatibility claim.
