import Foundation
import Testing
@testable import NextingDeviceKit

private final class RelayHarness {
    var nowMs: Int64 = 1_000
    var sent: [NextingDeviceMessage] = []
    var answers: [NextingDeviceChoice] = []
    lazy var relay = NextingDevicePromptRelay(
        sendToDevice: { [weak self] data in
            if let message = NextingDeviceCodec.decode(data) {
                self?.sent.append(message)
            }
            return true
        },
        answerPrompt: { [weak self] choice in self?.answers.append(choice) },
        nowMs: { [weak self] in self?.nowMs ?? 0 }
    )
}

@Test("present tracks and sends one approval")
func relayPresent() {
    let harness = RelayHarness()
    #expect(
        harness.relay.present(
            requestId: "r1",
            summary: "Allow push?",
            ttlMs: 30_000
        )
    )
    #expect(harness.relay.hasPending)
    #expect(harness.relay.pendingRequestId == "r1")
    #expect(
        harness.sent == [
            .present(requestId: "r1", summary: "Allow push?", ttlMs: 30_000),
        ]
    )
}

@Test("authorized answer resolves only after commit and retries after failure")
func relayDeviceAnswerUsesTwoPhaseCommit() {
    let harness = RelayHarness()
    harness.relay.present(requestId: "r1", summary: "Allow?", ttlMs: 30_000)

    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "r1",
            choice: .allow,
            deviceAuthorized: true
        ) == .init(accepted: true, reason: nil, choice: .allow)
    )
    #expect(harness.answers == [.allow])
    #expect(harness.sent.count == 1)
    #expect(harness.relay.hasPending)

    harness.relay.answerFailed(requestId: "r1")
    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "r1",
            choice: .deny,
            deviceAuthorized: true
        ).reason == "choice_locked"
    )
    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "r1",
            choice: .allow,
            deviceAuthorized: true
        ).accepted
    )
    #expect(harness.answers == [.allow, .allow])
    #expect(harness.sent.count == 1)

    harness.relay.answerSucceeded(requestId: "stale")
    #expect(harness.relay.hasPending)
    harness.relay.answerSucceeded(requestId: "r1")
    #expect(harness.sent.last == .resolved(requestId: "r1", reason: .answered))
    #expect(!harness.relay.hasPending)
}

@Test("unauthorized and stale answers fail closed")
func relayRejectsUnauthorizedAndStale() {
    let harness = RelayHarness()
    harness.relay.present(requestId: "r1", summary: "Allow?", ttlMs: 30_000)

    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "r1",
            choice: .allow,
            deviceAuthorized: false
        ).reason == "unauthorized"
    )
    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "stale",
            choice: .allow,
            deviceAuthorized: true
        ).reason == "stale_or_unknown"
    )
    #expect(harness.answers.isEmpty)
    #expect(harness.relay.hasPending)
}

@Test("replacement resolves old request and keeps new request")
func relayReplacement() {
    let harness = RelayHarness()
    harness.relay.present(requestId: "r1", summary: "first", ttlMs: 30_000)
    harness.relay.present(requestId: "r2", summary: "second", ttlMs: 30_000)

    #expect(harness.sent[1] == .resolved(requestId: "r1", reason: .replaced))
    #expect(harness.relay.pendingRequestId == "r2")
}

@Test("phone-first race resolves hardware only after API success")
func relayPhoneFirst() {
    let harness = RelayHarness()
    harness.relay.present(requestId: "r1", summary: "Allow?", ttlMs: 30_000)
    #expect(harness.relay.phoneAnswerStarted(requestId: "r1"))

    #expect(harness.sent.count == 1)
    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "r1",
            choice: .deny,
            deviceAuthorized: true
        ).reason == "answer_in_flight"
    )
    #expect(harness.answers.isEmpty)

    harness.relay.answerFailed(requestId: "r1")
    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "r1",
            choice: .deny,
            deviceAuthorized: true
        ).accepted
    )
    harness.relay.answerSucceeded(requestId: "r1")
    #expect(harness.sent.last == .resolved(requestId: "r1", reason: .answered))
}

@Test("expiry uses the boundary millisecond and fails closed")
func relayExpiry() {
    let harness = RelayHarness()
    harness.relay.present(requestId: "r1", summary: "Allow?", ttlMs: 10)
    harness.nowMs = 1_010

    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "r1",
            choice: .allow,
            deviceAuthorized: true
        ).reason == "expired"
    )
    #expect(harness.sent.last == .resolved(requestId: "r1", reason: .expired))
    #expect(harness.answers.isEmpty)
}

@Test("an in-flight answer may succeed after its original deadline")
func relayInFlightSuccessAfterDeadline() {
    let harness = RelayHarness()
    harness.relay.present(requestId: "r1", summary: "Allow?", ttlMs: 10)
    #expect(
        harness.relay.onDeviceAnswer(
            requestId: "r1",
            choice: .allow,
            deviceAuthorized: true
        ).accepted
    )
    harness.nowMs = 1_010
    harness.relay.tick()
    #expect(harness.sent.count == 1)
    harness.relay.answerSucceeded(requestId: "r1")
    #expect(harness.sent.last == .resolved(requestId: "r1", reason: .answered))
}

@Test("a failed in-flight answer expires when its deadline has passed")
func relayInFlightFailureAfterDeadline() {
    let harness = RelayHarness()
    harness.relay.present(requestId: "r1", summary: "Allow?", ttlMs: 10)
    _ = harness.relay.onDeviceAnswer(
        requestId: "r1",
        choice: .allow,
        deviceAuthorized: true
    )
    harness.nowMs = 1_010
    harness.relay.answerFailed(requestId: "r1")
    #expect(harness.sent.last == .resolved(requestId: "r1", reason: .expired))
}

@Test("tick cancel and disconnect clean up deterministically")
func relayCleanup() {
    let expiry = RelayHarness()
    expiry.relay.present(requestId: "r1", summary: "Allow?", ttlMs: 10)
    expiry.nowMs = 1_010
    expiry.relay.tick()
    #expect(expiry.sent.last == .resolved(requestId: "r1", reason: .expired))

    let cancel = RelayHarness()
    cancel.relay.present(requestId: "r2", summary: "Allow?", ttlMs: 10)
    cancel.relay.cancel(requestId: "r2")
    #expect(cancel.sent.last == .resolved(requestId: "r2", reason: .cancelled))

    let disconnect = RelayHarness()
    disconnect.relay.present(requestId: "r3", summary: "Allow?", ttlMs: 10)
    disconnect.relay.disconnect()
    #expect(!disconnect.relay.hasPending)
    #expect(disconnect.sent.count == 1)
}

@Test("invalid replacement leaves current request untouched")
func relayInvalidPresent() {
    let harness = RelayHarness()
    harness.relay.present(requestId: "r1", summary: "valid", ttlMs: 30_000)
    #expect(
        !harness.relay.present(
            requestId: "bad id",
            summary: "invalid",
            ttlMs: 30_000
        )
    )
    #expect(harness.relay.pendingRequestId == "r1")
    #expect(harness.sent.count == 1)
}
