# Nexting Devices

Open interfaces for building physical control surfaces for AI agents.

Nexting Devices lets a button, wearable, desk panel, macropad, or custom product show one pending AI approval and return a real user's Allow or Deny choice through a trusted Host/App.

**Nearby device, agent anywhere.** The device talks to the Host App over Bluetooth LE; the Host reaches the user's agent wherever it runs — the computer across the room or across the internet. Every device built on this contract is a remote control surface for its owner's agents, not a desk-bound accessory.

## The first product experience

1. An AI Agent asks for permission.
2. The trusted Host sends an opaque request ID, a minimum summary, and a short TTL to an authorized nearby device.
3. The device shows Pending and the user presses Allow or Deny.
4. The Host verifies identity, expiry, duplicates, and phone/device races before it answers the Agent.
5. Late, malformed, unauthorized, or repeated input never approves anything.

The device does not run the Agent session and does not receive Agent credentials, internal session identifiers, account data, or cloud routes.

## Choose your path

| You want to…                                 | Start here                                                                                                                                                 |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Get ideas for what to build                  | [Browse the use cases](docs/use-cases.md)                                                                                                                  |
| Build and flash a supported board            | [Run the first approval](docs/first-approval.md), then use the [reference-board track](docs/implementation-tracks.md#track-1-run-a-reference-board)        |
| Prepare an ILX MultiPad USB device           | Read the [MultiPad USB guide](docs/multipad-usb.md); confirm the PCB before any flash write                                                                |
| Reproduce the first public hardware case     | Follow the [ILX MultiPad case](docs/cases/multipad-first-case.md); build the upstream firmware before adding Nexting                                       |
| Add a physical control surface to a Host/App | [Browse every public interface](docs/interfaces.md), then use the [Host integration track](docs/implementation-tracks.md#track-2-integrate-a-host-or-app)  |
| Port a new MCU, RTOS, or chip family         | [Build the public foundation](docs/foundation-development.md), then use the [MCU port track](docs/implementation-tracks.md#track-3-port-a-new-mcu-or-rtos) |
| Implement another language SDK               | Use the [language SDK track](docs/implementation-tracks.md#track-4-maintain-a-new-language-sdk)                                                            |
| Make a compatibility claim                   | [Understand compatibility evidence](docs/conformance.md)                                                                                                   |
| Upgrade an Experimental 0.1 integration      | [Read the 0.2 migration guide](docs/migration-0.1-to-0.2.md)                                                                                               |

## Experimental 0.2

The first two profiles remain deliberately small: one active `approval/1`
request with Allow and Deny, plus bounded `status/1` Agent indicators over
Bluetooth LE. Version 0.2 adds extensible Device Info and Android SDK parity
without changing wire major 1.

Public:

- BLE GATT and wire specification;
- shared schema and valid/hostile vectors;
- JavaScript readable reference;
- Swift and Kotlin Host SDKs plus a portable C99 device SDK;
- macOS BLE simulator;
- equal Zephyr references for nRF52840 and ESP32;
- conformance tests, build workflows, and developer documentation.

Not public:

- Nexting App product code and UI;
- Mac Bridge, Agent adapters, accounts, cloud routes, and risk policy;
- production identity, certificates, OTA signing, provisioning, manufacturing, and production PIN/Ring firmware;
- any public Nexting cloud, HTTP, MQTT, WebSocket, or device-to-Agent endpoint.

The wire format may change before 1.0. Developer Reference devices are not production security certifications.

## The capability direction

Experimental 0.2 is the first tile, not the ceiling. Device Info can already
describe identity, buttons, rotary controls, display, haptics, standard battery
support, and inert vendor facts so iOS and Android can render one extensible
information table. Interactive command keys, navigation, rotary events, text,
voice, and configuration remain separate future profiles. See [the capability
roadmap](docs/foundation-development.md#beyond-experimental-02-the-capability-roadmap).

Two identity tiers share the contract: Nexting first-party products (PIN, Ring) carry production identity and the full capability set; third-party and DIY devices use the same protocol under explicit, revocable user authorization in the Host App.

## Reference boards

- Nordic nRF52840 DK;
- Seeed XIAO nRF52840 / Sense;
- Seeed XIAO ESP32C3;
- Seeed XIAO ESP32S3.

All four share one Zephyr application and the portable C99 core. All four are Build verified with the pinned Zephyr 4.3.0 / SDK 0.17.4 workflow. Their real-board checklists remain separate evidence.

## Documentation

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
- [Prepare an ILX MultiPad](docs/multipad-usb.md)
- [Give a coding Agent the repository rules](AGENTS.md)

## Repository status

This directory is the audited export candidate consumed by the private product
through public interfaces. It is exported without private Git history into the
existing [`Nexting-ai/nexting`](https://github.com/Nexting-ai/nexting)
repository as `devices/`; it is not a separate repository or submodule.

## Licensing

Code, SDKs, tests, and reference firmware are Apache-2.0. Specifications and documentation are CC BY 4.0. The licenses do not grant the Nexting name, logo, or Nexting Compatible badge.
