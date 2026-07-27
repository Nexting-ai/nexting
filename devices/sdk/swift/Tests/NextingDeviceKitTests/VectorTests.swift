import Foundation
import Testing
@testable import NextingDeviceKit

private struct VectorDocument: Decodable {
    let spec: String
    let wire: Int
    let profile: String
    let valid: [ValidVector]
    let invalid: [String]
}

private struct ValidVector: Decodable {
    let wire: String
    let decoded: DecodedVector
}

private struct DecodedVector: Decodable {
    let type: String
    let requestId: String?
    let summary: String?
    let options: [String]?
    let ttlMs: Int?
    let choice: String?
    let reason: String?
    let code: String?

    var message: NextingDeviceMessage {
        get throws {
            switch type {
            case "present":
                return .present(
                    requestId: try #require(requestId),
                    summary: try #require(summary),
                    ttlMs: try #require(ttlMs)
                )
            case "answer":
                let rawChoice = try #require(choice)
                return .answer(
                    requestId: try #require(requestId),
                    choice: try #require(NextingDeviceChoice(rawValue: rawChoice))
                )
            case "resolved":
                let rawReason = try #require(reason)
                return .resolved(
                    requestId: try #require(requestId),
                    reason: try #require(NextingDeviceResolutionReason(rawValue: rawReason))
                )
            case "error":
                let rawCode = try #require(code)
                return .error(
                    requestId: requestId,
                    code: try #require(NextingDeviceErrorCode(rawValue: rawCode))
                )
            default:
                Issue.record("unknown vector type \(type)")
                return .error(requestId: nil, code: .badMessage)
            }
        }
    }
}

private func loadVectors() throws -> VectorDocument {
    let testFile = URL(fileURLWithPath: #filePath)
    let repository = testFile
        .deletingLastPathComponent() // NextingDeviceKitTests
        .deletingLastPathComponent() // Tests
        .deletingLastPathComponent() // swift
        .deletingLastPathComponent() // sdk
        .deletingLastPathComponent() // nexting-devices
    let url = repository.appendingPathComponent(
        "protocol/vectors/approval-v1.json",
        isDirectory: false
    )
    return try JSONDecoder().decode(
        VectorDocument.self,
        from: Data(contentsOf: url)
    )
}

@Test("shared vectors round-trip canonically")
func sharedVectorsRoundTrip() throws {
    let vectors = try loadVectors()
    #expect(vectors.spec == "0.1.0-experimental")
    #expect(vectors.wire == 1)
    #expect(vectors.profile == "approval/1")

    for item in vectors.valid {
        let expected = try item.decoded.message
        #expect(NextingDeviceCodec.decode(Data(item.wire.utf8)) == expected)
        #expect(NextingDeviceCodec.encode(expected) == Data(item.wire.utf8))
    }
    for wire in vectors.invalid {
        #expect(NextingDeviceCodec.decode(Data(wire.utf8)) == nil)
    }
}

@Test("summary limit counts UTF-8 bytes")
func summaryLimitCountsBytes() {
    let oversized = String(repeating: "批", count: 81)
    #expect(Data(oversized.utf8).count == 243)
    #expect(
        NextingDeviceCodec.encode(
            .present(requestId: "r1", summary: oversized, ttlMs: 30_000)
        ) == nil
    )
}

@Test("summary rejects NUL on encode and decode")
func summaryRejectsNUL() {
    #expect(
        NextingDeviceCodec.encode(
            .present(requestId: "r1", summary: "approve\0deny", ttlMs: 30_000)
        ) == nil
    )
    #expect(
        NextingDeviceCodec.decode(
            Data(
                #"{"v":1,"t":"present","id":"r1","sum":"approve\u0000deny","opt":["allow","deny"],"ttl":30000}"#.utf8
            )
        ) == nil
    )
}

@Test("direct codec rejects a complete frame above the host limit")
func codecRejectsOversizedFrame() {
    let wire = Data(
        "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\",\"future\":\"\(String(repeating: "x", count: 4096))\"}\n".utf8
    )
    #expect(wire.count > 4096)
    #expect(NextingDeviceCodec.decode(wire) == nil)
}

@Test("direct codec reserves one byte for the required newline")
func codecCountsImplicitNewline() {
    let prefix = #"{"v":1,"t":"answer","id":"r1","ch":"allow","future":""#
    let suffix = #""}"#
    let payloadBytes = NextingDeviceCodec.maxMessageBytes - 1
    let exactPayload = prefix
        + String(repeating: "x", count: payloadBytes - prefix.utf8.count - suffix.utf8.count)
        + suffix
    let exactWire = Data((exactPayload + "\n").utf8)
    #expect(exactWire.count == NextingDeviceCodec.maxMessageBytes)
    #expect(NextingDeviceCodec.decode(exactWire) != nil)
    #expect(NextingDeviceCodec.decode(Data(exactPayload.utf8)) != nil)

    let tooLargeWithoutNewline = prefix
        + String(repeating: "x", count: payloadBytes + 1 - prefix.utf8.count - suffix.utf8.count)
        + suffix
    let tooLargeData = Data(tooLargeWithoutNewline.utf8)
    #expect(tooLargeData.count == NextingDeviceCodec.maxMessageBytes)
    #expect(NextingDeviceCodec.decode(tooLargeData) == nil)
}

@Test("error can omit id but rejects explicit null id on the wire")
func errorIDRules() {
    let message = NextingDeviceMessage.error(requestId: nil, code: .badMessage)
    let wire = Data("{\"v\":1,\"t\":\"error\",\"code\":\"bad_message\"}\n".utf8)
    #expect(NextingDeviceCodec.encode(message) == wire)
    #expect(NextingDeviceCodec.decode(wire) == message)
    #expect(
        NextingDeviceCodec.decode(
            Data("{\"v\":1,\"t\":\"error\",\"id\":null,\"code\":\"bad_message\"}\n".utf8)
        ) == nil
    )
}

@Test("decoder ignores one unknown optional field")
func decoderIgnoresUnknownOptionalField() {
    let wire = Data(
        #"{"v":1,"t":"answer","id":"r1","ch":"deny","future":{"note":"comma, brace } and quote \" remain data"}}"#.utf8
    ) + Data([0x0A])
    #expect(
        NextingDeviceCodec.decode(wire) == .answer(requestId: "r1", choice: .deny)
    )
    let longUnknownKey = String(repeating: "k", count: 241)
    let longKeyWire = Data(
        "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\",\"\(longUnknownKey)\":true}".utf8
    )
    #expect(
        NextingDeviceCodec.decode(longKeyWire) == .answer(requestId: "r1", choice: .allow)
    )
}

@Test("line decoder buffers bytes, bounds messages, and resets")
func lineDecoderBehavior() {
    let wire = Data("{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\"}\n".utf8)
    var decoder = NextingDeviceLineDecoder(maxMessageBytes: 64)
    #expect(decoder.push(wire.prefix(8)) == [])
    #expect(
        decoder.push(wire.dropFirst(8)) == [
            .answer(requestId: "r1", choice: .allow),
        ]
    )

    #expect(
        decoder.push(Data(repeating: 0x78, count: 64)) == [
            .error(requestId: nil, code: .messageTooLarge),
        ]
    )
    #expect(decoder.push(Data("discard\n".utf8)) == [])
    decoder.reset()
    #expect(decoder.push(wire) == [.answer(requestId: "r1", choice: .allow)])
}

@Test("line decoder preserves multibyte UTF-8 split across chunks")
func lineDecoderMultibyteSplit() {
    let wire = Data(
        "{\"v\":1,\"t\":\"present\",\"id\":\"r1\",\"sum\":\"批准?\",\"opt\":[\"allow\",\"deny\"],\"ttl\":1000}\n".utf8
    )
    let split = try! #require(wire.firstIndex(of: 0xE6)) + 1
    var decoder = NextingDeviceLineDecoder(maxMessageBytes: 512)
    #expect(decoder.push(wire.prefix(upTo: split)) == [])
    #expect(
        decoder.push(wire.suffix(from: split)) == [
            .present(requestId: "r1", summary: "批准?", ttlMs: 1000),
        ]
    )
}

@Test("line decoder message limit counts the terminating newline")
func lineDecoderCountsNewline() {
    let wire = Data("{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\"}\n".utf8)
    var exact = NextingDeviceLineDecoder(maxMessageBytes: wire.count)
    #expect(exact.push(wire) == [.answer(requestId: "r1", choice: .allow)])

    var tooSmall = NextingDeviceLineDecoder(maxMessageBytes: wire.count - 1)
    #expect(
        tooSmall.push(wire) == [
            .error(requestId: nil, code: .messageTooLarge),
        ]
    )
}

@Test("one hostile chunk cannot amplify into unbounded results")
func lineDecoderBoundsChunkOutput() {
    var decoder = NextingDeviceLineDecoder()
    #expect(
        decoder.push(Data(repeating: 0x0A, count: 4_097)) == [
            .error(requestId: nil, code: .messageTooLarge),
        ]
    )
}
