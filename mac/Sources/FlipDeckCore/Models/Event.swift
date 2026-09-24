import Foundation

public enum Severity: String, Codable, Sendable, CaseIterable, Comparable {
    case info
    case success
    case warning
    case error
    /// Reserved for things that genuinely need the user to intervene.
    case actionRequired = "action_required"

    private var rank: Int {
        switch self {
        case .info: return 0
        case .success: return 1
        case .warning: return 2
        case .error: return 3
        case .actionRequired: return 4
        }
    }

    public static func < (lhs: Severity, rhs: Severity) -> Bool { lhs.rank < rhs.rank }

    /// Single-letter code used on the wire (FDP/1 `sev`).
    public var wireCode: String {
        switch self {
        case .info: return "i"
        case .success: return "s"
        case .warning: return "w"
        case .error: return "e"
        case .actionRequired: return "a"
        }
    }
}

public enum EventSource: String, Codable, Sendable {
    case git
    case process
    case agent
    case vercel
    case flipper
    case system
}

/// Open set of event types; the known ones are listed as statics so
/// providers and rules can't drift apart on spelling.
public struct EventType: RawRepresentable, Codable, Hashable, Sendable, ExpressibleByStringLiteral {
    public let rawValue: String
    public init(rawValue: String) { self.rawValue = rawValue }
    public init(stringLiteral value: String) { self.rawValue = value }

    public static let deploymentStarted: EventType = "deployment.started"
    public static let deploymentSucceeded: EventType = "deployment.succeeded"
    public static let deploymentFailed: EventType = "deployment.failed"
    public static let deploymentCanceled: EventType = "deployment.canceled"
    public static let testsStarted: EventType = "tests.started"
    public static let testsPassed: EventType = "tests.passed"
    public static let testsFailed: EventType = "tests.failed"
    public static let serverStarted: EventType = "server.started"
    public static let serverStopped: EventType = "server.stopped"
    public static let agentStarted: EventType = "agent.started"
    public static let agentCompleted: EventType = "agent.completed"
    public static let agentFailed: EventType = "agent.failed"
    /// The agent process ended; FlipDeck did not spawn it, so its exit status
    /// is unknowable and this is deliberately *not* "completed" or "failed".
    public static let agentExited: EventType = "agent.exited"
    public static let gitChanged: EventType = "git.changed"
    public static let integrationError: EventType = "integration.error"
    public static let actionPerformed: EventType = "action.performed"
    public static let actionFailed: EventType = "action.failed"
}

public struct FDEvent: Codable, Hashable, Identifiable, Sendable {
    public let id: String
    public let timestamp: Date
    public let source: EventSource
    public let projectID: String?
    public let projectName: String?
    public let type: EventType
    public let severity: Severity
    public let title: String
    public let message: String
    public let actions: [FDAction]
    public let metadata: [String: String]
    public var acknowledged: Bool

    public init(
        id: String = UUID().uuidString,
        timestamp: Date,
        source: EventSource,
        projectID: String? = nil,
        projectName: String? = nil,
        type: EventType,
        severity: Severity,
        title: String,
        message: String = "",
        actions: [FDAction] = [],
        metadata: [String: String] = [:],
        acknowledged: Bool = false
    ) {
        self.id = id
        self.timestamp = timestamp
        self.source = source
        self.projectID = projectID
        self.projectName = projectName
        self.type = type
        self.severity = severity
        self.title = title
        self.message = message
        self.actions = actions
        self.metadata = metadata
        self.acknowledged = acknowledged
    }
}
