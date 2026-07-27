import Testing
@testable import NextingDeviceKit

@Test("host smoke arguments have safe defaults and bounded overrides")
func hostSmokeArguments() throws {
    let defaults = try NextingDeviceHostSmokeConfiguration.parse(arguments: [])
    #expect(defaults.summary == "Allow the Nexting hardware smoke test?")
    #expect(defaults.timeoutSeconds == 60)

    let custom = try NextingDeviceHostSmokeConfiguration.parse(arguments: [
        "--summary", "Allow fixture 7?",
        "--timeout", "15",
    ])
    #expect(custom.summary == "Allow fixture 7?")
    #expect(custom.timeoutSeconds == 15)

    #expect(throws: NextingDeviceHostSmokeConfiguration.ParseError.self) {
        try NextingDeviceHostSmokeConfiguration.parse(arguments: ["--timeout", "0"])
    }
    #expect(throws: NextingDeviceHostSmokeConfiguration.ParseError.self) {
        try NextingDeviceHostSmokeConfiguration.parse(arguments: ["--mystery"])
    }
}

@Test("host smoke PASS line is machine-readable")
func hostSmokePassLine() {
    #expect(
        NextingDeviceHostSmokeConfiguration.passLine(choice: .allow)
            == "PASS answer=allow"
    )
    #expect(
        NextingDeviceHostSmokeConfiguration.passLine(choice: .deny)
            == "PASS answer=deny"
    )
}
