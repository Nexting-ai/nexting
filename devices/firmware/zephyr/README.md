# Zephyr reference firmware

One firmware source supports the Nordic nRF52840 and Espressif ESP32-C3/S3 reference families. BLE and GPIO come from Zephyr; all message parsing, framing, TTL, choice locking, and retry behavior comes from the portable C99 core in `sdk/c`.

Start with [the public foundation](../../docs/foundation-development.md) and [reference-board track](../../docs/implementation-tracks.md#track-1-run-a-reference-board). Compatibility wording follows [the conformance guide](../../docs/conformance.md).

## Reference wiring

| Board                 | Allow            | Deny             | Pending output                               |
| --------------------- | ---------------- | ---------------- | -------------------------------------------- |
| nRF52840 DK           | Button 1 (`sw0`) | Button 2 (`sw1`) | LED 1 (`led0`)                               |
| XIAO nRF52840 / Sense | D0 to GND        | D1 to GND        | onboard red LED                              |
| XIAO ESP32-C3         | D0 to GND        | D1 to GND        | D2 through a resistor to an LED, active high |
| XIAO ESP32-S3         | D0 to GND        | D1 to GND        | onboard LED                                  |

The button inputs use internal pull-ups and are active low.

Press and release one button to answer. Holding both buttons starts a separate safety gesture: release them before three seconds and no answer or reset occurs; hold both for three seconds to clear every BLE bond. After the erase succeeds, the Pending LED turns on for one second as confirmation and connectable advertising resumes. The current approval and partial frames are erased and the phone is disconnected either way. If bond erasure fails, the firmware logs the error, keeps the confirmation LED off, and leaves advertising stopped so an old bond is never presented as cleared. Also remove the accessory from the phone before pairing again because each side stores its own copy of the bond.

## Build examples

From a Zephyr or nRF Connect SDK workspace, use an absolute application path:

```sh
west build -p always -b nrf52840dk/nrf52840 /path/to/nexting-devices/firmware/zephyr
west build -p always -b xiao_ble/nrf52840/sense /path/to/nexting-devices/firmware/zephyr
west build -p always -b xiao_esp32c3/esp32c3 /path/to/nexting-devices/firmware/zephyr
west build -p always -b xiao_esp32s3/esp32s3/procpu /path/to/nexting-devices/firmware/zephyr
```

Espressif targets need an upstream Zephyr workspace with `hal_espressif`, its required radio blobs, and a compatible Zephyr SDK. A Nordic-only nRF Connect SDK manifest is not enough even when it lists the ESP board definitions.

## Security boundary

The downlink and notification subscription require an encrypted link. Secure Connections, mandatory bonding, and bond storage are enabled. These button-only references use Just Works pairing, so they do not provide authenticated MITM protection. They are developer references, not production-certified accessories.

If pairing or the requested security upgrade fails, the peer is disconnected so it cannot occupy the board's single connection slot. Advertising restarts from a delayed work item after disconnect and retries transient controller errors rather than assuming the slot is already free inside the disconnect callback.

Only bonds persist. The current approval, partial frames, cached answer, and LED state live in RAM and are cleared on disconnect, unsubscribe, reboot, or expiry.

## Declared capabilities

Device Info advertises `profiles":["approval/1"]` only. Profile `status/1` exists in the shared C core, but these boards have a single Pending LED and no per-slot indicator mapping yet, so the firmware does not declare `statusSlots`. A conforming Host therefore sends no `status` traffic to the reference boards. A board that adds indicator rendering must declare `statusSlots`, document its slot-to-indicator mapping here, and pass the real-board checklist before claiming support.

Answers use one fixed RAM frame and are sent in ordered notification fragments no larger than the negotiated ATT payload. A new application-level retry begins only after the complete prior frame has passed through the notification completion callbacks.
