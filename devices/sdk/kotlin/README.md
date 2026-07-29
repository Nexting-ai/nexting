# Nexting Devices Kotlin SDK — 0.2.0-experimental.2

The Kotlin/JVM Host SDK parses the same bounded Device Info and nine-profile
Wire vectors as the Swift, C99, and JavaScript implementations. It exposes
typed hardware identity, capabilities, inert vendor facts, and standard Battery
Service constants for Android Hosts without containing Nexting App, account,
Agent, or cloud code.

## Requirements

- JDK 17 bytecode target
- `curl` and `unzip` for the checksum-pinned Gradle 9.0.0 launcher
- Kotlin 2.2

## Test

```sh
./gradlew test
```

The launcher downloads the official binary distribution into the user Gradle
cache and verifies its published SHA-256 before execution.

`DeviceInfoCodec.decode(ByteArray)` returns `null` for malformed core data.
Malformed optional vendor data is omitted while otherwise valid core data
remains usable. `DeviceBattery.decodeLevel(ByteArray)` accepts exactly one byte
and clamps it to 0–100.

## Encode and route an interaction

```kotlin
val message = DeviceMessage.RotaryEvent(
    slot = 0,
    delta = 1,
    sequence = 42,
)
if (deviceInfo.supportsProfile(message.requiredProfile)) {
    val wire = DeviceMessageCodec.encode(message) ?: return
    transport.send(wire)
}
```

`DeviceMessage` covers `approval/1`, `status/1`, `navigation/1`, `keys/1`,
`rotary/1`, `voice/1`, `text/1`, `usage/1`, and `config/1`.
`interactionSequence` identifies the per-source counter used to reject replayed
or out-of-order navigation, key, rotary, and push-to-talk events.

`voice/1` is control only. Android microphone permission, audio capture,
transcription, Agent mapping, encrypted authorization storage, and UI stay in
the Host application.
