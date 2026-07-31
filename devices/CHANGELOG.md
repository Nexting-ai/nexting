# Changelog

This file records changes to public protocol behavior, shared vectors, SDK surfaces, developer-reference firmware, and board evidence. It does not record private Nexting App, Agent adapter, cloud, or production-firmware work.

## Unreleased

### Added

- A source-first ILX MultiPad USB CDC developer binding that reuses the portable
  C99 state machine, preserves the upstream HID and `AA BB xx` commands, and
  includes a host-side CMake contract test.
- A GitHub-ready MultiPad guide, conservative Device Info template, CDC smoke
  check, and fail-closed STM32 serial-flash preparation script. Physical board
  and bootloader evidence remain explicitly pending.

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
