# Build the reference approval controller

This Developer Reference uses two buttons to demonstrate `approval/1`. It is
not the only or default shape of a Nexting device. A successful smoke test
proves a bounded physical approval path, not production identity, signed
firmware, live Agent mapping, or physical security.

## 1. Gather and wire

You need a Seeed XIAO nRF52840 or XIAO nRF52840 Sense, two normally-open
momentary buttons, jumper wires, and a data-capable USB-C cable.

| Button | Connect |
| --- | --- |
| Allow | D0 to GND |
| Deny | D1 to GND |

The reference firmware enables internal pull-ups. Do not connect either input
to 3.3 V.

## 2. Get the exact release

```sh
git clone --branch devices-v0.2.0-experimental.2 --depth 1 \
  https://github.com/Nexting-ai/nexting.git
cd nexting/devices
```

## 3. Build

On macOS or Linux with Python 3.11+:

```sh
./scripts/bootstrap-zephyr.sh \
  --board xiao-nrf52840-sense \
  --install-sdk \
  --build
```

The script creates an isolated Python environment beside `devices/`,
initializes the pinned Zephyr 4.3.0 workspace, installs west 1.5.0, optionally
installs Zephyr SDK 0.17.4, and builds:

```text
devices/build/xiao-nrf52840-sense/zephyr/zephyr.uf2
```

Rerunning is safe. Use `--dry-run` to inspect every resolved version and path
without changing the workspace.

## 4. Flash

1. Connect the XIAO over USB-C.
2. Double-press Reset. A volume named `XIAO BLE` appears.
3. Copy `zephyr.uf2` to that volume. It ejects automatically.
4. The Pending LED remains off until a Host presents an approval.

## 5. Prove the public BLE path

From `nexting/devices` on macOS:

```sh
swift run --package-path sdk/swift nexting-device-host-smoke \
  --summary "Allow the Nexting hardware smoke test?"
```

Allow Bluetooth access. The Host prints the discovered device, Device Info, and
connection state. Press Allow or Deny once. Success is explicit:

```text
PASS answer=allow
```

or:

```text
PASS answer=deny
```

This proves discovery, encrypted subscription, bounded framing, the
`approval/1` state machine, and a real button without requiring Agent
credentials.

## 6. Understand the evidence boundary

The result is a local protocol proof. It is not production enrollment or a live
Agent connection, and it does not make this Developer Reference a certified
approval device. Before a production claim, add authenticated application
identity, signed firmware, explicit enrollment and revocation, and the dated
real-board evidence required by
[Conformance](conformance.md) and
[Board verification](board-verification.md).

Public third-party enrollment is tracked in
[`availability.json`](availability.json). Do not use an unpublished App build
or weaken BLE authorization while that gate is closed.
