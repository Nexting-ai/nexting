# macOS device simulator

Use a Mac as a real Bluetooth Low Energy peripheral when you do not have a development board nearby. The simulator advertises the complete Nexting Device Protocol Experimental 0.2 GATT service, including Device Info, and accepts the same fragmented downlink writes as hardware.

## Build and run

```sh
swiftc -warnings-as-errors -o /tmp/nexting-device-sim \
  main.swift ../../sdk/swift/Sources/NextingDeviceKit/Protocol.swift
/tmp/nexting-device-sim
```

macOS may ask for Bluetooth permission for your terminal. A real iPhone is required for the host side because the iOS Simulator does not provide CoreBluetooth device access.

When the Nexting App presents an approval, the terminal prints the summary:

- press `a` for Allow;
- press `d` for Deny.

The first valid choice is locked, sent as lowercase protocol data, and retried once per second until the App resolves it or its TTL expires. Disconnecting clears all messages and partial frames. Nothing is persisted.

This tool is for development, demos, and interoperability testing. It is not a production security device.
