import Foundation

public struct FlipDeckSettings: Codable, Equatable, Sendable {
    public var projectRoots: [String]
    /// Bundle id of the editor for "Open on Mac" (nil → Finder).
    public var editorBundleID: String?
    public var processScanInterval: TimeInterval
    public var gitRefreshInterval: TimeInterval
    public var discoveryInterval: TimeInterval
    public var vercelInterval: TimeInterval
    public var flipperEnabled: Bool
    /// CoreBluetooth identifier of the paired Flipper, once known.
    public var flipperPeripheralID: String?
    public var flipperAlerts: Bool
    public var macNotifications: Bool
    public var allowDestructiveFromFlipper: Bool

    public static let suggestedRoots = ["~/Developer", "~/Projects", "~/Documents/GitHub", "~/code", "~/src"]

    public init(
        projectRoots: [String] = [],
        editorBundleID: String? = nil,
        processScanInterval: TimeInterval = 4,
        gitRefreshInterval: TimeInterval = 20,
        discoveryInterval: TimeInterval = 600,
        vercelInterval: TimeInterval = 30,
        flipperEnabled: Bool = true,
        flipperPeripheralID: String? = nil,
        flipperAlerts: Bool = true,
        macNotifications: Bool = true,
        allowDestructiveFromFlipper: Bool = true
    ) {
        self.projectRoots = projectRoots
        self.editorBundleID = editorBundleID
        self.processScanInterval = processScanInterval
        self.gitRefreshInterval = gitRefreshInterval
        self.discoveryInterval = discoveryInterval
        self.vercelInterval = vercelInterval
        self.flipperEnabled = flipperEnabled
        self.flipperPeripheralID = flipperPeripheralID
        self.flipperAlerts = flipperAlerts
        self.macNotifications = macNotifications
        self.allowDestructiveFromFlipper = allowDestructiveFromFlipper
    }

    /// First-run defaults: the conventional project folders that exist.
    public static func firstRun(fileManager: FileManager = .default) -> FlipDeckSettings {
        let roots = suggestedRoots.filter { fileManager.fileExists(atPath: ProjectScanner.standardize($0)) }
        return FlipDeckSettings(projectRoots: roots)
    }

    // Tolerant decoding: missing keys (older files) fall back to defaults.
    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        let d = FlipDeckSettings()
        projectRoots = try c.decodeIfPresent([String].self, forKey: .projectRoots) ?? d.projectRoots
        editorBundleID = try c.decodeIfPresent(String.self, forKey: .editorBundleID)
        processScanInterval = try c.decodeIfPresent(TimeInterval.self, forKey: .processScanInterval) ?? d.processScanInterval
        gitRefreshInterval = try c.decodeIfPresent(TimeInterval.self, forKey: .gitRefreshInterval) ?? d.gitRefreshInterval
        discoveryInterval = try c.decodeIfPresent(TimeInterval.self, forKey: .discoveryInterval) ?? d.discoveryInterval
        vercelInterval = try c.decodeIfPresent(TimeInterval.self, forKey: .vercelInterval) ?? d.vercelInterval
        flipperEnabled = try c.decodeIfPresent(Bool.self, forKey: .flipperEnabled) ?? d.flipperEnabled
        flipperPeripheralID = try c.decodeIfPresent(String.self, forKey: .flipperPeripheralID)
        flipperAlerts = try c.decodeIfPresent(Bool.self, forKey: .flipperAlerts) ?? d.flipperAlerts
        macNotifications = try c.decodeIfPresent(Bool.self, forKey: .macNotifications) ?? d.macNotifications
        allowDestructiveFromFlipper = try c.decodeIfPresent(Bool.self, forKey: .allowDestructiveFromFlipper) ?? d.allowDestructiveFromFlipper
    }
}

public enum AppPaths {
    /// ~/Library/Application Support/FlipDeck on macOS, ~/.local/share/FlipDeck on Linux.
    public static func supportDirectory(fileManager: FileManager = .default) -> URL {
        let base = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent(".flipdeck")
        return base.appendingPathComponent("FlipDeck", isDirectory: true)
    }
}

/// JSON-file-backed settings. Never stores secrets (those go to `SecretStore`).
public final class SettingsStore: @unchecked Sendable {
    private let url: URL?
    private let lock = NSLock()
    private var cached: FlipDeckSettings

    public init(url: URL?, fileManager: FileManager = .default) {
        self.url = url
        if let url, let data = try? Data(contentsOf: url), let decoded = try? JSONDecoder().decode(FlipDeckSettings.self, from: data) {
            cached = decoded
        } else {
            cached = FlipDeckSettings.firstRun(fileManager: fileManager)
        }
    }

    public var settings: FlipDeckSettings {
        lock.lock()
        defer { lock.unlock() }
        return cached
    }

    public func save(_ settings: FlipDeckSettings) throws {
        lock.lock()
        cached = settings
        lock.unlock()
        guard let url else { return }
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try encoder.encode(settings).write(to: url, options: .atomic)
    }
}
