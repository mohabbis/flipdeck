import Foundation

/// Record-count and field-length limits shared with the Flipper app
/// (flipper/fd_state.h). Changing any of these is a protocol change.
public enum FlipperLimits {
    public static let projects = 12
    public static let services = 8
    public static let agents = 6
    public static let attention = 6
    public static let events = 10
    public static let actions = 64

    public static let id = 7
    public static let name = 20
    public static let branch = 24
    public static let host = 20
    public static let provider = 12
    public static let title = 40
    public static let eventTitle = 48
    public static let message = 80
    public static let label = 20
    public static let resultMessage = 60
}

public struct FlipperSnapshot: Equatable, Sendable {
    /// Records between SNAP and END, in order.
    public let records: [Frame]
    /// Wire action id → action, for resolving REQ frames.
    public let actions: [String: FDAction]
    /// Event id → owner id used on the wire (for ALR and SEEN).
    public let eventOwners: [String: String]

    public static let empty = FlipperSnapshot(records: [], actions: [:], eventOwners: [:])
}

public enum FlipperIDs {
    public static func project(_ project: Project) -> String { StableID.short("p", project.id) }
    public static func server(_ server: DevServer) -> String {
        StableID.short("s", "\(server.pid):\(Int(server.startedAt?.timeIntervalSince1970 ?? 0))")
    }
    public static func agent(_ agent: AgentSession) -> String { StableID.short("g", agent.id) }
    public static func attention(_ item: AttentionItem) -> String { StableID.short("t", item.id) }
    public static func event(_ eventID: String) -> String { StableID.short("e", eventID) }
    public static func action(owner: String, _ action: FDAction) -> String { StableID.short("a", owner + "|" + action.id) }
}

public enum FlipperSnapshotBuilder {
    static func s(_ text: String?, _ max: Int) -> String { FrameCodec.sanitize(text ?? "", maxLength: max) }
    static func unix(_ date: Date?) -> String { date.map { String(Int($0.timeIntervalSince1970)) } ?? "-" }

    static let timeFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "HH:mm"
        return formatter
    }()

    public static func build(state: EngineState, attention: [AttentionItem], events: [FDEvent], timeZone: TimeZone = .current) -> FlipperSnapshot {
        var records: [Frame] = []
        var actionTable: [String: FDAction] = [:]
        var eventOwners: [String: String] = [:]
        var actionRecords: [Frame] = []

        func addActions(owner: String, _ actions: [FDAction]) {
            for action in actions {
                guard let code = action.kind.wireCode, actionRecords.count < FlipperLimits.actions else { continue }
                let wireID = FlipperIDs.action(owner: owner, action)
                guard actionTable[wireID] == nil else { continue }
                actionTable[wireID] = action
                let flags = (action.destructive ? "d" : "") + (action.requiresConfirmation ? "c" : "")
                actionRecords.append(Frame("ACT", [owner, wireID, code, flags, ""]))
            }
        }

        let attentionProjects = Set(attention.compactMap(\.projectID))
        let projectOrder = orderedProjects(state: state, attentionProjects: attentionProjects)

        records.append(Frame("SUM", [
            String(attention.count), String(state.projects.count), String(state.servers.count), String(state.agents.count),
        ]))

        // Attention first: its actions are the most important ones to keep
        // within the action budget.
        for item in attention.prefix(FlipperLimits.attention) {
            let owner = FlipperIDs.attention(item)
            if let eventID = item.eventID { eventOwners[eventID] = owner }
            records.append(Frame("ATN", [owner, item.severity.wireCode, s(item.projectName, FlipperLimits.name), s(item.title, FlipperLimits.title)]))
            addActions(owner: owner, item.actions)
        }

        for project in projectOrder.prefix(FlipperLimits.projects) {
            let owner = FlipperIDs.project(project)
            let git: String
            if !project.isGitRepository { git = "n" }
            else if let status = project.git { git = status.isDirty ? "d" : "c" }
            else { git = "-" }
            let server = state.servers(for: project.id).first
            let deployment = state.latestDeployment(for: project.id)
            records.append(Frame("PRJ", [
                owner,
                s(project.name, FlipperLimits.name),
                s(project.git?.branch ?? (project.git != nil ? "detached" : ""), FlipperLimits.branch),
                git,
                project.git?.ahead.map(String.init) ?? "-",
                project.git?.behind.map(String.init) ?? "-",
                String(server?.primaryPort ?? 0),
                deployment?.state.wireCode ?? "-",
                attentionProjects.contains(project.id) ? "1" : "0",
            ]))
            var projectActions = ActionCatalog.actions(for: project)
            if let deployment { projectActions += ActionCatalog.actions(for: deployment) }
            addActions(owner: owner, projectActions)
        }

        for server in state.servers.prefix(FlipperLimits.services) {
            let owner = FlipperIDs.server(server)
            records.append(Frame("SVC", [
                owner, s(server.displayName, FlipperLimits.name), String(server.primaryPort),
                s(state.project(id: server.projectID)?.name, FlipperLimits.name), unix(server.startedAt),
            ]))
            addActions(owner: owner, ActionCatalog.actions(for: server))
        }

        for agent in state.agents.prefix(FlipperLimits.agents) {
            let owner = FlipperIDs.agent(agent)
            records.append(Frame("AGT", [
                owner, s(agent.providerName, FlipperLimits.provider), s(state.project(id: agent.projectID)?.name, FlipperLimits.name),
                agent.state.wireCode, unix(agent.startedAt),
            ]))
            if let project = state.project(id: agent.projectID) {
                addActions(owner: owner, [FDAction(kind: .openOnMac, target: .project(path: project.path), source: .agent)])
            }
        }

        let formatter = timeFormatter
        formatter.timeZone = timeZone
        for event in events.prefix(FlipperLimits.events) {
            let owner = eventOwners[event.id] ?? FlipperIDs.event(event.id)
            eventOwners[event.id] = owner
            records.append(Frame("EVT", [
                owner, event.severity.wireCode, formatter.string(from: event.timestamp),
                s(event.projectName, FlipperLimits.name), s(event.title, FlipperLimits.eventTitle),
            ]))
            addActions(owner: owner, event.actions)
        }

        return FlipperSnapshot(records: records + actionRecords, actions: actionTable, eventOwners: eventOwners)
    }

    /// Attention first, then anything active, then most recently committed.
    static func orderedProjects(state: EngineState, attentionProjects: Set<String>) -> [Project] {
        let active = Set(state.servers.compactMap(\.projectID) + state.agents.compactMap(\.projectID) + state.processes.compactMap(\.projectID))
        func rank(_ project: Project) -> Int {
            if attentionProjects.contains(project.id) { return 0 }
            if active.contains(project.id) { return 1 }
            if state.latestDeployment(for: project.id)?.state.isInProgress == true { return 1 }
            return 2
        }
        return state.projects.sorted { a, b in
            let (ra, rb) = (rank(a), rank(b))
            if ra != rb { return ra < rb }
            let da = a.git?.lastCommit?.date ?? .distantPast
            let db = b.git?.lastCommit?.date ?? .distantPast
            if da != db { return da > db }
            return a.name.localizedCaseInsensitiveCompare(b.name) == .orderedAscending
        }
    }

    public static func machineFrame(_ machine: MachineStatus) -> Frame {
        func percent(_ value: Double?) -> String {
            // Quantized to 5% so jitter doesn't cause constant BLE traffic.
            guard let value else { return "-" }
            return String(min(100, max(0, Int((value * 20).rounded()) * 5)))
        }
        return Frame("MAC", [
            s(machine.hostName, FlipperLimits.host),
            percent(machine.cpuUsage),
            percent(machine.memoryUsage),
            machine.battery.map { String($0.percent) } ?? "-",
            machine.battery.map { $0.charging ? "1" : "0" } ?? "-",
            machine.networkReachable.map { $0 ? "1" : "0" } ?? "-",
            unix(machine.bootTime),
        ])
    }
}
