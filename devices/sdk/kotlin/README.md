# Nexting Devices Kotlin SDK

The Kotlin/JVM Host SDK parses the same bounded Device Info 0.2 vectors as the
Swift, C99, and JavaScript implementations. It exposes typed hardware identity,
capabilities, inert vendor facts, and standard Battery Service constants for
Android hosts without containing Nexting App, account, Agent, or cloud code.

## Requirements

- JDK 17 bytecode target
- Gradle 9 or newer
- Kotlin 2.2

## Test

```sh
gradle test
```

`DeviceInfoCodec.decode(ByteArray)` returns `null` for malformed core data.
Malformed optional vendor data is omitted while otherwise valid core data
remains usable. `DeviceBattery.decodeLevel(ByteArray)` accepts exactly one byte
and clamps it to 0–100.
