# Nexting Devices documentation

Choose the task you are trying to complete.

## Understand the product and foundation

- [Project overview](../README.md): product promise, public/private boundary, reference boards, and repository status.
- [Migrate from Experimental 0.1 to 0.2](migration-0.1-to-0.2.md): compatibility, metadata, and Host integration changes.
- [Use cases](use-cases.md): what makers build today and on the roadmap, mapped to real profiles and limits.
- [Foundation development blueprint](foundation-development.md): lifecycle, modules, dependency direction, exact files, change map, and the capability roadmap.
- [Public interface catalog](interfaces.md): BLE, wire messages, Swift Host API, and portable C API.
- [Protocol specification](../SPEC.md): normative wire, BLE, state, limits, and versioning.
- [Security model](../SECURITY.md): trust boundary, minimum controls, threats, and non-claims.

## Run a working example

- [First hardware approval](first-approval.md): build, flash, enroll, and answer one request.
- [macOS BLE simulator](../examples/macos-device-simulator/README.md): exercise a real iPhone without a board.

## Implement or extend

- [Implementation tracks](implementation-tracks.md): reference board, Host/App, MCU/RTOS, and new-language routes.
- [Local development workflow](development.md): prerequisites, commands, TDD order, and CI.
- [Swift Host SDK](../sdk/swift/README.md): codec, authorization, relay, coordinator, and CoreBluetooth central.
- [Kotlin Host SDK](../sdk/kotlin/README.md): bounded Device Info and protocol APIs for Android hosts.
- [Portable C99 SDK](../sdk/c/README.md): fixed-buffer device codec, stream, and state.
- [JavaScript reference](../reference/js/README.md): readable protocol, framing, and relay behavior.
- [Zephyr reference firmware](../firmware/zephyr/README.md): shared Nordic and Espressif adapter.
- [MultiPad USB CDC guide](multipad-usb.md): open-source STM32 adapter, board variant check, and fail-closed flash preparation.
- [First case: ILX MultiPad](cases/multipad-first-case.md): reproduce the upstream build, add the public adapter, map the first two keys, and verify the flash path.
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
