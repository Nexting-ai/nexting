import Foundation

public protocol NextingDeviceRelayTransport: AnyObject {
    var onMessage: ((NextingDevicePeripheralIdentity, NextingDeviceMessage) -> Void)? { get set }
    var onConnectedChange: ((Bool) -> Void)? { get set }
    var connectedIdentity: NextingDevicePeripheralIdentity? { get }
    var connectedDeviceInfo: NextingDeviceInfo? { get }
    var isReady: Bool { get }

    @discardableResult
    func send(_ data: Data) -> Bool
    func startScan()
    func disconnect()
}

public enum NextingDeviceOptionMapping {
    public static func index(for choice: NextingDeviceChoice) -> Int {
        choice == .allow ? 0 : 1
    }
}

public final class NextingDeviceRelayCoordinator<Context> {
    private struct Pending {
        let requestId: String
        let context: Context
        let summary: String
        let deadlineMs: Int64
        var presented: Bool
        var answerInFlight: Bool
        var lockedDeviceChoice: NextingDeviceChoice?
    }

    private let transport: NextingDeviceRelayTransport
    private let isAuthorized: (NextingDevicePeripheralIdentity) -> Bool
    private let nowMs: () -> Int64
    private let requestId: () -> String
    private let answerPrompt: (Context, NextingDeviceChoice) -> Void
    private var pending: Pending?
    private var relay: NextingDevicePromptRelay!

    public init(
        transport: NextingDeviceRelayTransport,
        isAuthorized: @escaping (NextingDevicePeripheralIdentity) -> Bool,
        nowMs: @escaping () -> Int64 = {
            Int64((ProcessInfo.processInfo.systemUptime * 1_000).rounded(.down))
        },
        requestId: @escaping () -> String = { UUID().uuidString },
        answerPrompt: @escaping (Context, NextingDeviceChoice) -> Void
    ) {
        self.transport = transport
        self.isAuthorized = isAuthorized
        self.nowMs = nowMs
        self.requestId = requestId
        self.answerPrompt = answerPrompt

        relay = NextingDevicePromptRelay(
            sendToDevice: { [weak self] data in
                self?.sendIfAuthorized(data) ?? false
            },
            answerPrompt: { [weak self] choice in self?.routeDeviceAnswer(choice) },
            nowMs: nowMs,
            onResolve: { [weak self] id, reason in
                guard reason != .answered else { return }
                self?.clearPending(requestId: id)
            }
        )
        transport.onMessage = { [weak self] identity, message in
            self?.receive(message, from: identity)
        }
        transport.onConnectedChange = { [weak self] ready in
            self?.connectionChanged(ready: ready)
        }
    }

    public func start() {
        transport.startScan()
        presentIfPossible()
    }

    public func resume() {
        start()
    }

    public func restartDiscovery() {
        start()
    }

    public func stop() {
        cancel()
        transport.disconnect()
    }

    @discardableResult
    public func present(context: Context, summary: String, ttlMs: Int) -> String? {
        cancel()
        let id = requestId()
        guard NextingDeviceCodec.encode(
            .present(requestId: id, summary: summary, ttlMs: ttlMs)
        ) != nil else { return nil }
        let (deadline, overflow) = nowMs().addingReportingOverflow(Int64(ttlMs))
        guard !overflow else { return nil }

        pending = Pending(
            requestId: id,
            context: context,
            summary: summary,
            deadlineMs: deadline,
            presented: false,
            answerInFlight: false,
            lockedDeviceChoice: nil
        )
        presentIfPossible()
        return pending?.requestId == id ? id : nil
    }

    @discardableResult
    public func phoneAnswerStarted() -> String? {
        guard var current = pending, !current.answerInFlight else { return nil }
        if current.presented,
           !relay.phoneAnswerStarted(requestId: current.requestId) { return nil }
        current.answerInFlight = true
        pending = current
        return current.requestId
    }

    public func answerSucceeded(requestId: String) {
        guard let current = pending,
              current.requestId == requestId,
              current.answerInFlight else { return }
        if current.presented { relay.answerSucceeded(requestId: requestId) }
        pending = nil
    }

    public func answerFailed(requestId: String) {
        guard var current = pending,
              current.requestId == requestId,
              current.answerInFlight else { return }
        current.answerInFlight = false
        pending = current
        if current.presented { relay.answerFailed(requestId: requestId) }
        else if nowMs() >= current.deadlineMs { pending = nil }
        else { presentIfPossible() }
    }

    public func cancel() {
        guard let current = pending else { return }
        if current.presented { relay.cancel(requestId: current.requestId) }
        pending = nil
    }

    /// Sends a full-replacement status frame. Returns false without sending
    /// when the connected device did not declare `statusSlots`, the frame is
    /// invalid, or the link is not authorized and ready.
    @discardableResult
    public func publishStatus(_ agents: [NextingDeviceAgentStatus]) -> Bool {
        guard let info = transport.connectedDeviceInfo, info.supportsStatusV1,
              let wire = NextingDeviceCodec.encode(.status(agents: agents)) else {
            return false
        }
        return sendIfAuthorized(wire)
    }

    public func tick() {
        guard let current = pending else { return }
        if current.presented {
            relay.tick()
        } else if !current.answerInFlight && nowMs() >= current.deadlineMs {
            pending = nil
        }
    }

    private func connectionChanged(ready: Bool) {
        if ready {
            presentIfPossible()
        } else if var current = pending, current.presented {
            relay.disconnect()
            current.presented = false
            pending = current
        }
    }

    private func presentIfPossible() {
        guard var current = pending, !current.presented, !current.answerInFlight,
              let identity = transport.connectedIdentity,
              transport.isReady, isAuthorized(identity) else { return }
        let remaining = current.deadlineMs - nowMs()
        guard remaining > 0,
              remaining <= Int64(NextingDeviceCodec.maxTTLMilliseconds),
              let summary = NextingDeviceTransportPolicy.boundedPresentSummary(
                requestId: current.requestId,
                summary: current.summary,
                ttlMs: Int(remaining),
                deviceInfo: transport.connectedDeviceInfo
              ),
              let wire = NextingDeviceCodec.encode(.present(
                requestId: current.requestId,
                summary: summary,
                ttlMs: Int(remaining)
              )),
              NextingDeviceTransportPolicy.accepts(
                frameByteCount: wire.count,
                deviceInfo: transport.connectedDeviceInfo
              ) else {
            pending = nil
            return
        }

        guard relay.present(
            requestId: current.requestId,
            summary: summary,
            ttlMs: Int(remaining)
        ) else {
            pending = nil
            return
        }
        current.presented = true
        pending = current
    }

    private func receive(
        _ message: NextingDeviceMessage,
        from identity: NextingDevicePeripheralIdentity
    ) {
        guard isAuthorized(identity), identity == transport.connectedIdentity,
              case let .answer(id, choice) = message,
              let current = pending,
              current.requestId == id,
              !current.answerInFlight else { return }
        if let lockedChoice = current.lockedDeviceChoice,
           lockedChoice != choice { return }
        _ = relay.onDeviceAnswer(
            requestId: id,
            choice: choice,
            deviceAuthorized: true
        )
    }

    private func routeDeviceAnswer(_ choice: NextingDeviceChoice) {
        guard var current = pending, !current.answerInFlight else { return }
        if let lockedChoice = current.lockedDeviceChoice,
           lockedChoice != choice { return }
        current.lockedDeviceChoice = choice
        current.answerInFlight = true
        pending = current
        answerPrompt(current.context, choice)
    }

    private func clearPending(requestId: String) {
        guard pending?.requestId == requestId else { return }
        pending = nil
    }

    private func sendIfAuthorized(_ data: Data) -> Bool {
        guard transport.isReady,
              let identity = transport.connectedIdentity,
              isAuthorized(identity) else { return false }
        return transport.send(data)
    }
}
