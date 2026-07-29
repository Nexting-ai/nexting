# Project status

Snapshot: 2026-07-29. Release `0.2.0-experimental.2` is implemented as the open
Devices SDK inside the existing `Nexting-ai/nexting` repository under
`devices/`.

## Current evidence

| Area | Evidence | Status |
| --- | --- | --- |
| Wire and vectors | Nine negotiated profiles, bounded Device Info 0.2, JSON Schema, valid and hostile vectors | Passing; wire major remains `1` |
| JavaScript reference | Protocol, framing, relay, Device Info, documentation, simulator, and export tests | Passing |
| Swift Host SDK | Nine-profile codec, Device Info/profile negotiation, sequence sources, authorization, relay, coordinator, CoreBluetooth, and SwiftPM tests | Passing |
| Kotlin Host SDK | Matching nine-profile codec, Device Info negotiation, and sequence sources used directly by Android | Passing |
| Portable C99 SDK | Fixed-buffer nine-profile protocol, approval/status state, Device Info, ASan/UBSan suites | Passing |
| iOS App integration | Explicit enrollment/revocation, secure remembered devices, one active lease, battery, continuous information table, metadata sync, independent Claude Code/Codex adapters | iOS Simulator build and integration contracts pass |
| Android App integration | Public Kotlin SDK, BLE enrollment, encrypted authorization storage, battery, continuous information table, metadata sync, independent Claude Code/Codex adapters | Debug APK, unit tests, and lint pass |
| Cloud metadata | Account-owned custom name, optional number, and notes by stable instance key; owner-only RLS | Route/service tests pass |
| Public export | Allowlisted deterministic `devices/` export, root SwiftPM package, README marker block, SHA-256 manifest, hostile-path/content/symlink tests | Passing |
| nRF52840 DK | Pinned Zephyr 4.3.0 / SDK 0.17.4 workflow artifact | Build verified; physical checklist pending |
| XIAO nRF52840 / Sense | Pinned Zephyr 4.3.0 / SDK 0.17.4 workflow artifact | Build verified; physical checklist pending |
| XIAO ESP32-C3 | Pinned Zephyr 4.3.0 / SDK 0.17.4 workflow artifact | Build verified; physical checklist pending |
| XIAO ESP32-S3 | Pinned Zephyr 4.3.0 / SDK 0.17.4 workflow artifact | Build verified; physical checklist pending |

## What 0.2 adds

- Frozen `navigation/1`, `keys/1`, `rotary/1`, `voice/1`, `text/1`,
  `usage/1`, and `config/1` interaction contracts alongside `approval/1` and
  `status/1`.
- Strict profile negotiation and sequence gates in iOS and Android integration
  so undeclared, replayed, or out-of-order physical input is discarded.
- Host-only microphone capture for push-to-talk: `voice/1` carries control
  events and acknowledgement, never audio or transcripts.
- Atomic remote configuration. One invalid setting rejects the complete update
  without changing the active configuration.
- Optional typed identity and hardware capabilities: buttons, approval/custom
  buttons, rotary controls, rotary presses, display, haptics, standard Battery
  Service support, and inert bounded vendor facts.
- The same ordered, continuous key/value accessory information table on iOS and
  Android. Unsupported fields are omitted; custom and DIY devices can add safe
  vendor facts without creating a grid of special-purpose UI cards.
- Editable account-owned name, user number, and notes without allowing the user
  or cloud to rewrite hardware identity.
- Multiple remembered accessories with exactly one active transport lease.
- Separate Claude Code and Codex adapters sharing transport primitives and the
  one-answer race gate without sharing action sinks.
- A Kotlin/JVM SDK and Android integration matching the Apple contract.
- A reproducible exporter into the established `Nexting-ai/nexting` repository,
  so Devices grows the existing project's history and stars instead of
  splitting them into a second repository.

## Remaining physical evidence

No Android or iPhone BLE device was attached to the 2026-07-29 verification
environment. Therefore:

- no real-radio, pairing, reconnect, battery, button, or cross-platform
  screenshot claim is made;
- all four reference boards remain **Build verified**, not **Board verified**;
- `board-verification.md` must be completed on exact hardware before that label
  changes.

This does not block publishing the Experimental SDK, host contracts, Apps, or
build artifacts. It does block any claim that a physical third-party board has
completed the interoperability checklist.

## Public/private boundary

The public repository contains specifications, SDKs, vectors, reference
firmware, simulator, tests, and documentation. It does not contain Nexting App
source, Agent session models, account/cloud implementation, production
firmware, manufacturing material, credentials, provisioning, OTA signing, or
private Git history. The exporter rejects unknown roots, sensitive paths,
private-boundary content, signing material, and symlinks before replacing only
the public `devices/` subtree.

## Claim rules

- Passing source tests does not prove a radio works.
- A successful firmware build does not prove pairing, notification delivery,
  race handling, battery reporting, or bond revocation.
- Device Info capabilities describe hardware; versioned profile declarations
  separately authorize behavior. Static button or dial counts never imply
  `keys/1` or `rotary/1`.
- User metadata and `device_id` are not authorization credentials.
- A Developer Reference device is not a production security certification.

See `hardware-support.md` for label definitions and `board-verification.md` for
the required physical evidence.
