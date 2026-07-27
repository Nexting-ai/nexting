import Foundation
import Testing
@testable import NextingDeviceKit

private struct DeviceInfoVectors: Decodable {
    struct Valid: Decodable {
        struct Expected: Decodable {
            let model: String
            let statusSlots: Int
            let deviceId: String?
            let buttonCount: Int?
            let batteryService: Bool
            let vendorNamespace: String?
        }

        let name: String
        let wire: String
        let decoded: Expected
    }

    struct InvalidCore: Decodable {
        let name: String
        let wire: String
    }

    struct InvalidVendor: Decodable {
        let name: String
        let vendor: JSONValue
    }

    let spec: String
    let wire: Int
    let valid: [Valid]
    let invalidCore: [InvalidCore]
    let invalidVendor: [InvalidVendor]
}

private enum JSONValue: Codable {
    case string(String)
    case integer(Int)
    case array([JSONValue])
    case object([String: JSONValue])
    case bool(Bool)
    case null

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        if container.decodeNil() {
            self = .null
        } else if let value = try? container.decode(Bool.self) {
            self = .bool(value)
        } else if let value = try? container.decode(Int.self) {
            self = .integer(value)
        } else if let value = try? container.decode(String.self) {
            self = .string(value)
        } else if let value = try? container.decode([JSONValue].self) {
            self = .array(value)
        } else {
            self = .object(try container.decode([String: JSONValue].self))
        }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case let .string(value): try container.encode(value)
        case let .integer(value): try container.encode(value)
        case let .array(value): try container.encode(value)
        case let .object(value): try container.encode(value)
        case let .bool(value): try container.encode(value)
        case .null: try container.encodeNil()
        }
    }
}

private func loadDeviceInfoVectors() throws -> DeviceInfoVectors {
    let testFile = URL(fileURLWithPath: #filePath)
    let repository = testFile
        .deletingLastPathComponent()
        .deletingLastPathComponent()
        .deletingLastPathComponent()
        .deletingLastPathComponent()
        .deletingLastPathComponent()
    return try JSONDecoder().decode(
        DeviceInfoVectors.self,
        from: Data(contentsOf: repository.appendingPathComponent(
            "protocol/vectors/device-info-v1.json"
        ))
    )
}

@Test("Device Info 0.2 shared vectors normalize identically")
func deviceInfoSharedVectors() throws {
    let vectors = try loadDeviceInfoVectors()
    #expect(vectors.spec == "0.2.0-experimental.0")
    #expect(vectors.wire == 1)

    for item in vectors.valid {
        let info = try #require(
            NextingDeviceInfo.decode(Data(item.wire.utf8)),
            Comment(rawValue: item.name)
        )
        #expect(info.model == item.decoded.model)
        #expect(info.statusSlots == item.decoded.statusSlots)
        #expect(info.identity.deviceId?.uuidString.lowercased() == item.decoded.deviceId)
        #expect(info.capabilities.buttonCount == item.decoded.buttonCount)
        #expect(info.capabilities.batteryService == item.decoded.batteryService)
        #expect(info.vendor?.namespace == item.decoded.vendorNamespace)
    }

    for item in vectors.invalidCore {
        #expect(NextingDeviceInfo.decode(Data(item.wire.utf8)) == nil, Comment(rawValue: item.name))
    }
}

@Test("invalid vendor facts are dropped without rejecting valid core")
func invalidVendorIsLossy() throws {
    let vectors = try loadDeviceInfoVectors()
    let core = try #require(
        JSONSerialization.jsonObject(with: Data(vectors.valid[0].wire.utf8))
            as? [String: Any]
    )

    for item in vectors.invalidVendor {
        let encodedVendor = try JSONEncoder().encode(item.vendor)
        var payload = core
        payload["vendor"] = try JSONSerialization.jsonObject(with: encodedVendor)
        let wire = try JSONSerialization.data(withJSONObject: payload, options: [.sortedKeys])
        let info = try #require(
            NextingDeviceInfo.decode(wire),
            Comment(rawValue: item.name)
        )
        #expect(info.vendor == nil)
    }
}

@Test("standard Battery Level parsing is bounded and optional")
func batteryLevelParsing() {
    #expect(NextingDeviceBattery.decodeLevel(Data([0])) == 0)
    #expect(NextingDeviceBattery.decodeLevel(Data([55])) == 55)
    #expect(NextingDeviceBattery.decodeLevel(Data([100])) == 100)
    #expect(NextingDeviceBattery.decodeLevel(Data([101])) == 100)
    #expect(NextingDeviceBattery.decodeLevel(Data()) == nil)
    #expect(NextingDeviceBattery.decodeLevel(Data([1, 2])) == nil)
}
