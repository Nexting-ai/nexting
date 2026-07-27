import Foundation

public struct NextingDeviceAnswerClaimGate: Sendable {
    public private(set) var currentPromptID: String?
    private var answerInFlight = false
    private var hardwareSuppressed = false

    public init() {}

    @discardableResult
    public mutating func observePrompt(_ promptID: String) -> Bool {
        guard currentPromptID != promptID else { return false }
        currentPromptID = promptID
        answerInFlight = false
        hardwareSuppressed = false
        return true
    }

    public func mayPresentHardware(for promptID: String) -> Bool {
        currentPromptID == promptID && !hardwareSuppressed
    }

    public mutating func claimPhoneAnswer(for promptID: String) -> Bool {
        claimAnswer(for: promptID)
    }

    public mutating func claimDeviceAnswer(for promptID: String) -> Bool {
        claimAnswer(for: promptID)
    }

    public mutating func answerFailed(for promptID: String) {
        guard currentPromptID == promptID else { return }
        answerInFlight = false
    }

    public mutating func suppressHardware(for promptID: String) {
        guard currentPromptID == promptID else { return }
        hardwareSuppressed = true
    }

    public mutating func clearPrompt(_ promptID: String) {
        guard currentPromptID == promptID else { return }
        self = NextingDeviceAnswerClaimGate()
    }

    private mutating func claimAnswer(for promptID: String) -> Bool {
        guard currentPromptID == promptID, !answerInFlight else { return false }
        answerInFlight = true
        hardwareSuppressed = true
        return true
    }
}

struct NextingDeviceDiscoveryRecoveryState {
    private var quarantinedIDs: Set<UUID> = []

    func canAttempt(_ id: UUID) -> Bool {
        !quarantinedIDs.contains(id)
    }

    mutating func quarantine(_ id: UUID) {
        quarantinedIDs.insert(id)
    }

    mutating func beginExplicitScanCycle() {
        quarantinedIDs.removeAll()
    }

    mutating func connectionDidEnd(
        _ id: UUID,
        wasReady: Bool,
        shouldScan: Bool
    ) -> Bool {
        guard shouldScan else { return false }
        if !wasReady { quarantine(id) }
        return true
    }
}
