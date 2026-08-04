# Security

Nexting Devices carries approval input, so a working BLE demo is not automatically a secure product. This document states the trust boundary, minimum controls, and limits of Experimental 0.2.

## Trust boundary

The trusted Nexting App is the final decision-maker. A device can display a request and report physical input, but the App verifies device authorization, request identity, expiry, single consumption, and risk policy before forwarding an answer to an Agent.

The device receives only an opaque request ID, a bounded summary, fixed choices, and a relative lifetime. It does not receive an account ID, Agent name, session ID, terminal ID, prompt ID, route, cloud token, or Agent credential.

## Minimum transport controls

- BLE LE Secure Connections and bonding;
- encrypted GATT permissions required for control writes and notification
  subscription;
- explicit user authorization and revocation in the App;
- fail-closed parsing and bounded buffers;
- relative expiry, single consumption, and duplicate suppression;
- state and partial-frame clearing on disconnect;
- production firmware signing;
- no name-only authorization in release builds.

## Protected local link

The Zephyr Developer Reference enables BLE LE Secure Connections, bonding,
controller privacy, and bonding-required policy. It requests
`BT_SECURITY_L2` after connecting. Control writes require
`BT_GATT_PERM_WRITE_ENCRYPT`; notification subscription requires encrypted read
and write permissions.

Device Info may remain readable for compatibility discovery. Reading identity
and declared capabilities never authorizes a device or a physical action.

## Authorization and freshness

The trusted Host validates explicit authorization, current request identity,
expiry, choice membership, sequence or revision, duplicate and replay state,
and single consumption before it applies physical intent. Phone and hardware
inputs compete for the same one-answer gate. Unsupported, stale, malformed, or
already-consumed input fails closed.

## Minimum disclosure

Firmware receives bounded profile data such as an opaque request ID, short
summary, fixed choices, relative lifetime, or volatile display state. Agent
credentials, account, session, terminal, prompt and cloud route identifiers,
and tokens remain on the trusted Host.

`voice/1` controls the Host microphone lifecycle and never carries audio bytes or transcripts.
Audio permission, capture, encoding, transport, and transcription remain Host
responsibilities.

## State and revocation

State and partial-frame clearing on disconnect prevents a device from rendering
or acting on an abandoned exchange. Disconnect, reboot, a new bond, or physical
bond reset clears the applicable volatile requests, rendered state,
sequence/revision memory, and partial frames defined by the protocol. The Host
can revoke a previously authorized device independently of its BLE bond.

## Developer Reference is not certification

The macOS simulator and reference boards are development tools. They must be visibly identified as Developer Reference devices in names, logs, and App UI.

BLE “Just Works” can create an encrypted bonded link but does not provide authenticated protection against an active person-in-the-middle during pairing. A no-display reference board therefore must not be marketed as a production-secure approval device solely because bonding is enabled.

A production device needs an authenticated application identity, such as a QR/OOB bootstrap secret or a device-certificate challenge. A third party may operate its own identity root. Use of a future “Nexting Compatible” security certification may require the Nexting certification and revocation service.

The App may require phone confirmation for high-risk actions even when the accessory identity is valid.

## Threats considered

- a nearby unauthorized BLE peripheral attempting to receive summaries or answer;
- a previously authorized but revoked device;
- replayed, duplicate, stale, replaced, or expired answers;
- phone and hardware answers racing;
- malformed JSON, invalid UTF-8, oversized or partial frames;
- disconnect and reconnect during an active request;
- a malicious device claiming unsupported capabilities;
- accidental publication of private App code, production firmware, secrets, or internal routing.

## Not solved by Experimental 0.2 alone

- physical compromise of the device;
- a malicious authorized device automatically pressing Allow;
- authenticated pairing for a no-input/no-display board without an application identity layer;
- compromise of the phone, computer, Agent runtime, or third-party firmware supply chain;
- public cloud device access, which is outside this BLE-only release.

## Reporting a vulnerability

Do not open a public issue for an unpatched vulnerability. Until the dedicated public security address is activated, report privately through the security contact listed on [nexting.ai](https://nexting.ai/). Include the affected spec version, device or SDK, reproduction steps, and whether approval confidentiality or integrity is affected.

No bounty or response-time promise is made for this Experimental public
preview.
