# Run the first hardware approval

This guide gets a reference board to the point where an iPhone can present one real approval and receive an Allow or Deny button press.

## Before you start

You need:

- a real iPhone; the iOS Simulator cannot provide this Bluetooth path;
- a Debug build of the current private Nexting App;
- one supported board, two buttons, and the wiring from the firmware README;
- a Zephyr or nRF Connect SDK workspace.

The current Nexting App exposes explicit accessory discovery, enrollment, and
revocation on iOS and Android. Scan for a nearby device, inspect its verified
Device Info table, and confirm the device before it is remembered. The App may
remember multiple accessories but holds only one active transport lease.

For firmware development, `debug-test-device.conf` still provides the
reference advertised name. That name helps discovery only; it is never the
authorization decision. A release host authorizes the confirmed peripheral
record and encrypted link, then keeps user-owned name/number/notes separate
from read-only hardware identity.

## Build nRF52840

From a configured west workspace:

```sh
west build -p always -b xiao_ble/nrf52840/sense \
  /path/to/nexting-devices/firmware/zephyr \
  -- -DEXTRA_CONF_FILE=debug-test-device.conf
```

For the nRF52840 DK, replace the board target with `nrf52840dk/nrf52840`.

The XIAO build produces `build/zephyr/zephyr.uf2`. Double-tap Reset, open the `XIAO BLE` volume, and copy the UF2 to it. The DK can use `west flash` with its onboard debugger.

## Build ESP32

Use the pinned upstream Zephyr workspace from `west.yml`, install the matching Zephyr SDK toolchains, and fetch Espressif radio blobs:

```sh
west blobs fetch hal_espressif
west build -p always -b xiao_esp32c3/esp32c3 \
  /path/to/nexting-devices/firmware/zephyr \
  -- -DEXTRA_CONF_FILE=debug-test-device.conf
```

For XIAO ESP32-S3 use `xiao_esp32s3/esp32s3/procpu`. The public CI is the reproducible compiler gate until these targets are also built locally.

## Run the approval

1. Launch the App on the iPhone, open **My Devices & Nearby**, and choose Scan.
2. Power the board, inspect its Device Info table, and explicitly confirm enrollment. The App should pair when it subscribes to the encrypted answer characteristic.
3. Cause Claude Code or Codex to show an eligible ordinary two-choice permission prompt.
4. Confirm the pending LED turns on and only the bounded summary is visible on the board log.
5. Press and release Allow or Deny once.
6. Confirm the originating Agent continues with that option and the LED turns off after `resolved`. Claude Code and Codex keep independent adapters and action sinks.

Then complete every case in `board-verification.md`. A successful happy path alone is not enough for the `Board verified` label.

## Clear a development bond

1. Hold Allow and Deny together for at least three seconds. A shorter two-button press is ignored and must not answer the approval.
2. Confirm the current approval disappears, the connection closes, and the Pending LED stays on for one second as reset confirmation.
3. On the iPhone, forget the old Bluetooth accessory or remove its App authorization. The board cannot erase the phone's copy of a bond.
4. Confirm the board advertises again, then perform a fresh encrypted pairing before sending another approval.

The one-second LED is a success signal, not merely an acknowledgement of the button gesture. If the board logs `Bond reset failed`, keeps the LED off, and does not advertise, reboot it and retry the reset before pairing; do not treat the old bond as cleared.

This is a local developer-reference recovery path, not product enrollment or account revocation. A release App still needs an explicit authorization and revocation UI.

## If discovery does not happen

- Confirm the phone is real hardware and Bluetooth is enabled.
- Confirm the App has Bluetooth permission and the device has been explicitly enrolled.
- Confirm Device Info reports wire `1`, profile `approval/1`, and at least 512 message bytes.
- Hold both board buttons for three seconds, then forget the old accessory on iOS if security configuration changed.
- Check that the board advertises the service UUID, not only the local name.
- Do not weaken encrypted GATT permissions to make pairing “easier.”
