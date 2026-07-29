# Nexting Device Protocol — Experimental 0.2

This document is normative for wire major `1` and profiles `approval/1`,
`status/1`, `navigation/1`, `keys/1`, `rotary/1`, `voice/1`, `text/1`,
`usage/1`, and `config/1`.

## Product contract

The protocol carries one current approval from a trusted host App to a nearby control surface. The App is the authoritative state and security decision-maker. A device presents a request and reports user input; it cannot directly authorize an internal Agent action.

Profile `approval/1` represents exactly two one-time actions: Allow maps to source option `0`, and Deny maps to source option `1`. A host must keep any prompt with a third action, persistent grant, multi-select behavior, or ambiguous option meaning on the phone. It must never infer Allow from a selected or highlighted row.

## Roles and public connection surface

- Device: BLE peripheral and GATT server.
- Host: Nexting App acting as BLE central and GATT client.
- Public transport: BLE only in Experimental 0.2.
- Public TCP, UDP, HTTP, MQTT, and cloud API ports: none.

## GATT service

| Role | UUID | Required properties | Direction |
| --- | --- | --- | --- |
| Service | `6EADC0DE-0001-4A21-9C5E-1B7F3D9E42A0` | Primary Service | — |
| Downlink | `6EADC0DE-0002-4A21-9C5E-1B7F3D9E42A0` | Write | Host → device |
| Uplink | `6EADC0DE-0003-4A21-9C5E-1B7F3D9E42A0` | Notify | Device → host |
| Device Info | `6EADC0DE-0004-4A21-9C5E-1B7F3D9E42A0` | Read | Device → host |

Write Without Response is optional. A host uses Write With Response by default and may use the optional mode only when it implements bounded flow control.

Approval traffic requires an encrypted, bonded BLE link. Device Info may be readable before authorization so the App can perform compatibility discovery, but reading it does not authorize the device.

## Device Info

The Device Info value is one compact UTF-8 JSON object:

```json
{"protocol":"nexting-device","spec":"0.2.0-experimental.2","wire":[1],"profiles":["approval/1","status/1","navigation/1","keys/1","rotary/1","voice/1","text/1","usage/1","config/1"],"model":"multi-pad","fw":"0.2.0","max_message_bytes":4096,"max_summary_bytes":240,"statusSlots":3,"device_id":"5cc0a66e-a204-4c33-a3ef-b2b352a35489","manufacturer":"ILX","display_name":"Desk Controller","serial_number":"MP-0007","button_count":12,"approval_button_count":2,"custom_button_count":10,"rotary_count":2,"rotary_press_count":2,"battery_service":true}
```

The required fields remain `protocol`, `spec`, `wire`, `profiles`, `model`,
`fw`, `max_message_bytes`, and `max_summary_bytes`. `model`, `fw`, and `spec`
are non-empty printable strings of at most 64 UTF-8 bytes.
`max_message_bytes` is 512–4294967295 and `max_summary_bytes` is 1–240. A host uses
the smaller of its own limit and the device limit. It validates the complete
newline-terminated logical frame before fragmenting or queueing it, and bounds
queued bytes independently of the negotiated frame limit.

The entire Device Info value is at most 4096 UTF-8 bytes and at most eight
nested object/array levels below the root. Duplicate known fields, decoded NUL,
control characters in descriptors, non-canonical integers, invalid UTF-8, and
unsupported required values reject the complete value. Unknown bounded fields
are ignored.

A device that implements profile `status/1` lists it in `profiles` and adds one optional field:

- `statusSlots`: integer 0–8, the number of agent-status indicators the device can render. `0` or absent means no status support; a host must not send `status` traffic to such a device.

Capability fields are additive and optional. A host ignores capability fields it does not understand, and a device omits capabilities it does not implement.

### Optional identity

- `device_id`: canonical UUID, used only to correlate user metadata. It is
  never an authorization credential.
- `manufacturer`, `display_name`, `serial_number`: printable strings of at
  most 64 UTF-8 bytes.

### Optional typed capabilities

- `button_count`, `approval_button_count`, `custom_button_count`: integers
  0–1024. A specialized count cannot exceed `button_count` when the total is
  present.
- `rotary_count`, `rotary_press_count`: integers 0–64. Press-capable rotaries
  cannot exceed total rotaries when both are present.
- `statusSlots`: integer 0–8. A positive value also requires `status/1`.
- `battery_service`: boolean. `true` declares the standard Bluetooth Battery
  Service; live level is read from the standard one-byte Battery Level
  characteristic and clamped to 0–100.
- `display`: `{ "type": string, "width": integer, "height": integer }` with a
  1–32 byte printable type and dimensions 1–4096.
- `haptics`: 1–8 unique printable pattern names, each at most 32 UTF-8 bytes.

The App omits UI rows for capabilities that are absent. Static metadata never
implies command-key, rotary-input, voice, text, or configuration profiles.

### Optional vendor facts

DIY hardware may add one inert vendor section:

```json
{"vendor":{"namespace":"com.ilx.multipad","facts":[{"key":"layers","label":"Key layers","value":"4"}]}}
```

The section is at most 1024 encoded bytes, contains a reverse-domain namespace
of at most 128 bytes, and contains 1–16 facts. Keys are unique 1–32 byte ASCII
identifiers. Labels are 1–64 printable UTF-8 bytes and values are 1–128
printable UTF-8 bytes or canonical integers. HTML, Markdown, scripts, URLs,
control characters, images, and actions are invalid. A host drops an invalid
vendor section while preserving otherwise valid core Device Info. Vendor facts
are displayed only in a vendor section and cannot override system-owned rows.

## Framing

Each logical message is one compact UTF-8 JSON object followed by a newline byte (`0x0A`). A sender may split that byte sequence across ATT writes or notifications. A receiver:

1. appends bytes to a fixed-size buffer;
2. parses exactly one message at each newline;
3. counts the terminating newline inside the logical-message byte limit;
4. supports at least 512 bytes per logical message;
5. defaults to a maximum of 4096 bytes;
6. clears the current logical message after invalid UTF-8, oversize input, or disconnect;
7. never executes a partial message;
8. does not accept interleaved fragments from different logical messages.

The official JavaScript, Swift, Kotlin, and portable C99 implementations cap a complete Experimental 0.2 frame at 4096 bytes even when Device Info advertises a larger capacity. A host may negotiate a smaller ceiling. Transport integrations must feed bounded radio or socket chunks rather than an arbitrarily large aggregate input buffer.

Downlink fragments are sent serially with Write With Response by default. Uplink answers use Notify and application-level retry: if a device does not receive `resolved` within one second, it may resend the exact same answer until resolution or local TTL expiry. It must not change the choice during retry. Answers are idempotent by request ID at the host.

All TTL comparisons use elapsed monotonic time. Wall-clock changes, time-zone changes, and clock synchronization must not extend an approval.

## Common rules

- `v` is the canonical unsigned decimal integer `1`; decimal fractions and exponent notation are invalid for every integer field.
- `id` is 1–64 ASCII characters matching `[A-Za-z0-9._:-]+`.
- Enum values are lowercase and case-sensitive.
- Unknown optional fields are ignored when their value contains no more than eight nested JSON object/array levels below the top-level message. Their field names have no separate length cap inside the bounded logical frame.
- Duplicate known fields are invalid, including keys written with equivalent JSON escapes.
- Decoded `U+0000` is invalid in every JSON string.
- Unknown message types, missing required fields, invalid bounds, and invalid enum values fail closed.
- Messages never include internal Agent type, account, session, terminal, prompt, route, or cloud identifiers.

## Messages

### Present

```json
{"v":1,"t":"present","id":"3bb7","sum":"Allow git push?","opt":["allow","deny"],"ttl":30000}
```

- `sum`: 0–240 UTF-8 bytes.
- `opt`: exactly `['allow', 'deny']` in that order.
- `ttl`: relative milliseconds from receipt, integer 1–300000.

The host sends Present only after confirming that the source prompt is an exactly two-option, one-time permission route whose option `0` is Allow and option `1` is Deny. Labels and selection state are not sufficient evidence. Ineligible prompts remain phone-only.

A new present replaces the current request. The host resolves the old request as `replaced` before presenting the new one.

### Answer

```json
{"v":1,"t":"answer","id":"3bb7","ch":"allow"}
```

`ch` is `allow` or `deny`. The device sends an answer only for its currently visible request and then waits for resolution. Repeated identical answers are allowed; the host keeps the first hardware choice locked and permits at most one authoritative action-sink attempt at a time.

### Resolved

```json
{"v":1,"t":"resolved","id":"3bb7","r":"answered"}
```

`r` is one of:

- `answered`: the host's authoritative action sink accepted the winning answer;
- `expired`: the request lifetime ended;
- `cancelled`: the host withdrew the request;
- `replaced`: a newer request superseded it.

The device clears matching UI and cached retry state immediately.

### Error

```json
{"v":1,"t":"error","id":"3bb7","code":"unknown_request"}
```

Error codes:

- `bad_message`;
- `message_too_large`;
- `unsupported_version`;
- `unsupported_profile`;
- `unknown_request`;
- `not_authorized`;
- `busy`.

An implementation may silently drop malformed or attacker-controlled input when replying would amplify traffic or disclose authorization state.

### Status

Profile `status/1`. The host sends this message on the Downlink characteristic only to a device that declared `statusSlots` of at least `1` in Device Info:

```json
{"v":1,"t":"status","agents":[{"slot":0,"state":"thinking","label":"fix login bug"}]}
```

- `agents`: array of 0–8 entries. An empty array clears every slot.
- `slot`: integer 0–7, unique within one message.
- `state`: one of `idle`, `thinking`, `working`, `complete`, `needs_input`, `error`.
- `label`: optional, 1–64 UTF-8 bytes, no control characters (U+0000–U+001F, U+007F).

Each status message carries the complete current state of every slot the host tracks: full replacement, idempotent, last write wins. A device with fewer physical indicators than slots renders the lowest-numbered slots and ignores the rest; exceeding the declared `statusSlots` is a device-side ignore, not a host error. Devices without a display ignore `label`.

Status is volatile: disconnect, reboot, or a new bond clears all rendered slots to no display. A status message never creates, answers, expires, or resolves an approval request, and an approval message never changes status rendering.

Fail closed: unknown `state`, duplicate `slot`, out-of-range `slot`, more than 8 `agents` entries, an oversized or control-character `label`, or any other malformed field discards the entire frame and keeps the previously rendered state.

### Navigation

Profile `navigation/1` presents 2–8 bounded choices without exposing the
source Agent prompt. It shares exclusive interactive focus with `approval/1`.

```json
{"v":1,"t":"nav_present","id":"q7","items":["Fix it","Explain"],"cursor":0,"ttl":30000}
{"v":1,"t":"nav_move","id":"q7","dir":"next","seq":12}
{"v":1,"t":"nav_select","id":"q7","index":1,"seq":13}
{"v":1,"t":"nav_resolved","id":"q7","r":"selected"}
```

- `id`: the common 1–64 byte request identifier.
- `items`: 2–8 unique strings, each 1–64 UTF-8 bytes with no control
  characters.
- `cursor`: integer `0...items.count - 1`.
- `ttl`: 1–300000 milliseconds.
- `dir`: `prev`, `next`, `up`, `down`, `left`, or `right`.
- `index`: integer 0–7. The Host additionally checks it against the current
  presentation.
- `seq`: unsigned 32-bit sequence number.
- terminal `r`: `selected`, `cancelled`, `expired`, or `replaced`.

Moves are advisory. The Host remains authoritative and replaces
`nav_present` when cursor state changes. A selection is consumed at most once.

### Keys

Profile `keys/1` separates a physical slot from its private Host action:

```json
{"v":1,"t":"keymap","rev":4,"keys":[{"slot":0,"label":"Approve","enabled":true,"light":"solid","rgb":[0,200,90]}]}
{"v":1,"t":"key_event","slot":0,"event":"press","seq":41}
```

`keymap` is a full volatile replacement with 0–64 unique slots in 0–63.
`label` is 1–32 UTF-8 bytes with no control characters. `light` is `off`,
`dim`, `solid`, or `pulse`. Optional `rgb` is exactly three integers in 0–255.
`event` is `press`, `release`, `hold`, or `double`.

A device emits no event for a disabled or undeclared slot. The Host maps a
slot to an authorized action; labels are presentation only. Repeating an
identical `rev` is idempotent. Conflicting content at the same revision is
rejected.

### Rotary

Profile `rotary/1` reports bounded physical rotation and press gestures:

```json
{"v":1,"t":"rotary_map","rev":8,"controls":[{"slot":0,"label":"Model","value":2,"min":0,"max":3,"wrap":true}]}
{"v":1,"t":"rotary_event","slot":0,"delta":1,"seq":52}
{"v":1,"t":"rotary_press","slot":0,"event":"press","seq":53}
```

`rotary_map` is a full volatile replacement with 0–16 unique slots in 0–15.
`label` is 1–32 UTF-8 bytes with no control characters. `min`, `max`, and
`value` are integers in -1000000–1000000 with
`min <= value <= max`. `wrap` is boolean. `delta` is a non-zero integer in
-127–127. Rotary press uses the key gesture enum. The Host sends a replacement
map after applying a delta; the device does not infer the authoritative value.

### Voice control

Profile `voice/1` controls a Host-owned microphone lifecycle:

```json
{"v":1,"t":"voice_event","event":"start","seq":61}
{"v":1,"t":"voice_state","state":"listening","label":"Release to send"}
```

`voice_event.event` is `start`, `stop`, or `cancel`. `voice_state.state` is
`idle`, `listening`, `transcribing`, `submitted`, or `error`. Optional `label`
is 1–64 UTF-8 bytes with no control characters. `stop` and `cancel` require a
currently accepted `start`.

This profile never carries audio bytes, transcripts, or credentials. A future
device-microphone transport requires a separate negotiated profile.

### Text

Profile `text/1` replaces one plain-text display channel:

```json
{"v":1,"t":"text","channel":0,"title":"Current task","content":"Waiting for approval"}
```

`channel` is 0–7. Optional `title` is 1–64 UTF-8 bytes with no control
characters. `content` is 0–1024 UTF-8 bytes; line feed and tab are permitted
and all other control characters are rejected. Empty content clears the
channel. Content is never executable markup, a link, or an action. The Host
privacy-filters it before transport.

### Usage

Profile `usage/1` publishes volatile model/token counters:

```json
{"v":1,"t":"usage","model":"GPT-5.6","input_tokens":1234,"output_tokens":567,"cached_tokens":89,"context_used":1800,"context_limit":128000}
{"v":1,"t":"usage_clear"}
```

`model` is 1–64 UTF-8 bytes with no control characters. Counters are
non-negative canonical integers at most 9007199254740991.
`cached_tokens` is optional. `context_used` and `context_limit` are optional
only as a pair and require `context_used <= context_limit`. Billing, price,
plan, account, and organization data are not part of this profile.

### Configuration

Profile `config/1` proposes a complete atomic device-owned configuration:

```json
{"v":1,"t":"config","rev":7,"entries":[{"key":"key.0.mode","value":"momentary"},{"key":"display.brightness","value":70}]}
{"v":1,"t":"config_result","rev":7,"status":"applied"}
```

`entries` contains 0–32 unique keys. A key is 1–48 ASCII characters matching
`[A-Za-z0-9][A-Za-z0-9._-]*`. A value is a boolean, integer in
-1000000–1000000, or string of 0–128 UTF-8 bytes with no control characters.
Objects, arrays, floats, and null are rejected.

The SDK validates the complete proposal before exposing it to device code.
Device code validates every supported key and commits all entries or none.
`config_result.status` is `applied` or `rejected`. An applied result has no
`code`; a rejected result requires `unknown_key`, `invalid_value`,
`storage_error`, or `unsupported`. Rejection preserves the previous revision
and configuration.

### Shared revision and sequence behavior

`rev` and `seq` are canonical unsigned 32-bit integers. A receiver remembers
the latest event sequence per physical source and connection. Repeating a
sequence is idempotent; a lower sequence is stale and ignored. Wraparound
starts a new epoch only after reconnect.

Full-replacement messages accept a newer revision. Repeating an identical
revision and content is idempotent; different content at that revision fails
closed. Disconnect clears navigation, key/rotary presentation, voice state,
text, usage, and sequence/revision memory. Only a successfully applied
`config/1` payload may persist.

## State and races

The device state is Idle, Pending, or Waiting Resolution.

- Idle + present → Pending.
- Pending + valid local input → send answer, Waiting Resolution.
- Pending + local TTL expiry → Idle.
- Pending + new present → replace local request, Pending.
- Waiting Resolution + matching resolved → Idle.
- Any state + disconnect → Idle and clear framing buffers.
- Idle + late or unknown input → ignore.

The host is authoritative. It accepts an answer only when the device is currently authorized, the request ID is current, the request is unexpired, the choice is valid, and no other answer attempt is in flight. Phone and hardware inputs compete for the same single-consumption gate.

Host answer completion is two-phase:

1. A valid phone or hardware input claims the gate and starts delivery to the authoritative Agent or action sink. A hardware claim also locks its choice for the lifetime of the request.
2. While delivery is in flight, the host does not re-present the request or start another delivery attempt, including across BLE reconnects.
3. On authoritative success, the host sends `resolved(answered)` and clears the request.
4. On failure, the host releases the in-flight gate without changing the locked hardware choice. The same hardware choice or a phone retry may attempt delivery again; a different hardware choice is rejected.
5. If TTL expires during an in-flight delivery, success may still complete it. A failure after the deadline resolves the request as `expired`.

After reconnect, the device never restores an approval from persistent storage. If the host still considers it active and no answer delivery is in flight, the host re-presents it with the remaining lifetime encoded as a fresh relative TTL.

## Versioning

Experimental releases may make breaking changes with a spec version, vector, and changelog update. After 1.0, wire major `1` only gains optional fields or new negotiated profiles. Implementations reject unsupported wire majors and profiles without guessing.

The canonical examples and rejection cases are in the versioned files under
`protocol/vectors/`. Every official SDK must produce identical behavior for
all vector files.
