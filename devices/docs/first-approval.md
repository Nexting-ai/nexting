# Run the first hardware approval

This guide explains the device-side approval lifecycle after the public
[Quickstart](../QUICKSTART.md) has proved the board and protocol path.

## Before you start

You need one supported board and the physical controls declared by its
firmware. The official Nexting App is the Host: it discovers the board, checks
Device Info, authorizes the encrypted link, and maps the result to the selected
Agent. This public package does not ask you to build another Host.

For firmware development, `debug-test-device.conf` still provides the
reference advertised name. That name helps discovery only; it is never the
authorization decision. A release host authorizes the confirmed peripheral
record and encrypted link, then keeps user-owned name/number/notes separate
from read-only hardware identity.

## Build nRF52840

From `nexting/devices`, let the public bootstrap prepare the pinned workspace:

```sh
./scripts/bootstrap-zephyr.sh --board xiao-nrf52840-sense --build
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

1. Open the official Nexting App and pair the board through its device flow.
2. Confirm the App shows the declared Device Info and negotiated profiles.
3. Trigger a one-time approval from the selected Agent.
4. Press the physical control mapped by your firmware to `allow` or `deny`.
5. Confirm the App resolves the request and the device clears its pending state.

Then complete every case in `board-verification.md`. A successful happy path alone is not enough for the `Board verified` label.

## Clear a development bond

1. Hold the two reset controls together for at least three seconds. A shorter two-button press is ignored and must not answer the approval.
2. Confirm the current approval disappears, the connection closes, and the Pending LED stays on for one second as reset confirmation.
3. In the Nexting App, forget the old Bluetooth accessory or remove its authorization. The board cannot erase the phone's copy of a bond.
4. Confirm the board advertises again, then perform a fresh encrypted pairing before sending another approval.

The one-second LED is a success signal, not merely an acknowledgement of the button gesture. If the board logs `Bond reset failed`, keeps the LED off, and does not advertise, reboot it and retry the reset before pairing; do not treat the old bond as cleared.

This is a local developer-reference recovery path, not product enrollment or account revocation. A release App still needs an explicit authorization and revocation UI.

## If discovery does not happen

- Confirm macOS Bluetooth is enabled and the terminal has Bluetooth permission.
- Confirm Device Info reports wire `1`, profile `approval/1`, and at least 512 message bytes.
- Hold both board buttons for three seconds, then forget the old accessory on iOS if security configuration changed.
- Check that the board advertises the service UUID, not only the local name.
- Do not weaken encrypted GATT permissions to make pairing “easier.”
