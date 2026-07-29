# Changelog

This file records changes to public protocol behavior, shared vectors, SDK surfaces, developer-reference firmware, and board evidence. It does not record private Nexting App, Agent adapter, cloud, or production-firmware work.

## Unreleased

## 0.2.0-experimental.2 — 2026-07-29

This release keeps wire major 1 and adds seven frozen interaction profiles
without changing `approval/1` or `status/1`.

### Added

- Frozen interaction profiles `navigation/1`, `keys/1`, `rotary/1`, `voice/1`,
  `text/1`, `usage/1`, and `config/1`.
- Shared valid and hostile vectors plus JSON Schema definitions for all 15 new
  messages.
- Strict JavaScript, fixed-buffer C99, Swift 6, and Kotlin implementations of
  every new message.
- Device Info profile negotiation helpers and Host-side profile gating.
- Sequence numbers for replay/out-of-order rejection on navigation, key,
  rotary, and push-to-talk input.
- Versioned, atomic configuration results. Invalid configuration leaves the
  current device configuration unchanged.

### Changed

- The Codex Host guide now distinguishes the official high-level SDK from App
  Server and freezes the fail-closed one-time approval projection into
  `approval/1`.
- `voice/1` carries control only. Audio capture, permission, and transcription
  stay on the Host microphone; audio and transcripts never cross this BLE
  profile.

## 0.2.0-experimental.1 — 2026-07-28

This developer-experience release keeps wire major 1 and the `approval/1` and
`status/1` profiles unchanged.

### Added

- A public XIAO nRF52840 Quickstart with exact wiring, flashing, expected
  output, and an explicit public-App availability gate.
- A rerunnable Zephyr 4.3.0 / west 1.5.0 / SDK 0.17.4 bootstrap with board
  aliases, dry-run output, isolated Python dependencies, and actionable errors.
- A macOS `nexting-device-host-smoke` executable that validates Device Info,
  uses encrypted BLE, presents one synthetic approval, and emits a
  machine-readable real-button `PASS`.
- Setup, toolchain, flashing, Bluetooth, Device Info, and public-App
  troubleshooting.

### Changed

- Public docs no longer require unpublished product software.
- Claude Code and Codex docs separate published protocol surfaces from roadmap
  profiles and from public App availability.
- Website SDK pages have stable shareable routes and include Kotlin/Android as
  a first-class Host reference.

## 0.2.0-experimental.0 — 2026-07-27

Experimental 0.2 prepares the SDK for publication in the existing
`Nexting-ai/nexting` repository under `devices/`. The SDK version advances
without changing wire major 1 or the shipped `approval/1` and `status/1`
profile versions.

### Added

- BLE GATT and newline-delimited `approval/1` protocol for one bounded Allow/Deny request.
- Profile `status/1`: a Host→device downlink that replaces the full rendered state of up to eight anonymous agent-status slots (`idle`, `thinking`, `working`, `complete`, `needs_input`, `error`) with an optional 64-byte label, negotiated through the new optional `statusSlots` Device Info capability field and cleared on disconnect or reboot. Ships with shared `status-v1.json` vectors, a schema def, and JavaScript, Swift, and C implementations. The Zephyr reference does not declare `statusSlots` until per-slot indicator rendering is implemented and board-verified.
- Shared valid and hostile-input vectors plus a JSON Schema.
- Executable JavaScript reference with explicit `protocol`, `framing`, and `relay` entry points.
- `NextingDeviceKit` for Apple hosts and a fixed-buffer portable C99 device core.
- Developer documentation for architecture, security, ports, hardware support, verification, and Agent-driven changes.
- Public foundation blueprint, interface catalog, implementation tracks, conformance evidence rules, and an executable documentation contract for third-party and Agent development.
- Optional, bounded Device Info identity, hardware-capability, Battery Service,
  and vendor-extension metadata shared by all language SDKs.
- A Kotlin/JVM protocol SDK for Android hosts.
- Explicit iOS and Android enrollment, revocation, and extensible accessory
  information-table contracts in the Nexting App.
- A deterministic allowlisted exporter into `Nexting-ai/nexting/devices`,
  root SwiftPM integration, SHA-256 manifest, and hostile-input tests.

### Security and correctness

- Official JavaScript, Swift, and C decoders share 15 valid and 29 hostile vectors, covering every enum and exact summary boundaries while consistently rejecting duplicate known fields, decoded NUL, lone surrogates, non-canonical integers, excessive nesting, oversized frames, invalid UTF-8, and over-limit summaries.
- Complete-frame byte limits include the newline across all three stream decoders; Device Info accepts safely negotiable larger capacities while rejecting incompatible profiles and control characters.
- Physical approval is restricted to exact two-option, one-time Allow/Deny prompts and resolves only after the Agent accepts the answer.
- Host queues limit bytes and retained frame objects; synchronous enqueue rejection rolls back presentation state, and discovery recovers from malformed or failed Device Info reads.
- The macOS peripheral simulator requires encrypted answer subscription, accepts writes only from the subscribed central, reuses the Swift codec and monotonic TTL, and fragments notifications without interleaving frames.
- The Zephyr Developer Reference requires bonding, offers a fail-closed three-second physical bond reset, fragments one fixed answer frame through notification completion callbacks with stream resynchronization after an abandoned partial frame, disconnects failed security upgrades, and retries advertising after the connection slot is released.

### Verification status

- JavaScript, Swift, Kotlin, C sanitizer, iOS Simulator, Android build/unit/lint,
  private integration, exporter, boundary, and simulator gates pass at the
  2026-07-27 snapshot.
- Pinned Zephyr 4.3.0 / SDK 0.17.4 builds produce named artifacts for all four
  reference boards. Real-board BLE evidence remains pending; see
  [`docs/project-status.md`](docs/project-status.md) rather than treating this
  changelog as a certification claim.
