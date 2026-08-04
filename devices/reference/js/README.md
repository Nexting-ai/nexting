# JavaScript protocol reference

This package is the executable, human-readable reference for Nexting Device Protocol Experimental 0.2. Use it to understand the wire contract, generate conformance fixtures, and compare another SDK or firmware port with the canonical behavior.

It is deliberately marked `private`. Experimental 0.2 does not publish this package to npm, and a product should not make its runtime depend on the reference implementation. Use it for protocol tests, fixture generation, and a portable implementation reference; firmware should use the C99 device core. The official Nexting App owns Bluetooth enrollment, authorization, and Agent actions.

## Explicit entry points

```js
import { decode, encode } from "@nexting-ai/device-reference/protocol";
import { createLineDecoder } from "@nexting-ai/device-reference/framing";
import { createApprovalRelay } from "@nexting-ai/device-reference/relay";
```

- `protocol` validates and encodes the four `approval/1` message types.
- `framing` turns bounded newline-delimited byte chunks into messages.
- `relay` demonstrates replacement, TTL, authorization, phone/device races, and two-phase Agent completion.

The official reference enforces the Experimental 0.2 host ceiling: a complete JSON message plus its terminating newline must fit within 4096 bytes. `createLineDecoder` accepts a configured maximum from 1 through 4096 bytes and rejects an individual input chunk above 4096 bytes, so callers must stream transport-sized chunks rather than an unbounded file or socket buffer.

For local tools, install the directory as a file dependency or run code from inside this package. The named exports are intentional; importing private files below `src/` is not a supported interface.

## Verify

```sh
npm test
```

Normative behavior still comes from [`SPEC.md`](../../SPEC.md) and the [shared vectors](../../protocol/vectors/approval-v1.json). If source, tests, and specification disagree, follow the change order in [`AGENTS.md`](../../AGENTS.md) rather than choosing one implementation silently.
