# Use cases

What people build on Nexting Devices, and which profiles each product consumes. Every scenario runs the same loop: the device talks BLE to the trusted Host App, and the Host reaches the user's agents anywhere — across the room or across the internet. A device never holds agent credentials.

Each scenario lists the profiles it uses today and the evidence level it can realistically claim. Claim wording is governed by [the conformance guide](conformance.md); build routes are in [the implementation tracks](implementation-tracks.md).

## Available profiles

### 1. Two-button approval pad

The first product: a small pad with an Allow button, a Deny button, and one Pending light. An agent asks for permission; the pad lights up; the user answers without picking up the phone.

- Profiles: `approval/1`.
- Hardware: any reference board, two buttons, one LED. The Zephyr reference firmware is exactly this product.
- Evidence: Build verified from the pinned CI matrix today; Board verified after the real-iPhone checklist.

### 2. Agent status light bar

A strip or cluster of indicator lights (up to eight) that mirrors what the user's agents are doing: idle, thinking, working, complete, needs input, error. This is the dedicated-macropad experience — a glance replaces opening the app.

- Profiles: `status/1`. The device declares `statusSlots` in Device Info (one per light); the Host pushes full-replacement status frames.
- Hardware: 1–8 LEDs, key backlights, or a tiny display. No buttons required.
- Notes: status is volatile by design — it clears on disconnect, so the bar never lies about a stale agent. A malformed frame keeps the previous display instead of blanking or corrupting it.

### 3. Desk status panel with screen

A desktop panel (LCD or e-ink) that shows both dimensions: the current approval with its summary text, and per-agent status rows with short labels like "fix login bug".

- Profiles: `approval/1` + `status/1`, including the optional 64-byte status `label`.
- Hardware: any reference MCU family plus a display; the C99 core leaves rendering entirely to the integrator.
- Notes: the Host sends minimum-necessary text. A panel never receives file contents, diffs, secrets, or session identifiers — summaries and labels only.

### 4. Wearable answer badge

A clip-on badge: one RGB LED for the current slot-0 agent state, one button that answers Allow, and a long-press for Deny. Approvals and agent state follow the user around the house while the agent runs on a workstation or in the cloud.

- Profiles: `approval/1` + `status/1` with `statusSlots: 1`.
- Hardware: smallest XIAO-class board, one LED, one button; battery-friendly because both profiles are idle-quiet — traffic only flows on state changes.

## Full control-surface scenarios

These profiles ship in `0.2.0-experimental.2` with normative SPEC sections,
shared vectors, and C99, Swift, Kotlin, and JavaScript codecs. Product actions
still require a trusted Host adapter; a generic key event does not itself
authorize or invoke an Agent command.

| Scenario | Needs | Product shape |
| --- | --- | --- |
| Full command macropad | `keys/1` + `status/1` | 8–13 generic keys mapped by the Host to approve, decline, fork, send, or fast, with status backlighting |
| Menu navigator | `navigation/1` | Stick or wheel for bounded options; Host validates the selected request |
| Rotary controller | `rotary/1` | Relative dial events for a Host-owned session or model list, without exposing internal IDs |
| Reader device | `text/1` | Bounded plain text for a declared screen; no markup, file content, or secrets |
| Talk-to-agent remote | `voice/1` | Push-to-talk control while the Host microphone performs capture and transcription; no BLE audio |
| Usage display | `usage/1` | Model label and bounded counters that are informational rather than billing authority |
| Reconfigurable pad | `config/1` + `keys/1` | Atomic key, lighting, and display preferences downloaded from the Host without reflashing |

## Rules every scenario inherits

- Deny by default: a device does nothing until the user explicitly authorizes it in the Host App; revocation stops all traffic.
- Fail closed: malformed, stale, oversized, or unauthorized input never approves anything and never corrupts the display.
- Volatile state: approvals and status live in RAM and clear on disconnect, reboot, or a new bond.
- Honest capabilities: declare only what the hardware implements. A device with one LED declares `statusSlots: 1`, not 8; a device without a dial omits `rotary/1` and receives no rotary map.
