import Foundation

/// Where an event should surface. Everything goes to Activity; `flipper` and
/// `mac` are escalations.
public struct NotificationRoute: OptionSet, Codable, Hashable, Sendable {
    public let rawValue: Int
    public init(rawValue: Int) { self.rawValue = rawValue }

    public static let activity = NotificationRoute(rawValue: 1 << 0)
    public static let flipper = NotificationRoute(rawValue: 1 << 1)
    public static let mac = NotificationRoute(rawValue: 1 << 2)
}

/// Deterministic routing table. Kept as data so a smarter prioritizer can
/// replace or wrap it later without touching providers or transports.
public struct NotificationRules: Sendable {
    public var table: [EventType: NotificationRoute]
    public var flipperAlertsEnabled: Bool
    public var macNotificationsEnabled: Bool

    public static let defaultTable: [EventType: NotificationRoute] = [
        .deploymentStarted: .activity,
        .deploymentSucceeded: .activity,
        .deploymentFailed: [.activity, .flipper, .mac],
        .deploymentCanceled: .activity,
        .testsStarted: .activity,
        .testsPassed: .activity,
        .testsFailed: [.activity, .flipper],
        .serverStarted: .activity,
        .serverStopped: .activity,
        .agentStarted: .activity,
        .agentCompleted: [.activity, .flipper],
        .agentFailed: [.activity, .flipper, .mac],
        // Outcome unknown, but "your agent stopped" is exactly what someone
        // who walked away wants to know.
        .agentExited: [.activity, .flipper],
        .gitChanged: .activity,
        .integrationError: [.activity, .mac],
        .actionPerformed: .activity,
        .actionFailed: .activity,
    ]

    public init(table: [EventType: NotificationRoute] = NotificationRules.defaultTable, flipperAlertsEnabled: Bool = true, macNotificationsEnabled: Bool = true) {
        self.table = table
        self.flipperAlertsEnabled = flipperAlertsEnabled
        self.macNotificationsEnabled = macNotificationsEnabled
    }

    public func route(for event: FDEvent) -> NotificationRoute {
        var route = table[event.type] ?? Self.fallback(for: event.severity)
        route.insert(.activity)
        if !flipperAlertsEnabled { route.remove(.flipper) }
        if !macNotificationsEnabled { route.remove(.mac) }
        return route
    }

    static func fallback(for severity: Severity) -> NotificationRoute {
        switch severity {
        case .error, .actionRequired: return [.activity, .flipper, .mac]
        default: return .activity
        }
    }
}

/// Bounded, de-duplicated event history with optional JSON persistence.
public final class ActivityLog: @unchecked Sendable {
    public let capacity: Int
    private let fileURL: URL?
    private let lock = NSLock()
    private var events: [FDEvent] = []
    private var ids: Set<String> = []
    private var dirty = false

    public init(capacity: Int = 500, fileURL: URL? = nil) {
        self.capacity = capacity
        self.fileURL = fileURL
        if let fileURL, let data = try? Data(contentsOf: fileURL) {
            let decoder = JSONDecoder()
            decoder.dateDecodingStrategy = .iso8601
            if let stored = try? decoder.decode([FDEvent].self, from: data) {
                events = Array(stored.prefix(capacity))
                ids = Set(events.map(\.id))
            }
        }
    }

    /// Adds events not seen before; returns the ones actually added (newest first).
    @discardableResult
    public func append(_ newEvents: [FDEvent]) -> [FDEvent] {
        lock.lock()
        defer { lock.unlock() }
        var added: [FDEvent] = []
        for event in newEvents where !ids.contains(event.id) {
            ids.insert(event.id)
            added.append(event)
        }
        guard !added.isEmpty else { return [] }
        events.insert(contentsOf: added.sorted { $0.timestamp > $1.timestamp }, at: 0)
        if events.count > capacity {
            for dropped in events[capacity...] { ids.remove(dropped.id) }
            events.removeLast(events.count - capacity)
        }
        dirty = true
        return added
    }

    public func recent(_ limit: Int = 200) -> [FDEvent] {
        lock.lock()
        defer { lock.unlock() }
        return Array(events.prefix(limit))
    }

    public func event(id: String) -> FDEvent? {
        lock.lock()
        defer { lock.unlock() }
        return events.first { $0.id == id }
    }

    @discardableResult
    public func acknowledge(id: String) -> Bool {
        lock.lock()
        defer { lock.unlock() }
        guard let index = events.firstIndex(where: { $0.id == id }), !events[index].acknowledged else { return false }
        events[index].acknowledged = true
        dirty = true
        return true
    }

    public func acknowledgeAll() {
        lock.lock()
        defer { lock.unlock() }
        for index in events.indices where !events[index].acknowledged {
            events[index].acknowledged = true
            dirty = true
        }
    }

    /// Writes to disk if anything changed since the last save.
    public func saveIfNeeded() throws {
        lock.lock()
        guard dirty, let fileURL else { lock.unlock(); return }
        let snapshot = events
        dirty = false
        lock.unlock()

        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        let data = try encoder.encode(snapshot)
        try FileManager.default.createDirectory(at: fileURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try data.write(to: fileURL, options: .atomic)
    }
}

/// Things that need the user: failed deployments (from live state, so it
/// holds even across restarts) and unacknowledged serious events.
public enum AttentionBuilder {
    public static func build(state: EngineState, events: [FDEvent], now: Date, window: TimeInterval = 24 * 3600) -> [AttentionItem] {
        var items: [AttentionItem] = []
        var coveredDeployments: Set<String> = []

        for project in state.projects {
            guard let latest = state.latestDeployment(for: project.id), latest.state == .error else { continue }
            coveredDeployments.insert(latest.id)
            let eventID = "\(EventType.deploymentFailed.rawValue):\(latest.id)"
            // Dismissing the failure event (Mac or Flipper) clears this item until the next failure.
            if events.first(where: { $0.id == eventID })?.acknowledged == true { continue }
            items.append(AttentionItem(
                id: "deploy:\(latest.id)",
                severity: latest.isProduction ? .error : .warning,
                projectID: project.id,
                projectName: project.name,
                title: "\(latest.targetLabel) deploy failed",
                eventID: eventID,
                actions: ActionCatalog.actions(for: latest) + [FDAction(kind: .openOnMac, target: .project(path: project.path), source: .vercel)]
            ))
        }

        for event in events where !event.acknowledged && event.severity >= .error && now.timeIntervalSince(event.timestamp) < window {
            if let deployment = event.metadata["deployment"], coveredDeployments.contains(deployment) { continue }
            // A failure that has since been superseded by a newer deployment is resolved.
            if event.type == .deploymentFailed, let projectID = event.projectID,
               let latest = state.latestDeployment(for: projectID), latest.id != event.metadata["deployment"] { continue }
            items.append(AttentionItem(
                id: "event:\(event.id)",
                severity: event.severity,
                projectID: event.projectID,
                projectName: event.projectName,
                title: event.title,
                eventID: event.id,
                actions: event.actions
            ))
        }
        return items.sorted { $0.severity > $1.severity }
    }
}
