import Foundation

public struct NextingDeviceAnswerResult: Equatable, Sendable {
    public let accepted: Bool
    public let reason: String?
    public let choice: NextingDeviceChoice?

    public init(
        accepted: Bool,
        reason: String?,
        choice: NextingDeviceChoice?
    ) {
        self.accepted = accepted
        self.reason = reason
        self.choice = choice
    }
}

public final class NextingDevicePromptRelay {
    private struct Pending {
        let requestId: String
        let deadlineMs: Int64
        var lockedDeviceChoice: NextingDeviceChoice?
        var answerInFlight: Bool
    }

    private let sendToDevice: (Data) -> Bool
    private let answerPrompt: (NextingDeviceChoice) -> Void
    private let nowMs: () -> Int64
    private let onResolve: ((String, NextingDeviceResolutionReason) -> Void)?
    private var pending: Pending?

    public init(
        sendToDevice: @escaping (Data) -> Bool,
        answerPrompt: @escaping (NextingDeviceChoice) -> Void,
        nowMs: @escaping () -> Int64 = {
            Int64((ProcessInfo.processInfo.systemUptime * 1_000).rounded(.down))
        },
        onResolve: ((String, NextingDeviceResolutionReason) -> Void)? = nil
    ) {
        self.sendToDevice = sendToDevice
        self.answerPrompt = answerPrompt
        self.nowMs = nowMs
        self.onResolve = onResolve
    }

    public var hasPending: Bool { pending != nil }
    public var pendingRequestId: String? { pending?.requestId }

    @discardableResult
    public func present(requestId: String, summary: String, ttlMs: Int) -> Bool {
        let message = NextingDeviceMessage.present(
            requestId: requestId,
            summary: summary,
            ttlMs: ttlMs
        )
        guard let wire = NextingDeviceCodec.encode(message) else { return false }
        let (deadline, overflow) = nowMs().addingReportingOverflow(Int64(ttlMs))
        guard !overflow else { return false }

        if pending != nil { finish(.replaced) }
        guard sendToDevice(wire) else { return false }
        pending = Pending(
            requestId: requestId,
            deadlineMs: deadline,
            lockedDeviceChoice: nil,
            answerInFlight: false
        )
        return true
    }

    public func onDeviceAnswer(
        requestId: String,
        choice: NextingDeviceChoice,
        deviceAuthorized: Bool
    ) -> NextingDeviceAnswerResult {
        guard deviceAuthorized else {
            return .init(accepted: false, reason: "unauthorized", choice: nil)
        }
        guard var pending else {
            return .init(accepted: false, reason: "no_pending", choice: nil)
        }
        guard requestId == pending.requestId else {
            return .init(accepted: false, reason: "stale_or_unknown", choice: nil)
        }
        guard nowMs() < pending.deadlineMs else {
            finish(.expired)
            return .init(accepted: false, reason: "expired", choice: nil)
        }
        guard !pending.answerInFlight else {
            return .init(accepted: false, reason: "answer_in_flight", choice: nil)
        }
        if let locked = pending.lockedDeviceChoice, locked != choice {
            return .init(accepted: false, reason: "choice_locked", choice: nil)
        }

        if pending.lockedDeviceChoice == nil { pending.lockedDeviceChoice = choice }
        pending.answerInFlight = true
        self.pending = pending
        answerPrompt(choice)
        return .init(accepted: true, reason: nil, choice: choice)
    }

    @discardableResult
    public func phoneAnswerStarted(requestId: String) -> Bool {
        guard var pending, pending.requestId == requestId else { return false }
        guard nowMs() < pending.deadlineMs else {
            finish(.expired)
            return false
        }
        guard !pending.answerInFlight else { return false }
        pending.answerInFlight = true
        self.pending = pending
        return true
    }

    public func answerSucceeded(requestId: String) {
        guard pending?.requestId == requestId,
              pending?.answerInFlight == true else { return }
        finish(.answered)
    }

    public func answerFailed(requestId: String) {
        guard var pending, pending.requestId == requestId,
              pending.answerInFlight else { return }
        pending.answerInFlight = false
        self.pending = pending
        if nowMs() >= pending.deadlineMs { finish(.expired) }
    }

    public func cancel(requestId: String? = nil) {
        guard let pending else { return }
        if requestId == nil || requestId == pending.requestId { finish(.cancelled) }
    }

    public func tick() {
        if let pending, !pending.answerInFlight,
           nowMs() >= pending.deadlineMs { finish(.expired) }
    }

    public func disconnect() {
        pending = nil
    }

    private func finish(_ reason: NextingDeviceResolutionReason) {
        guard let pending else { return }
        self.pending = nil
        if let wire = NextingDeviceCodec.encode(
            .resolved(requestId: pending.requestId, reason: reason)
        ) {
            _ = sendToDevice(wire)
        }
        onResolve?(pending.requestId, reason)
    }
}
