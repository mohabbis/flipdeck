#if os(macOS)
import SwiftUI
import FlipDeckCore

struct RootView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        NavigationSplitView {
            List(selection: Binding<SidebarItem?>(get: { model.section }, set: { if let value = $0 { model.section = value } })) {
                ForEach(SidebarItem.allCases) { item in
                    Label(item.title, systemImage: item.symbol)
                        .badge(badge(for: item))
                        .tag(item)
                }
            }
            .navigationSplitViewColumnWidth(min: 170, ideal: 190, max: 240)
            .safeAreaInset(edge: .bottom) { FlipperStatusFooter().padding(10) }
        } detail: {
            Group {
                switch model.section {
                case .overview: OverviewView()
                case .projects: ProjectsView()
                case .agents: AgentsView()
                case .services: ServicesView()
                case .activity: ActivityView()
                case .flipper: FlipperView()
                case .settings: SettingsView()
                }
            }
            .navigationTitle(model.section.title)
            .toolbar {
                ToolbarItem(placement: .status) {
                    if let toast = model.toast {
                        Text(toast).font(.callout).foregroundStyle(.secondary).transition(.opacity)
                    }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button { model.refresh() } label: { Label("Refresh", systemImage: "arrow.clockwise") }
                        .help("Rescan projects, processes and integrations (⌘R)")
                }
            }
        }
    }

    private func badge(for item: SidebarItem) -> Int {
        guard let snapshot = model.snapshot else { return 0 }
        switch item {
        case .overview: return snapshot.attention.count
        case .agents: return snapshot.state.agents.count
        case .services: return snapshot.state.servers.count
        default: return 0
        }
    }
}

struct FlipperStatusFooter: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        let described = Self.describe(model.snapshot?.flipper ?? FlipperLinkStatus())
        Button { model.section = .flipper } label: {
            HStack(spacing: 6) {
                StatusDot(color: described.0)
                Text(described.1).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                Spacer()
            }
        }
        .buttonStyle(.plain)
    }

    static func describe(_ status: FlipperLinkStatus) -> (Color, String) {
        switch status.link {
        case .ready: return (.green, status.peerName ?? "Flipper connected")
        case .handshaking: return (.yellow, "Connecting…")
        case .incompatible: return (.orange, "Flipper app needs update")
        case .disconnected:
            switch status.transport {
            case .unavailable: return (.red, "Bluetooth unavailable")
            case .scanning: return (.secondary, "Looking for Flipper")
            case .connecting: return (.yellow, "Connecting…")
            default: return (.secondary, "Flipper not connected")
            }
        }
    }
}
#endif
