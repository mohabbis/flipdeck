#if os(macOS)
import Foundation
import SwiftUI
import FlipDeckCore
import FlipDeckMacPlatform

enum SidebarItem: String, CaseIterable, Identifiable, Hashable {
    case overview, projects, agents, services, activity, flipper, settings

    var id: String { rawValue }

    var title: String {
        switch self {
        case .overview: return "Overview"
        case .projects: return "Projects"
        case .agents: return "Agents"
        case .services: return "Services"
        case .activity: return "Activity"
        case .flipper: return "Flipper"
        case .settings: return "Settings"
        }
    }

    var symbol: String {
        switch self {
        case .overview: return "gauge.with.dots.needle.33percent"
        case .projects: return "folder"
        case .agents: return "sparkles"
        case .services: return "server.rack"
        case .activity: return "clock.arrow.circlepath"
        case .flipper: return "dot.radiowaves.left.and.right"
        case .settings: return "gearshape"
        }
    }
}

/// Main-thread view model. Holds the engine's latest snapshot and forwards
/// user intents to the engine actor.
@MainActor
final class AppModel: ObservableObject {
    @Published private(set) var snapshot: EngineSnapshot?
    @Published var section: SidebarItem = .overview
    @Published var selectedProjectID: String?
    @Published var toast: String?

    let engine: FlipDeckEngine
    let transport: BLESerialTransport
    let editors: [EditorApp]
    private var toastTask: Task<Void, Never>?

    init() {
        let support = AppPaths.supportDirectory()
        let settingsStore = SettingsStore(url: support.appendingPathComponent("settings.json"))
        let installedEditors = Editors.installed()
        var settings = settingsStore.settings
        if settings.editorBundleID == nil, let preferred = installedEditors.first {
            settings.editorBundleID = preferred.bundleID
            try? settingsStore.save(settings)
        }
        editors = installedEditors

        let transport = BLESerialTransport(knownPeripheralID: settings.flipperPeripheralID.flatMap(UUID.init(uuidString:)))
        self.transport = transport
        let engine = FlipDeckEngine(dependencies: EngineDependencies(
            secrets: KeychainSecretStore(),
            effects: MacSystemEffects(),
            metrics: MacMachineMetrics(),
            transport: transport,
            notifier: MacUserNotifier(),
            logSink: OSLogSink(),
            settingsStore: settingsStore,
            activityLog: ActivityLog(fileURL: support.appendingPathComponent("activity.json")),
            hostName: MacHost.computerName()
        ))
        self.engine = engine
        transport.onPeripheralIdentified = { id in
            Task { await engine.rememberFlipper(id.uuidString) }
        }

        let sink: @Sendable (EngineSnapshot) -> Void = { [weak self] snapshot in
            Task { @MainActor in self?.snapshot = snapshot }
        }
        Task {
            await engine.setObserver(sink)
            await engine.start()
        }
    }

    // MARK: Intents

    func perform(_ action: FDAction) {
        Task {
            let result = await engine.perform(action)
            show(result.message)
        }
    }

    func acknowledge(_ eventID: String) {
        Task { await engine.acknowledge(eventID: eventID) }
    }

    func acknowledgeAll() {
        Task { await engine.acknowledgeAll() }
    }

    func refresh() {
        Task { await engine.refreshNow() }
        show("Refreshing…")
    }

    func updateSettings(_ change: (inout FlipDeckSettings) -> Void) {
        guard var settings = snapshot?.settings else { return }
        change(&settings)
        let updated = settings
        Task { await engine.updateSettings(updated) }
    }

    func setVercelToken(_ token: String?) {
        Task {
            await engine.setVercelToken(token)
            show(token == nil ? "Vercel token removed" : "Vercel token saved to Keychain")
        }
    }

    func forgetFlipper() {
        transport.forget()
        Task { await engine.rememberFlipper(nil) }
        show("Searching for a FlipDeck Flipper…")
    }

    func shutdown() async {
        await engine.stop()
    }

    func show(_ message: String) {
        toast = message
        toastTask?.cancel()
        toastTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: 3_000_000_000)
            guard !Task.isCancelled else { return }
            self?.toast = nil
        }
    }

    // MARK: Derived

    var state: EngineState { snapshot?.state ?? EngineState() }

    func projectName(_ id: String?) -> String? { state.project(id: id)?.name }
}
#endif
