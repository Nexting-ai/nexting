# Conformance and compatibility evidence

Compatibility is a product claim backed by a specific kind of evidence. Passing one layer never proves the next layer.

## Evidence ladder

| Level                   | What it proves                                                                                         | What it does not prove                                                           |
| ----------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------- |
| **Protocol conformant** | An implementation matches shared valid/invalid vectors, framing limits, enums, and state semantics     | A compiler target, radio, physical controls, or product security                 |
| **Core tested**         | The portable implementation passes codec, stream, and approval-state tests under its supported runtime | BLE integration or a named board                                                 |
| **Build verified**      | A pinned toolchain produced firmware for one exact target and retained an artifact                     | Pairing, delivery, buttons, LEDs, race handling, or revocation                   |
| **Board verified**      | A named physical board and firmware commit completed the dated real-iPhone checklist                   | Certification of a different board, firmware, App, or production security design |

Build verified is not Board verified. Board verified is not Nexting Compatible.

## Protocol conformant

Run every shared vector and framing/state test for the implementation. The official reference gates are:

```sh
npm run test:reference
swift test --package-path sdk/swift
cmake -S sdk/c -B /tmp/nexting-device-c -DNEXTING_DEVICE_SANITIZE=ON
cmake --build /tmp/nexting-device-c
ctest --test-dir /tmp/nexting-device-c --output-on-failure
```

Record language/runtime versions, the exact repository commit, and command results. A new language must consume `protocol/vectors/approval-v1.json` instead of maintaining a private copy.

## Core tested

For the official device core, use the ASan/UBSan C build above. For a different portable core, preserve fixed or explicitly bounded memory, newline-inclusive frame limits, oversize discard, monotonic TTL, choice locking, retry, replacement, and disconnect cleanup.

## Build verified

Run:

```sh
npm run test:firmware
```

Then build the exact board using a pinned toolchain. Retain:

- board target;
- firmware commit;
- Zephyr and SDK versions;
- required blobs;
- ELF/BIN/HEX/UF2 artifacts that the target produces;
- Flash and RAM report;
- CI run or reproducible local command.

Experimental 0.2 has Build verified evidence for nRF52840 DK, XIAO nRF52840 / Sense, XIAO ESP32-C3, and XIAO ESP32-S3 using Zephyr 4.3.0 and Zephyr SDK 0.17.4. This is not radio evidence.

## Board verified

Copy [the board verification template](board-verification.md) into a dated evidence record. Include:

- exact board and hardware revision;
- firmware commit and artifact;
- App version, iPhone model, and iOS version;
- fresh-bond state;
- serial logs;
- every checklist result;
- failures, retries, and deviations.

The checklist covers encrypted traffic, enrollment, authorization, Allow/Deny, phone-first and device-first races, Agent failure retry, expiry, replacement, cancellation, small ATT payloads, disconnect, reconnect, reboot, malformed input, oversize input, and bond revocation.

## Allowed wording

| Evidence       | Allowed public wording                                                                                               |
| -------------- | -------------------------------------------------------------------------------------------------------------------- |
| Protocol only  | “Implements Nexting Device Protocol Experimental 0.2” with the tested version                                        |
| Core tested    | “Core tested against the Experimental 0.2 vectors”                                                                   |
| Build verified | “Build verified for XIAO ESP32-C3 target `xiao_esp32c3/esp32c3` on Zephyr 4.3.0 / SDK 0.17.4”                        |
| Board verified | “Board verified,” followed by a link to the dated evidence record that names firmware, App, iPhone, and iOS versions |

Do not say “secure,” “certified,” “production ready,” or “Nexting Compatible” from these gates alone.

## Nexting Compatible

Nexting Compatible is a future governed program covering version support, evidence review, product identity, security posture, maintenance, and trademark permission. It is not available during Experimental 0.2 and cannot be self-awarded by running repository tests.

The two device identity tiers follow the same rule: a Nexting first-party product earns its label through production identity and internal evidence; a third-party or DIY device can only claim the evidence levels above, with its own dated records.

## When evidence expires

Repeat the affected layer when the wire version, profile, SDK behavior, BLE/security configuration, board target, toolchain, firmware, App integration, or physical design changes. Keep older records, but label the exact versions they support.
