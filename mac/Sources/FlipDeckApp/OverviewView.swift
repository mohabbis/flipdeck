#if os(macOS)
import SwiftUI
import FlipDeckCore

struct OverviewView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        if let snapshot = model.snapshot {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    if !snapshot.attention.isEmpty {
                        AttentionList(items: snapshot.attention)
                    }
                    MachineStrip(machine: snapshot.state.machine)
                    ActiveList(state: snapshot.state)
                    SectionHeader(title: "Recent", detail: snapshot.events.isEmpty ? nil : "\(snapshot.events.count) events")
                    if snapshot.events.isEmpty {
                        Text("Nothing has happened yet. Events appear here as servers start, agents finish, commits land, and deployments complete.")
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(snapshot.events.prefix(8)) { event in
                            EventRow(event: event)
                            Divider()
                        }
                        Button("Show all activity") { model.section = .activity }
                            .buttonStyle(.link)
                    }
                }
                .padding(20)
                .frame(maxWidth: 980, alignment: .leading)
            }
        } else {
            ProgressView("Scanning…").frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }
}

struct AttentionList: View {
    @EnvironmentObject private var model: AppModel
    let items: [AttentionItem]

    var body: some View {
        SectionHeader(title: "Needs attention", detail: "\(items.count)")
        ForEach(items) { item in
            HStack(spacing: 10) {
                SeverityIcon(severity: item.severity)
                Text(item.title).fontWeight(.semibold)
                if let project = item.projectName { Text(project).foregroundStyle(.secondary) }
                Spacer()
                ActionButtons(actions: item.actions)
                if let eventID = item.eventID {
                    Button("Dismiss") { model.acknowledge(eventID) }.controlSize(.small)
                }
            }
            .padding(.vertical, 4)
            Divider()
        }
    }
}

struct MachineStrip: View {
    let machine: MachineStatus?

    var body: some View {
        SectionHeader(title: "Mac", detail: machine?.hostName)
        if let machine {
            HStack(alignment: .top, spacing: 32) {
                Metric(label: "CPU", value: machine.cpuUsage.map { "\(Int(($0 * 100).rounded()))%" } ?? "—")
                Metric(label: "Memory", value: memoryText(machine))
                if let battery = machine.battery {
                    Metric(label: "Battery", value: "\(battery.percent)%" + (battery.charging ? " ⚡︎" : battery.onAC ? " (AC)" : ""))
                }
                Metric(label: "Network", value: machine.networkReachable.map { $0 ? (machine.networkInterface ?? "Online") : "Offline" } ?? "—")
                Metric(label: "Uptime", value: elapsed(since: machine.bootTime))
            }
        } else {
            Text("Collecting…").foregroundStyle(.secondary)
        }
    }

    private func memoryText(_ machine: MachineStatus) -> String {
        guard let used = machine.memoryUsedBytes, let total = machine.memoryTotalBytes else { return "—" }
        return "\(byteString(used)) / \(byteString(total))"
    }
}

struct Metric: View {
    let label: String
    let value: String

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label).font(.caption).foregroundStyle(.secondary)
            Text(value).font(.title3.monospacedDigit())
        }
    }
}

struct ActiveList: View {
    @EnvironmentObject private var model: AppModel
    let state: EngineState

    var body: some View {
        let count = state.servers.count + state.agents.count + state.processes.count
        SectionHeader(title: "Active", detail: count == 0 ? nil : "\(count)")
        if count == 0 {
            Text(state.observed.processes ? "No dev servers, agents, builds or test runs detected." : "Scanning processes…")
                .foregroundStyle(.secondary)
        }
        ForEach(state.servers) { server in
            ActiveRow(symbol: "server.rack", color: .green, title: server.displayName,
                      detail: "localhost:\(server.primaryPort)", project: model.projectName(server.projectID),
                      since: server.startedAt, actions: ActionCatalog.actions(for: server))
        }
        ForEach(state.agents) { agent in
            ActiveRow(symbol: "sparkles", color: .purple, title: agent.providerName, detail: "running",
                      project: model.projectName(agent.projectID) ?? agent.cwd, since: agent.startedAt, actions: [])
        }
        ForEach(state.processes) { process in
            ActiveRow(symbol: process.kind == .testRunner ? "checklist" : "hammer", color: .blue, title: process.label,
                      detail: process.kind == .testRunner ? "tests" : process.kind.rawValue, project: model.projectName(process.projectID),
                      since: process.startedAt, actions: [])
        }
    }
}

struct ActiveRow: View {
    let symbol: String
    let color: Color
    let title: String
    let detail: String
    let project: String?
    let since: Date?
    let actions: [FDAction]

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: symbol).foregroundStyle(color).frame(width: 16)
            Text(title).fontWeight(.medium)
            Text(detail).font(.callout.monospaced()).foregroundStyle(.secondary)
            if let project { Text(project).foregroundStyle(.secondary) }
            Spacer()
            Text(elapsed(since: since)).font(.callout.monospacedDigit()).foregroundStyle(.secondary)
            ActionButtons(actions: actions, compact: true)
        }
        .padding(.vertical, 2)
    }
}
#endif
