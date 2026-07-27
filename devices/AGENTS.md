# Agent Development Guide

This repository contains the exportable Nexting Devices developer boundary. It is not the Nexting App, cloud control plane, Mac Bridge, or production firmware.

## Read before changing code

Read these sources in order:

1. `README.md` for the product promise and release boundary.
2. `docs/foundation-development.md` for architecture, dependency direction, and exact file ownership.
3. `docs/interfaces.md` for the complete public interface catalog.
4. `SPEC.md` and `protocol/vectors/approval-v1.json` for normative behavior.
5. `SECURITY.md` for trust boundaries and non-claims.
6. `docs/development.md` for commands and TDD order.
7. `docs/conformance.md` before changing a compatibility label.

When documents conflict, do not guess. `SPEC.md` and shared vectors own intended protocol behavior; tests show the current implementation; `docs/project-status.md` records current evidence.

## Public boundary

You may change the protocol, shared vectors, JavaScript reference, Swift SDK, C99 SDK, simulator, developer-reference firmware, tests, and public documentation in this repository.

Never copy in:

- Nexting App source or private UI;
- Mac Bridge or Claude Code/Codex adapter implementation;
- cloud routes, account data, internal identifiers, or credentials;
- Nexting PIN/Ring production firmware or hardware design;
- OTA signing, factory provisioning, device keys, or production test fixtures.

Do not import files from outside this repository root into public code. The exported repository must build and test without the private monorepo.

## Choose the task route

- Interface or behavior change: `docs/foundation-development.md` → `docs/interfaces.md` → `SPEC.md` → vectors → component README.
- New chip or board: `docs/foundation-development.md` → `docs/implementation-tracks.md` → `docs/porting-guide.md` → `docs/hardware-support.md` → `docs/conformance.md`.
- Host product integration: `docs/interfaces.md` → the Host track in `docs/implementation-tracks.md` → `sdk/swift/README.md`. Keep the product adapter thin and private.
- Documentation change: update the owning guide and the documentation-contract test in the same commit.

## Protocol change order

For any wire, validation, or state-machine change:

1. update `SPEC.md` if normative behavior changes;
2. add valid or invalid cases to `protocol/vectors/approval-v1.json`;
3. make the JavaScript reference test fail;
4. implement JavaScript behavior;
5. port identical behavior to Swift and C;
6. update firmware only through the shared C core unless the change is transport-specific;
7. run every relevant gate.

Do not create language-specific behavior that is absent from the shared contract. Unknown, malformed, stale, expired, duplicate, or unauthorized input must fail closed.

## Required local gates

From the repository root:

```sh
npm run test:reference
swift test --package-path sdk/swift
cmake -S sdk/c -B /tmp/nexting-device-c -DNEXTING_DEVICE_SANITIZE=ON
cmake --build /tmp/nexting-device-c
ctest --test-dir /tmp/nexting-device-c --output-on-failure
npm run test:firmware
npm run check:boundary
npm run check:naming
```

`npm run check` is the final aggregate gate. At the 2026-07-27 snapshot it
passes, including Device Info 0.2, the audited exporter, mandatory bonding,
three-second bond reset, and notification fragmentation. Run
`gradle test` in `sdk/kotlin` as the Android Host SDK gate. Do not weaken or
delete those assertions; a source contract and compiler gate still do not
replace the real-board checklist.

## Hardware claims

- `Core tested` means shared C behavior passes desktop tests.
- `Build verified` means one exact board target compiled.
- `Board verified` requires the complete real-iPhone checklist and recorded evidence.
- `Nexting Compatible` is not available during Experimental 0.2.

Never promote a label without the evidence required by `docs/hardware-support.md` and `docs/board-verification.md`.

## Current execution plan

Follow `docs/project-status.md` for the current order. A containing private monorepo may carry a more detailed execution plan, but public code and documentation must remain self-contained here.
