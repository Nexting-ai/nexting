# Nexting Devices documentation

Choose the task you are trying to complete.

Start with the root [Quickstart](../QUICKSTART.md). It takes one supported
board from wiring to a public Host `PASS` result. If a checkpoint fails, use
[Troubleshooting](troubleshooting.md) before reading the architecture guides.

## Understand the product and foundation

- [Project overview](../README.md): product promise, public/private boundary, reference boards, and repository status.
- [Migrate from Experimental 0.1 to 0.2](migration-0.1-to-0.2.md): compatibility, metadata, and Host integration changes.
- [Use cases](use-cases.md): what makers can build with the nine current profiles and their limits.
- [Foundation development blueprint](foundation-development.md): lifecycle, modules, dependency direction, exact files, change map, and the current capability set.
- [Public interface catalog](interfaces.md): BLE, wire messages, Swift Host API, and portable C API.
- [Codex App Server Host guide](codex-app-server.md): choose the official rich-client surface and project only one-time approvals into `approval/1`.
- [Protocol specification](../SPEC.md): normative wire, BLE, state, limits, and versioning.
- [Security model](../SECURITY.md): trust boundary, minimum controls, threats, and non-claims.

## Run a working example

- [Public Quickstart](../QUICKSTART.md): wire, build, flash, and prove the golden board.
- [Troubleshooting](troubleshooting.md): repair setup, toolchain, flash, BLE, and Device Info failures.
- [First hardware approval](first-approval.md): build, flash, enroll, and answer one request.
- [macOS BLE simulator](../examples/macos-device-simulator/README.md): exercise a real iPhone without a board.

## Implement or extend

- [Implementation tracks](implementation-tracks.md): reference board, Host/App, MCU/RTOS, and new-language routes.
- [Codex App Server Host guide](codex-app-server.md): exact request mapping, fail-closed policy, and authoritative settlement.
- [Local development workflow](development.md): prerequisites, commands, TDD order, and CI.
- [Swift Host SDK](../sdk/swift/README.md): codec, authorization, relay, coordinator, and CoreBluetooth central.
- [Kotlin Host SDK](../sdk/kotlin/README.md): bounded Device Info and protocol APIs for Android hosts.
- [Portable C99 SDK](../sdk/c/README.md): fixed-buffer device codec, stream, and state.
- [JavaScript reference](../reference/js/README.md): readable protocol, framing, and relay behavior.
- [Zephyr reference firmware](../firmware/zephyr/README.md): shared Nordic and Espressif adapter.
- [Port a chip](porting-guide.md): platform contract and adapter rules.

## Verify and make claims

- [Conformance and compatibility evidence](conformance.md): Protocol/Core/Build/Board levels and allowed wording.
- [Hardware support](hardware-support.md): official references, candidates, and support labels.
- [Board verification](board-verification.md): dated real-iPhone evidence template.
- [Current project status](project-status.md): verified components and remaining release blockers.
- [Shared vectors](../protocol/vectors/approval-v1.json): executable cross-language cases.
- [Status vectors](../protocol/vectors/status-v1.json): executable status-profile cases.
- [JSON Schema](../schemas/message.schema.json): machine-readable message shape.

## Contribute with an Agent

Read [AGENTS.md](../AGENTS.md) before changing files. It defines the source order, file boundary, TDD sequence, evidence rules, and private material that must never enter this repository. Then read [CONTRIBUTING.md](../CONTRIBUTING.md) and [CHANGELOG.md](../CHANGELOG.md).
