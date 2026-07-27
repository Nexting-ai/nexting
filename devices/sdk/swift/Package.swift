// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "NextingDeviceKit",
    platforms: [
        .iOS(.v16),
        .macOS(.v13),
    ],
    products: [
        .library(name: "NextingDeviceKit", targets: ["NextingDeviceKit"]),
        .executable(
            name: "nexting-device-host-smoke",
            targets: ["NextingDeviceHostSmoke"]
        ),
    ],
    targets: [
        .target(name: "NextingDeviceKit"),
        .executableTarget(
            name: "NextingDeviceHostSmoke",
            dependencies: ["NextingDeviceKit"]
        ),
        .testTarget(name: "NextingDeviceKitTests", dependencies: ["NextingDeviceKit"]),
    ]
)
