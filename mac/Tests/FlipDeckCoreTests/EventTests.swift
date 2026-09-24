import XCTest
@testable import FlipDeckCore

final class EventDifferTests: XCTestCase {
    let now = Date.at(10_000)

    func baseState() -> EngineState {
        var state = EngineState()
        state.projects = [project("/dev/web", name: "web")]
        state.observed.processes = true
        return state
    }

    func testFirstProcessScanIsSilentBaseline() {
        var old = EngineState()
        old.projects = [project("/dev/web")]
        var new = old
        new.servers = [server(pid: 1, port: 3000, projectID: "/dev/web")]
        new.agents = [AgentSession(providerID: "codex", providerName: "Codex", pid: 5, startedAt: .at(1), command: "codex")]
        new.observed.processes = true
        XCTAssertTrue(EventDiffer.diff(old: old, new: new, now: now).isEmpty)
    }

    func testServerStartAndStop() {
        let old = baseState()
        var new = old
        new.servers = [server(pid: 1, port: 5173, projectID: "/dev/web")]
        let started = EventDiffer.diff(old: old, new: new, now: now)
        XCTAssertEqual(started.map(\.type), [.serverStarted])
        XCTAssertEqual(started[0].title, "Vite started on :5173")
        XCTAssertEqual(started[0].projectName, "web")
        XCTAssertEqual(started[0].actions.map(\.kind), [.openLocalhost, .copyURL, .stopServer])

        let stopped = EventDiffer.diff(old: new, new: old, now: now)
        XCTAssertEqual(stopped.map(\.type), [.serverStopped])
        XCTAssertEqual(stopped[0].severity, .info)
    }

    func testAgentExitIsNotReportedAsCompleted() {
        var old = baseState()
        old.agents = [AgentSession(providerID: "claude-code", providerName: "Claude Code", pid: 9, startedAt: now.addingTimeInterval(-38 * 60), projectID: "/dev/web", command: "claude")]
        let events = EventDiffer.diff(old: old, new: baseState(), now: now)
        XCTAssertEqual(events.map(\.type), [.agentExited])
        XCTAssertEqual(events[0].title, "Claude Code finished")
        XCTAssertEqual(events[0].message, "in web after 38m · exit status not available")
        XCTAssertEqual(events[0].actions.map(\.kind), [.openOnMac])
    }

    func testGitBranchAndCommitChanges() {
        var old = baseState()
        old.projects[0].git = GitStatus(branch: "main", headOID: "a1")
        var branchSwitch = old
        branchSwitch.projects[0].git = GitStatus(branch: "feature", headOID: "b2")
        XCTAssertEqual(EventDiffer.diff(old: old, new: branchSwitch, now: now).map(\.title), ["Switched to feature"])

        var newCommit = old
        newCommit.projects[0].git = GitStatus(branch: "main", headOID: "c3", lastCommit: CommitInfo(oid: "c3", subject: "Add thing", date: now))
        let events = EventDiffer.diff(old: old, new: newCommit, now: now)
        XCTAssertEqual(events.map(\.title), ["New commit on main"])
        XCTAssertEqual(events[0].message, "Add thing")

        // Dirty-only changes are not events (too noisy).
        var dirty = old
        dirty.projects[0].git?.changedCount = 3
        XCTAssertTrue(EventDiffer.diff(old: old, new: dirty, now: now).isEmpty)

        // First observation of a project's git state is a baseline.
        var unknown = old
        unknown.projects[0].git = nil
        XCTAssertTrue(EventDiffer.diff(old: unknown, new: old, now: now).isEmpty)
    }

    func deployment(_ id: String, _ state: DeploymentState, created: TimeInterval, target: String? = "production") -> Deployment {
        Deployment(id: id, provider: "vercel", projectID: "/dev/web", state: state, target: target, url: "https://web-\(id).vercel.app",
                   inspectorURL: "https://vercel.com/acme/web/\(id)", createdAt: .at(created), branch: "main", commitMessage: "Ship it")
    }

    func testDeploymentLifecycle() {
        var old = baseState()
        old.observed.deploymentsByProject = ["/dev/web"]
        old.deployments["/dev/web"] = [deployment("d1", .ready, created: 100)]

        var building = old
        building.deployments["/dev/web"] = [deployment("d2", .building, created: 200), deployment("d1", .ready, created: 100)]
        XCTAssertEqual(EventDiffer.diff(old: old, new: building, now: now).map(\.type), [.deploymentStarted])

        var failed = building
        failed.deployments["/dev/web"]![0].state = .error
        failed.deployments["/dev/web"]![0].errorMessage = "Type error: x is not assignable"
        let events = EventDiffer.diff(old: building, new: failed, now: now)
        XCTAssertEqual(events.map(\.type), [.deploymentFailed])
        XCTAssertEqual(events[0].severity, .error)
        XCTAssertEqual(events[0].title, "Production deployment failed")
        XCTAssertEqual(events[0].message, "Type error: x is not assignable")
        XCTAssertEqual(events[0].actions.map(\.kind), [.openLogs])
        XCTAssertEqual(events[0].id, "deployment.failed:d2")

        // queued → building produces no second "started".
        var queued = old
        queued.deployments["/dev/web"] = [deployment("d3", .queued, created: 300)] + old.deployments["/dev/web"]!
        var nowBuilding = queued
        nowBuilding.deployments["/dev/web"]![0].state = .building
        XCTAssertTrue(EventDiffer.diff(old: queued, new: nowBuilding, now: now).isEmpty)
    }

    func testDeploymentBaselineAndPreviewSeverity() {
        let old = baseState() // not yet observed
        var new = old
        new.deployments["/dev/web"] = [deployment("d1", .error, created: 100)]
        XCTAssertTrue(EventDiffer.diff(old: old, new: new, now: now).isEmpty)

        var observed = old
        observed.observed.deploymentsByProject = ["/dev/web"]
        observed.deployments["/dev/web"] = []
        var preview = observed
        preview.deployments["/dev/web"] = [deployment("p1", .error, created: 100, target: nil)]
        let events = EventDiffer.diff(old: observed, new: preview, now: now)
        XCTAssertEqual(events.first?.severity, .warning)
        XCTAssertEqual(events.first?.title, "Preview deployment failed")
    }

    func testDeploymentsFallingOutOfWindowAreIgnored() {
        var old = baseState()
        old.observed.deploymentsByProject = ["/dev/web"]
        old.deployments["/dev/web"] = [deployment("d5", .ready, created: 500), deployment("d4", .ready, created: 400)]
        var new = old
        new.deployments["/dev/web"] = [deployment("d5", .ready, created: 500), deployment("d3", .ready, created: 300)]
        XCTAssertTrue(EventDiffer.diff(old: old, new: new, now: now).isEmpty)
    }
}

final class NotificationRulesTests: XCTestCase {
    func event(_ type: EventType, _ severity: Severity = .info) -> FDEvent {
        FDEvent(timestamp: .at(0), source: .system, type: type, severity: severity, title: "t")
    }

    func testDefaultRouting() {
        let rules = NotificationRules()
        XCTAssertEqual(rules.route(for: event(.deploymentSucceeded)), .activity)
        XCTAssertEqual(rules.route(for: event(.deploymentFailed)), [.activity, .flipper, .mac])
        XCTAssertEqual(rules.route(for: event(.testsPassed)), .activity)
        XCTAssertEqual(rules.route(for: event(.testsFailed)), [.activity, .flipper])
        XCTAssertEqual(rules.route(for: event(.agentCompleted)), [.activity, .flipper])
        XCTAssertEqual(rules.route(for: event(.serverStarted)), .activity)
        // Unknown types fall back on severity.
        XCTAssertEqual(rules.route(for: event("custom.thing", .error)), [.activity, .flipper, .mac])
        XCTAssertEqual(rules.route(for: event("custom.thing", .success)), .activity)
    }

    func testMasterSwitches() {
        let rules = NotificationRules(flipperAlertsEnabled: false, macNotificationsEnabled: false)
        XCTAssertEqual(rules.route(for: event(.deploymentFailed)), .activity)
    }
}

final class ActivityLogTests: XCTestCase {
    func event(_ id: String, _ t: TimeInterval, _ severity: Severity = .info) -> FDEvent {
        FDEvent(id: id, timestamp: .at(t), source: .system, type: .gitChanged, severity: severity, title: id)
    }

    func testDedupeOrderingAndCapacity() {
        let log = ActivityLog(capacity: 3)
        XCTAssertEqual(log.append([event("a", 1), event("b", 2)]).count, 2)
        XCTAssertEqual(log.append([event("a", 1)]).count, 0)
        log.append([event("c", 3), event("d", 4)])
        XCTAssertEqual(log.recent().map(\.id), ["d", "c", "b"])
        // "a" was evicted, so it may be recorded again.
        XCTAssertEqual(log.append([event("a", 5)]).count, 1)
    }

    func testPersistenceRoundTripAndAcknowledge() throws {
        let tmp = TempDir()
        let url = URL(fileURLWithPath: tmp.path + "/nested/activity.json")
        let log = ActivityLog(fileURL: url)
        log.append([event("x", 1, .error), event("y", 2)])
        XCTAssertTrue(log.acknowledge(id: "x"))
        XCTAssertFalse(log.acknowledge(id: "x"))
        try log.saveIfNeeded()

        let reloaded = ActivityLog(fileURL: url)
        XCTAssertEqual(reloaded.recent().map(\.id), ["y", "x"])
        XCTAssertEqual(reloaded.event(id: "x")?.acknowledged, true)
    }

    func testCorruptFileStartsEmpty() throws {
        let tmp = TempDir()
        tmp.write("activity.json", "{nope")
        XCTAssertTrue(ActivityLog(fileURL: URL(fileURLWithPath: tmp.path + "/activity.json")).recent().isEmpty)
    }
}

final class AttentionTests: XCTestCase {
    func testFailedDeploymentNeedsAttentionUntilAcknowledgedOrSuperseded() {
        var state = EngineState()
        state.projects = [project("/dev/web", name: "web")]
        let failed = Deployment(id: "d1", provider: "vercel", projectID: "/dev/web", state: .error, target: "production", url: nil,
                                inspectorURL: "https://vercel.com/a/web/d1", createdAt: .at(1), branch: nil, commitMessage: nil)
        state.deployments["/dev/web"] = [failed]

        let items = AttentionBuilder.build(state: state, events: [], now: .at(100))
        XCTAssertEqual(items.map(\.title), ["Production deploy failed"])
        XCTAssertEqual(items[0].actions.map(\.kind), [.openLogs, .openOnMac])

        let failureEvent = FDEvent(id: "deployment.failed:d1", timestamp: .at(50), source: .vercel, projectID: "/dev/web",
                                   type: .deploymentFailed, severity: .error, title: "x", metadata: ["deployment": "d1"], acknowledged: true)
        XCTAssertTrue(AttentionBuilder.build(state: state, events: [failureEvent], now: .at(100)).isEmpty)

        var unacked = failureEvent
        unacked.acknowledged = false
        // Covered by the state-derived item: listed once, not twice.
        XCTAssertEqual(AttentionBuilder.build(state: state, events: [unacked], now: .at(100)).count, 1)

        // A newer successful deployment resolves it.
        var fixed = state
        fixed.deployments["/dev/web"] = [Deployment(id: "d2", provider: "vercel", projectID: "/dev/web", state: .ready, target: "production", url: nil, inspectorURL: nil, createdAt: .at(2), branch: nil, commitMessage: nil), failed]
        XCTAssertTrue(AttentionBuilder.build(state: fixed, events: [unacked], now: .at(100)).isEmpty)
    }

    func testOldOrLowSeverityEventsAreNotAttention() {
        let old = FDEvent(timestamp: .at(0), source: .system, type: "x", severity: .error, title: "old")
        let warning = FDEvent(timestamp: .at(99_000), source: .system, type: "x", severity: .warning, title: "warn")
        XCTAssertTrue(AttentionBuilder.build(state: EngineState(), events: [old, warning], now: .at(100_000)).isEmpty)
    }
}
