import Foundation

// MARK: - Projects

public struct CommitInfo: Codable, Hashable, Sendable {
    public let oid: String
    public let subject: String
    public let date: Date

    public init(oid: String, subject: String, date: Date) {
        self.oid = oid
        self.subject = subject
        self.date = date
    }
}

public struct GitStatus: Codable, Hashable, Sendable {
    /// nil when HEAD is detached.
    public var branch: String?
    public var headOID: String?
    public var upstream: String?
    /// nil when there is no upstream to compare against.
    public var ahead: Int?
    public var behind: Int?
    public var changedCount: Int
    public var untrackedCount: Int
    public var conflictedCount: Int
    public var lastCommit: CommitInfo?
    public var remoteURL: String?

    public init(
        branch: String? = nil, headOID: String? = nil, upstream: String? = nil,
        ahead: Int? = nil, behind: Int? = nil,
        changedCount: Int = 0, untrackedCount: Int = 0, conflictedCount: Int = 0,
        lastCommit: CommitInfo? = nil, remoteURL: String? = nil
    ) {
        self.branch = branch
        self.headOID = headOID
        self.upstream = upstream
        self.ahead = ahead
        self.behind = behind
        self.changedCount = changedCount
        self.untrackedCount = untrackedCount
        self.conflictedCount = conflictedCount
        self.lastCommit = lastCommit
        self.remoteURL = remoteURL
    }

    public var isDirty: Bool { changedCount + untrackedCount + conflictedCount > 0 }
}

public struct ProjectRuntime: Codable, Hashable, Sendable {
    public var framework: String?
    public var runtime: String?
    public var packageManager: String?

    public init(framework: String? = nil, runtime: String? = nil, packageManager: String? = nil) {
        self.framework = framework
        self.runtime = runtime
        self.packageManager = packageManager
    }
}

public struct VercelLink: Codable, Hashable, Sendable {
    public let projectID: String
    public let orgID: String
    public let projectName: String?

    public init(projectID: String, orgID: String, projectName: String? = nil) {
        self.projectID = projectID
        self.orgID = orgID
        self.projectName = projectName
    }
}

public struct Project: Codable, Hashable, Identifiable, Sendable {
    /// Absolute, standardized path. Stable across launches.
    public let id: String
    public var name: String
    public var isGitRepository: Bool
    public var git: GitStatus?
    /// Set when `git` couldn't be read (timeout, corrupt repo, ...).
    public var gitError: String?
    public var runtime: ProjectRuntime
    public var vercel: VercelLink?

    public init(
        path: String, name: String, isGitRepository: Bool,
        git: GitStatus? = nil, gitError: String? = nil,
        runtime: ProjectRuntime = ProjectRuntime(), vercel: VercelLink? = nil
    ) {
        self.id = path
        self.name = name
        self.isGitRepository = isGitRepository
        self.git = git
        self.gitError = gitError
        self.runtime = runtime
        self.vercel = vercel
    }

    public var path: String { id }
}

// MARK: - Processes

public enum DevProcessKind: String, Codable, Sendable {
    case devServer
    case testRunner
    case build
    case packageManager
    case container
    case agent
}

/// A developer-relevant process (not every process on the machine).
public struct DevProcess: Codable, Hashable, Identifiable, Sendable {
    public let pid: Int32
    public let kind: DevProcessKind
    /// Short human label, e.g. "vitest", "next build".
    public let label: String
    public let command: String
    public let startedAt: Date?
    public var cwd: String?
    public var projectID: String?

    public var id: Int32 { pid }

    public init(pid: Int32, kind: DevProcessKind, label: String, command: String, startedAt: Date?, cwd: String? = nil, projectID: String? = nil) {
        self.pid = pid
        self.kind = kind
        self.label = label
        self.command = command
        self.startedAt = startedAt
        self.cwd = cwd
        self.projectID = projectID
    }
}

public struct DevServer: Codable, Hashable, Identifiable, Sendable {
    public let pid: Int32
    /// Sorted ascending; `ports[0]` is the primary port.
    public let ports: [Int]
    public let processName: String
    public let command: String
    public let framework: String?
    public let startedAt: Date?
    public var cwd: String?
    public var projectID: String?

    public init(pid: Int32, ports: [Int], processName: String, command: String, framework: String?, startedAt: Date?, cwd: String? = nil, projectID: String? = nil) {
        self.pid = pid
        self.ports = ports.sorted()
        self.processName = processName
        self.command = command
        self.framework = framework
        self.startedAt = startedAt
        self.cwd = cwd
        self.projectID = projectID
    }

    public var id: String { "\(pid)" }
    public var primaryPort: Int { ports.first ?? 0 }
    public var url: String { "http://localhost:\(primaryPort)" }
    public var displayName: String { framework ?? processName }
}

// MARK: - Agents

public enum AgentState: String, Codable, Sendable {
    case running
    case waiting
    case completed
    case failed
    /// Process gone, outcome unknown.
    case exited

    public var wireCode: String {
        switch self {
        case .running: return "r"
        case .waiting: return "w"
        case .completed: return "c"
        case .failed: return "f"
        case .exited: return "x"
        }
    }
}

public struct AgentSession: Codable, Hashable, Identifiable, Sendable {
    public let providerID: String
    public let providerName: String
    public let pid: Int32
    public let startedAt: Date?
    public var cwd: String?
    public var projectID: String?
    public var state: AgentState
    public let command: String

    public init(providerID: String, providerName: String, pid: Int32, startedAt: Date?, cwd: String? = nil, projectID: String? = nil, state: AgentState = .running, command: String) {
        self.providerID = providerID
        self.providerName = providerName
        self.pid = pid
        self.startedAt = startedAt
        self.cwd = cwd
        self.projectID = projectID
        self.state = state
        self.command = command
    }

    public var id: String { "\(providerID):\(pid)" }
}

// MARK: - Deployments

public enum DeploymentState: String, Codable, Sendable {
    case queued
    case building
    case ready
    case error
    case canceled

    public var wireCode: String {
        switch self {
        case .queued: return "q"
        case .building: return "b"
        case .ready: return "r"
        case .error: return "e"
        case .canceled: return "x"
        }
    }

    public var isInProgress: Bool { self == .queued || self == .building }
}

public struct Deployment: Codable, Hashable, Identifiable, Sendable {
    public let id: String
    public let provider: String
    public let projectID: String
    public var state: DeploymentState
    /// "production", "preview", ...
    public let target: String?
    public let url: String?
    public let inspectorURL: String?
    public let createdAt: Date
    public let branch: String?
    public let commitMessage: String?
    public var errorMessage: String?

    public init(id: String, provider: String, projectID: String, state: DeploymentState, target: String?, url: String?, inspectorURL: String?, createdAt: Date, branch: String?, commitMessage: String?, errorMessage: String? = nil) {
        self.id = id
        self.provider = provider
        self.projectID = projectID
        self.state = state
        self.target = target
        self.url = url
        self.inspectorURL = inspectorURL
        self.createdAt = createdAt
        self.branch = branch
        self.commitMessage = commitMessage
        self.errorMessage = errorMessage
    }

    public var isProduction: Bool { target == "production" }
    public var targetLabel: String { (target ?? "preview").capitalized }
}

// MARK: - Machine

public struct BatteryStatus: Codable, Hashable, Sendable {
    public let percent: Int
    public let charging: Bool
    public let onAC: Bool

    public init(percent: Int, charging: Bool, onAC: Bool) {
        self.percent = percent
        self.charging = charging
        self.onAC = onAC
    }
}

public struct MachineStatus: Codable, Hashable, Sendable {
    public var hostName: String
    public var osVersion: String
    /// 0...1, nil until two samples exist.
    public var cpuUsage: Double?
    public var memoryUsedBytes: UInt64?
    public var memoryTotalBytes: UInt64?
    /// nil on desktops without a battery.
    public var battery: BatteryStatus?
    public var networkReachable: Bool?
    public var networkInterface: String?
    public var bootTime: Date?

    public init(hostName: String, osVersion: String, cpuUsage: Double? = nil, memoryUsedBytes: UInt64? = nil, memoryTotalBytes: UInt64? = nil, battery: BatteryStatus? = nil, networkReachable: Bool? = nil, networkInterface: String? = nil, bootTime: Date? = nil) {
        self.hostName = hostName
        self.osVersion = osVersion
        self.cpuUsage = cpuUsage
        self.memoryUsedBytes = memoryUsedBytes
        self.memoryTotalBytes = memoryTotalBytes
        self.battery = battery
        self.networkReachable = networkReachable
        self.networkInterface = networkInterface
        self.bootTime = bootTime
    }

    public var memoryUsage: Double? {
        guard let used = memoryUsedBytes, let total = memoryTotalBytes, total > 0 else { return nil }
        return Double(used) / Double(total)
    }
}

// MARK: - Integrations

public enum IntegrationHealth: Codable, Hashable, Sendable {
    case notConfigured
    case ok
    case unauthorized
    case error(String)
}

public struct IntegrationStatus: Codable, Hashable, Sendable, Identifiable {
    public let id: String
    public let displayName: String
    public var health: IntegrationHealth
    public var lastSync: Date?
    public var linkedProjects: Int

    public init(id: String, displayName: String, health: IntegrationHealth, lastSync: Date? = nil, linkedProjects: Int = 0) {
        self.id = id
        self.displayName = displayName
        self.health = health
        self.lastSync = lastSync
        self.linkedProjects = linkedProjects
    }
}

// MARK: - Attention

public struct AttentionItem: Codable, Hashable, Identifiable, Sendable {
    public let id: String
    public let severity: Severity
    public let projectID: String?
    public let projectName: String?
    public let title: String
    public let eventID: String?
    public let actions: [FDAction]

    public init(id: String, severity: Severity, projectID: String?, projectName: String?, title: String, eventID: String?, actions: [FDAction]) {
        self.id = id
        self.severity = severity
        self.projectID = projectID
        self.projectName = projectName
        self.title = title
        self.eventID = eventID
        self.actions = actions
    }
}

// MARK: - Engine state

/// Which providers have produced at least one observation. Events are only
/// derived from a section once it has a baseline, so launching FlipDeck
/// doesn't report every already-running server as "started".
public struct ObservedSections: Codable, Hashable, Sendable {
    public var processes = false
    public var deploymentsByProject: Set<String> = []

    public init() {}
}

public struct EngineState: Codable, Hashable, Sendable {
    public var machine: MachineStatus?
    public var projects: [Project] = []
    public var servers: [DevServer] = []
    public var agents: [AgentSession] = []
    public var processes: [DevProcess] = []
    /// Recent deployments per project id, newest first.
    public var deployments: [String: [Deployment]] = [:]
    public var integrations: [IntegrationStatus] = []
    public var observed = ObservedSections()
    public var lastProcessScan: Date?
    public var lastProjectScan: Date?

    public init() {}

    public func project(id: String?) -> Project? {
        guard let id else { return nil }
        return projects.first { $0.id == id }
    }

    public func latestDeployment(for projectID: String) -> Deployment? {
        deployments[projectID]?.first
    }

    public func servers(for projectID: String) -> [DevServer] {
        servers.filter { $0.projectID == projectID }
    }

    public func agents(for projectID: String) -> [AgentSession] {
        agents.filter { $0.projectID == projectID }
    }
}
