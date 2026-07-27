# Use cases

What people build on Nexting Devices, and which profiles each product consumes. Every scenario runs the same loop: the device talks BLE to the trusted Host App, and the Host reaches the user's agents anywhere — across the room or across the internet. A device never holds agent credentials.

Each scenario lists the profiles it uses today and the evidence level it can realistically claim. Claim wording is governed by [the conformance guide](conformance.md); build routes are in [the implementation tracks](implementation-tracks.md).

## Today: profiles `approval/1` and `status/1`

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

## Roadmap scenarios

These need profiles that are specified but not yet shipped. They are listed so makers can plan hardware; do not claim them until each profile lands with its own vectors and evidence. See [the capability roadmap](foundation-development.md#beyond-experimental-02-the-capability-roadmap).

| Scenario | Needs | Product shape |
| --- | --- | --- |
| Full command macropad | Command keys profile | 8–13 keys mapped to agent commands (approve, decline, fork, send, fast) with status backlighting |
| Menu navigator | Navigation input profile | Stick or wheel for radial menus and workflow selection |
| Rotary controller | Rotary profile | Dial scrubbing through option levels, with the current level pushed down to the device |
| Reader device | Text content profile | Larger conversation content for screened devices |
| Talk-to-agent remote | Voice profile | Push-to-talk key with device-microphone audio or Host-side capture |
| Reconfigurable pad | Configuration profile | Key maps and lighting downloaded from the Host, no reflash |

## Rules every scenario inherits

- Deny by default: a device does nothing until the user explicitly authorizes it in the Host App; revocation stops all traffic.
- Fail closed: malformed, stale, oversized, or unauthorized input never approves anything and never corrupts the display.
- Volatile state: approvals and status live in RAM and clear on disconnect, reboot, or a new bond.
- Honest capabilities: declare only what the hardware renders. A device with one LED declares `statusSlots: 1`, not 8; a device with no indicator omits the field and receives no status traffic.
