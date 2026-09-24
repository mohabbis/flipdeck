#if os(macOS)
import SwiftUI
import FlipDeckCore

extension Severity {
    var color: Color {
        switch self {
        case .info: return .secondary
        case .success: return .green
        case .warning: return .orange
        case .error: return .red
        case .actionRequired: return .purple
        }
    }

    var symbol: String {
        switch self {
        case .info: return "circle.fill"
        case .success: return "checkmark.circle.fill"
        case .warning: return "exclamationmark.triangle.fill"
        case .error: return "xmark.octagon.fill"
        case .actionRequired: return "hand.raised.fill"
        }
    }
}

extension DeploymentState {
    var color: Color {
        switch self {
        case .queued, .building: return .blue
        case .ready: return .green
        case .error: return .red
        case .canceled: return .secondary
        }
    }

    var label: String {
        switch self {
        case .queued: return "Queued"
        case .building: return "Building"
        case .ready: return "Live"
        case .error: return "Failed"
        case .canceled: return "Canceled"
        }
    }
}

struct StatusDot: View {
    let color: Color
    var body: some View {
        Circle().fill(color).frame(width: 7, height: 7)
    }
}

struct SeverityIcon: View {
    let severity: Severity
    var body: some View {
        Image(systemName: severity.symbol)
            .foregroundStyle(severity.color)
            .imageScale(severity == .info ? .small : .medium)
            .frame(width: 16)
    }
}

/// Small uppercase header used instead of boxed cards.
struct SectionHeader: View {
    let title: String
    var detail: String?

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title.uppercased())
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)
            if let detail {
                Text(detail).font(.caption).foregroundStyle(.tertiary)
            }
            Spacer()
        }
        .padding(.top, 6)
    }
}

/// Buttons for a set of actions. Destructive ones ask first.
struct ActionButtons: View {
    @EnvironmentObject private var model: AppModel
    let actions: [FDAction]
    var compact = false
    @State private var pending: FDAction?

    var body: some View {
        HStack(spacing: 6) {
            ForEach(actions) { action in
                Button(role: action.destructive ? .destructive : nil) {
                    if action.requiresConfirmation { pending = action } else { model.perform(action) }
                } label: {
                    if compact {
                        Image(systemName: Self.symbol(for: action.kind))
                    } else {
                        Text(action.label)
                    }
                }
                .help(action.label)
                .controlSize(.small)
            }
        }
        .confirmationDialog(
            pending.map { "\($0.label)?" } ?? "",
            isPresented: Binding(get: { pending != nil }, set: { if !$0 { pending = nil } }),
            presenting: pending
        ) { action in
            Button(action.label, role: .destructive) { model.perform(action) }
            Button("Cancel", role: .cancel) {}
        } message: { action in
            Text(Self.confirmationMessage(for: action))
        }
    }

    static func confirmationMessage(for action: FDAction) -> String {
        if case .process(let pid, _, _) = action.target {
            return "Sends SIGTERM to process \(pid). Unsaved work in that process may be lost."
        }
        return "This can't be undone."
    }

    static func symbol(for kind: ActionKind) -> String {
        switch kind {
        case .openOnMac: return "chevron.left.forwardslash.chevron.right"
        case .openInFinder: return "folder"
        case .openTerminal: return "terminal"
        case .openLocalhost: return "safari"
        case .copyURL: return "doc.on.doc"
        case .stopServer: return "stop.circle"
        case .openLogs: return "doc.text.magnifyingglass"
        case .openDeployment: return "arrow.up.right.square"
        }
    }
}

struct EmptyState: View {
    let title: String
    let symbol: String
    let message: String

    var body: some View {
        ContentUnavailableView(title, systemImage: symbol, description: Text(message))
    }
}

func elapsed(since date: Date?) -> String {
    guard let date else { return "—" }
    return Format.duration(Date().timeIntervalSince(date))
}

let clockFormatter: DateFormatter = {
    let formatter = DateFormatter()
    formatter.timeStyle = .short
    formatter.dateStyle = .none
    return formatter
}()

let dayFormatter: DateFormatter = {
    let formatter = DateFormatter()
    formatter.dateStyle = .medium
    formatter.timeStyle = .short
    formatter.doesRelativeDateFormatting = true
    return formatter
}()

func byteString(_ bytes: UInt64) -> String {
    ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .memory)
}

/// One row of the activity feed.
struct EventRow: View {
    @EnvironmentObject private var model: AppModel
    let event: FDEvent
    var showActions = true

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Text(clockFormatter.string(from: event.timestamp))
                .font(.callout.monospacedDigit())
                .foregroundStyle(.secondary)
                .frame(width: 64, alignment: .trailing)
            SeverityIcon(severity: event.severity)
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 6) {
                    Text(event.title).fontWeight(event.acknowledged || event.severity < .warning ? .regular : .semibold)
                    if let project = event.projectName {
                        Text(project).foregroundStyle(.secondary)
                    }
                }
                if !event.message.isEmpty {
                    Text(event.message).font(.callout).foregroundStyle(.secondary).lineLimit(2)
                }
            }
            Spacer(minLength: 8)
            if showActions {
                ActionButtons(actions: event.actions, compact: true)
                if event.severity >= .error && !event.acknowledged {
                    Button("Dismiss") { model.acknowledge(event.id) }.controlSize(.small)
                }
            }
        }
        .padding(.vertical, 2)
    }
}
#endif
