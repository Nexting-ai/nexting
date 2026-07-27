# Build your first Nexting device

This is the shortest public path from an empty checkout to one real Allow or
Deny button press. It uses a Seeed XIAO nRF52840 / Sense and requires neither
Agent credentials nor unpublished product software.

## 1. Gather and wire

You need a XIAO nRF52840 or XIAO nRF52840 Sense, two normally-open momentary
buttons, jumper wires, and a data-capable USB-C cable.

| Button | Connect |
| --- | --- |
| Allow | D0 to GND |
| Deny | D1 to GND |

The reference firmware enables internal pull-ups. Do not connect either input
to 3.3 V.

## 2. Get the exact release

```sh
git clone --branch devices-v0.2.0-experimental.1 --depth 1 \
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

The script creates an isolated Python environment beside `devices/`, initializes
the pinned Zephyr 4.3.0 workspace, installs west 1.5.0, optionally installs
Zephyr SDK 0.17.4, and builds:

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

This test proves discovery, encrypted subscription, bounded framing, the
`approval/1` state machine, and a real button without requiring an Agent.

## 6. Connect an Agent

Public availability is a separate gate. As of 2026-07-27, **App Store 2.4**
does not include Experimental 0.2 developer-device enrollment. Do not weaken
BLE security or look for an unpublished build. Use the public Host smoke test
today. The web SDK page will name the first verified iOS and Android versions
when the enrollment UI ships publicly.

Next: read [troubleshooting](docs/troubleshooting.md), then the
[Device SDK](sdk/c/README.md), [Swift Host SDK](sdk/swift/README.md), or
[Kotlin Host SDK](sdk/kotlin/README.md).
