// swift-tools-version:5.10
import PackageDescription

let package = Package(
    name: "FlipDeck",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "FlipDeck", targets: ["FlipDeckApp"]),
        .executable(name: "flipdeck-headless", targets: ["flipdeck-headless"]),
        .library(name: "FlipDeckCore", targets: ["FlipDeckCore"]),
    ],
    targets: [
        // Platform-independent core: models, monitoring, events, protocol, engine.
        // Builds and tests on Linux as well as macOS.
        .target(name: "FlipDeckCore"),
        // macOS-only adapters (CoreBluetooth, Keychain, IOKit, AppKit). Every file is
        // wrapped in `#if os(macOS)` so the package still builds on Linux.
        .target(name: "FlipDeckMacPlatform", dependencies: ["FlipDeckCore"]),
        .executableTarget(name: "FlipDeckApp", dependencies: ["FlipDeckCore", "FlipDeckMacPlatform"]),
        .executableTarget(name: "flipdeck-headless", dependencies: ["FlipDeckCore"]),
        .testTarget(name: "FlipDeckCoreTests", dependencies: ["FlipDeckCore"]),
    ]
)
