# Device audio (`device-audio/1`)

`device-audio/1` is the standard path from a microphone on a physical device to
the official Nexting App. It is not a remote microphone switch and it is not a
renaming of `voice/1`:

- `voice/1` asks the Host App to start, stop, or cancel the phone/computer
  microphone;
- `device-audio/1` carries audio that the physical device sampled itself.

Both paths enter the App's existing speech-to-text and selected-Agent send
pipeline. A device never receives Agent credentials, session IDs, cloud routes,
or transcripts through this profile.

## Capability declaration

Declare the profile only when the production firmware has a working microphone
hook and can sustain the exact advertised format:

```json
{
  "profiles": ["approval/1", "device-audio/1"],
  "audio": {
    "source": "device_microphone",
    "codecs": ["ima_adpcm"],
    "sample_rates": [16000],
    "channels": 1,
    "frame_ms": [20],
    "max_duration_ms": 120000,
    "startup_buffer_ms": 200
  }
}
```

A stub, simulator, failed microphone initialization, or transport below the
64-byte ATT payload baseline must omit `device-audio/1`; it cannot advertise the
profile and fail later.

## Privacy and physical-start rule

The Host sends `audio_config` to say it is authorized and ready. That message,
credits, state messages, reconnects, and Agent actions cannot start sampling.
Only the device's physical voice input can create `audio_begin`. The device may
buffer at most 200 ms captured after that input; always-on capture and pre-roll
before the input are forbidden.

Audio remains in bounded RAM on the device. The Host clears transport buffers
and temporary containers when the active stream ends. A submitted recording may
follow the Host App's existing voice-history, retry, retention, and deletion
policy; device integrations do not get a separate storage path. Cancel, Reject,
disconnect, timeout, and error paths erase the device-side stream.

## Standard interaction

- Voice key down starts capture immediately.
- Release before 450 ms latches recording; the next voice-key down submits.
- Release at or after 450 ms submits the held recording.
- Submit drains queued frames and ends the stream.
- Reject cancels and erases the stream.
- Maximum duration is 120 seconds.
- Reconnect never restores a recording.

The device and App must visibly distinguish ready, recording, transcribing,
submitted, and error states. Silent recording is non-conformant.

## Transport and failure limits

The wire layout is normative in [`SPEC.md`](../SPEC.md); executable examples are
in [`device-audio-v1.json`](../protocol/vectors/device-audio-v1.json).

- eight initial frame credits;
- ten queued frames (200 ms) maximum;
- 500 ms maximum continuous credit stall;
- four fragments per logical frame maximum;
- sequence `0...5999`, with sample index exactly `sequence × 320`;
- final sample count exactly `(last sequence + 1) × 320`;
- 64-byte negotiated ATT payload minimum;
- duplicate frames are ignored;
- one missing frame inserts 20 ms silence;
- three consecutive or ten total missing frames terminate the stream;
- bad CRC, unknown stream, inconsistent fragments, invalid ADPCM metadata, or
  uncertain sample position terminate and erase the stream.

Run the JavaScript/C conformance and deterministic lifecycle tests with:

```sh
npm run test:reference
npm run test:simulator
cmake -S sdk/c -B sdk/c/.build
cmake --build sdk/c/.build --parallel
ctest --test-dir sdk/c/.build --output-on-failure
```
