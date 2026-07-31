# Start with Nexting

Nexting turns nearby buttons, rotaries, LEDs, and displays into a physical
remote for an Agent. The device never talks directly to Claude Code, Codex, or
another Agent:

```text
Nexting device ⇄ encrypted BLE ⇄ trusted Host ⇄ Agent integration
```

The trusted Host is the Nexting App or another compatible phone/computer Host.
It owns credentials, authorization, policy, and Agent-specific mapping. The
device owns physical input and minimum display state.

## What each layer does

| Layer | Responsibility | Does not receive |
| --- | --- | --- |
| Nexting device | Report physical intent and render bounded state | Agent credentials, accounts, session routes |
| Trusted Host | Authorize the device, validate fresh input, and map profiles to Agent actions | Unbounded or unauthenticated device commands |
| Agent integration | Continue the Claude Code, Codex, or compatible Agent session | A direct connection from accessory firmware |

This separation lets one public device protocol support a button, wearable,
macropad, desk panel, or custom display without placing private Agent logic in
firmware.

## Choose your path

| Path | Start | First proof |
| --- | --- | --- |
| Use a supported first-party Nexting product | Follow that product's in-App onboarding, connect the Agent, then pair and authorize the product | The same Agent session receives one validated physical action |
| Build with the Nexting SDK | Download the public SDK and run the public Host or simulator before adapting hardware | A deterministic profile exchange or `PASS answer=...` local protocol proof |

First-party product onboarding does not authorize third-party or DIY hardware.
If you do not own a supported product, use the SDK path today.

**Have a supported board?**
[Build the reference approval controller now](docs/reference-approval-controller.md)
and prove a real button without turning the general Quickstart into a board
assembly guide.

Public third-party developer-device enrollment is a separate release gate. The
machine-readable status in [`docs/availability.json`](docs/availability.json)
currently records iOS and Android enrollment as unavailable. Do not use an
unpublished App build or weaken BLE authorization. The supported fallback is
the public Host smoke test.

## First remote interaction

Every supported interaction follows the same shape:

1. The Agent produces a request or state change.
2. The Host verifies that the selected device is authorized and declared the
   required profile.
3. The Host sends only bounded state over encrypted BLE.
4. The user presses, turns, navigates, or holds a physical control.
5. The Host validates identity, freshness, sequence, and policy before applying
   the mapped action to the same Agent session.

With a released first-party product and supported Agent integration, this is a
remote Agent interaction. With the public SDK and Host smoke test, it is a
local protocol proof: useful for validating the open contract, but not a claim
that the public Nexting App already enrolls DIY hardware.

## Why the Host exists

- Agent credentials and authoritative sessions stay on the trusted Host.
- Firmware implements stable physical intent rather than Claude Code or Codex
  internals.
- Agent mappings and risk policy can change without reflashing the accessory.
- The Host rejects undeclared profiles, stale sequences, expired requests,
  duplicate answers, and revoked devices.
- The same device contract works across richer custom hardware.

## Security in one minute

The local control link uses BLE LE Secure Connections, bonding, and encrypted
GATT access. The device receives minimum profile data, such as an opaque
request ID, bounded summary, fixed choices, relative lifetime, or volatile
display state.

The Developer Reference uses BLE “Just Works.” That encrypts the bonded link
but does not authenticate against an active person-in-the-middle during
pairing. Production devices need authenticated application identity and signed
firmware. Read the complete [Security model](SECURITY.md) before making a
product claim.

## Next steps

- [Build the reference approval controller](docs/reference-approval-controller.md)
- [Troubleshoot setup, BLE, and Device Info](docs/troubleshooting.md)
- [Map a Codex App Server request in your own Host](docs/codex-app-server.md)
- [Integrate the C99 Device SDK](sdk/c/README.md)
- [Integrate the Swift Host SDK](sdk/swift/README.md)
- [Integrate the Kotlin Host SDK](sdk/kotlin/README.md)
- [Read every public interface](docs/interfaces.md)
- [Read the normative protocol](SPEC.md)
