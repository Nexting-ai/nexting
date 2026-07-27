import Foundation

public struct NextingDeviceHostSmokeConfiguration: Equatable, Sendable {
    public enum ParseError: Error, Equatable {
        case missingValue(String)
        case invalidTimeout(String)
        case invalidSummary
        case unknownArgument(String)
    }

    public let summary: String
    public let timeoutSeconds: Int

    public init(summary: String, timeoutSeconds: Int) {
        self.summary = summary
        self.timeoutSeconds = timeoutSeconds
    }

    public static func parse(arguments: [String]) throws -> Self {
        var summary = "Allow the Nexting hardware smoke test?"
        var timeoutSeconds = 60
        var index = 0

        while index < arguments.count {
            switch arguments[index] {
            case "--summary":
                guard index + 1 < arguments.count else {
                    throw ParseError.missingValue("--summary")
                }
                summary = arguments[index + 1]
                index += 2
            case "--timeout":
                guard index + 1 < arguments.count else {
                    throw ParseError.missingValue("--timeout")
                }
                let raw = arguments[index + 1]
                guard let value = Int(raw), (5 ... 300).contains(value) else {
                    throw ParseError.invalidTimeout(raw)
                }
                timeoutSeconds = value
                index += 2
            default:
                throw ParseError.unknownArgument(arguments[index])
            }
        }

        guard NextingDeviceCodec.encode(.present(
            requestId: "host-smoke",
            summary: summary,
            ttlMs: timeoutSeconds * 1_000
        )) != nil else {
            throw ParseError.invalidSummary
        }
        return Self(summary: summary, timeoutSeconds: timeoutSeconds)
    }

    public static func passLine(choice: NextingDeviceChoice) -> String {
        "PASS answer=\(choice.rawValue)"
    }
}
