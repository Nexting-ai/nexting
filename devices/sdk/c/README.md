# Portable C99 core

This SDK is the chip-neutral part of a Nexting-compatible device. It provides:

- deterministic Experimental 0.2 message encoding and decoding;
- fixed-buffer Device Info 0.2 parsing for typed identity, capabilities, and
  bounded inert vendor facts;
- fixed-buffer newline framing for BLE writes and notifications;
- the device-side Idle → Pending → Waiting Resolution state machine;
- exact answer retry, expiry, replacement, and disconnect behavior.

It does not initialize Bluetooth, read buttons, drive LEDs, persist approvals, or allocate memory. Platform adapters supply those pieces for ESP-IDF, Arduino, Zephyr, nRF Connect SDK, or another embedded runtime.

Start with [the public foundation](../../docs/foundation-development.md) and [interface catalog](../../docs/interfaces.md). Platform work follows [the MCU/RTOS track](../../docs/implementation-tracks.md#track-3-port-a-new-mcu-or-rtos).

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The configure step uses Node.js to turn the repository's shared JSON vectors into a build-only C fixture. The shipped library itself has no Node.js or JSON-library dependency.

For sanitizer checks on a desktop compiler:

```sh
cmake -S . -B build-sanitize -DNEXTING_DEVICE_SANITIZE=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

## Integration contract

Give `nexting_device_stream_t` a caller-owned receive buffer and feed it every downlink fragment. Forward complete `present` and `resolved` messages to `nexting_device_state_t`. Encode the returned `answer` into a caller-owned transmit buffer and notify the host. Call `nexting_device_state_disconnect` and `nexting_device_stream_reset` whenever the BLE connection ends.

The state is intentionally volatile. A device must never restore an approval after reboot or reconnect.
