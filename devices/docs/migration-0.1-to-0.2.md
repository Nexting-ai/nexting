# Migrate from Experimental 0.1 to 0.2

Experimental 0.2 keeps wire major `1` and the `approval/1` and `status/1`
profiles. Existing 0.1 approval hardware remains protocol-compatible when its
required Device Info fields are valid. The migration adds optional metadata and
new Host SDK surfaces; it does not grant new approval authority to a device.

## Device firmware

1. Change the Device Info `spec` string to `0.2.0-experimental.0`.
2. Keep every required 0.1 field and its existing bound.
3. Add only capabilities the physical device actually implements:
   `device_id`, manufacturer descriptors, button and rotary counts, display,
   haptics, `battery_service`, or a bounded vendor section.
4. If `battery_service` is `true`, expose the standard Bluetooth Battery
   Service and one-byte Battery Level characteristic. Do not invent a private
   battery field.
5. Run the Device Info vectors and the board checklist again. Optional metadata
   is display-only and never bypasses enrollment, bonding, expiry, or the
   single-consumption answer gate.

Do not use `device_id`, advertised name, serial number, or vendor facts as an
authorization secret. Hosts bind authorization to their enrolled peripheral
record and revoke it explicitly.

## Apple hosts

- Update `NextingDeviceKit` to `0.2.0-experimental.0`.
- Render `NextingDeviceInfo` as an ordered key/value table. Omit absent
  capabilities rather than filling the screen with empty placeholders.
- Read live battery from the standard Battery Service only when declared.
- Keep account-owned name, user number, and notes separate from immutable
  hardware identity.
- Maintain separate Claude Code and Codex adapters. Both may use the shared
  transport and claim gate, but one Agent's request must never be answered
  through the other's action sink.

## Android hosts

Add the Kotlin SDK module in `devices/sdk/kotlin`, then use the same Device Info
row order and visibility rules as Apple hosts. Android's BLE integration must
complete service discovery, a valid bounded Device Info read, encrypted
notification subscription, and explicit user confirmation before persisting
authorization. Store authorization in encrypted platform storage.

## User metadata

Experimental 0.2 may sync these account-owned fields by stable instance key:

- custom device name;
- optional user-visible number;
- optional notes.

They are presentation metadata, not protocol capabilities. Hardware
manufacturer, model, serial number, firmware, button counts, display, battery
support, and vendor facts remain read-only values reported by the device.

## Compatibility

| Pair                                       | Expected behavior                                                     |
| ------------------------------------------ | --------------------------------------------------------------------- |
| 0.2 Host + valid 0.1 device                | Approval/status continue; new metadata rows are absent                |
| 0.1 Host + 0.2 device                      | Required fields continue; unknown bounded optional fields are ignored |
| 0.2 Host + malformed optional vendor block | Core Device Info remains; vendor block is dropped                     |
| Unsupported wire/profile                   | Connection fails closed as before                                     |

The 0.2 label describes the SDK and Device Info contract. It does not change
wire major `1`, self-certify a board, or make production firmware public.
