# Troubleshooting

Start with the first failed checkpoint. Keep BLE encryption enabled; a security
failure is not repaired by making a characteristic public.

## `west: unknown command "build"`

You ran a globally installed `west` outside an initialized workspace. From
`nexting/devices`, use the repository bootstrap:

```sh
./scripts/bootstrap-zephyr.sh --board xiao-nrf52840-sense --build
```

It creates `.west` beside `devices/` and uses its own west 1.5.0 virtual
environment. If the next error names a missing ARM toolchain, rerun once with
`--install-sdk`.

## `XIAO BLE` never appears

- Use a data-capable USB-C cable.
- Double-press Reset quickly.
- Try a direct USB port instead of a charge-only hub.
- Confirm you built the `xiao_ble/nrf52840/sense` target and copied
  `zephyr.uf2`, not an ELF file.

## The Host finds no Bluetooth device

- Turn Bluetooth on and grant the terminal Bluetooth permission in System
  Settings → Privacy & Security → Bluetooth.
- Confirm the board advertises the service UUID `6EADC0DE-0001-4A21-9C5E-1B7F3D9E42A0`.
- Hold Allow and Deny for three seconds, forget the old macOS bond, and retry.
- Do not authorize by advertised name alone.

## The Host rejects Device Info

The smoke test requires wire major `1`, `approval/1`, at least two declared
buttons, and a bounded message size of at least 512 bytes. Reflash the tagged
reference firmware if Device Info is missing or malformed. A user name, serial,
battery value, or vendor fact is display metadata—not authorization.

## It connects but no answer arrives

- Verify Allow is D0-to-GND and Deny is D1-to-GND.
- Press and release one button; holding both starts the bond-reset gesture.
- Confirm the Host subscribed to the encrypted answer characteristic before it
  sent `present`.
- Run the Host again and keep the terminal visible for the exact timeout or
  protocol error.

## Agent connection is unavailable

App Store 2.4 does not yet enroll Experimental 0.2 developer devices. Passing
the public Host smoke test is the current external-developer completion point.
Agent connection becomes public only when the SDK page names a verified iOS and
Android release.
