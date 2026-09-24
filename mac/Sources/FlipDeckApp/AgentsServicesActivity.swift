#if os(macOS)
import SwiftUI
import FlipDeckCore

// MARK: - Agents

struct AgentsView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        let state = model.state
        let agentEvents = (model.snapshot?.events ?? []).filter { $0.source == .agent }
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                SectionHeader(title: "Running", detail: state.agents.isEmpty ? nil : "\(state.agents.count)")
                if state.agents.isEmpty {
                    Text("No coding agents running.").foregroundStyle(.secondary)
                } else {
                    ForEach(state.agents) { agent in
                        HStack(spacing: 10) {
                            Image(systemName: "sparkles").foregroundStyle(.purple).frame(width: 16)
                            Text(agent.providerName).fontWeight(.medium)
                            Text(model.projectName(agent.projectID) ?? agent.cwd ?? "unknown directory").foregroundStyle(.secondary)
                            Spacer()
                            Text("pid \(agent.pid)").font(.callout.monospacedDigit()).foregroundStyle(.tertiary)
                            Text(elapsed(since: agent.startedAt)).font(.callout.monospacedDigit()).foregroundStyle(.secondary)
                            if let project = state.project(id: agent.projectID) {
                                ActionButtons(actions: [FDAction(kind: .openOnMac, target: .project(path: project.path), source: .agent)], compact: true)
                            }
                        }
                        Divider()
                    }
                }

                SectionHeader(title: "What FlipDeck can tell")
                Grid(alignment: .leading, horizontalSpacing: 18, verticalSpacing: 6) {
                    GridRow {
                        Text("")
                        ForEach(Self.capabilityColumns, id: \.1) { Text($0.1).font(.caption).foregroundStyle(.secondary) }
                    }
                    ForEach(model.snapshot?.agentProviders ?? []) { provider in
                        GridRow {
                            Text(provider.displayName)
                            ForEach(Self.capabilityColumns, id: \.1) { column in
                                let supported = provider.capabilities.contains(column.0)
                                Image(systemName: supported ? "checkmark" : "minus")
                                    .foregroundStyle(supported ? Color.green : Color.secondary)
                            }
                        }
                    }
                }
                Text("Agents are detected from the process table. That reliably shows that an agent is running, for how long, and in which project. It cannot show whether a session finished successfully, failed, or is waiting for input, so FlipDeck reports an agent that stops as \"finished\" without guessing the outcome.")
                    .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)

                SectionHeader(title: "History")
                if agentEvents.isEmpty {
                    Text("No agent activity yet.").foregroundStyle(.secondary)
                }
                ForEach(agentEvents.prefix(20)) { event in
                    EventRow(event: event)
                    Divider()
                }
            }
            .padding(20)
            .frame(maxWidth: 980, alignment: .leading)
        }
    }

    static let capabilityColumns: [(AgentCapabilities, String)] = [
        (.running, "Running"), (.elapsedTime, "Elapsed"), (.workingDirectory, "Project"),
        (.exitStatus, "Success/failure"), (.waitingState, "Waiting for input"),
    ]
}

// MARK: - Services

struct ServicesView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        let state = model.state
        if state.servers.isEmpty && state.processes.isEmpty {
            EmptyState(
                title: state.observed.processes ? "No dev servers running" : "Scanning…",
                symbol: "server.rack",
                message: "FlipDeck lists processes you own that listen on a TCP port (1024 and up), if they're a known dev runtime or run from inside one of your projects."
            )
        } else {
            VStack(alignment: .leading, spacing: 0) {
                Table(state.servers) {
                    TableColumn("Server") { server in
                        HStack(spacing: 6) {
                            StatusDot(color: .green)
                            Text(server.displayName).fontWeight(.medium)
                        }
                    }
                    TableColumn("URL") { server in
                        Text(server.ports.map { "localhost:\($0)" }.joined(separator: ", ")).font(.body.monospaced())
                    }
                    TableColumn("Project") { server in
                        Text(model.projectName(server.projectID) ?? server.cwd ?? "—").foregroundStyle(.secondary)
                    }
                    TableColumn("PID") { server in
                        Text("\(server.pid)").monospacedDigit().foregroundStyle(.secondary)
                    }
                    .width(60)
                    TableColumn("Up") { server in
                        Text(elapsed(since: server.startedAt)).monospacedDigit()
                    }
                    .width(70)
                    TableColumn("") { server in
                        ActionButtons(actions: ActionCatalog.actions(for: server), compact: true)
                    }
                    .width(110)
                }
                if !state.processes.isEmpty {
                    Divider()
                    VStack(alignment: .leading, spacing: 4) {
                        SectionHeader(title: "Builds, tests and tools", detail: "\(state.processes.count)")
                        ForEach(state.processes) { process in
                            ActiveRow(symbol: process.kind == .testRunner ? "checklist" : "hammer", color: .blue, title: process.label,
                                      detail: "pid \(process.pid)", project: model.projectName(process.projectID), since: process.startedAt, actions: [])
                        }
                    }
                    .padding(16)
                }
            }
        }
    }
}

// MARK: - Activity

struct ActivityView: View {
    @EnvironmentObject private var model: AppModel
    @State private var filter: Filter = .all

    enum Filter: String, CaseIterable, Identifiable {
        case all = "All"
        case important = "Warnings & errors"
        case deployments = "Deployments"
        case servers = "Servers"
        case agents = "Agents"
        case git = "Git"
        case flipper = "Flipper"

        var id: String { rawValue }

        func matches(_ event: FDEvent) -> Bool {
            switch self {
            case .all: return true
            case .important: return event.severity >= .warning
            case .deployments: return event.type.rawValue.hasPrefix("deployment.")
            case .servers: return event.type.rawValue.hasPrefix("server.")
            case .agents: return event.source == .agent
            case .git: return event.source == .git
            case .flipper: return event.source == .flipper
            }
        }
    }

    var body: some View {
        let events = (model.snapshot?.events ?? []).filter(filter.matches)
        VStack(spacing: 0) {
            HStack {
                Picker("Show", selection: $filter) {
                    ForEach(Filter.allCases) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.menu)
                .fixedSize()
                Spacer()
                Button("Mark all as seen") { model.acknowledgeAll() }
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 8)
            Divider()
            if events.isEmpty {
                EmptyState(title: "No activity", symbol: "clock", message: "Server starts and stops, agent sessions, commits, branch switches and deployments are recorded here.")
            } else {
                List(events) { event in
                    EventRow(event: event)
                }
                .listStyle(.inset)
            }
        }
    }
}
#endif
