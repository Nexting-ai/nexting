# Hardware support

Nexting Devices treats ESP32 and Nordic as equal product families. “Equal” means they implement the same public protocol and pass the same conformance behavior. It does not mean every board has the same radio, power profile, SDK, or verification status.

## What the labels mean

| Label              | Product meaning                                                                                                                                                     |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Core tested        | The chip adapter uses the shared C99 codec and state machine, whose desktop tests pass. This says nothing about its radio.                                          |
| Build verified     | A pinned toolchain produced firmware for the exact board target. No physical Bluetooth claim is implied.                                                            |
| Board verified     | A named board completed the published iPhone, race, expiry, disconnect, reboot, and malformed-input checklist.                                                      |
| Nexting Compatible | A separately governed compatibility program has accepted the product, version, security posture, and evidence. This badge is not available during Experimental 0.2. |

“Source available” is not “Build verified,” and “Build verified” is not “Board verified.” Each table entry is deliberately conservative.

## Official Experimental 0.2 references

| Family             | Board                       | Runtime | Current status                                                          | Why it is first-class                                                                                        |
| ------------------ | --------------------------- | ------- | ----------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| Nordic nRF52840    | Seeed XIAO nRF52840 / Sense | Zephyr  | **Build verified** on Zephyr `4.3.0` / SDK `0.17.4`; board test pending | Small, battery-friendly, mature BLE, UF2-friendly developer board, same family as our current hardware work. |
| Nordic nRF52840    | nRF52840 DK                 | Zephyr  | **Build verified** on Zephyr `4.3.0` / SDK `0.17.4`; board test pending | Best debug visibility and four built-in buttons/LEDs; our reference for bring-up failures.                   |
| Espressif ESP32-C3 | XIAO ESP32C3                | Zephyr  | **Build verified** on Zephyr `4.3.0` / SDK `0.17.4`; board test pending | Low-cost RISC-V, BLE plus Wi-Fi, good community availability.                                                |
| Espressif ESP32-S3 | XIAO ESP32S3                | Zephyr  | **Build verified** on Zephyr `4.3.0` / SDK `0.17.4`; board test pending | More I/O and memory for displays, touch, richer desk controls, and future local UI.                          |

All four use one Zephyr application and `sdk/c`. There is no Nordic parser and separate ESP parser to drift apart.

## Next candidates

These are product priorities, not compatibility claims.

| Priority | Family                                  | Best use                                        | Why it is attractive                                                             | What must be proved first                                                                             |
| -------: | --------------------------------------- | ----------------------------------------------- | -------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
|        1 | nRF54L15 / XIAO nRF54L15 and nRF54LM20A | Next-generation wearable and battery device     | Newer Nordic low-power platform, modern radio/security, strong Zephyr direction. | Stable board/toolchain target, secure storage path, power and reconnect measurements.                 |
|        1 | ESP32-C6                                | Low-cost connected panels and hubs              | BLE 5.3, Wi-Fi 6, and 802.15.4 leave room for Matter/Thread products.            | BLE bonding interop, memory budget, and real iPhone regression run.                                   |
|        2 | nRF5340                                 | Display, audio, or multi-radio premium controls | Dual-core headroom and mature nRF Connect SDK.                                   | Network-core build/release complexity and a justified product that needs it.                          |
|        2 | ESP32-H2                                | Low-power BLE/Thread button                     | No Wi-Fi radio overhead; BLE plus 802.15.4 is a good control-device shape.       | Board availability, Zephyr target build, and power data.                                              |
|        2 | Silicon Labs EFR32BG24 / XIAO MG24      | OEM low-power products                          | Strong BLE/Matter platform and good energy profile.                              | SDK/license review, open build reproducibility, adapter maintainer.                                   |
|        3 | Raspberry Pi Pico 2 W                   | Education and maker ecosystem                   | Accessible hardware and a large community.                                       | Reliable BLE peripheral/bonding behavior through the external radio and a maintained BTstack adapter. |
|        3 | STM32WB0/WB55                           | Industrial OEM designs                          | Established MCU vendor, BLE portfolio, industrial supply options.                | Reproducible open toolchain, stack integration, signed-update ownership.                              |
|        3 | TI CC2340R5                             | Cost-sensitive OEM button                       | Purpose-built low-power BLE economics.                                           | SimpleLink adapter, CI licensing, secure bond storage, community ownership.                           |

## Explicitly not a v0.2 target

- ESP32-S2 and ESP8266: no Bluetooth LE radio for this public connection surface.
- RP2040 without a wireless companion: no BLE transport.
- classic Arduino Uno-class boards: insufficient integrated radio/security story for the reference.
- nRF52832: technically possible with a 512-byte frame cap, but its tighter memory and older product position make it a community port, not an official reference.
- Wi-Fi-only, HTTP, MQTT, USB HID, and cloud-direct devices: those would be new transports, not silent variations of the BLE profile.

## USB CDC developer track (MultiPad)

The ILX MultiPad is a source-available STM32F103VET6 keyboard with a USB HID +
CDC composite interface. The public adapter and its contract test live in
[`firmware/multipad`](../firmware/multipad/README.md). This is a **developer
binding**, not an Experimental 0.2 BLE compatibility claim and not Nexting App
USB enrollment. The board variant, bootloader path, backup, and CDC proof must
be recorded before calling a physical unit Build verified or Board verified.

## Hardware contract

An Experimental 0.2 port needs:

1. Bluetooth LE peripheral/GATT server support;
2. encrypted bonding and a way to revoke a bond;
3. at least 512 bytes of logical receive buffering;
4. two unambiguous local actions for Allow and Deny;
5. an output that can distinguish Pending from Idle;
6. a monotonic millisecond clock;
7. volatile approval state that is cleared on reboot and disconnect.

The reference boards use button-only Just Works pairing. It encrypts and bonds the link but does not prove man-in-the-middle resistance. A production accessory should add authenticated pairing through a display, numeric comparison, NFC/OOB, or another reviewed mechanism.
