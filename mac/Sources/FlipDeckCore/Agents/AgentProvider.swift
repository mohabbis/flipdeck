import Foundation

/// What a provider can actually determine. The UI shows unsupported
/// capabilities as "not available" instead of inventing state.
public struct AgentCapabilities: OptionSet, Codable, Hashable, Sendable {
    public let rawValue: Int
    public init(rawValue: Int) { self.rawValue = rawValue }

    public static let running = AgentCapabilities(rawValue: 1 << 0)
    public static let elapsedTime = AgentCapabilities(rawValue: 1 << 1)
    public static let workingDirectory = AgentCapabilities(rawValue: 1 << 2)
    /// Distinguish completed from failed. Needs a cooperating source (e.g.
    /// hooks); process observation alone can't provide it.
    public static let exitStatus = AgentCapabilities(rawValue: 1 << 3)
    /// Detect "waiting for user input". Same constraint as `exitStatus`.
    public static let waitingState = AgentCapabilities(rawValue: 1 << 4)

    public static let processObservation: AgentCapabilities = [.running, .elapsedTime, .workingDirectory]
}

public protocol AgentProvider: Sendable {
    var id: String { get }
    var displayName: String { get }
    var capabilities: AgentCapabilities { get }
    /// Returns the processes that are *sessions* of this agent (outermost
    /// process per session; helpers and child processes excluded).
    func sessions(in records: [ProcessRecord], currentUID: UInt32) -> [ProcessRecord]
}

/// Shared logic for agents detected from the process table.
public struct ProcessAgentProvider: AgentProvider {
    public let id: String
    public let displayName: String
    public let capabilities: AgentCapabilities = .processObservation
    let matcher: @Sendable (ProcessRecord) -> Bool

    public init(id: String, displayName: String, matcher: @escaping @Sendable (ProcessRecord) -> Bool) {
        self.id = id
        self.displayName = displayName
        self.matcher = matcher
    }

    public func sessions(in records: [ProcessRecord], currentUID: UInt32) -> [ProcessRecord] {
        let matching = records.filter { $0.uid == currentUID && matcher($0) }
        let matchingPIDs = Set(matching.map(\.pid))
        let byPID = Dictionary(records.map { ($0.pid, $0) }, uniquingKeysWith: { a, _ in a })
        // Keep only the outermost process: a launcher (`node cli.js`) that
        // spawns the native binary is one session, not two.
        return matching.filter { record in
            var parent = record.ppid
            var hops = 0
            while parent > 1, hops < 16 {
                if matchingPIDs.contains(parent) { return false }
                guard let next = byPID[parent] else { break }
                parent = next.ppid
                hops += 1
            }
            return true
        }
    }
}

public enum AgentProviders {
    static func isAppBundle(_ record: ProcessRecord) -> Bool {
        record.args.contains(".app/Contents/")
    }

    public static let claudeCode = ProcessAgentProvider(id: "claude-code", displayName: "Claude Code") { record in
        guard !isAppBundle(record) else { return false }
        let args = record.args
        if args.contains("@anthropic-ai/claude-code") { return true }
        if let first = record.tokens.first, first.contains("/claude/versions/") { return true }
        // Case-sensitive: the Claude desktop app's processes are "Claude".
        return record.executableName == "claude"
    }

    public static let codex = ProcessAgentProvider(id: "codex", displayName: "Codex") { record in
        guard !isAppBundle(record) else { return false }
        if record.args.contains("@openai/codex") { return true }
        return record.executableName == "codex"
    }

    public static let all: [any AgentProvider] = [claudeCode, codex]
}
