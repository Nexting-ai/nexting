# Nexting Compatibility Tunnel v1

Native Nexting GATT is the preferred production transport. Compatibility
Tunnel v1 exists for hardware whose supported firmware API can register bounded
commands inside an existing encrypted BLE service but cannot add the native
Nexting service.

The tunnel changes only the envelope. Device Info, JSONL control messages,
binary audio frames, authorization, sequence, credit, and cancellation rules
remain the standard Nexting contracts.

## Data-only descriptor

The official App loads audited descriptors matching
[`transport-descriptor.schema.json`](../schemas/transport-descriptor.schema.json).
A descriptor can declare only:

- an integration ID and display name;
- one discovery service UUID;
- write/notify characteristic UUIDs;
- the fixed `opcode-prefix-v1` envelope;
- four distinct opcodes for `control_down`, `control_up`, `audio_up`, and
  `flow_down`;
- bounded payload and ATT requirements;
- mandatory bonding and encryption.

It cannot provide code, a parser, a URL, a cloud route, an Agent action, a
script, credentials, or arbitrary Bluetooth behavior. The only envelope is one
opcode byte followed by a bounded unmodified Nexting payload.

## Trust and selection

A descriptor match only permits a Nexting handshake; it does not authorize the
device. Enrollment is explicit and revocable. The handshake must return strict
Device Info with a stable `device_id` and the relevant profile.

If the same stable device exposes native GATT and a tunnel, the Host represents
one device and activates native GATT only. It falls back to an already enrolled
tunnel after native failure; it never opens parallel approval or audio streams.

Scanning a vendor service occurs only while the user is adding a third-party
device or reconnecting the currently enrolled device. Removing a descriptor
must leave native devices, the App core, and every generic conformance test
working.

## Implement and verify

Start from the vendor-neutral vector in
[`compatibility-tunnel-v1.json`](../protocol/vectors/compatibility-tunnel-v1.json).
Keep vendor UUIDs and opcodes in the descriptor and device adapter, never in
App core, Agent routing, accounts, or cloud code.

```js
import {
  encodeTunnelPacket,
  parseTransportDescriptor,
  parseTunnelPacket,
} from "@nexting-ai/device-reference/tunnel";
```

The reference parser additionally rejects duplicate opcodes, wrong
characteristic direction, an ATT payload below 64 bytes, unknown fields, and
unknown strategies.
