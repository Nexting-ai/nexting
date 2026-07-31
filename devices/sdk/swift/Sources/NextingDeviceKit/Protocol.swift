import CoreFoundation
import Foundation

public enum NextingDeviceChoice: String, Codable, Equatable, Sendable {
    case allow
    case deny
}

public enum NextingDeviceResolutionReason: String, Codable, Equatable, Sendable {
    case answered
    case expired
    case cancelled
    case replaced
}

public enum NextingDeviceErrorCode: String, Codable, Equatable, Sendable {
    case badMessage = "bad_message"
    case messageTooLarge = "message_too_large"
    case unsupportedVersion = "unsupported_version"
    case unsupportedProfile = "unsupported_profile"
    case unknownRequest = "unknown_request"
    case notAuthorized = "not_authorized"
    case busy
}

public enum NextingDeviceAgentState: String, Codable, Equatable, Sendable {
    case idle
    case thinking
    case working
    case complete
    case needsInput = "needs_input"
    case error
}

public struct NextingDeviceAgentStatus: Equatable, Sendable {
    public let slot: Int
    public let state: NextingDeviceAgentState
    public let label: String?

    public init(slot: Int, state: NextingDeviceAgentState, label: String? = nil) {
        self.slot = slot
        self.state = state
        self.label = label
    }
}

public enum NextingDeviceMessage: Equatable, Sendable {
    case present(requestId: String, summary: String, ttlMs: Int)
    case answer(requestId: String, choice: NextingDeviceChoice)
    case resolved(requestId: String, reason: NextingDeviceResolutionReason)
    case error(requestId: String?, code: NextingDeviceErrorCode)
    case status(agents: [NextingDeviceAgentStatus])
}

public enum NextingDeviceCodec {
    public static let wireVersion = 1
    public static let profile = "approval/1"
    public static let statusProfile = "status/1"
    public static let maxRequestIDBytes = 64
    public static let maxSummaryBytes = 240
    public static let maxTTLMilliseconds = 300_000
    public static let maxMessageBytes = 4_096
    public static let maxStatusAgents = 8
    public static let maxStatusLabelBytes = 64

    private static let knownWireFields: Set<String> = [
        "v", "t", "id", "sum", "opt", "ttl", "ch", "r", "code", "agents",
    ]

    public static func encode(_ message: NextingDeviceMessage) -> Data? {
        let text: String
        switch message {
        case let .present(requestId, summary, ttlMs):
            guard validID(requestId), validSummary(summary), validTTL(ttlMs) else {
                return nil
            }
            text = "{\"v\":1,\"t\":\"present\",\"id\":\(jsonString(requestId)),\"sum\":\(jsonString(summary)),\"opt\":[\"allow\",\"deny\"],\"ttl\":\(ttlMs)}\n"
        case let .answer(requestId, choice):
            guard validID(requestId) else { return nil }
            text = "{\"v\":1,\"t\":\"answer\",\"id\":\(jsonString(requestId)),\"ch\":\(jsonString(choice.rawValue))}\n"
        case let .resolved(requestId, reason):
            guard validID(requestId) else { return nil }
            text = "{\"v\":1,\"t\":\"resolved\",\"id\":\(jsonString(requestId)),\"r\":\(jsonString(reason.rawValue))}\n"
        case let .error(requestId, code):
            if let requestId {
                guard validID(requestId) else { return nil }
                text = "{\"v\":1,\"t\":\"error\",\"id\":\(jsonString(requestId)),\"code\":\(jsonString(code.rawValue))}\n"
            } else {
                text = "{\"v\":1,\"t\":\"error\",\"code\":\(jsonString(code.rawValue))}\n"
            }
        case let .status(agents):
            guard validStatusAgents(agents) else { return nil }
            let entries = agents.map { agent -> String in
                var entry = "{\"slot\":\(agent.slot),\"state\":\(jsonString(agent.state.rawValue))"
                if let label = agent.label {
                    entry += ",\"label\":\(jsonString(label))"
                }
                return entry + "}"
            }.joined(separator: ",")
            text = "{\"v\":1,\"t\":\"status\",\"agents\":[\(entries)]}\n"
        }
        return Data(text.utf8)
    }

    public static func decode(_ data: Data) -> NextingDeviceMessage? {
        guard data.count <= maxMessageBytes,
              data.count < maxMessageBytes || data.last == 0x0A,
              var text = String(data: data, encoding: .utf8) else { return nil }
        if text.last == "\n" { text.removeLast() }
        guard !text.isEmpty, !text.contains("\n"), !text.contains("\r"),
              rawObjectIsSafe(text),
              let object = try? JSONSerialization.jsonObject(with: Data(text.utf8)),
              let message = object as? [String: Any],
              integer(message["v"]) == wireVersion,
              let type = message["t"] as? String else { return nil }

        switch type {
        case "present":
            guard let requestID = message["id"] as? String, validID(requestID),
                  let summary = message["sum"] as? String, validSummary(summary),
                  let options = message["opt"] as? [String], options == ["allow", "deny"],
                  let ttlMs = integer(message["ttl"]), validTTL(ttlMs) else { return nil }
            return .present(requestId: requestID, summary: summary, ttlMs: ttlMs)
        case "answer":
            guard let requestID = message["id"] as? String, validID(requestID),
                  let rawChoice = message["ch"] as? String,
                  let choice = NextingDeviceChoice(rawValue: rawChoice) else { return nil }
            return .answer(requestId: requestID, choice: choice)
        case "resolved":
            guard let requestID = message["id"] as? String, validID(requestID),
                  let rawReason = message["r"] as? String,
                  let reason = NextingDeviceResolutionReason(rawValue: rawReason) else { return nil }
            return .resolved(requestId: requestID, reason: reason)
        case "error":
            let requestID: String?
            if message.keys.contains("id") {
                guard let value = message["id"] as? String, validID(value) else { return nil }
                requestID = value
            } else {
                requestID = nil
            }
            guard let rawCode = message["code"] as? String,
                  let code = NextingDeviceErrorCode(rawValue: rawCode) else { return nil }
            return .error(requestId: requestID, code: code)
        case "status":
            guard let rawAgents = message["agents"] as? [[String: Any]] else { return nil }
            var agents: [NextingDeviceAgentStatus] = []
            agents.reserveCapacity(rawAgents.count)
            for rawAgent in rawAgents {
                guard let slot = integer(rawAgent["slot"]),
                      let rawState = rawAgent["state"] as? String,
                      let state = NextingDeviceAgentState(rawValue: rawState) else {
                    return nil
                }
                var label: String?
                if rawAgent.keys.contains("label") {
                    guard let value = rawAgent["label"] as? String else { return nil }
                    label = value
                }
                agents.append(.init(slot: slot, state: state, label: label))
            }
            guard validStatusAgents(agents) else { return nil }
            return .status(agents: agents)
        default:
            return nil
        }
    }

    private static let allowedIDCharacters = CharacterSet(
        charactersIn: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._:-"
    )

    private static func validID(_ value: String) -> Bool {
        !value.isEmpty
            && value.utf8.count <= maxRequestIDBytes
            && value.unicodeScalars.allSatisfy { allowedIDCharacters.contains($0) }
    }

    private static func validSummary(_ value: String) -> Bool {
        value.utf8.count <= maxSummaryBytes
            && value.unicodeScalars.allSatisfy { $0.value != 0 }
    }

    private static func validTTL(_ value: Int) -> Bool {
        (1 ... maxTTLMilliseconds).contains(value)
    }

    private static func validStatusLabel(_ value: String) -> Bool {
        !value.isEmpty
            && value.utf8.count <= maxStatusLabelBytes
            && value.unicodeScalars.allSatisfy {
                $0.value > 0x1F && $0.value != 0x7F
            }
    }

    private static func validStatusAgents(_ agents: [NextingDeviceAgentStatus]) -> Bool {
        guard agents.count <= maxStatusAgents else { return false }
        var seenSlots = Set<Int>()
        for agent in agents {
            guard (0 ... maxStatusAgents - 1).contains(agent.slot),
                  seenSlots.insert(agent.slot).inserted else { return false }
            if let label = agent.label, !validStatusLabel(label) { return false }
        }
        return true
    }

    private static func integer(_ value: Any?) -> Int? {
        guard let number = value as? NSNumber,
              CFGetTypeID(number) != CFBooleanGetTypeID(),
              number.doubleValue.rounded(.towardZero) == number.doubleValue,
              number.doubleValue >= Double(Int.min),
              number.doubleValue <= Double(Int.max) else { return nil }
        return number.intValue
    }

    private static func rawObjectIsSafe(_ text: String) -> Bool {
        let bytes = Array(text.utf8)
        var objectDepth = 0
        var arrayDepth = 0
        var expectingTopLevelKey = false
        var rootStarted = false
        var seenKnownFields = Set<String>()
        var index = 0

        while index < bytes.count {
            switch bytes[index] {
            case 0x22:
                guard let end = scanJSONString(bytes, from: index) else { return false }
                if objectDepth == 1, arrayDepth == 0, expectingTopLevelKey {
                    guard let key = try? JSONDecoder().decode(
                        String.self,
                        from: Data(bytes[index ..< end])
                    ) else { return false }
                    if knownWireFields.contains(key), !seenKnownFields.insert(key).inserted {
                        return false
                    }
                    if (key == "v" || key == "ttl"),
                       !hasCanonicalUnsignedIntegerValue(bytes, afterKey: end) {
                        return false
                    }
                    expectingTopLevelKey = false
                }
                index = end
                continue
            case 0x7B:
                objectDepth += 1
                if !rootStarted {
                    rootStarted = true
                    expectingTopLevelKey = true
                }
            case 0x7D:
                objectDepth -= 1
            case 0x5B:
                arrayDepth += 1
            case 0x5D:
                arrayDepth -= 1
            case 0x2C where objectDepth == 1 && arrayDepth == 0:
                expectingTopLevelKey = true
            default:
                break
            }
            if objectDepth + arrayDepth > 9 { return false }
            index += 1
        }

        return rootStarted && objectDepth == 0 && arrayDepth == 0
    }

    private static func hasCanonicalUnsignedIntegerValue(
        _ bytes: [UInt8],
        afterKey start: Int
    ) -> Bool {
        var index = start
        while index < bytes.count, isJSONWhitespace(bytes[index]) { index += 1 }
        guard index < bytes.count, bytes[index] == 0x3A else { return false }
        index += 1
        while index < bytes.count, isJSONWhitespace(bytes[index]) { index += 1 }
        guard index < bytes.count, (0x30 ... 0x39).contains(bytes[index]) else {
            return false
        }
        if bytes[index] == 0x30 {
            index += 1
        } else {
            while index < bytes.count, (0x30 ... 0x39).contains(bytes[index]) {
                index += 1
            }
        }
        while index < bytes.count, isJSONWhitespace(bytes[index]) { index += 1 }
        return index < bytes.count && (bytes[index] == 0x2C || bytes[index] == 0x7D)
    }

    private static func isJSONWhitespace(_ byte: UInt8) -> Bool {
        byte == 0x20 || byte == 0x09 || byte == 0x0A || byte == 0x0D
    }

    private static func scanJSONString(_ bytes: [UInt8], from start: Int) -> Int? {
        var index = start + 1
        while index < bytes.count {
            switch bytes[index] {
            case 0x22:
                return index + 1
            case 0x00:
                return nil
            case 0x5C:
                guard index + 1 < bytes.count else { return nil }
                if bytes[index + 1] == 0x75 {
                    guard index + 5 < bytes.count else { return nil }
                    if bytes[(index + 2) ... (index + 5)].allSatisfy({ $0 == 0x30 }) {
                        return nil
                    }
                    index += 6
                } else {
                    index += 2
                }
                continue
            default:
                index += 1
            }
        }
        return nil
    }

    private static func jsonString(_ value: String) -> String {
        let data = try! JSONSerialization.data(withJSONObject: [value])
        let array = String(decoding: data, as: UTF8.self)
        return String(array.dropFirst().dropLast())
    }
}
