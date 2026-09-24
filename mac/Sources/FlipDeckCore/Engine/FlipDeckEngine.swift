import Foundation

public struct EngineDependencies: Sendable {
    public var runner: CommandRunner
    public var http: HTTPClient
    public var secrets: SecretStore
    public var effects: SystemEffects
    public var metrics: MachineMetricsProvider
    public var transport: FlipperTransport?
    public var notifier: MacNotifier?
    public var logSink: LogSink
    public var settingsStore: SettingsStore
    public var activityLog: ActivityLog
    public var agentProviders: [any AgentProvider]
    public var hostName: String

    public init(
        runner: CommandRunner = ProcessCommandRunner(),
        http: HTTPClient = URLSessionHTTPClient(),
        secrets: SecretStore,
        effects: SystemEffects,
        metrics: MachineMetricsProvider = BasicMachineMetrics(),
        transport: FlipperTransport? = nil,
        notifier: MacNotifier? = nil,
        logSink: LogSink = StderrLogSink(),
        settingsStore: SettingsStore,
        activityLog: ActivityLog,
        agentProviders: [any AgentProvider] = AgentProviders.all,
        hostName: String = ProcessInfo.processInfo.hostName
    ) {
        self.runner = runner
        self.http = http
        self.secrets = secrets
        self.effects = effects
        self.metrics = metrics
        self.transport = transport
        self.notifier = notifier
        self.logSink = logSink
        self.settingsStore = settingsStore
        self.activityLog = activityLog
        self.agentProviders = agentProviders
        self.hostName = hostName
    }
}

/// Everything a UI needs, delivered as one immutable value.
public struct EngineSnapshot: Sendable {
    public let state: EngineState
    public let events: [FDEvent]
    public let attention: [AttentionItem]
    public let flipper: FlipperLinkStatus
    public let settings: FlipDeckSettings
    public let hasVercelToken: Bool
    public let agentProviders: [AgentProviderInfo]
}

public struct AgentProviderInfo: Sendable, Hashable, Identifiable {
    public let id: String
    public let displayName: String
    public let capabilities: AgentCapabilities
}

/// Owns engine state. Providers are polled on independent cadences; every
/// state change goes through `apply`, which diffs old vs new state into
/// events, routes them, and publishes a snapshot to the UI and the Flipper.
public actor FlipDeckEngine {
    let deps: EngineDependencies
    let log: Logger
    let git: GitClient
    let processScanner: ProcessScanner
    let executor: ActionExecutor
    let vercel: VercelIntegration
    let session: FlipperSession?

    public private(set) var state = EngineState()
    private var settings: FlipDeckSettings
    private var rules: NotificationRules
    private var observer: (@Sendable (EngineSnapshot) -> Void)?
    private var loops: [Task<Void, Never>] = []
    private var flipperStatus = FlipperLinkStatus()
    private var lastGitRefresh: [String: Date] = [:]
    private var lastSave = Date.distantPast
    private var vercelToken: String?
    private var discoveryGeneration = 0
    /// Serializes everything sent to the session so a snapshot always
    /// reaches it before an alert that refers to the snapshot's actions.
    private var flipperChain: Task<Void, Never>?

    public init(dependencies: EngineDependencies) {
        deps = dependencies
        log = Logger("engine", sink: dependencies.logSink)
        git = GitClient(runner: dependencies.runner)
        processScanner = ProcessScanner(runner: dependencies.runner)
        executor = ActionExecutor(effects: dependencies.effects)
        vercel = VercelIntegration(http: dependencies.http, log: Logger("vercel", sink: dependencies.logSink))
        settings = dependencies.settingsStore.settings
        rules = NotificationRules(flipperAlertsEnabled: settings.flipperAlerts, macNotificationsEnabled: settings.macNotifications)
        vercelToken = dependencies.secrets.get(.vercelToken)
        if let transport = dependencies.transport {
            session = FlipperSession(transport: transport, hostName: dependencies.hostName, log: Logger("flipper", sink: dependencies.logSink))
        } else {
            session = nil
        }
    }

    // MARK: Lifecycle

    public func setObserver(_ observer: @escaping @Sendable (EngineSnapshot) -> Void) {
        self.observer = observer
        publish()
    }

    public func start() async {
        guard loops.isEmpty else { return }
        log.info("Starting with roots: \(settings.projectRoots.joined(separator: ", "))")
        await startSession()
        await discoverProjects()
        loops = [
            loop(every: { $0.processScanInterval }) { await $0.scanProcesses() },
            loop(every: { $0.gitRefreshInterval / 4 }) { await $0.refreshGit() },
            loop(every: { $0.discoveryInterval }, skipFirst: true) { await $0.discoverProjects() },
            loop(every: { $0.vercelInterval }) { await $0.refreshIntegrations() },
            loop(every: { _ in 5 }) { await $0.sampleMachine() },
            loop(every: { _ in 1 }) { await $0.tickFlipper() },
        ]
    }

    func startSession() async {
        guard let session else { return }
        await session.setHandlers(
            action: { [weak self] action in await self?.performFromFlipper(action) ?? .failure("Engine stopped") },
            seen: { [weak self] eventID in await self?.acknowledge(eventID: eventID) },
            status: { [weak self] status in Task { await self?.flipperStatusChanged(status) } }
        )
        if settings.flipperEnabled { await session.start() }
    }

    public func stop() async {
        loops.forEach { $0.cancel() }
        loops.removeAll()
        await session?.stop()
        try? deps.activityLog.saveIfNeeded()
    }

    private func loop(every interval: @escaping @Sendable (FlipDeckSettings) -> TimeInterval, skipFirst: Bool = false, _ body: @escaping @Sendable (FlipDeckEngine) async -> Void) -> Task<Void, Never> {
        Task { [weak self] in
            var first = true
            while !Task.isCancelled {
                guard let self else { return }
                if !(first && skipFirst) { await body(self) }
                first = false
                let seconds = max(0.5, interval(await self.currentSettings))
                try? await Task.sleep(nanoseconds: UInt64(seconds * 1_000_000_000))
            }
        }
    }

    var currentSettings: FlipDeckSettings { settings }

    // MARK: Commands from the UI

    public func updateSettings(_ newSettings: FlipDeckSettings) async {
        let rootsChanged = newSettings.projectRoots != settings.projectRoots
        let flipperToggled = newSettings.flipperEnabled != settings.flipperEnabled
        settings = newSettings
        rules.flipperAlertsEnabled = newSettings.flipperAlerts
        rules.macNotificationsEnabled = newSettings.macNotifications
        do { try deps.settingsStore.save(newSettings) } catch { log.error("Couldn't save settings: \(error)") }
        if flipperToggled, let session {
            if newSettings.flipperEnabled { await session.start() } else { await session.stop() }
        }
        if rootsChanged { await discoverProjects() }
        publish()
    }

    public func setVercelToken(_ token: String?) async {
        let trimmed = token?.trimmingCharacters(in: .whitespacesAndNewlines)
        let value = (trimmed?.isEmpty ?? true) ? nil : trimmed
        do { try deps.secrets.set(value, for: .vercelToken) } catch { log.error("Couldn't store Vercel token: \(error)") }
        vercelToken = value
        if value == nil {
            apply { state in
                state.deployments.removeAll()
                state.observed.deploymentsByProject.removeAll()
            }
        }
        await refreshIntegrations()
    }

    public func perform(_ action: FDAction) async -> ActionResult {
        let result = await executor.perform(action, origin: .mac, state: state, policy: policy)
        if !result.ok { log.warning("Action \(action.kind.rawValue) failed: \(result.message)") }
        return result
    }

    public func acknowledge(eventID: String) async {
        if deps.activityLog.acknowledge(id: eventID) {
            await session?.acknowledged(eventID: eventID)
            publish()
        }
    }

    public func acknowledgeAll() async {
        deps.activityLog.acknowledgeAll()
        publish()
    }

    public func refreshNow() async {
        await sampleMachine()
        await discoverProjects()
        await scanProcesses()
        await refreshIntegrations()
    }

    // MARK: Providers

    func discoverProjects() async {
        discoveryGeneration += 1
        let generation = discoveryGeneration
        let roots = settings.projectRoots
        // Filesystem walk + manifest reads happen off the actor.
        let discovered: [(ProjectScanner.Found, ProjectRuntime, VercelLink?)] = await Task.detached(priority: .utility) {
            ProjectScanner().scan(roots: roots).map { found in
                (found, FrameworkDetector.detect(at: found.path), VercelLinkReader.read(projectPath: found.path))
            }
        }.value
        guard generation == discoveryGeneration else { return } // superseded by a newer scan

        var remotes: [String: String] = [:]
        for (found, _, _) in discovered where found.isGitRepository {
            if let url = try? await git.remoteURL(at: found.path) { remotes[found.path] = url }
        }

        apply { state in
            let previous = Dictionary(state.projects.map { ($0.id, $0) }, uniquingKeysWith: { a, _ in a })
            state.projects = discovered.map { found, runtime, link in
                var project = previous[found.path] ?? Project(
                    path: found.path,
                    name: URL(fileURLWithPath: found.path).lastPathComponent,
                    isGitRepository: found.isGitRepository
                )
                project.isGitRepository = found.isGitRepository
                project.runtime = runtime
                project.vercel = link
                if project.git != nil { project.git?.remoteURL = remotes[found.path] }
                return project
            }
            state.lastProjectScan = Date()
            let ids = Set(state.projects.map(\.id))
            state.deployments = state.deployments.filter { ids.contains($0.key) }
        }
        log.info("Discovered \(discovered.count) projects")
        await refreshGit(force: true)
    }

    func refreshGit(force: Bool = false) async {
        let now = Date()
        let hot = Set(state.servers.compactMap(\.projectID) + state.agents.compactMap(\.projectID) + state.processes.compactMap(\.projectID))
        let due = state.projects.filter { project in
            guard project.isGitRepository else { return false }
            if force { return true }
            let interval = hot.contains(project.id) ? settings.gitRefreshInterval : settings.gitRefreshInterval * 6
            return now.timeIntervalSince(lastGitRefresh[project.id] ?? .distantPast) >= interval
        }
        guard !due.isEmpty else { return }
        for project in due { lastGitRefresh[project.id] = now }

        let previous = Dictionary(state.projects.map { ($0.id, $0.git) }, uniquingKeysWith: { a, _ in a })
        let client = git
        let results: [(String, GitStatus?, String?)] = await withTaskGroup(of: (String, GitStatus?, String?).self) { group in
            var iterator = due.makeIterator()
            var results: [(String, GitStatus?, String?)] = []
            func addNext() -> Bool {
                guard let project = iterator.next() else { return false }
                let prior = previous[project.id] ?? nil
                group.addTask {
                    do {
                        var status = try await client.status(at: project.path)
                        if let prior, prior.headOID == status.headOID, prior.lastCommit != nil {
                            status.lastCommit = prior.lastCommit
                        } else {
                            status.lastCommit = try? await client.lastCommit(at: project.path)
                        }
                        status.remoteURL = prior?.remoteURL
                        if status.remoteURL == nil { status.remoteURL = try? await client.remoteURL(at: project.path) }
                        return (project.id, status, nil)
                    } catch {
                        return (project.id, nil, String(describing: error))
                    }
                }
                return true
            }
            // At most 4 git processes at once.
            for _ in 0..<4 { _ = addNext() }
            while let result = await group.next() {
                results.append(result)
                _ = addNext()
            }
            return results
        }

        apply { state in
            for (id, status, error) in results {
                guard let index = state.projects.firstIndex(where: { $0.id == id }) else { continue }
                if let status {
                    state.projects[index].git = status
                    state.projects[index].gitError = nil
                } else {
                    state.projects[index].gitError = error
                }
            }
        }
    }

    func scanProcesses() async {
        let records: [ProcessRecord]
        let listeners: [Listener]
        do {
            records = try await processScanner.processTable()
        } catch {
            log.warning("Process scan failed: \(error)")
            return
        }
        do {
            listeners = try await processScanner.listeners()
        } catch {
            log.warning("Listener scan failed: \(error)")
            listeners = []
        }

        let uid = processScanner.currentUID
        let associator = ProjectAssociator(projects: state.projects)
        let byPID = Dictionary(records.map { ($0.pid, $0) }, uniquingKeysWith: { a, _ in a })

        var agentRecords: [(any AgentProvider, ProcessRecord)] = []
        for provider in deps.agentProviders {
            for record in provider.sessions(in: records, currentUID: uid) { agentRecords.append((provider, record)) }
        }
        let classified = ProcessClassifier.collapseWrappers(
            records.filter { $0.uid == uid }.compactMap { record in ProcessClassifier.classify(record).map { (record, $0) } }
        )

        // Batch cwd lookups for the handful of processes we care about.
        var interesting = Set(listeners.compactMap { byPID[$0.pid]?.uid == uid ? $0.pid : nil })
        interesting.formUnion(agentRecords.map(\.1.pid))
        interesting.formUnion(classified.map(\.0.pid))
        let cwds = await processScanner.cwds(for: Array(interesting.sorted().prefix(96)))

        let servers = DevServerDetector.detect(records: records, listeners: listeners, cwds: cwds, currentUID: uid, associator: associator)
        let serverPIDs = Set(servers.map(\.pid))
        let agents = agentRecords.map { provider, record in
            AgentSession(
                providerID: provider.id, providerName: provider.displayName, pid: record.pid, startedAt: record.startedAt,
                cwd: cwds[record.pid], projectID: associator.projectID(cwd: cwds[record.pid], args: record.args),
                command: String(record.args.prefix(300))
            )
        }
        let agentPIDs = Set(agents.map(\.pid))
        let processes = classified
            .filter { !serverPIDs.contains($0.0.pid) && !agentPIDs.contains($0.0.pid) }
            .map { record, match in
                DevProcess(
                    pid: record.pid, kind: match.kind, label: match.label, command: String(record.args.prefix(300)),
                    startedAt: record.startedAt, cwd: cwds[record.pid],
                    projectID: associator.projectID(cwd: cwds[record.pid], args: record.args)
                )
            }

        apply { state in
            state.servers = servers
            state.agents = agents.sorted { ($0.startedAt ?? .distantPast) > ($1.startedAt ?? .distantPast) }
            state.processes = processes.sorted { $0.pid < $1.pid }
            state.observed.processes = true
            state.lastProcessScan = Date()
        }
    }

    func refreshIntegrations() async {
        let refresh = await vercel.refresh(projects: state.projects, token: vercelToken)
        let previousHealth = state.integrations.first { $0.id == refresh.status.id }?.health
        apply { state in
            for (projectID, deployments) in refresh.deployments {
                state.deployments[projectID] = deployments.sorted { $0.createdAt > $1.createdAt }
                // Baseline is set *after* this apply's diff, see below.
            }
            if let index = state.integrations.firstIndex(where: { $0.id == refresh.status.id }) {
                state.integrations[index] = refresh.status
            } else {
                state.integrations.append(refresh.status)
            }
        }
        // Mark fetched projects as observed only after the first diff ran, so
        // the first fetch is a silent baseline and later ones produce events.
        let fetched = Set(refresh.deployments.keys)
        if !fetched.isSubset(of: state.observed.deploymentsByProject) {
            state.observed.deploymentsByProject.formUnion(fetched)
        }

        if refresh.status.health == .unauthorized, previousHealth != .unauthorized {
            record(FDEvent(
                id: "integration.unauthorized:vercel:\(Int(Date().timeIntervalSince1970))",
                timestamp: Date(), source: .vercel, type: .integrationError, severity: .warning,
                title: "Vercel token rejected", message: "Update the token in Settings → Integrations"
            ))
        }
    }

    func sampleMachine() async {
        let machine = await deps.metrics.sample()
        state.machine = machine
        publish()
    }

    func tickFlipper() async {
        await session?.tick()
        if Date().timeIntervalSince(lastSave) > 5 {
            lastSave = Date()
            do { try deps.activityLog.saveIfNeeded() } catch { log.error("Couldn't save activity: \(error)") }
        }
    }

    func flipperStatusChanged(_ status: FlipperLinkStatus) {
        guard status != flipperStatus else { return }
        flipperStatus = status
        publish()
    }

    func performFromFlipper(_ action: FDAction) async -> ActionResult {
        let result = await executor.perform(action, origin: .flipper, state: state, policy: policy)
        let projectName = state.projects.first { project in
            if case .project(let path) = action.target { return path == project.path }
            return false
        }?.name
        record(FDEvent(
            timestamp: Date(), source: .flipper, projectName: projectName,
            type: result.ok ? .actionPerformed : .actionFailed,
            severity: result.ok ? .info : .warning,
            title: "\(action.label) from Flipper",
            message: result.message
        ))
        return result
    }

    var policy: ActionPolicy {
        ActionPolicy(allowDestructiveFromFlipper: settings.allowDestructiveFromFlipper, editorBundleID: settings.editorBundleID)
    }

    // MARK: State pipeline

    private func apply(_ mutate: (inout EngineState) -> Void) {
        let old = state
        mutate(&state)
        let events = EventDiffer.diff(old: old, new: state, now: Date())
        route(events)
    }

    private func record(_ event: FDEvent) {
        route([event])
    }

    private func route(_ events: [FDEvent]) {
        let added = deps.activityLog.append(events)
        var alerts: [FDEvent] = []
        for event in added {
            let route = rules.route(for: event)
            log.info("event \(event.type.rawValue): \(event.title)")
            if route.contains(.mac) { deps.notifier?.post(event) }
            if route.contains(.flipper) { alerts.append(event) }
        }
        publish()
        if let session, !alerts.isEmpty {
            // Queued behind the snapshot enqueued by publish() above.
            let ordered = Array(alerts.reversed())
            enqueueFlipper { for alert in ordered { await session.alert(alert) } }
        }
    }

    private func enqueueFlipper(_ work: @escaping @Sendable () async -> Void) {
        let previous = flipperChain
        flipperChain = Task {
            await previous?.value
            await work()
        }
    }

    private func publish() {
        let events = deps.activityLog.recent(200)
        let attention = AttentionBuilder.build(state: state, events: events, now: Date())
        if let session {
            let snapshot = FlipperSnapshotBuilder.build(state: state, attention: attention, events: events)
            let machine = state.machine
            enqueueFlipper { await session.update(snapshot: snapshot, machine: machine) }
        }
        observer?(EngineSnapshot(
            state: state,
            events: events,
            attention: attention,
            flipper: flipperStatus,
            settings: settings,
            hasVercelToken: vercelToken != nil,
            agentProviders: deps.agentProviders.map { AgentProviderInfo(id: $0.id, displayName: $0.displayName, capabilities: $0.capabilities) }
        ))
    }
}
