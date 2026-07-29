import Foundation

public struct NextingDeviceIdentity: Equatable, Sendable {
    public let deviceId: UUID?
    public let manufacturer: String?
    public let displayName: String?
    public let serialNumber: String?
}

public struct NextingDeviceDisplay: Equatable, Sendable {
    public let type: String
    public let width: Int
    public let height: Int
}

public struct NextingDeviceCapabilities: Equatable, Sendable {
    public let buttonCount: Int?
    public let approvalButtonCount: Int?
    public let customButtonCount: Int?
    public let rotaryCount: Int?
    public let rotaryPressCount: Int?
    public let statusSlots: Int
    public let batteryService: Bool
    public let display: NextingDeviceDisplay?
    public let haptics: [String]
}

public struct NextingDeviceVendorFact: Equatable, Sendable {
    public let key: String
    public let label: String
    public let value: String
}

public struct NextingDeviceVendorInfo: Equatable, Sendable {
    public let namespace: String
    public let facts: [NextingDeviceVendorFact]
}

public struct NextingDeviceInfo: Equatable, Sendable {
    public static let maxEncodedBytes = 4_096

    public let protocolName: String
    public let spec: String
    public let wireVersions: [Int]
    public let profiles: [String]
    public let model: String
    public let firmwareVersion: String
    public let maxMessageBytes: Int
    public let maxSummaryBytes: Int
    public let identity: NextingDeviceIdentity
    public let capabilities: NextingDeviceCapabilities
    public let vendor: NextingDeviceVendorInfo?

    public var statusSlots: Int { capabilities.statusSlots }

    public var supportsApprovalV1: Bool {
        protocolName == "nexting-device"
            && wireVersions.contains(NextingDeviceCodec.wireVersion)
            && profiles.contains(NextingDeviceCodec.profile)
    }

    public var supportsStatusV1: Bool {
        supportsApprovalV1
            && profiles.contains(NextingDeviceCodec.statusProfile)
            && statusSlots >= 1
    }

    public func supportsProfile(_ profile: String) -> Bool {
        supportsApprovalV1 && profiles.contains(profile)
    }

    public var supportsNavigationV1: Bool {
        supportsProfile(NextingDeviceCodec.navigationProfile)
    }

    public var supportsKeysV1: Bool {
        supportsProfile(NextingDeviceCodec.keysProfile)
    }

    public var supportsRotaryV1: Bool {
        supportsProfile(NextingDeviceCodec.rotaryProfile)
    }

    public var supportsVoiceV1: Bool {
        supportsProfile(NextingDeviceCodec.voiceProfile)
    }

    public var supportsTextV1: Bool {
        supportsProfile(NextingDeviceCodec.textProfile)
    }

    public var supportsUsageV1: Bool {
        supportsProfile(NextingDeviceCodec.usageProfile)
    }

    public var supportsConfigV1: Bool {
        supportsProfile(NextingDeviceCodec.configProfile)
    }

    public static func decode(_ data: Data) -> NextingDeviceInfo? {
        guard data.count <= maxEncodedBytes,
              String(data: data, encoding: .utf8) != nil,
              let payload = try? JSONDecoder().decode(Payload.self, from: data),
              payload.protocolName == "nexting-device",
              validDescriptor(payload.spec),
              validUniqueDescriptors(payload.profiles, maximumCount: 16, maximumBytes: 32),
              payload.profiles.contains(NextingDeviceCodec.profile),
              !payload.wireVersions.isEmpty,
              payload.wireVersions.count <= 4,
              Set(payload.wireVersions).count == payload.wireVersions.count,
              payload.wireVersions.allSatisfy({ (1 ... 65_535).contains($0) }),
              payload.wireVersions.contains(NextingDeviceCodec.wireVersion),
              validDescriptor(payload.model),
              validDescriptor(payload.firmwareVersion),
              (512 ... Int(UInt32.max)).contains(payload.maxMessageBytes),
              (1 ... NextingDeviceCodec.maxSummaryBytes).contains(payload.maxSummaryBytes)
        else { return nil }

        guard validOptionalDescriptor(payload.manufacturer),
              validOptionalDescriptor(payload.displayName),
              validOptionalDescriptor(payload.serialNumber)
        else { return nil }

        let statusSlots = payload.statusSlots ?? 0
        guard (0 ... NextingDeviceCodec.maxStatusAgents).contains(statusSlots),
              statusSlots == 0 || payload.profiles.contains(NextingDeviceCodec.statusProfile),
              validCount(payload.buttonCount, maximum: 1_024),
              validCount(payload.approvalButtonCount, maximum: 1_024),
              validCount(payload.customButtonCount, maximum: 1_024),
              validCount(payload.rotaryCount, maximum: 64),
              validCount(payload.rotaryPressCount, maximum: 64),
              specializedCount(payload.approvalButtonCount, fitsWithin: payload.buttonCount),
              specializedCount(payload.customButtonCount, fitsWithin: payload.buttonCount),
              specializedCount(payload.rotaryPressCount, fitsWithin: payload.rotaryCount),
              validDisplay(payload.display),
              validHaptics(payload.haptics)
        else { return nil }

        let deviceId: UUID?
        if let rawDeviceId = payload.deviceId {
            guard rawDeviceId.utf8.count == 36,
                  let parsed = UUID(uuidString: rawDeviceId)
            else { return nil }
            deviceId = parsed
        } else {
            deviceId = nil
        }

        let info = NextingDeviceInfo(
            protocolName: payload.protocolName,
            spec: payload.spec,
            wireVersions: payload.wireVersions,
            profiles: payload.profiles,
            model: payload.model,
            firmwareVersion: payload.firmwareVersion,
            maxMessageBytes: payload.maxMessageBytes,
            maxSummaryBytes: payload.maxSummaryBytes,
            identity: NextingDeviceIdentity(
                deviceId: deviceId,
                manufacturer: payload.manufacturer,
                displayName: payload.displayName,
                serialNumber: payload.serialNumber
            ),
            capabilities: NextingDeviceCapabilities(
                buttonCount: payload.buttonCount,
                approvalButtonCount: payload.approvalButtonCount,
                customButtonCount: payload.customButtonCount,
                rotaryCount: payload.rotaryCount,
                rotaryPressCount: payload.rotaryPressCount,
                statusSlots: statusSlots,
                batteryService: payload.batteryService ?? false,
                display: payload.display.map {
                    NextingDeviceDisplay(type: $0.type, width: $0.width, height: $0.height)
                },
                haptics: payload.haptics ?? []
            ),
            vendor: decodeVendor(from: data)
        )
        return info.supportsApprovalV1 ? info : nil
    }

    private static func validDescriptor(
        _ value: String,
        maximumBytes: Int = 64
    ) -> Bool {
        !value.isEmpty
            && value.utf8.count <= maximumBytes
            && value.unicodeScalars.allSatisfy {
                !CharacterSet.controlCharacters.contains($0)
            }
    }

    private static func validOptionalDescriptor(_ value: String?) -> Bool {
        value.map { validDescriptor($0) } ?? true
    }

    private static func validUniqueDescriptors(
        _ values: [String],
        maximumCount: Int,
        maximumBytes: Int
    ) -> Bool {
        !values.isEmpty
            && values.count <= maximumCount
            && Set(values).count == values.count
            && values.allSatisfy { validDescriptor($0, maximumBytes: maximumBytes) }
    }

    private static func validCount(_ value: Int?, maximum: Int) -> Bool {
        value.map { (0 ... maximum).contains($0) } ?? true
    }

    private static func specializedCount(_ value: Int?, fitsWithin total: Int?) -> Bool {
        guard let value, let total else { return true }
        return value <= total
    }

    private static func validDisplay(_ display: Payload.Display?) -> Bool {
        guard let display else { return true }
        return validDescriptor(display.type, maximumBytes: 32)
            && (1 ... 4_096).contains(display.width)
            && (1 ... 4_096).contains(display.height)
    }

    private static func validHaptics(_ values: [String]?) -> Bool {
        guard let values else { return true }
        return validUniqueDescriptors(values, maximumCount: 8, maximumBytes: 32)
    }

    private static func decodeVendor(from data: Data) -> NextingDeviceVendorInfo? {
        guard let root = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let rawVendor = root["vendor"]
        else { return nil }
        guard let vendor = rawVendor as? [String: Any],
              let namespace = vendor["namespace"] as? String,
              validDescriptor(namespace, maximumBytes: 128),
              namespace.range(
                of: #"^[a-z0-9](?:[a-z0-9-]{0,62}\.)+[a-z0-9][a-z0-9-]{0,62}$"#,
                options: .regularExpression
              ) != nil,
              let rawFacts = vendor["facts"] as? [[String: Any]],
              (1 ... 16).contains(rawFacts.count),
              let encodedVendor = try? JSONSerialization.data(
                withJSONObject: vendor,
                options: [.sortedKeys]
              ),
              encodedVendor.count <= 1_024
        else { return nil }

        var keys = Set<String>()
        var facts: [NextingDeviceVendorFact] = []
        for rawFact in rawFacts {
            guard let key = rawFact["key"] as? String,
                  key.range(
                    of: #"^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$"#,
                    options: .regularExpression
                  ) != nil,
                  keys.insert(key).inserted,
                  let label = rawFact["label"] as? String,
                  validInertText(label, maximumBytes: 64),
                  let value = vendorValue(rawFact["value"]),
                  validInertText(value, maximumBytes: 128)
            else { return nil }
            facts.append(NextingDeviceVendorFact(key: key, label: label, value: value))
        }
        return NextingDeviceVendorInfo(namespace: namespace, facts: facts)
    }

    private static func vendorValue(_ raw: Any?) -> String? {
        if let value = raw as? String { return value }
        if raw is Bool { return nil }
        if let value = raw as? NSNumber {
            let double = value.doubleValue
            guard double.isFinite,
                  double.rounded(.towardZero) == double
            else { return nil }
            return value.stringValue
        }
        return nil
    }

    private static func validInertText(_ value: String, maximumBytes: Int) -> Bool {
        validDescriptor(value, maximumBytes: maximumBytes)
            && value.range(
                of: #"(?:</?[a-z]|https?://|www\.|[`*_#\[\]()])"#,
                options: [.regularExpression, .caseInsensitive]
            ) == nil
    }

    private struct Payload: Decodable {
        struct Display: Decodable {
            let type: String
            let width: Int
            let height: Int
        }

        let protocolName: String
        let spec: String
        let wireVersions: [Int]
        let profiles: [String]
        let model: String
        let firmwareVersion: String
        let maxMessageBytes: Int
        let maxSummaryBytes: Int
        let statusSlots: Int?
        let deviceId: String?
        let manufacturer: String?
        let displayName: String?
        let serialNumber: String?
        let buttonCount: Int?
        let approvalButtonCount: Int?
        let customButtonCount: Int?
        let rotaryCount: Int?
        let rotaryPressCount: Int?
        let batteryService: Bool?
        let display: Display?
        let haptics: [String]?

        enum CodingKeys: String, CodingKey {
            case protocolName = "protocol"
            case spec
            case wireVersions = "wire"
            case profiles
            case model
            case firmwareVersion = "fw"
            case maxMessageBytes = "max_message_bytes"
            case maxSummaryBytes = "max_summary_bytes"
            case statusSlots
            case deviceId = "device_id"
            case manufacturer
            case displayName = "display_name"
            case serialNumber = "serial_number"
            case buttonCount = "button_count"
            case approvalButtonCount = "approval_button_count"
            case customButtonCount = "custom_button_count"
            case rotaryCount = "rotary_count"
            case rotaryPressCount = "rotary_press_count"
            case batteryService = "battery_service"
            case display
            case haptics
        }
    }
}
