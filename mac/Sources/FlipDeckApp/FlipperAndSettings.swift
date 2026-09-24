#if os(macOS)
import AppKit
import SwiftUI
import FlipDeckCore
import FlipDeckMacPlatform

// MARK: - Flipper

struct FlipperView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        let status = model.snapshot?.flipper ?? FlipperLinkStatus()
        let settings = model.snapshot?.settings ?? FlipDeckSettings()
        Form {
            Section {
                LabeledContent("Status") {
                    let described = FlipperStatusFooter.describe(status)
                    HStack(spacing: 6) { StatusDot(color: described.0); Text(described.1) }
                }
                LabeledContent("Bluetooth", value: status.transport.label)
                if case .ready(let version) = status.link {
                    LabeledContent("FlipDeck on Flipper", value: version)
                }
                if case .incompatible(let proto) = status.link {
                    LabeledContent("Protocol") {
                        Text("Flipper speaks FDP/\(proto), this Mac speaks FDP/\(FDP.protocolVersion). Install matching versions.").foregroundStyle(.orange)
                    }
                }
                if let last = status.lastFrameAt {
                    LabeledContent("Last message", value: dayFormatter.string(from: last))
                }
                LabeledContent("Snapshot generation", value: "\(status.committedGeneration)")
                LabeledContent("Messages", value: "\(status.framesReceived) received · \(status.framesDropped) rejected · \(status.actionsHandled) actions")
            }

            Section("Connection") {
                Toggle("Connect to Flipper", isOn: Binding(
                    get: { settings.flipperEnabled },
                    set: { value in model.updateSettings { $0.flipperEnabled = value } }
                ))
                LabeledContent("Paired Flipper") {
                    HStack {
                        Text(settings.flipperPeripheralID == nil ? "None yet" : (status.peerName ?? "Remembered"))
                        Spacer()
                        if settings.flipperPeripheralID != nil {
                            Button("Forget") { model.forgetFlipper() }
                        }
                    }
                }
            }

            Section("First-time setup") {
                VStack(alignment: .leading, spacing: 6) {
                    Text("1. Install flipdeck.fap on the Flipper (Apps → Tools) and open it.")
                    Text("2. Keep the Flipper near this Mac. FlipDeck finds it as \u{201C}FlipDeck <name>\u{201D}.")
                    Text("3. On first connection macOS asks for a PIN. Enter the PIN shown on the Flipper.")
                    Text("The Flipper only mirrors state and can only request the actions listed on its screen. It can never run commands on this Mac.")
                        .foregroundStyle(.secondary)
                }
                .font(.callout)
            }
        }
        .formStyle(.grouped)
    }
}

// MARK: - Settings

struct SettingsView: View {
    @EnvironmentObject private var model: AppModel
    @State private var tokenDraft = ""

    var body: some View {
        let settings = model.snapshot?.settings ?? FlipDeckSettings()
        let vercel = model.state.integrations.first { $0.id == "vercel" }
        Form {
            Section {
                ForEach(settings.projectRoots, id: \.self) { root in
                    HStack {
                        Image(systemName: "folder")
                        Text(root).font(.body.monospaced())
                        Spacer()
                        Button(role: .destructive) {
                            model.updateSettings { $0.projectRoots.removeAll { $0 == root } }
                        } label: { Image(systemName: "minus.circle") }
                        .buttonStyle(.borderless)
                        .help("Stop scanning this folder")
                    }
                }
                Button("Add Folder…") { addFolder() }
            } header: {
                Text("Project folders")
            } footer: {
                Text("Scanned for Git repositories up to three levels deep. Dependency and build folders (node_modules, .build, dist, …) are skipped.")
            }

            Section("Open on Mac") {
                Picker("Editor", selection: Binding(
                    get: { settings.editorBundleID ?? "" },
                    set: { value in model.updateSettings { $0.editorBundleID = value.isEmpty ? nil : value } }
                )) {
                    Text("Finder").tag("")
                    ForEach(model.editors) { Text($0.name).tag($0.bundleID) }
                }
            }

            Section {
                LabeledContent("Status") {
                    Text(Self.describe(vercel, hasToken: model.snapshot?.hasVercelToken ?? false))
                }
                HStack {
                    SecureField("Vercel access token", text: $tokenDraft)
                    Button("Save") {
                        model.setVercelToken(tokenDraft)
                        tokenDraft = ""
                    }
                    .disabled(tokenDraft.trimmingCharacters(in: .whitespaces).isEmpty)
                    if model.snapshot?.hasVercelToken == true {
                        Button("Remove", role: .destructive) { model.setVercelToken(nil) }
                    }
                }
            } header: {
                Text("Vercel")
            } footer: {
                Text("Create a token at vercel.com/account/tokens. It is stored in your Keychain. Projects are matched through the .vercel/project.json that `vercel link` creates.")
            }

            Section("Alerts") {
                Toggle("Mac notifications", isOn: Binding(
                    get: { settings.macNotifications },
                    set: { value in model.updateSettings { $0.macNotifications = value } }
                ))
                Toggle("Flipper alerts", isOn: Binding(
                    get: { settings.flipperAlerts },
                    set: { value in model.updateSettings { $0.flipperAlerts = value } }
                ))
                Toggle("Allow stopping servers from the Flipper", isOn: Binding(
                    get: { settings.allowDestructiveFromFlipper },
                    set: { value in model.updateSettings { $0.allowDestructiveFromFlipper = value } }
                ))
                DisclosureGroup("Routing rules") {
                    Grid(alignment: .leading, horizontalSpacing: 14, verticalSpacing: 3) {
                        ForEach(Self.rules, id: \.0.rawValue) { rule in
                            GridRow {
                                Text(rule.0.rawValue).font(.callout.monospaced())
                                Text(Self.describe(rule.1)).font(.callout).foregroundStyle(.secondary)
                            }
                        }
                    }
                }
            }
        }
        .formStyle(.grouped)
    }

    static let rules = NotificationRules.defaultTable.sorted { $0.key.rawValue < $1.key.rawValue }.map { ($0.key, $0.value) }

    static func describe(_ route: NotificationRoute) -> String {
        var parts = ["Activity"]
        if route.contains(.flipper) { parts.append("Flipper") }
        if route.contains(.mac) { parts.append("Mac") }
        return parts.joined(separator: " + ")
    }

    static func describe(_ status: IntegrationStatus?, hasToken: Bool) -> String {
        guard hasToken else { return "No token" }
        guard let status else { return "Checking…" }
        let linked = "\(status.linkedProjects) linked project\(status.linkedProjects == 1 ? "" : "s")"
        switch status.health {
        case .notConfigured: return "No token"
        case .ok: return "Connected · \(linked)" + (status.lastSync.map { " · synced \(clockFormatter.string(from: $0))" } ?? "")
        case .unauthorized: return "Token rejected by Vercel"
        case .error(let message): return "Error: \(message)"
        }
    }

    private func addFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = true
        panel.prompt = "Add"
        guard panel.runModal() == .OK else { return }
        let home = NSHomeDirectory()
        let paths = panel.urls.map { url -> String in
            let path = url.path
            return path.hasPrefix(home) ? "~" + String(path.dropFirst(home.count)) : path
        }
        model.updateSettings { settings in
            for path in paths where !settings.projectRoots.contains(path) { settings.projectRoots.append(path) }
        }
    }
}
#endif
