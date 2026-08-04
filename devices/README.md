# Nexting Devices

Open interfaces for building physical control surfaces for AI agents.

Nexting Devices lets a button, wearable, desk panel, macropad, or custom
product control and display bounded AI Agent interactions through the official
Nexting App.

**New here?**
[Understand the connection and run your first interaction](QUICKSTART.md).
Hardware developers can then
[build the reference approval controller](docs/reference-approval-controller.md).
App availability is machine-readable in
[`docs/availability.json`](docs/availability.json).

**Nearby device, agent anywhere.** The device talks to the official Nexting App over Bluetooth LE; the App reaches the user's agent wherever it runs — the computer across the room or across the internet. Every device built on this contract is a remote control surface for its owner's agents, not a desk-bound accessory.

## The first product experience

1. An AI Agent asks for permission.
2. The trusted Host sends an opaque request ID, a minimum summary, and a short TTL to an authorized nearby device.
3. The device shows Pending and the user presses Allow or Deny.
4. The Host verifies identity, expiry, duplicates, and phone/device races before it answers the Agent.
5. Late, malformed, unauthorized, or repeated input never approves anything.

The device does not run the Agent session and does not receive Agent credentials, internal session identifiers, account data, or cloud routes.

## Choose your path

| You want to… | Start here |
| --- | --- |
| Understand and connect Nexting | [Follow the public Quickstart](QUICKSTART.md) |
| Build the XIAO approval reference | [Build the reference approval controller](docs/reference-approval-controller.md) |
| Get ideas for what to build | [Browse the use cases](docs/use-cases.md) |
| Build and flash a supported board | [Run the first approval](docs/first-approval.md), then use the [reference-board track](docs/implementation-tracks.md#track-1-run-a-reference-board) |
| Understand the device connection contract | [Browse every public interface](docs/interfaces.md) |
| Port a new MCU, RTOS, or chip family | [Build the public foundation](docs/foundation-development.md), then use the [MCU port track](docs/implementation-tracks.md#track-3-port-a-new-mcu-or-rtos) |
| Implement another protocol tool | Use the [protocol-tooling track](docs/implementation-tracks.md#track-3-maintain-protocol-tooling) |
| Make a compatibility claim | [Understand compatibility evidence](docs/conformance.md) |
| Upgrade an Experimental 0.1 integration | [Read the 0.2 migration guide](docs/migration-0.1-to-0.2.md) |

## Experimental 0.2.0-experimental.2

The release keeps wire major 1 and publishes nine independently negotiated
profiles. A device declares only the profiles and hardware it implements:

| Profile | What it carries |
| --- | --- |
| `approval/1` | One active Allow/Deny request with TTL |
| `status/1` | Volatile Agent state for up to eight slots |
| `navigation/1` | Bounded options, cursor movement, and selection |
| `keys/1` | Host-defined key labels/light state and generic key events |
| `rotary/1` | Host-defined dial labels plus relative turn/press events |
| `voice/1` | Push-to-talk start/stop/cancel control; no audio |
| `text/1` | Bounded plain text for a device display |
| `usage/1` | Model label and bounded usage counters |
| `config/1` | Versioned atomic configuration and result |

Public:

- BLE GATT and wire specification;
- shared schema and valid/hostile vectors;
- JavaScript readable reference;
- portable C99 device SDK and JavaScript protocol reference;
- equal Zephyr references for nRF52840 and ESP32;
- conformance tests, build workflows, and developer documentation.

Not public:

- Nexting App product code and UI;
- Mac Bridge, Agent adapters, accounts, cloud routes, and risk policy;
- production identity, certificates, OTA signing, provisioning, manufacturing, and production PIN/Ring firmware;
- any public Nexting cloud, HTTP, MQTT, WebSocket, or device-to-Agent endpoint.

The wire format may change before 1.0. Developer Reference devices are not production security certifications.

## Extensible physical controls

Device Info describes identity, buttons, rotary controls, display, haptics,
standard battery support, inert vendor facts, and the exact versioned profiles
the device supports. The Host rejects traffic for undeclared profiles.

The profiles carry generic physical intent, not private Agent commands. The
trusted Host's Agent adapter decides whether key 3 means `fork`, whether a dial
switches a session or model, and which bounded text is safe to display.
`voice/1` carries only push-to-talk control: capture, permission, audio, and
transcription stay on the Host microphone. This lets DIY hardware remain useful
without receiving Agent credentials, internal session IDs, or account data.

Two identity tiers share the contract: Nexting first-party products (PIN, Ring) carry production identity and the full capability set; third-party and DIY devices use the same protocol under explicit, revocable user authorization in the Host App.

## Reference boards

- Nordic nRF52840 DK;
- Seeed XIAO nRF52840 / Sense;
- Seeed XIAO ESP32C3;
- Seeed XIAO ESP32S3.

All four share one Zephyr application and the portable C99 core. All four are Build verified with the pinned Zephyr 4.3.0 / SDK 0.17.4 workflow. Their real-board checklists remain separate evidence.

## Documentation

- [Understand the device–App–Agent connection](QUICKSTART.md)
- [Build the reference approval controller](docs/reference-approval-controller.md)
- [Troubleshoot setup, build, BLE, and Device Info](docs/troubleshooting.md)
- [Documentation by task](docs/README.md)
- [Browse the use cases](docs/use-cases.md)
- [Build the public foundation](docs/foundation-development.md)
- [Browse every public interface](docs/interfaces.md)
- [Choose an implementation track](docs/implementation-tracks.md)
- [Understand compatibility evidence](docs/conformance.md)
- [Read the normative protocol](SPEC.md)
- [Review the security model](SECURITY.md)
- [Check current evidence and blockers](docs/project-status.md)
- [Develop and test locally](docs/development.md)
- [Give a coding Agent the repository rules](AGENTS.md)

## Repository status

This directory is the audited export candidate consumed by the private product
through public interfaces. It is exported without private Git history into the
existing [`Nexting-ai/nexting`](https://github.com/Nexting-ai/nexting)
repository as `devices/`; it is not a separate repository or submodule.

## Licensing

Code, SDKs, tests, and reference firmware are Apache-2.0. Specifications and documentation are CC BY 4.0. The licenses do not grant the Nexting name, logo, or Nexting Compatible badge.
