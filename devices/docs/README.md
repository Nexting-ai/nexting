# Nexting Devices documentation

Choose the task you are trying to complete.

Start with the root [Quickstart](../QUICKSTART.md). It explains the
device–App–Agent architecture, separates first-party product onboarding from
SDK development, and leads to a remote interaction or honest local protocol
proof. If a checkpoint fails, use [Troubleshooting](troubleshooting.md).

## Understand the product and foundation

- [Project overview](../README.md): product promise, public/private boundary, reference boards, and repository status.
- [Migrate from Experimental 0.1 to 0.2](migration-0.1-to-0.2.md): compatibility, metadata, and device profile changes.
- [Use cases](use-cases.md): what makers can build with the nine current profiles and their limits.
- [Foundation development blueprint](foundation-development.md): lifecycle, modules, dependency direction, exact files, change map, and the current capability set.
- [Public interface catalog](interfaces.md): BLE, wire messages, and the portable C device API.
- [Protocol specification](../SPEC.md): normative wire, BLE, state, limits, and versioning.
- [Security model](../SECURITY.md): trust boundary, minimum controls, threats, and non-claims.

## Run a working example

- [Public Quickstart](../QUICKSTART.md): understand, connect, and choose the supported first result.
- [Reference approval controller](reference-approval-controller.md): wire, build, flash, and prove the XIAO Developer Reference.
- [Public availability](availability.json): machine-readable SDK version and third-party App enrollment gate.
- [Troubleshooting](troubleshooting.md): repair setup, toolchain, flash, BLE, and Device Info failures.
- [First hardware approval](first-approval.md): build, flash, enroll, and answer one request.
- Protocol and firmware tests can run without a board; the official Nexting App is the Host used for a real device.

## Implement or extend

- [Implementation tracks](implementation-tracks.md): reference board, MCU/RTOS, and protocol-tooling routes.
- [Local development workflow](development.md): prerequisites, commands, TDD order, and CI.
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
