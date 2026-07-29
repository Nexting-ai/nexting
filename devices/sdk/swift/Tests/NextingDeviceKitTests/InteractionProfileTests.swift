import Foundation
import Testing
@testable import NextingDeviceKit

private struct InteractionVectorDocument: Decodable {
    struct Valid: Decodable {
        let name: String
        let wire: String
    }

    struct Invalid: Decodable {
        let name: String
        let wire: String
    }

    let profile: String
    let valid: [Valid]
    let invalid: [Invalid]
}

private func interactionVectorDirectory() -> URL {
    URL(fileURLWithPath: #filePath)
        .deletingLastPathComponent()
        .deletingLastPathComponent()
        .deletingLastPathComponent()
        .deletingLastPathComponent()
        .deletingLastPathComponent()
        .appendingPathComponent("protocol/vectors", isDirectory: true)
}

@Test("all interaction vectors use the same strict Swift codec")
func interactionVectorsRoundTrip() throws {
    let profiles = ["navigation", "keys", "rotary", "voice", "text", "usage", "config"]
    for profile in profiles {
        let document = try JSONDecoder().decode(
            InteractionVectorDocument.self,
            from: Data(
                contentsOf: interactionVectorDirectory()
                    .appendingPathComponent("\(profile)-v1.json")
            )
        )
        #expect(document.profile == "\(profile)/1")
        for vector in document.valid {
            let message = try #require(
                NextingDeviceCodec.decode(Data(vector.wire.utf8)),
                Comment(rawValue: vector.name)
            )
            #expect(
                NextingDeviceCodec.encode(message) == Data(vector.wire.utf8),
                Comment(rawValue: vector.name)
            )
        }
        for vector in document.invalid {
            #expect(
                NextingDeviceCodec.decode(Data(vector.wire.utf8)) == nil,
                Comment(rawValue: vector.name)
            )
        }
    }
}

@Test("interaction profile negotiation is explicit")
func interactionProfileNegotiation() throws {
    let root = interactionVectorDirectory()
        .appendingPathComponent("device-info-v1.json")
    let document = try JSONSerialization.jsonObject(
        with: Data(contentsOf: root)
    ) as! [String: Any]
    let valid = (document["valid"] as! [[String: Any]])[0]
    var payload = try JSONSerialization.jsonObject(
        with: Data((valid["wire"] as! String).utf8)
    ) as! [String: Any]
    payload["profiles"] = ["approval/1", "navigation/1", "keys/1"]
    let info = try #require(
        NextingDeviceInfo.decode(
            try JSONSerialization.data(withJSONObject: payload)
        )
    )
    #expect(info.supportsNavigationV1)
    #expect(info.supportsKeysV1)
    #expect(!info.supportsConfigV1)
}
