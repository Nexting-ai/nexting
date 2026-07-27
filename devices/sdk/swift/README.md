# NextingDeviceKit

`NextingDeviceKit` is the Apple host SDK for Nexting Device Protocol Experimental 0.2. It provides strict message encoding and decoding, newline framing, extensible Device Info negotiation, standard Battery Service helpers, explicit peripheral authorization, approval coordination, and a CoreBluetooth central.

The package does not contain the Nexting App, Agent adapters, cloud APIs, UI, accounts, or production device identity. A host application supplies its own prompt context, enrollment UI, risk policy, and final Agent answer path.

Start with [the public foundation](../../docs/foundation-development.md) and [interface catalog](../../docs/interfaces.md). Product integration steps are in [the Host/App track](../../docs/implementation-tracks.md#track-2-integrate-a-host-or-app).

## Requirements

- Swift 6
- iOS 16 or newer
- macOS 13 or newer
- A real iPhone for BLE host testing

## Build and test

From the repository root:

```sh
swift test --package-path sdk/swift
swift build --package-path sdk/swift
```

The test target reads the canonical vectors from
`protocol/vectors/approval-v1.json`, `status-v1.json`, and
`device-info-v1.json`; it does not keep a private copy.

## Public surface

| Type | Responsibility |
| --- | --- |
| `NextingDeviceMessage` | Present, Answer, Resolved, and Error values |
| `NextingDeviceCodec` | Strict newline-terminated wire encoding and decoding |
| `NextingDeviceLineDecoder` | Bounded fragmented-message assembly |
| `NextingDeviceInfo` | Negotiated wire, identity, capabilities, vendor facts, firmware, and limits |
| `NextingDeviceBattery` | Standard Battery Service UUIDs and bounded Battery Level parsing |
| `NextingDeviceAuthorizationStore` | Explicit stable-peripheral allowlist |
| `NextingDeviceAuthorizationPolicy` | Release deny-by-default and optional Debug simulator rule |
| `NextingDeviceCentral` | Recoverable CoreBluetooth setup, negotiated limits, and bounded acknowledged writes |
| `NextingDeviceSendRejection` | Explicit not-ready, oversize-frame, and queue-backpressure results |
| `NextingDevicePromptRelay` | One-current-prompt state, two-phase answer completion, and race handling |
| `NextingDeviceRelayCoordinator` | Connects an authorized transport to host prompt context and authoritative action result |
| `NextingDeviceAnswerClaimGate` | Single-consumption gate shared by phone and hardware input |

## Encode a message

```swift
import NextingDeviceKit

let wire = NextingDeviceCodec.encode(
    .present(
        requestId: "request-1",
        summary: "Allow git push?",
        ttlMs: 30_000
    )
)
```

`wire` is optional because invalid IDs, summaries, or TTL values fail closed.

The codec's public Experimental 0.2 ceiling is 4096 bytes for the complete compact JSON message including its terminating newline. `NextingDeviceLineDecoder` may be configured lower, but not higher, and transport callbacks should pass bounded chunks into it.

## Create an authorized BLE central

```swift
import NextingDeviceKit

let store = NextingDeviceAuthorizationStore()
let policy = NextingDeviceAuthorizationPolicy(
    store: store,
    allowDebugTestDevice: false
)
let central = NextingDeviceCentral(
    authorizationPredicate: policy.isAuthorized
)

central.onMessage = { identity, message in
    guard policy.isAuthorized(identity) else { return }
    print(message)
}
central.onSendRejected = { reason in
    print("Device send rejected: \(reason)")
}
central.startScan()
```

Production code must provide explicit enrollment and revocation before calling `store.authorize`. Never authorize by peripheral name in a release build.

## Integration rules

- Keep the host App authoritative. A device reports physical input; it does not directly authorize an Agent action.
- Present hardware only for an exact two-option, one-time permission route: Allow is source option `0`, Deny is source option `1`, and highlight state is irrelevant.
- Call `phoneAnswerStarted()` before a phone API attempt, then call request-scoped `answerSucceeded` or `answerFailed` only after the authoritative action result. Do the same completion step after a routed hardware answer.
- Send only an opaque request ID, bounded summary, fixed choices, and relative TTL.
- Share one claim gate between phone and hardware answers.
- Clear framing and approval state on disconnect.
- Respect Device Info limits and reject incompatible wire versions or profiles.
- Treat a `false` return from `send` as a delivery failure. `onSendRejected` also reports whether the cause was not-ready state, an oversized logical frame, or write-queue backpressure.
- Keep both pending bytes and pending frame count bounded. `NextingDeviceCentral` defaults to at most 64 retained logical frames in addition to its byte ceiling.
- Require an encrypted bonded link for approval traffic.

See [the protocol specification](../../SPEC.md), [security model](../../SECURITY.md), and [current status](../../docs/project-status.md) before using the package in a product.
