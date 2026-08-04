# Public interfaces

Nexting Devices `0.2.0-experimental.2` exposes a bounded physical control
surface between a trusted Host and an authorized nearby device. This page helps
developers discover the interfaces. [`SPEC.md`](../SPEC.md) remains normative.

## What is public

- one Bluetooth LE GATT service;
- nine independently negotiated profiles over one newline-delimited Wire;
- a portable fixed-buffer C99 device SDK;
- a JavaScript protocol reference for conformance and tooling;
- shared vectors, reference firmware, and verification rules.

Experimental 0.2 does not expose a Nexting cloud API. There is no public TCP, UDP, HTTP, MQTT, or WebSocket endpoint, no device-to-Agent credential, and no account or session API in this repository. A device communicates with a user's authorized Host/App over BLE — and because the Host may reach the agent anywhere, every compatible device is remote-capable through its Host.

Agent adapters and the official App implementation remain outside this public
package. This repository does not provide an App SDK or an Agent connection
API.

## BLE transport

The device is the BLE peripheral/GATT server. The Host/App is the central/GATT client.

| Surface | UUID | Direction | Product purpose | Required behavior | Implemented by |
| --- | --- | --- | --- | --- | --- |
| Primary Service | `6EADC0DE-0001-4A21-9C5E-1B7F3D9E42A0` | — | Discover a Nexting device | Advertise the service UUID | official App: private; device: `firmware/zephyr/src/main.c` |
| Downlink | `6EADC0DE-0002-4A21-9C5E-1B7F3D9E42A0` | Host → device | Present and resolve requests | Encrypted write; with-response is the default | official App: private; device: `main.c` + C stream |
| Uplink | `6EADC0DE-0003-4A21-9C5E-1B7F3D9E42A0` | device → Host | Report physical answers or errors | Encrypted notification subscription | device: `main.c`; official App: private |
| Device Info | `6EADC0DE-0004-4A21-9C5E-1B7F3D9E42A0` | device → Host | Negotiate version, profiles, model, firmware, limits, and declared capabilities such as `statusSlots` | Read before approval traffic | device: `main.c`; official App: private |

One compact UTF-8 JSON object plus `\n` is one logical frame. BLE fragments preserve byte order and never interleave logical frames. The default complete-frame ceiling is 4096 bytes including the newline; every compatible implementation supports at least 512 bytes.

The Device Info characteristic carries the
[capability declaration](foundation-development.md#experimental-02-the-capability-set).
The Host sends or accepts a message only when the device declared that
message's exact profile.

## Wire messages

| Message | Direction | Product meaning | Fail-closed rule | Source and implementations |
| --- | --- | --- | --- | --- |
| `present` | Host → device | Show one bounded Allow/Deny request with a relative TTL | Invalid fields do not change current state | vectors; `reference/js/src/protocol.mjs`; `nexting_device.c` |
| `answer` | device → Host | Report the user's locked Allow or Deny choice | Unknown, stale, expired, unauthorized, or changed choices do not commit | vectors; JS relay test fixture; C state |
| `resolved` | Host → device | End the current request as answered, expired, cancelled, or replaced | A nonmatching ID does not clear another request | vectors; JS reference; C state |
| `error` | either direction | Report a bounded protocol failure when useful | An error never carries authority to approve | vectors; JS reference; C99 core |
| `status` (profile `status/1`) | Host → device | Replace the full rendered state of up to 8 anonymous agent slots (idle, thinking, working, complete, needs_input, error, plus an optional 64-byte label) | A malformed frame is discarded whole; the previous rendered state stays; never touches approval state | `status-v1.json` vectors; JS reference; C99 core |
| `nav_present` / `nav_resolved` (`navigation/1`) | Host → device | Present or close a bounded option list | Invalid lists or cursor values do not alter the current menu | `navigation-v1.json`; JS reference; C99 core |
| `nav_move` / `nav_select` (`navigation/1`) | device → Host | Move a cursor or select an index | Sequence gate rejects replay and reordering; Host validates current request | `navigation-v1.json`; JS reference; C99 core |
| `keymap` / `key_event` (`keys/1`) | both | Render Host-owned key labels/light state and report generic physical key activity | Device never chooses Agent semantics; sequence gate rejects duplicate input | `keys-v1.json`; JS reference; C99 core |
| `rotary_map` / `rotary_event` / `rotary_press` (`rotary/1`) | both | Render a dial label and report relative turns or presses | Bounded relative delta only; no model or session ID crosses the Wire | `rotary-v1.json`; JS reference; C99 core |
| `voice_event` / `voice_state` (`voice/1`) | both | Start, stop, cancel, and acknowledge push-to-talk control | No audio or transcript; capture and transcription remain in the official App | `voice-v1.json`; JS reference; C99 core |
| `text` (`text/1`) | Host → device | Render bounded plain text in a declared display region | No markup or secret payload; malformed content leaves the old display intact | `text-v1.json`; JS reference; C99 core |
| `usage` / `usage_clear` (`usage/1`) | Host → device | Render a model label and bounded counters | Counters are display data, not billing authority | `usage-v1.json`; JS reference; C99 core |
| `config` / `config_result` (`config/1`) | both | Apply a versioned set of bounded preferences and report the result | Atomic: any invalid entry rejects the whole update and preserves current config | `config-v1.json`; JS reference; C99 core |

Exact fields, enums, byte limits, canonical JSON rules, nesting limits, and
state transitions are in [the specification](../SPEC.md). Executable examples
live in `protocol/vectors/`, including
[approval-v1.json](../protocol/vectors/approval-v1.json),
[status-v1.json](../protocol/vectors/status-v1.json), and
[navigation-v1.json](../protocol/vectors/navigation-v1.json). A Host sends
`status` only to a device whose Device Info declares `statusSlots` of at least
1; volatile display and interaction state clears on disconnect or reboot.

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
| interaction payloads in `nexting_device_message_t` | Decode and encode navigation, key, rotary, voice, text, usage, and config messages without allocation | `nexting_device.h` / `nexting_device.c` |

The caller owns every buffer. The core does not initialize Bluetooth, allocate memory, read GPIO, drive an LED, persist an approval, or provide a wall clock. The Zephyr reference firmware does not declare `statusSlots` until per-slot indicator rendering is implemented and board-verified, so Hosts send it no status traffic today.

## Verify the interfaces

```sh
npm run test:reference
cmake -S sdk/c -B /tmp/nexting-device-c -DNEXTING_DEVICE_SANITIZE=ON
cmake --build /tmp/nexting-device-c
ctest --test-dir /tmp/nexting-device-c --output-on-failure
npm run test:firmware
```

The JavaScript reference verifies readable wire behavior and test-only relay
fixtures. C sanitizers verify the fixed-buffer device API. The firmware
contract verifies the Zephyr adapter's required security and transport
structure. Physical behavior still requires the board checklist.

## Required limits and guarantees

| Contract | Experimental 0.2 |
| --- | --- |
| Wire version | `1` |
| Profiles | `approval/1`, `status/1`, `navigation/1`, `keys/1`, `rotary/1`, `voice/1`, `text/1`, `usage/1`, `config/1` |
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

Typed Device Info counts describe hardware; the nine versioned profile strings
negotiate behavior. A new profile still requires SPEC text, schema, vectors,
all public codecs, docs, and evidence before devices may claim it. Wi-Fi, HTTP,
MQTT, USB, multiple approval prompts, device-microphone audio, and production
identity remain outside this release. Nothing may silently reuse an
Experimental 0.2 compatibility claim.
