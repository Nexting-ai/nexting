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

public enum NextingNavigationDirection: String, Equatable, Sendable {
    case previous = "prev"
    case next
    case up
    case down
    case left
    case right
}

public enum NextingNavigationResolution: String, Equatable, Sendable {
    case selected
    case cancelled
    case expired
    case replaced
}

public enum NextingControlGesture: String, Equatable, Sendable {
    case press
    case release
    case hold
    case double
}

public enum NextingKeyLight: String, Equatable, Sendable {
    case off
    case dim
    case solid
    case pulse
}

public struct NextingRGB: Equatable, Sendable {
    public let red: Int
    public let green: Int
    public let blue: Int

    public init(red: Int, green: Int, blue: Int) {
        self.red = red
        self.green = green
        self.blue = blue
    }
}

public struct NextingKeyPresentation: Equatable, Sendable {
    public let slot: Int
    public let label: String
    public let enabled: Bool
    public let light: NextingKeyLight
    public let rgb: NextingRGB?

    public init(
        slot: Int,
        label: String,
        enabled: Bool,
        light: NextingKeyLight,
        rgb: NextingRGB? = nil
    ) {
        self.slot = slot
        self.label = label
        self.enabled = enabled
        self.light = light
        self.rgb = rgb
    }
}

public struct NextingRotaryControl: Equatable, Sendable {
    public let slot: Int
    public let label: String
    public let value: Int
    public let minimum: Int
    public let maximum: Int
    public let wrap: Bool

    public init(
        slot: Int,
        label: String,
        value: Int,
        minimum: Int,
        maximum: Int,
        wrap: Bool
    ) {
        self.slot = slot
        self.label = label
        self.value = value
        self.minimum = minimum
        self.maximum = maximum
        self.wrap = wrap
    }
}

public enum NextingVoiceEvent: String, Equatable, Sendable {
    case start
    case stop
    case cancel
}

public enum NextingVoiceState: String, Equatable, Sendable {
    case idle
    case listening
    case transcribing
    case submitted
    case error
}

public struct NextingUsageSnapshot: Equatable, Sendable {
    public let model: String
    public let inputTokens: Int
    public let outputTokens: Int
    public let cachedTokens: Int?
    public let contextUsed: Int?
    public let contextLimit: Int?

    public init(
        model: String,
        inputTokens: Int,
        outputTokens: Int,
        cachedTokens: Int? = nil,
        contextUsed: Int? = nil,
        contextLimit: Int? = nil
    ) {
        self.model = model
        self.inputTokens = inputTokens
        self.outputTokens = outputTokens
        self.cachedTokens = cachedTokens
        self.contextUsed = contextUsed
        self.contextLimit = contextLimit
    }
}

public enum NextingConfigValue: Equatable, Sendable {
    case boolean(Bool)
    case integer(Int)
    case string(String)
}

public struct NextingConfigEntry: Equatable, Sendable {
    public let key: String
    public let value: NextingConfigValue

    public init(key: String, value: NextingConfigValue) {
        self.key = key
        self.value = value
    }
}

public enum NextingConfigStatus: String, Equatable, Sendable {
    case applied
    case rejected
}

public enum NextingConfigError: String, Equatable, Sendable {
    case unknownKey = "unknown_key"
    case invalidValue = "invalid_value"
    case storageError = "storage_error"
    case unsupported
}

public enum NextingDeviceMessage: Equatable, Sendable {
    case present(requestId: String, summary: String, ttlMs: Int)
    case answer(requestId: String, choice: NextingDeviceChoice)
    case resolved(requestId: String, reason: NextingDeviceResolutionReason)
    case error(requestId: String?, code: NextingDeviceErrorCode)
    case status(agents: [NextingDeviceAgentStatus])
    case navigationPresent(requestId: String, items: [String], cursor: Int, ttlMs: Int)
    case navigationMove(requestId: String, direction: NextingNavigationDirection, sequence: UInt32)
    case navigationSelect(requestId: String, index: Int, sequence: UInt32)
    case navigationResolved(requestId: String, reason: NextingNavigationResolution)
    case keymap(revision: UInt32, keys: [NextingKeyPresentation])
    case keyEvent(slot: Int, event: NextingControlGesture, sequence: UInt32)
    case rotaryMap(revision: UInt32, controls: [NextingRotaryControl])
    case rotaryEvent(slot: Int, delta: Int, sequence: UInt32)
    case rotaryPress(slot: Int, event: NextingControlGesture, sequence: UInt32)
    case voiceEvent(event: NextingVoiceEvent, sequence: UInt32)
    case voiceState(state: NextingVoiceState, label: String?)
    case text(channel: Int, title: String?, content: String)
    case usage(NextingUsageSnapshot)
    case usageClear
    case config(revision: UInt32, entries: [NextingConfigEntry])
    case configResult(revision: UInt32, status: NextingConfigStatus, code: NextingConfigError?)

    public var requiredProfile: String {
        switch self {
        case .present, .answer, .resolved, .error:
            NextingDeviceCodec.profile
        case .status:
            NextingDeviceCodec.statusProfile
        case .navigationPresent, .navigationMove, .navigationSelect,
             .navigationResolved:
            NextingDeviceCodec.navigationProfile
        case .keymap, .keyEvent:
            NextingDeviceCodec.keysProfile
        case .rotaryMap, .rotaryEvent, .rotaryPress:
            NextingDeviceCodec.rotaryProfile
        case .voiceEvent, .voiceState:
            NextingDeviceCodec.voiceProfile
        case .text:
            NextingDeviceCodec.textProfile
        case .usage, .usageClear:
            NextingDeviceCodec.usageProfile
        case .config, .configResult:
            NextingDeviceCodec.configProfile
        }
    }

    public var interactionSequence: (source: String, value: UInt32)? {
        switch self {
        case let .navigationMove(requestId, _, sequence),
             let .navigationSelect(requestId, _, sequence):
            ("navigation:\(requestId)", sequence)
        case let .keyEvent(slot, _, sequence):
            ("key:\(slot)", sequence)
        case let .rotaryEvent(slot, _, sequence),
             let .rotaryPress(slot, _, sequence):
            ("rotary:\(slot)", sequence)
        case let .voiceEvent(_, sequence):
            ("voice", sequence)
        default:
            nil
        }
    }
}

public enum NextingDeviceCodec {
    public static let wireVersion = 1
    public static let profile = "approval/1"
    public static let statusProfile = "status/1"
    public static let navigationProfile = "navigation/1"
    public static let keysProfile = "keys/1"
    public static let rotaryProfile = "rotary/1"
    public static let voiceProfile = "voice/1"
    public static let textProfile = "text/1"
    public static let usageProfile = "usage/1"
    public static let configProfile = "config/1"
    public static let maxRequestIDBytes = 64
    public static let maxSummaryBytes = 240
    public static let maxTTLMilliseconds = 300_000
    public static let maxMessageBytes = 4_096
    public static let maxStatusAgents = 8
    public static let maxStatusLabelBytes = 64

    private static let knownWireFields: Set<String> = [
        "v", "t", "id", "sum", "opt", "ttl", "ch", "r", "code", "agents",
        "items", "cursor", "dir", "seq", "index", "rev", "keys", "slot",
        "event", "controls", "delta", "state", "label", "channel", "title",
        "content", "model", "input_tokens", "output_tokens", "cached_tokens",
        "context_used", "context_limit", "entries", "status",
    ]
    private static let canonicalUnsignedFields: Set<String> = [
        "v", "ttl", "cursor", "seq", "index", "rev", "slot", "channel",
        "input_tokens", "output_tokens", "cached_tokens", "context_used",
        "context_limit",
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
        case let .navigationPresent(requestId, items, cursor, ttlMs):
            guard validID(requestId), validNavigationItems(items),
                  items.indices.contains(cursor), validTTL(ttlMs) else { return nil }
            let encodedItems = items.map(jsonString).joined(separator: ",")
            text = "{\"v\":1,\"t\":\"nav_present\",\"id\":\(jsonString(requestId)),\"items\":[\(encodedItems)],\"cursor\":\(cursor),\"ttl\":\(ttlMs)}\n"
        case let .navigationMove(requestId, direction, sequence):
            guard validID(requestId) else { return nil }
            text = "{\"v\":1,\"t\":\"nav_move\",\"id\":\(jsonString(requestId)),\"dir\":\(jsonString(direction.rawValue)),\"seq\":\(sequence)}\n"
        case let .navigationSelect(requestId, index, sequence):
            guard validID(requestId), (0 ... 7).contains(index) else { return nil }
            text = "{\"v\":1,\"t\":\"nav_select\",\"id\":\(jsonString(requestId)),\"index\":\(index),\"seq\":\(sequence)}\n"
        case let .navigationResolved(requestId, reason):
            guard validID(requestId) else { return nil }
            text = "{\"v\":1,\"t\":\"nav_resolved\",\"id\":\(jsonString(requestId)),\"r\":\(jsonString(reason.rawValue))}\n"
        case let .keymap(revision, keys):
            guard validKeyPresentations(keys) else { return nil }
            let entries = keys.map { key in
                var entry = "{\"slot\":\(key.slot),\"label\":\(jsonString(key.label)),\"enabled\":\(key.enabled ? "true" : "false"),\"light\":\(jsonString(key.light.rawValue))"
                if let rgb = key.rgb {
                    entry += ",\"rgb\":[\(rgb.red),\(rgb.green),\(rgb.blue)]"
                }
                return entry + "}"
            }.joined(separator: ",")
            text = "{\"v\":1,\"t\":\"keymap\",\"rev\":\(revision),\"keys\":[\(entries)]}\n"
        case let .keyEvent(slot, event, sequence):
            guard (0 ... 63).contains(slot) else { return nil }
            text = "{\"v\":1,\"t\":\"key_event\",\"slot\":\(slot),\"event\":\(jsonString(event.rawValue)),\"seq\":\(sequence)}\n"
        case let .rotaryMap(revision, controls):
            guard validRotaryControls(controls) else { return nil }
            let entries = controls.map {
                "{\"slot\":\($0.slot),\"label\":\(jsonString($0.label)),\"value\":\($0.value),\"min\":\($0.minimum),\"max\":\($0.maximum),\"wrap\":\($0.wrap ? "true" : "false")}"
            }.joined(separator: ",")
            text = "{\"v\":1,\"t\":\"rotary_map\",\"rev\":\(revision),\"controls\":[\(entries)]}\n"
        case let .rotaryEvent(slot, delta, sequence):
            guard (0 ... 15).contains(slot), (-127 ... 127).contains(delta),
                  delta != 0 else { return nil }
            text = "{\"v\":1,\"t\":\"rotary_event\",\"slot\":\(slot),\"delta\":\(delta),\"seq\":\(sequence)}\n"
        case let .rotaryPress(slot, event, sequence):
            guard (0 ... 15).contains(slot) else { return nil }
            text = "{\"v\":1,\"t\":\"rotary_press\",\"slot\":\(slot),\"event\":\(jsonString(event.rawValue)),\"seq\":\(sequence)}\n"
        case let .voiceEvent(event, sequence):
            text = "{\"v\":1,\"t\":\"voice_event\",\"event\":\(jsonString(event.rawValue)),\"seq\":\(sequence)}\n"
        case let .voiceState(state, label):
            if let label {
                guard validText(label, minimumBytes: 1, maximumBytes: 64) else { return nil }
                text = "{\"v\":1,\"t\":\"voice_state\",\"state\":\(jsonString(state.rawValue)),\"label\":\(jsonString(label))}\n"
            } else {
                text = "{\"v\":1,\"t\":\"voice_state\",\"state\":\(jsonString(state.rawValue))}\n"
            }
        case let .text(channel, title, content):
            guard (0 ... 7).contains(channel),
                  validText(content, minimumBytes: 0, maximumBytes: 1_024, allowLayout: true),
                  title.map({ validText($0, minimumBytes: 1, maximumBytes: 64) }) ?? true
            else { return nil }
            var body = "{\"v\":1,\"t\":\"text\",\"channel\":\(channel)"
            if let title { body += ",\"title\":\(jsonString(title))" }
            text = body + ",\"content\":\(jsonString(content))}\n"
        case let .usage(snapshot):
            guard validUsage(snapshot) else { return nil }
            var body = "{\"v\":1,\"t\":\"usage\",\"model\":\(jsonString(snapshot.model)),\"input_tokens\":\(snapshot.inputTokens),\"output_tokens\":\(snapshot.outputTokens)"
            if let cached = snapshot.cachedTokens { body += ",\"cached_tokens\":\(cached)" }
            if let used = snapshot.contextUsed, let limit = snapshot.contextLimit {
                body += ",\"context_used\":\(used),\"context_limit\":\(limit)"
            }
            text = body + "}\n"
        case .usageClear:
            text = "{\"v\":1,\"t\":\"usage_clear\"}\n"
        case let .config(revision, entries):
            guard validConfigEntries(entries) else { return nil }
            let encodedEntries = entries.map { entry in
                let value: String
                switch entry.value {
                case let .boolean(flag): value = flag ? "true" : "false"
                case let .integer(number): value = String(number)
                case let .string(string): value = jsonString(string)
                }
                return "{\"key\":\(jsonString(entry.key)),\"value\":\(value)}"
            }.joined(separator: ",")
            text = "{\"v\":1,\"t\":\"config\",\"rev\":\(revision),\"entries\":[\(encodedEntries)]}\n"
        case let .configResult(revision, status, code):
            if status == .applied {
                guard code == nil else { return nil }
                text = "{\"v\":1,\"t\":\"config_result\",\"rev\":\(revision),\"status\":\"applied\"}\n"
            } else {
                guard let code else { return nil }
                text = "{\"v\":1,\"t\":\"config_result\",\"rev\":\(revision),\"status\":\"rejected\",\"code\":\(jsonString(code.rawValue))}\n"
            }
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
        case "nav_present":
            guard exactKeys(message, ["v", "t", "id", "items", "cursor", "ttl"]),
                  let requestID = message["id"] as? String, validID(requestID),
                  let items = message["items"] as? [String], validNavigationItems(items),
                  let cursor = integer(message["cursor"]), items.indices.contains(cursor),
                  let ttl = integer(message["ttl"]), validTTL(ttl)
            else { return nil }
            return .navigationPresent(
                requestId: requestID,
                items: items,
                cursor: cursor,
                ttlMs: ttl
            )
        case "nav_move":
            guard exactKeys(message, ["v", "t", "id", "dir", "seq"]),
                  let requestID = message["id"] as? String, validID(requestID),
                  let rawDirection = message["dir"] as? String,
                  let direction = NextingNavigationDirection(rawValue: rawDirection),
                  let sequence = uint32(message["seq"])
            else { return nil }
            return .navigationMove(
                requestId: requestID,
                direction: direction,
                sequence: sequence
            )
        case "nav_select":
            guard exactKeys(message, ["v", "t", "id", "index", "seq"]),
                  let requestID = message["id"] as? String, validID(requestID),
                  let index = integer(message["index"]), (0 ... 7).contains(index),
                  let sequence = uint32(message["seq"])
            else { return nil }
            return .navigationSelect(requestId: requestID, index: index, sequence: sequence)
        case "nav_resolved":
            guard exactKeys(message, ["v", "t", "id", "r"]),
                  let requestID = message["id"] as? String, validID(requestID),
                  let rawReason = message["r"] as? String,
                  let reason = NextingNavigationResolution(rawValue: rawReason)
            else { return nil }
            return .navigationResolved(requestId: requestID, reason: reason)
        case "keymap":
            guard exactKeys(message, ["v", "t", "rev", "keys"]),
                  let revision = uint32(message["rev"]),
                  let rawKeys = message["keys"] as? [[String: Any]]
            else { return nil }
            var keys: [NextingKeyPresentation] = []
            for rawKey in rawKeys {
                guard exactKeys(rawKey, ["slot", "label", "enabled", "light", "rgb"]),
                      let slot = integer(rawKey["slot"]),
                      let label = rawKey["label"] as? String,
                      let enabled = boolean(rawKey["enabled"]),
                      let rawLight = rawKey["light"] as? String,
                      let light = NextingKeyLight(rawValue: rawLight)
                else { return nil }
                var rgb: NextingRGB?
                if rawKey.keys.contains("rgb") {
                    guard let values = rawKey["rgb"] as? [Any], values.count == 3,
                          let red = integer(values[0]),
                          let green = integer(values[1]),
                          let blue = integer(values[2])
                    else { return nil }
                    rgb = NextingRGB(red: red, green: green, blue: blue)
                }
                keys.append(.init(
                    slot: slot,
                    label: label,
                    enabled: enabled,
                    light: light,
                    rgb: rgb
                ))
            }
            guard validKeyPresentations(keys) else { return nil }
            return .keymap(revision: revision, keys: keys)
        case "key_event":
            guard exactKeys(message, ["v", "t", "slot", "event", "seq"]),
                  let slot = integer(message["slot"]), (0 ... 63).contains(slot),
                  let rawEvent = message["event"] as? String,
                  let event = NextingControlGesture(rawValue: rawEvent),
                  let sequence = uint32(message["seq"])
            else { return nil }
            return .keyEvent(slot: slot, event: event, sequence: sequence)
        case "rotary_map":
            guard exactKeys(message, ["v", "t", "rev", "controls"]),
                  let revision = uint32(message["rev"]),
                  let rawControls = message["controls"] as? [[String: Any]]
            else { return nil }
            var controls: [NextingRotaryControl] = []
            for rawControl in rawControls {
                guard exactKeys(
                    rawControl,
                    ["slot", "label", "value", "min", "max", "wrap"]
                ),
                    let slot = integer(rawControl["slot"]),
                    let label = rawControl["label"] as? String,
                    let value = integer(rawControl["value"]),
                    let minimum = integer(rawControl["min"]),
                    let maximum = integer(rawControl["max"]),
                    let wrap = boolean(rawControl["wrap"])
                else { return nil }
                controls.append(.init(
                    slot: slot,
                    label: label,
                    value: value,
                    minimum: minimum,
                    maximum: maximum,
                    wrap: wrap
                ))
            }
            guard validRotaryControls(controls) else { return nil }
            return .rotaryMap(revision: revision, controls: controls)
        case "rotary_event":
            guard exactKeys(message, ["v", "t", "slot", "delta", "seq"]),
                  let slot = integer(message["slot"]), (0 ... 15).contains(slot),
                  let delta = integer(message["delta"]), (-127 ... 127).contains(delta),
                  delta != 0, let sequence = uint32(message["seq"])
            else { return nil }
            return .rotaryEvent(slot: slot, delta: delta, sequence: sequence)
        case "rotary_press":
            guard exactKeys(message, ["v", "t", "slot", "event", "seq"]),
                  let slot = integer(message["slot"]), (0 ... 15).contains(slot),
                  let rawEvent = message["event"] as? String,
                  let event = NextingControlGesture(rawValue: rawEvent),
                  let sequence = uint32(message["seq"])
            else { return nil }
            return .rotaryPress(slot: slot, event: event, sequence: sequence)
        case "voice_event":
            guard exactKeys(message, ["v", "t", "event", "seq"]),
                  let rawEvent = message["event"] as? String,
                  let event = NextingVoiceEvent(rawValue: rawEvent),
                  let sequence = uint32(message["seq"])
            else { return nil }
            return .voiceEvent(event: event, sequence: sequence)
        case "voice_state":
            guard exactKeys(message, ["v", "t", "state", "label"]),
                  let rawState = message["state"] as? String,
                  let state = NextingVoiceState(rawValue: rawState)
            else { return nil }
            let label = message["label"] as? String
            guard !message.keys.contains("label")
                    || label.map({ validText($0, minimumBytes: 1, maximumBytes: 64) }) == true
            else { return nil }
            return .voiceState(state: state, label: label)
        case "text":
            guard exactKeys(message, ["v", "t", "channel", "title", "content"]),
                  let channel = integer(message["channel"]), (0 ... 7).contains(channel),
                  let content = message["content"] as? String,
                  validText(content, minimumBytes: 0, maximumBytes: 1_024, allowLayout: true)
            else { return nil }
            let title = message["title"] as? String
            guard !message.keys.contains("title")
                    || title.map({ validText($0, minimumBytes: 1, maximumBytes: 64) }) == true
            else { return nil }
            return .text(channel: channel, title: title, content: content)
        case "usage":
            guard exactKeys(message, [
                "v", "t", "model", "input_tokens", "output_tokens",
                "cached_tokens", "context_used", "context_limit",
            ]),
                let model = message["model"] as? String,
                let input = integer(message["input_tokens"]),
                let output = integer(message["output_tokens"])
            else { return nil }
            let cached = optionalInteger(message, "cached_tokens")
            let contextUsed = optionalInteger(message, "context_used")
            let contextLimit = optionalInteger(message, "context_limit")
            guard cached.valid, contextUsed.valid, contextLimit.valid else { return nil }
            let snapshot = NextingUsageSnapshot(
                model: model,
                inputTokens: input,
                outputTokens: output,
                cachedTokens: cached.value,
                contextUsed: contextUsed.value,
                contextLimit: contextLimit.value
            )
            guard validUsage(snapshot) else { return nil }
            return .usage(snapshot)
        case "usage_clear":
            guard exactKeys(message, ["v", "t"]) else { return nil }
            return .usageClear
        case "config":
            guard exactKeys(message, ["v", "t", "rev", "entries"]),
                  let revision = uint32(message["rev"]),
                  let rawEntries = message["entries"] as? [[String: Any]]
            else { return nil }
            var entries: [NextingConfigEntry] = []
            for rawEntry in rawEntries {
                guard exactKeys(rawEntry, ["key", "value"]),
                      let key = rawEntry["key"] as? String,
                      let value = configValue(rawEntry["value"])
                else { return nil }
                entries.append(.init(key: key, value: value))
            }
            guard validConfigEntries(entries) else { return nil }
            return .config(revision: revision, entries: entries)
        case "config_result":
            guard exactKeys(message, ["v", "t", "rev", "status", "code"]),
                  let revision = uint32(message["rev"]),
                  let rawStatus = message["status"] as? String,
                  let status = NextingConfigStatus(rawValue: rawStatus)
            else { return nil }
            let code = (message["code"] as? String).flatMap(NextingConfigError.init(rawValue:))
            if status == .applied {
                guard !message.keys.contains("code") else { return nil }
            } else {
                guard code != nil else { return nil }
            }
            return .configResult(revision: revision, status: status, code: code)
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

    private static func exactKeys(
        _ value: [String: Any],
        _ allowed: Set<String>
    ) -> Bool {
        Set(value.keys).isSubset(of: allowed)
    }

    private static func validText(
        _ value: String,
        minimumBytes: Int,
        maximumBytes: Int,
        allowLayout: Bool = false
    ) -> Bool {
        let count = value.utf8.count
        guard (minimumBytes ... maximumBytes).contains(count) else { return false }
        return value.unicodeScalars.allSatisfy {
            if allowLayout, $0.value == 0x09 || $0.value == 0x0A { return true }
            return $0.value > 0x1F && $0.value != 0x7F
        }
    }

    private static func validNavigationItems(_ items: [String]) -> Bool {
        (2 ... 8).contains(items.count)
            && Set(items).count == items.count
            && items.allSatisfy {
                validText($0, minimumBytes: 1, maximumBytes: 64)
            }
    }

    private static func validKeyPresentations(
        _ keys: [NextingKeyPresentation]
    ) -> Bool {
        guard keys.count <= 64, Set(keys.map(\.slot)).count == keys.count else {
            return false
        }
        return keys.allSatisfy { key in
            (0 ... 63).contains(key.slot)
                && validText(key.label, minimumBytes: 1, maximumBytes: 32)
                && key.rgb.map {
                    [ $0.red, $0.green, $0.blue ].allSatisfy {
                        (0 ... 255).contains($0)
                    }
                } ?? true
        }
    }

    private static func validRotaryControls(
        _ controls: [NextingRotaryControl]
    ) -> Bool {
        guard controls.count <= 16,
              Set(controls.map(\.slot)).count == controls.count else {
            return false
        }
        return controls.allSatisfy {
            (0 ... 15).contains($0.slot)
                && validText($0.label, minimumBytes: 1, maximumBytes: 32)
                && (-1_000_000 ... 1_000_000).contains($0.minimum)
                && (-1_000_000 ... 1_000_000).contains($0.maximum)
                && $0.minimum <= $0.value
                && $0.value <= $0.maximum
        }
    }

    private static func validCounter(_ value: Int) -> Bool {
        (0 ... 9_007_199_254_740_991).contains(value)
    }

    private static func validUsage(_ snapshot: NextingUsageSnapshot) -> Bool {
        validText(snapshot.model, minimumBytes: 1, maximumBytes: 64)
            && validCounter(snapshot.inputTokens)
            && validCounter(snapshot.outputTokens)
            && (snapshot.cachedTokens.map(validCounter) ?? true)
            && ((snapshot.contextUsed == nil) == (snapshot.contextLimit == nil))
            && {
                guard let used = snapshot.contextUsed,
                      let limit = snapshot.contextLimit else { return true }
                return validCounter(used) && validCounter(limit) && used <= limit
            }()
    }

    private static let configKeyPattern = try! NSRegularExpression(
        pattern: #"^[A-Za-z0-9][A-Za-z0-9._-]{0,47}$"#
    )

    private static func validConfigEntries(
        _ entries: [NextingConfigEntry]
    ) -> Bool {
        guard entries.count <= 32,
              Set(entries.map(\.key)).count == entries.count else { return false }
        return entries.allSatisfy { entry in
            let range = NSRange(entry.key.startIndex..., in: entry.key)
            guard configKeyPattern.firstMatch(
                in: entry.key,
                range: range
            ) != nil else { return false }
            switch entry.value {
            case .boolean:
                return true
            case let .integer(value):
                return (-1_000_000 ... 1_000_000).contains(value)
            case let .string(value):
                return validText(
                    value,
                    minimumBytes: 0,
                    maximumBytes: 128
                )
            }
        }
    }

    private static func boolean(_ value: Any?) -> Bool? {
        guard let number = value as? NSNumber,
              CFGetTypeID(number) == CFBooleanGetTypeID() else { return nil }
        return number.boolValue
    }

    private static func uint32(_ value: Any?) -> UInt32? {
        guard let value = integer(value), (0 ... Int(UInt32.max)).contains(value)
        else { return nil }
        return UInt32(value)
    }

    private static func optionalInteger(
        _ message: [String: Any],
        _ key: String
    ) -> (valid: Bool, value: Int?) {
        guard message.keys.contains(key) else { return (true, nil) }
        guard let value = integer(message[key]) else { return (false, nil) }
        return (true, value)
    }

    private static func configValue(_ raw: Any?) -> NextingConfigValue? {
        if let value = boolean(raw) { return .boolean(value) }
        if let value = raw as? String { return .string(value) }
        if let value = integer(raw) { return .integer(value) }
        return nil
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
                    if canonicalUnsignedFields.contains(key),
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
