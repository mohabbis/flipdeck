import Foundation

/// The complete list of things FlipDeck can do. There is intentionally no
/// "run command" kind: every action maps to one vetted, parameter-checked
/// operation, and the Flipper can only reference actions the Mac generated.
public enum ActionKind: String, Codable, Sendable, CaseIterable {
    case openOnMac = "OPEN_ON_MAC"
    case openInFinder = "OPEN_IN_FINDER"
    case openTerminal = "OPEN_TERMINAL"
    case openLocalhost = "OPEN_LOCALHOST"
    case copyURL = "COPY_URL"
    case stopServer = "STOP_SERVER"
    case openLogs = "OPEN_LOGS"
    case openDeployment = "OPEN_DEPLOYMENT"

    public var label: String {
        switch self {
        case .openOnMac: return "Open on Mac"
        case .openInFinder: return "Reveal in Finder"
        case .openTerminal: return "Open Terminal"
        case .openLocalhost: return "Open localhost"
        case .copyURL: return "Copy URL"
        case .stopServer: return "Stop server"
        case .openLogs: return "Open logs"
        case .openDeployment: return "Open deployment"
        }
    }

    public var destructive: Bool { self == .stopServer }
    public var requiresConfirmation: Bool { destructive }

    /// Whether the Flipper may request this. Clipboard/Finder actions only
    /// make sense for someone sitting at the Mac.
    public var availableRemotely: Bool {
        switch self {
        case .copyURL, .openInFinder: return false
        default: return true
        }
    }

    /// FDP/1 `ACT.kind` code; nil for Mac-only kinds.
    public var wireCode: String? {
        switch self {
        case .openOnMac: return "OPEN"
        case .openTerminal: return "TERM"
        case .openLocalhost: return "LOCAL"
        case .stopServer: return "STOP"
        case .openLogs: return "LOGS"
        case .openDeployment: return "DEPLOY"
        case .copyURL, .openInFinder: return nil
        }
    }
}

public enum ActionTarget: Codable, Hashable, Sendable {
    /// A discovered project directory.
    case project(path: String)
    /// A running process, pinned by start time so a recycled PID can never
    /// be mistaken for the process the action was generated for.
    case process(pid: Int32, startedAt: Date?, port: Int?)
    /// An https URL the Mac already knows about (deployment, logs).
    case url(String)

    var key: String {
        switch self {
        case .project(let path): return "project:\(path)"
        case .process(let pid, let startedAt, let port):
            return "process:\(pid):\(startedAt.map { String(Int($0.timeIntervalSince1970)) } ?? "-"):\(port ?? 0)"
        case .url(let url): return "url:\(url)"
        }
    }
}

public struct FDAction: Codable, Hashable, Identifiable, Sendable {
    public let kind: ActionKind
    public let target: ActionTarget
    public let source: EventSource

    public init(kind: ActionKind, target: ActionTarget, source: EventSource) {
        self.kind = kind
        self.target = target
        self.source = source
    }

    /// Deterministic: the same action on the same target always has the same id.
    public var id: String { "\(kind.rawValue)|\(target.key)" }
    public var label: String { kind.label }
    public var destructive: Bool { kind.destructive }
    public var requiresConfirmation: Bool { kind.requiresConfirmation }
}

public struct ActionResult: Codable, Hashable, Sendable {
    public let ok: Bool
    public let message: String

    public init(ok: Bool, message: String) {
        self.ok = ok
        self.message = message
    }

    public static func success(_ message: String) -> ActionResult { ActionResult(ok: true, message: message) }
    public static func failure(_ message: String) -> ActionResult { ActionResult(ok: false, message: message) }
}
