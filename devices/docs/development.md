# Development workflow

This guide covers local work on the public Nexting Devices boundary. The official Nexting App is only needed when you are checking a real-device enrollment path.

For architecture and file ownership, start with [the foundation blueprint](foundation-development.md). For discoverable APIs use [the interface catalog](interfaces.md), and for goal-specific work use [the implementation tracks](implementation-tracks.md).

## Prerequisites

- Node.js 22 or newer for the JavaScript reference and contract tests.
- CMake and a C99 compiler for the portable SDK.
- A Zephyr or nRF Connect SDK workspace for reference firmware builds.
- A supported phone and the official Nexting App for real-device enrollment. Local protocol and firmware checks do not require either.

## Fast protocol loop

```sh
npm run test:reference
```

Current expected result: 33 tests pass, including the public JavaScript package-export contract. When changing protocol behavior, update `SPEC.md` and `protocol/vectors/approval-v1.json` before porting the behavior to C.

## Portable C99 SDK

Use a temporary build directory so generated files do not enter the repository:

```sh
cmake -S sdk/c -B /tmp/nexting-device-c -DNEXTING_DEVICE_SANITIZE=ON
cmake --build /tmp/nexting-device-c
ctest --test-dir /tmp/nexting-device-c --output-on-failure
```

Current expected result: codec, stream, and relay tests pass under AddressSanitizer and UndefinedBehaviorSanitizer. The codec suite poisons storage adjacent to unterminated caller-provided arrays, so an unbounded read fails under ASan instead of being hidden by the surrounding message struct.

## Firmware contract

```sh
npm run test:firmware
```

Current expected result: 7 of 7 tests pass. The contract requires mandatory bonding, a physical three-second bond-reset gesture, `bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY)`, and callback-driven ATT notification fragmentation.

## Aggregate public checks

```sh
npm run check
```

This runs the JavaScript reference, two public script contracts, firmware source contract, public-boundary scan, and naming scan. Every command is expected to pass before review.

Run the two checks that should already be green independently:

```sh
npm run check:boundary
npm run check:naming
```

## Reference firmware builds

All four reference targets are Build verified in the pinned workflow:

| Board | Target |
| --- | --- |
| nRF52840 DK | `nrf52840dk/nrf52840` |
| XIAO nRF52840 / Sense | `xiao_ble/nrf52840/sense` |
| XIAO ESP32-C3 | `xiao_esp32c3/esp32c3` |
| XIAO ESP32-S3 | `xiao_esp32s3/esp32s3/procpu` |

From a configured Zephyr workspace:

```sh
west build -p always -b nrf52840dk/nrf52840 /absolute/path/to/nexting-devices/firmware/zephyr
west build -p always -b xiao_ble/nrf52840/sense /absolute/path/to/nexting-devices/firmware/zephyr
west build -p always -b xiao_esp32c3/esp32c3 /absolute/path/to/nexting-devices/firmware/zephyr
west build -p always -b xiao_esp32s3/esp32s3/procpu /absolute/path/to/nexting-devices/firmware/zephyr
```

The reproducible gate pins Zephyr 4.3.0, Zephyr SDK 0.17.4, and west 1.5.0. Espressif jobs fetch the required `hal_espressif` radio blobs. Compilation earns only Build verified; follow [the conformance guide](conformance.md) and [board checklist](board-verification.md) before making a physical-board claim.

The ordinary `.github/workflows/ci.yml` runs language, simulator, and public-boundary gates. The separate `.github/workflows/firmware.yml` matches firmware, the shared C core, the west manifest, or its own workflow. A pull request whose entire diff is documentation-only does not match those compiler paths; adding documentation to a pull request that already changes firmware can cause GitHub to reevaluate the pull request's full matching diff.

## Change discipline

### Protocol or validation

1. Change the normative spec if behavior changes.
2. Add a shared vector that fails.
3. Fix JavaScript.
4. Fix C against the same vector.
5. Run the sanitizer build.

### Firmware transport

Keep JSON, framing, TTL, and approval state in `sdk/c`. Zephyr code should own only BLE, GPIO, bond lifecycle, timers, and fixed transport buffers.

### Documentation

Update `docs/project-status.md` whenever a verification label changes. Never turn a planned result into a verified claim without a command result or evidence record.

## Final local gate

Before a release candidate:

```sh
npm run check
cmake -S sdk/c -B /tmp/nexting-device-release -DNEXTING_DEVICE_SANITIZE=ON
cmake --build /tmp/nexting-device-release
ctest --test-dir /tmp/nexting-device-release --output-on-failure
git diff --check
```

Every command must exit zero. Board and ESP claims require their separate evidence even when this local gate is green.
