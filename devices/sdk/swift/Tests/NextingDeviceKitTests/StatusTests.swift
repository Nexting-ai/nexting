import Foundation
import Testing
@testable import NextingDeviceKit

private struct StatusVectorDocument: Decodable {
    let spec: String
    let wire: Int
    let profile: String
    let valid: [StatusValidVector]
    let invalid: [String]
}

private struct StatusValidVector: Decodable {
    let wire: String
    let decoded: StatusDecodedVector
}

private struct StatusDecodedVector: Decodable {
    struct Agent: Decodable {
        let slot: Int
        let state: String
        let label: String?
    }

    let type: String
    let agents: [Agent]

    var message: NextingDeviceMessage {
        get throws {
            #expect(type == "status")
            return .status(agents: try agents.map { agent in
                NextingDeviceAgentStatus(
                    slot: agent.slot,
                    state: try #require(NextingDeviceAgentState(rawValue: agent.state)),
                    label: agent.label
                )
            })
        }
    }
}

private func loadStatusVectors() throws -> StatusVectorDocument {
    let testFile = URL(fileURLWithPath: #filePath)
    let repository = testFile
        .deletingLastPathComponent() // NextingDeviceKitTests
        .deletingLastPathComponent() // Tests
        .deletingLastPathComponent() // swift
        .deletingLastPathComponent() // sdk
        .deletingLastPathComponent() // nexting-devices
    let url = repository.appendingPathComponent(
        "protocol/vectors/status-v1.json",
        isDirectory: false
    )
    return try JSONDecoder().decode(
        StatusVectorDocument.self,
        from: Data(contentsOf: url)
    )
}

@Test("status/1 shared vectors round-trip canonically")
func statusVectorsRoundTrip() throws {
    let vectors = try loadStatusVectors()
    #expect(vectors.spec == "0.1.0-experimental")
    #expect(vectors.wire == 1)
    #expect(vectors.profile == "status/1")

    for item in vectors.valid {
        let expected = try item.decoded.message
        #expect(NextingDeviceCodec.decode(Data(item.wire.utf8)) == expected)
        #expect(NextingDeviceCodec.encode(expected) == Data(item.wire.utf8))
    }
    for wire in vectors.invalid {
        #expect(NextingDeviceCodec.decode(Data(wire.utf8)) == nil)
    }
}

@Test("status/1 vectors cover every state and both boundaries")
func statusVectorCoverage() throws {
    let vectors = try loadStatusVectors()
    let states = Set(vectors.valid.flatMap { $0.decoded.agents.map(\.state) })
    #expect(
        states == ["idle", "thinking", "working", "complete", "needs_input", "error"]
    )
    #expect(vectors.valid.contains { $0.decoded.agents.isEmpty })
    #expect(
        vectors.valid.contains {
            $0.decoded.agents.count == NextingDeviceCodec.maxStatusAgents
        }
    )
    #expect(
        vectors.valid.contains { item in
            item.decoded.agents.contains {
                $0.label.map { $0.utf8.count == NextingDeviceCodec.maxStatusLabelBytes }
                    ?? false
            }
        }
    )
    #expect(vectors.invalid.count >= 8)
}

@Test("status encode rejects bad shapes")
func statusEncodeRejectsBadShapes() {
    #expect(
        NextingDeviceCodec.encode(
            .status(agents: [.init(slot: 8, state: .idle)])
        ) == nil
    )
    #expect(
        NextingDeviceCodec.encode(
            .status(agents: [
                .init(slot: 0, state: .idle),
                .init(slot: 0, state: .working),
            ])
        ) == nil
    )
    #expect(
        NextingDeviceCodec.encode(
            .status(agents: [
                .init(slot: 0, state: .idle, label: String(repeating: "x", count: 65)),
            ])
        ) == nil
    )
    #expect(
        NextingDeviceCodec.encode(
            .status(agents: [.init(slot: 0, state: .idle, label: "bad\u{7}bell")])
        ) == nil
    )
    #expect(
        NextingDeviceCodec.encode(
            .status(agents: [.init(slot: 0, state: .idle, label: "")])
        ) == nil
    )
}

@Test("device info parses the optional statusSlots capability")
func deviceInfoStatusSlots() throws {
    let capable = try #require(NextingDeviceInfo.decode(Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\",\"status/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240,\"statusSlots\":4}".utf8
    )))
    #expect(capable.statusSlots == 4)
    #expect(capable.supportsStatusV1)

    let plain = try #require(NextingDeviceInfo.decode(Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240}".utf8
    )))
    #expect(plain.statusSlots == 0)
    #expect(!plain.supportsStatusV1)

    let outOfRange = NextingDeviceInfo.decode(Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\",\"status/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240,\"statusSlots\":9}".utf8
    ))
    #expect(outOfRange == nil)

    let zeroSlots = try #require(NextingDeviceInfo.decode(Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\",\"status/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240,\"statusSlots\":0}".utf8
    )))
    #expect(!zeroSlots.supportsStatusV1)
}

@Test("coordinator publishes status only to capable authorized devices")
func coordinatorStatusGating() throws {
    let transport = StatusFakeTransport()
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    let attacker = NextingDevicePeripheralIdentity(id: UUID(), name: "Attacker")
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { "opaque-1" },
        answerPrompt: { _, _ in }
    )
    let agents: [NextingDeviceAgentStatus] = [
        .init(slot: 0, state: .thinking, label: "fix login bug"),
    ]

    transport.connect(attacker)
    #expect(!coordinator.publishStatus(agents))
    #expect(transport.sent.isEmpty)

    transport.connect(trusted)
    #expect(!coordinator.publishStatus(agents))
    #expect(transport.sent.isEmpty)

    transport.connectedDeviceInfo = try #require(NextingDeviceInfo.decode(Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\",\"status/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240,\"statusSlots\":2}".utf8
    )))
    #expect(coordinator.publishStatus(agents))
    #expect(transport.sent == [.status(agents: agents)])

    transport.sent = []
    #expect(coordinator.publishStatus([]))
    #expect(transport.sent == [.status(agents: [])])

    transport.sent = []
    #expect(
        !coordinator.publishStatus([.init(slot: 9, state: .idle)])
    )
    #expect(transport.sent.isEmpty)
}

private final class StatusFakeTransport: NextingDeviceRelayTransport {
    var onMessage: ((NextingDevicePeripheralIdentity, NextingDeviceMessage) -> Void)?
    var onConnectedChange: ((Bool) -> Void)?
    var connectedIdentity: NextingDevicePeripheralIdentity?
    var connectedDeviceInfo: NextingDeviceInfo?
    var isReady = false
    var sent: [NextingDeviceMessage] = []

    func send(_ data: Data) -> Bool {
        guard let message = NextingDeviceCodec.decode(data) else { return false }
        sent.append(message)
        return true
    }

    func startScan() {}

    func disconnect() {
        isReady = false
        connectedIdentity = nil
        onConnectedChange?(false)
    }

    func connect(_ identity: NextingDevicePeripheralIdentity) {
        connectedIdentity = identity
        isReady = true
        onConnectedChange?(true)
    }
}

private struct PromptContext: Equatable, Sendable {
    let prompt: String
}
