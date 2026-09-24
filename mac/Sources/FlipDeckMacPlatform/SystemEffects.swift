#if os(macOS)
import AppKit
import Foundation
import os
import UserNotifications
import FlipDeckCore

public struct EditorApp: Identifiable, Hashable, Sendable {
    public let bundleID: String
    public let name: String
    public var id: String { bundleID }
}

public enum Editors {
    static let known: [(String, String)] = [
        ("com.todesktop.230313mzl4w4u92", "Cursor"),
        ("com.microsoft.VSCode", "Visual Studio Code"),
        ("com.microsoft.VSCodeInsiders", "VS Code Insiders"),
        ("dev.zed.Zed", "Zed"),
        ("com.exafunction.windsurf", "Windsurf"),
        ("com.apple.dt.Xcode", "Xcode"),
        ("com.sublimetext.4", "Sublime Text"),
        ("com.panic.Nova", "Nova"),
        ("com.jetbrains.intellij", "IntelliJ IDEA"),
        ("com.jetbrains.WebStorm", "WebStorm"),
        ("com.jetbrains.pycharm", "PyCharm"),
    ]

    /// Editors that are actually installed, in preference order.
    public static func installed() -> [EditorApp] {
        known.compactMap { bundleID, name in
            NSWorkspace.shared.urlForApplication(withBundleIdentifier: bundleID) == nil ? nil : EditorApp(bundleID: bundleID, name: name)
        }
    }
}

public enum EffectError: Error, LocalizedError {
    case applicationNotFound(String)
    case signalFailed(Int32, String)

    public var errorDescription: String? {
        switch self {
        case .applicationNotFound(let id): return "Application \(id) is not installed"
        case .signalFailed(let pid, let reason): return "Couldn't signal pid \(pid): \(reason)"
        }
    }
}

public struct MacSystemEffects: SystemEffects {
    public init() {}

    public func openURL(_ url: URL) async throws {
        _ = await MainActor.run { NSWorkspace.shared.open(url) }
    }

    public func openProject(at path: String, editorBundleID: String?) async throws -> String {
        let directory = URL(fileURLWithPath: path, isDirectory: true)
        if let editorBundleID, let appURL = NSWorkspace.shared.urlForApplication(withBundleIdentifier: editorBundleID) {
            try await open(directory, with: appURL)
            return FileManager.default.displayName(atPath: appURL.path).replacingOccurrences(of: ".app", with: "")
        }
        _ = await MainActor.run { NSWorkspace.shared.open(directory) }
        return "Finder"
    }

    public func openTerminal(at path: String) async throws {
        guard let terminal = NSWorkspace.shared.urlForApplication(withBundleIdentifier: "com.apple.Terminal") else {
            throw EffectError.applicationNotFound("com.apple.Terminal")
        }
        try await open(URL(fileURLWithPath: path, isDirectory: true), with: terminal)
    }

    public func revealInFinder(_ path: String) async throws {
        await MainActor.run { NSWorkspace.shared.activateFileViewerSelecting([URL(fileURLWithPath: path)]) }
    }

    public func copyToClipboard(_ string: String) async {
        await MainActor.run {
            NSPasteboard.general.clearContents()
            NSPasteboard.general.setString(string, forType: .string)
        }
    }

    public func terminate(pid: Int32) throws {
        guard kill(pid, SIGTERM) == 0 else {
            throw EffectError.signalFailed(pid, String(cString: strerror(errno)))
        }
    }

    private func open(_ url: URL, with application: URL) async throws {
        let configuration = NSWorkspace.OpenConfiguration()
        configuration.activates = true
        _ = try await NSWorkspace.shared.open([url], withApplicationAt: application, configuration: configuration)
    }
}

/// Posts Mac notifications. UserNotifications only works from a real .app
/// bundle, so this is a no-op when running as a bare SwiftPM executable.
public final class MacUserNotifier: MacNotifier, @unchecked Sendable {
    let available: Bool

    public init() {
        available = Bundle.main.bundleURL.pathExtension == "app" && Bundle.main.bundleIdentifier != nil
        if available {
            UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { _, _ in }
        }
    }

    public func post(_ event: FDEvent) {
        guard available else { return }
        let content = UNMutableNotificationContent()
        content.title = event.title
        if let project = event.projectName { content.subtitle = project }
        content.body = event.message
        if event.severity >= .error { content.sound = .default }
        UNUserNotificationCenter.current().add(UNNotificationRequest(identifier: event.id, content: content, trigger: nil))
    }
}

/// Unified logging, viewable in Console.app under subsystem com.flipdeck.mac.
public struct OSLogSink: LogSink {
    public init() {}

    public func log(_ level: LogLevel, _ category: String, _ message: String) {
        let logger = os.Logger(subsystem: "com.flipdeck.mac", category: category)
        switch level {
        case .debug: logger.debug("\(message, privacy: .public)")
        case .info: logger.info("\(message, privacy: .public)")
        case .warning: logger.warning("\(message, privacy: .public)")
        case .error: logger.error("\(message, privacy: .public)")
        }
    }
}
#endif
