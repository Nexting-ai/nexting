import Foundation
import Testing
@testable import NextingDeviceKit

@Test("authorization defaults to deny and persists authorize revoke")
func authorizationPersistence() throws {
    let suite = "NextingDeviceAuthorizationTests.\(UUID().uuidString)"
    let defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    defaults.removePersistentDomain(forName: suite)
    let store = NextingDeviceAuthorizationStore(defaults: defaults)
    let id = UUID()

    #expect(!store.isAuthorized(id))
    store.authorize(id)
    #expect(store.isAuthorized(id))
    #expect(NextingDeviceAuthorizationStore(defaults: defaults).isAuthorized(id))
    store.revoke(id)
    #expect(!store.isAuthorized(id))
}

@Test("release policy never trusts a peripheral name")
func authorizationNamePolicy() throws {
    let suite = "NextingDeviceAuthorizationTests.\(UUID().uuidString)"
    let defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    let store = NextingDeviceAuthorizationStore(defaults: defaults)
    let simulator = NextingDevicePeripheralIdentity(id: UUID(), name: "Nexting-Device-Sim")

    #expect(
        !NextingDeviceAuthorizationPolicy(
            store: store,
            allowDebugTestDevice: false
        ).isAuthorized(simulator)
    )
    #expect(
        NextingDeviceAuthorizationPolicy(
            store: store,
            allowDebugTestDevice: true
        ).isAuthorized(simulator)
    )
    #expect(
        !NextingDeviceAuthorizationPolicy(
            store: store,
            allowDebugTestDevice: true
        ).isAuthorized(.init(id: UUID(), name: "Nearby-Device"))
    )
}

@Test("phone and device share a single claim gate")
func answerClaimGate() {
    var gate = NextingDeviceAnswerClaimGate()
    let observed = gate.observePrompt("p1")
    #expect(observed)
    #expect(gate.mayPresentHardware(for: "p1"))
    let deviceClaim = gate.claimDeviceAnswer(for: "p1")
    #expect(deviceClaim)
    let blockedPhoneClaim = gate.claimPhoneAnswer(for: "p1")
    #expect(!blockedPhoneClaim)
    gate.answerFailed(for: "p1")
    #expect(!gate.mayPresentHardware(for: "p1"))
    let phoneRetry = gate.claimPhoneAnswer(for: "p1")
    #expect(phoneRetry)
    gate.clearPrompt("p1")
    let staleClaim = gate.claimPhoneAnswer(for: "p1")
    #expect(!staleClaim)
}

@Test("discovery recovery quarantines malformed setup only for one scan cycle")
func discoveryRecovery() {
    let malformed = UUID()
    let ready = UUID()
    var state = NextingDeviceDiscoveryRecoveryState()

    let resumesAfterMalformed = state.connectionDidEnd(
        malformed,
        wasReady: false,
        shouldScan: true
    )
    #expect(resumesAfterMalformed)
    #expect(!state.canAttempt(malformed))
    let resumesAfterReady = state.connectionDidEnd(
        ready,
        wasReady: true,
        shouldScan: true
    )
    #expect(resumesAfterReady)
    #expect(state.canAttempt(ready))
    state.beginExplicitScanCycle()
    #expect(state.canAttempt(malformed))
}

@Test("device info read errors and malformed values require setup recovery")
func deviceInfoReadTransition() throws {
    let valid = Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":512,\"max_summary_bytes\":64}".utf8
    )

    #expect(
        NextingDeviceSetupTransition.deviceInfoRead(
            data: valid,
            hadError: true
        ) == .recover
    )
    #expect(
        NextingDeviceSetupTransition.deviceInfoRead(
            data: Data("not-json".utf8),
            hadError: false
        ) == .recover
    )
    let accepted = NextingDeviceSetupTransition.deviceInfoRead(
        data: valid,
        hadError: false
    )
    guard case let .accept(info) = accepted else {
        Issue.record("valid Device Info should be accepted")
        return
    }
    #expect(info.maxMessageBytes == 512)
    #expect(info.maxSummaryBytes == 64)
}

@Test("discovery disconnect resumes only when scanning is still requested")
func discoveryDisconnectTransition() {
    let id = UUID()
    var state = NextingDeviceDiscoveryRecoveryState()
    let stopped = state.connectionDidEnd(id, wasReady: false, shouldScan: false)
    #expect(!stopped)
    let resumed = state.connectionDidEnd(id, wasReady: false, shouldScan: true)
    #expect(resumed)
    #expect(!state.canAttempt(id))
}

@Test("transport policy enforces negotiated message and UTF-8 summary limits")
func transportPolicyLimits() throws {
    let info = try #require(NextingDeviceInfo.decode(Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":512,\"max_summary_bytes\":64}".utf8
    )))
    let summary = NextingDeviceTransportPolicy.boundedSummary(
        String(repeating: "你", count: 30),
        deviceInfo: info
    )

    #expect(summary.utf8.count == 63)
    #expect(NextingDeviceTransportPolicy.accepts(frameByteCount: 512, deviceInfo: info))
    #expect(!NextingDeviceTransportPolicy.accepts(frameByteCount: 513, deviceInfo: info))
}

@Test("write queue is byte bounded and advances only after acknowledgements")
func boundedWriteQueue() {
    var queue = NextingDeviceWriteQueue(maxPendingBytes: 8)
    let firstEnqueue = queue.enqueue(frame: Data([1, 2, 3, 4]))
    #expect(firstEnqueue)
    let secondEnqueue = queue.enqueue(frame: Data([5, 6, 7, 8]))
    #expect(secondEnqueue)
    let overflowEnqueue = queue.enqueue(frame: Data([9]))
    #expect(!overflowEnqueue)
    #expect(queue.pendingByteCount == 8)

    let firstChunk = queue.nextChunk(maxBytes: 3)
    #expect(firstChunk == Data([1, 2, 3]))
    let blockedChunk = queue.nextChunk(maxBytes: 3)
    #expect(blockedChunk == nil)
    let firstAcknowledgement = queue.acknowledgeInFlightChunk()
    #expect(firstAcknowledgement)
    #expect(queue.pendingByteCount == 5)
    let firstFrameRemainder = queue.nextChunk(maxBytes: 3)
    #expect(firstFrameRemainder == Data([4]))
    let firstFrameComplete = queue.acknowledgeInFlightChunk()
    #expect(firstFrameComplete)
    let secondFrameStart = queue.nextChunk(maxBytes: 3)
    #expect(secondFrameStart == Data([5, 6, 7]))
    let secondFrameStartAcknowledged = queue.acknowledgeInFlightChunk()
    #expect(secondFrameStartAcknowledged)
    let secondFrameRemainder = queue.nextChunk(maxBytes: 3)
    #expect(secondFrameRemainder == Data([8]))
    let secondFrameComplete = queue.acknowledgeInFlightChunk()
    #expect(secondFrameComplete)
    #expect(queue.pendingByteCount == 0)
    let emptyChunk = queue.nextChunk(maxBytes: 3)
    #expect(emptyChunk == nil)
    let staleAcknowledgement = queue.acknowledgeInFlightChunk()
    #expect(!staleAcknowledgement)
}

@Test("write queue also bounds retained frame objects")
func writeQueueBoundsFrameCount() {
    var queue = NextingDeviceWriteQueue(
        maxPendingBytes: 128,
        maxPendingFrames: 2
    )
    let first = queue.enqueue(frame: Data([1]))
    let second = queue.enqueue(frame: Data([2]))
    let overflow = queue.enqueue(frame: Data([3]))
    #expect(first)
    #expect(second)
    #expect(!overflow)
    #expect(queue.pendingByteCount == 2)
}

@Test("device info requires compatible wire profile and safe bounds")
func deviceInfoValidation() {
    let valid = Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240}".utf8
    )
    let info = NextingDeviceInfo.decode(valid)
    #expect(info?.model == "xiao-ref")
    #expect(info?.supportsApprovalV1 == true)
    #expect(info?.maxMessageBytes == 4096)

    let tooSmall = Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":128,\"max_summary_bytes\":240}".utf8
    )
    #expect(NextingDeviceInfo.decode(tooSmall) == nil)

    let highCapacity = Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":1000000,\"max_summary_bytes\":240}".utf8
    )
    #expect(NextingDeviceInfo.decode(highCapacity)?.maxMessageBytes == 1_000_000)

    for controlValue in [
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao\\u0000ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240}",
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\\nunsafe\",\"max_message_bytes\":4096,\"max_summary_bytes\":240}",
    ] {
        #expect(NextingDeviceInfo.decode(Data(controlValue.utf8)) == nil)
    }

    for incompatible in [
        "{\"protocol\":\"other\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240}",
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[2],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240}",
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"future/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240}",
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":0}",
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":241}",
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":4096}",
    ] {
        #expect(NextingDeviceInfo.decode(Data(incompatible.utf8)) == nil)
    }
}

private final class FakeTransport: NextingDeviceRelayTransport {
    var onMessage: ((NextingDevicePeripheralIdentity, NextingDeviceMessage) -> Void)?
    var onConnectedChange: ((Bool) -> Void)?
    var connectedIdentity: NextingDevicePeripheralIdentity?
    var connectedDeviceInfo: NextingDeviceInfo?
    var isReady = false
    var sent: [NextingDeviceMessage] = []
    var scanCount = 0
    var disconnectCount = 0
    var acceptsSend = true

    func send(_ data: Data) -> Bool {
        guard acceptsSend else { return false }
        if let message = NextingDeviceCodec.decode(data) { sent.append(message) }
        return true
    }

    func startScan() { scanCount += 1 }

    func disconnect() {
        disconnectCount += 1
        isReady = false
        connectedIdentity = nil
        onConnectedChange?(false)
    }

    func connect(_ identity: NextingDevicePeripheralIdentity) {
        connectedIdentity = identity
        isReady = true
        onConnectedChange?(true)
    }

    func loseConnection() {
        isReady = false
        connectedIdentity = nil
        onConnectedChange?(false)
    }

    func receive(
        _ message: NextingDeviceMessage,
        from identity: NextingDevicePeripheralIdentity
    ) {
        onMessage?(identity, message)
    }
}

@Test("coordinator reports a rejected Present as not presented")
func coordinatorRollsBackRejectedPresent() {
    let transport = FakeTransport()
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { "opaque-1" },
        answerPrompt: { _, _ in }
    )
    transport.connect(trusted)
    transport.acceptsSend = false

    #expect(
        coordinator.present(
            context: .init(prompt: "private"),
            summary: "Allow?",
            ttlMs: 30_000
        ) == nil
    )
    #expect(transport.sent.isEmpty)
}

private struct PromptContext: Equatable, Sendable {
    let prompt: String
}

@Test("coordinator never sends summary to unauthorized transport")
func coordinatorAuthorization() {
    let transport = FakeTransport()
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    let attacker = NextingDevicePeripheralIdentity(id: UUID(), name: "Attacker")
    var answers: [(PromptContext, NextingDeviceChoice)] = []
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { "opaque-1" },
        answerPrompt: { answers.append(($0, $1)) }
    )

    transport.connect(attacker)
    coordinator.present(context: .init(prompt: "private"), summary: "secret", ttlMs: 30_000)
    transport.receive(.answer(requestId: "opaque-1", choice: .allow), from: attacker)
    #expect(transport.sent.isEmpty)
    #expect(answers.isEmpty)
}

@Test("coordinator truncates Present summary to the connected device limit")
func coordinatorUsesNegotiatedSummaryLimit() throws {
    let transport = FakeTransport()
    let info = try #require(NextingDeviceInfo.decode(Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":512,\"max_summary_bytes\":64}".utf8
    )))
    transport.connectedDeviceInfo = info
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { "opaque-1" },
        answerPrompt: { _, _ in }
    )
    transport.connect(trusted)

    coordinator.present(
        context: .init(prompt: "private"),
        summary: String(repeating: "你", count: 30),
        ttlMs: 30_000
    )

    guard case let .present(_, summary, _)? = transport.sent.last else {
        Issue.record("expected a bounded Present")
        return
    }
    #expect(summary.utf8.count == 63)
}

@Test("coordinator fits Present within the negotiated logical message size")
func coordinatorFitsPresentWithinLogicalLimit() throws {
    let transport = FakeTransport()
    let info = try #require(NextingDeviceInfo.decode(Data(
        "{\"protocol\":\"nexting-device\",\"spec\":\"0.1.0-experimental\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"xiao-ref\",\"fw\":\"0.1.0\",\"max_message_bytes\":512,\"max_summary_bytes\":240}".utf8
    )))
    transport.connectedDeviceInfo = info
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { String(repeating: "r", count: 64) },
        answerPrompt: { _, _ in }
    )
    transport.connect(trusted)

    let requestID = coordinator.present(
        context: .init(prompt: "private"),
        summary: String(repeating: "\"", count: 240),
        ttlMs: 30_000
    )

    #expect(requestID != nil)
    guard case let .present(_, summary, _)? = transport.sent.last,
          let wire = NextingDeviceCodec.encode(transport.sent.last!) else {
        Issue.record("expected a fitted Present")
        return
    }
    #expect(summary.utf8.count < 240)
    #expect(wire.count <= 512)
}

@Test("coordinator resolves an opaque answer only after API success")
func coordinatorAnswerUsesTwoPhaseCommit() {
    let transport = FakeTransport()
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    var answers: [(PromptContext, NextingDeviceChoice)] = []
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { "opaque-1" },
        answerPrompt: { answers.append(($0, $1)) }
    )
    transport.connect(trusted)

    coordinator.present(
        context: .init(prompt: "internal-prompt-id"),
        summary: "Allow?",
        ttlMs: 30_000
    )
    transport.receive(.answer(requestId: "opaque-1", choice: .allow), from: trusted)

    #expect(answers.map(\.0) == [.init(prompt: "internal-prompt-id")])
    #expect(answers.map(\.1) == [.allow])
    #expect(transport.sent.last == .present(requestId: "opaque-1", summary: "Allow?", ttlMs: 30_000))

    coordinator.answerFailed(requestId: "opaque-1")
    transport.receive(.answer(requestId: "opaque-1", choice: .allow), from: trusted)
    #expect(answers.map(\.1) == [.allow, .allow])
    coordinator.answerSucceeded(requestId: "stale")
    #expect(transport.sent.last == .present(requestId: "opaque-1", summary: "Allow?", ttlMs: 30_000))
    coordinator.answerSucceeded(requestId: "opaque-1")
    #expect(transport.sent.last == .resolved(requestId: "opaque-1", reason: .answered))
}

@Test("coordinator phone claim blocks hardware until API success")
func coordinatorPhoneAnswerUsesTwoPhaseCommit() {
    let transport = FakeTransport()
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    var deviceAnswers: [NextingDeviceChoice] = []
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { "opaque-1" },
        answerPrompt: { _, choice in deviceAnswers.append(choice) }
    )
    transport.connect(trusted)
    coordinator.present(
        context: .init(prompt: "internal-prompt-id"),
        summary: "Allow?",
        ttlMs: 30_000
    )

    #expect(coordinator.phoneAnswerStarted() == "opaque-1")
    transport.receive(.answer(requestId: "opaque-1", choice: .deny), from: trusted)
    #expect(deviceAnswers.isEmpty)
    #expect(transport.sent.count == 1)
    coordinator.answerSucceeded(requestId: "opaque-1")
    #expect(transport.sent.last == .resolved(requestId: "opaque-1", reason: .answered))
}

@Test("coordinator ignores completion without an answer claim")
func coordinatorRequiresAnswerClaimBeforeCompletion() {
    let transport = FakeTransport()
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    var answers: [NextingDeviceChoice] = []
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { "opaque-1" },
        answerPrompt: { _, choice in answers.append(choice) }
    )
    transport.connect(trusted)
    coordinator.present(
        context: .init(prompt: "internal-prompt-id"),
        summary: "Allow?",
        ttlMs: 30_000
    )

    coordinator.answerSucceeded(requestId: "opaque-1")
    transport.receive(.answer(requestId: "opaque-1", choice: .allow), from: trusted)

    #expect(answers == [.allow])
    coordinator.answerSucceeded(requestId: "opaque-1")
    #expect(transport.sent.last == .resolved(requestId: "opaque-1", reason: .answered))
}

@Test("coordinator does not re-present while Agent commit is in flight")
func coordinatorHoldsInFlightAnswerAcrossReconnect() {
    let transport = FakeTransport()
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    var answers: [NextingDeviceChoice] = []
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { 1_000 },
        requestId: { "opaque-1" },
        answerPrompt: { _, choice in answers.append(choice) }
    )
    transport.connect(trusted)
    coordinator.present(
        context: .init(prompt: "internal-prompt-id"),
        summary: "Allow?",
        ttlMs: 30_000
    )
    transport.receive(.answer(requestId: "opaque-1", choice: .allow), from: trusted)

    transport.loseConnection()
    transport.connect(trusted)
    #expect(transport.sent.filter { message in
        if case .present = message { return true }
        return false
    }.count == 1)

    coordinator.answerFailed(requestId: "opaque-1")
    #expect(transport.sent.filter { message in
        if case .present = message { return true }
        return false
    }.count == 2)
    transport.receive(.answer(requestId: "opaque-1", choice: .deny), from: trusted)
    #expect(answers == [.allow])
    transport.receive(.answer(requestId: "opaque-1", choice: .allow), from: trusted)
    #expect(answers == [.allow, .allow])
}

@Test("coordinator defers and re-presents only the remaining TTL after reconnect")
func coordinatorReconnect() {
    let transport = FakeTransport()
    let trusted = NextingDevicePeripheralIdentity(id: UUID(), name: "Trusted")
    var nowMs: Int64 = 1_000
    let coordinator = NextingDeviceRelayCoordinator<PromptContext>(
        transport: transport,
        isAuthorized: { $0.id == trusted.id },
        nowMs: { nowMs },
        requestId: { "opaque-1" },
        answerPrompt: { _, _ in }
    )

    coordinator.present(context: .init(prompt: "p1"), summary: "Allow?", ttlMs: 30_000)
    nowMs = 6_000
    transport.connect(trusted)
    #expect(transport.sent.last == .present(requestId: "opaque-1", summary: "Allow?", ttlMs: 25_000))

    transport.loseConnection()
    nowMs = 7_000
    transport.connect(trusted)
    #expect(transport.sent.last == .present(requestId: "opaque-1", summary: "Allow?", ttlMs: 24_000))
}

@Test("physical choices always map to explicit one-time allow and deny")
func optionMapping() {
    #expect(NextingDeviceOptionMapping.index(for: .allow) == 0)
    #expect(NextingDeviceOptionMapping.index(for: .deny) == 1)
}
