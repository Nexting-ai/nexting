import Foundation
import NextingDeviceKit

#if canImport(Darwin)
import Darwin
#endif

private let usage = """
Usage: nexting-device-host-smoke [--summary TEXT] [--timeout 5...300]

Discovers one nearby Nexting reference device, validates Device Info, sends a
synthetic approval over encrypted BLE, and exits after a real button answer.
Success: PASS answer=allow or PASS answer=deny
"""

if CommandLine.arguments.dropFirst().contains("--help") {
    print(usage)
    exit(0)
}

let configuration: NextingDeviceHostSmokeConfiguration
do {
    configuration = try .parse(arguments: Array(CommandLine.arguments.dropFirst()))
} catch {
    FileHandle.standardError.write(
        Data("ERROR arguments=\(error)\n\(usage)\n".utf8)
    )
    exit(64)
}

var authorizedID: UUID?
var finished = false
let requestID = "host-smoke-\(UUID().uuidString.prefix(8).lowercased())"
let startedAt = Date()

let central = NextingDeviceCentral(
    authorizationPredicate: { identity in
        identity.id == authorizedID
    }
)

@MainActor
func fail(_ reason: String, repair: String) -> Never {
    guard !finished else { exit(2) }
    finished = true
    FileHandle.standardError.write(
        Data("FAIL reason=\(reason)\nREPAIR \(repair)\n".utf8)
    )
    central.disconnect()
    exit(2)
}

central.onDiscovered = { identity, rssi in
    guard authorizedID == nil else { return }
    authorizedID = identity.id
    print(
        "DISCOVERED name=\(identity.name ?? "(unnamed)") id=\(identity.id.uuidString) rssi=\(rssi)"
    )
    print("AUTHORIZATION ephemeral=first-discovered persistence=none")
}

central.onSendRejected = { rejection in
    fail(
        "send_rejected_\(String(describing: rejection))",
        repair: "Reflash the tagged reference firmware and retry."
    )
}

central.onConnectedChange = { ready in
    guard ready, !finished else { return }
    guard let identity = central.connectedIdentity,
          let info = central.connectedDeviceInfo else {
        fail(
            "missing_device_info",
            repair: "See docs/troubleshooting.md#the-host-rejects-device-info."
        )
    }

    print("CONNECTED id=\(identity.id.uuidString)")
    print("Device Info:")
    print("  model=\(info.model)")
    print("  firmware=\(info.firmwareVersion)")
    print("  protocol=\(info.protocolName)")
    print("  wire=\(info.wireVersions.map(String.init).joined(separator: ","))")
    print("  profiles=\(info.profiles.joined(separator: ","))")
    print("  max_message_bytes=\(info.maxMessageBytes)")
    print("  max_summary_bytes=\(info.maxSummaryBytes)")
    print("  buttons=\(info.capabilities.buttonCount.map(String.init) ?? "unknown")")
    if let battery = central.connectedBatteryLevel {
        print("  battery=\(battery)%")
    }

    guard let wire = NextingDeviceCodec.encode(.present(
        requestId: requestID,
        summary: configuration.summary,
        ttlMs: configuration.timeoutSeconds * 1_000
    )), central.send(wire) else {
        fail(
            "present_not_sent",
            repair: "Check Device Info limits and rerun the command."
        )
    }
    print("PRESENT id=\(requestID) ttl=\(configuration.timeoutSeconds)s")
    print("ACTION press Allow or Deny once")
}

central.onMessage = { identity, message in
    guard !finished, identity.id == authorizedID else { return }
    guard case let .answer(id, choice) = message, id == requestID else { return }
    finished = true
    if let resolved = NextingDeviceCodec.encode(.resolved(
        requestId: requestID,
        reason: .answered
    )) {
        _ = central.send(resolved)
    }
    print(NextingDeviceHostSmokeConfiguration.passLine(choice: choice))
    DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
        central.disconnect()
        exit(0)
    }
}

print("Bluetooth discovery starting; approve the macOS Bluetooth prompt if shown.")
central.startScan()

let timer = Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { _ in
    Task { @MainActor in
        guard !finished else { return }
        if Date().timeIntervalSince(startedAt) >= Double(configuration.timeoutSeconds) {
            fail(
                "timeout",
                repair: "Turn on Bluetooth, grant Terminal access, power the board, then read docs/troubleshooting.md."
            )
        }
    }
}
RunLoop.main.add(timer, forMode: .common)
RunLoop.main.run()
