import XCTest
@testable import FlipDeckCore

final class RecordingNotifier: MacNotifier, @unchecked Sendable {
    let posted = Counter()
    func post(_ event: FDEvent) { posted.add(event.type.rawValue) }
}

/// Drives the whole pipeline: real git + processes → state → events →
/// rules → Flipper frames → Flipper action request → effect.
final class EngineIntegrationTests: XCTestCase {
    func testEndToEnd() async throws {
        guard ToolLocator.find("lsof") != nil, ToolLocator.find("perl") != nil else { throw XCTSkip("lsof/perl not available") }

        let tmp = TempDir()
        let root = tmp.mkdir("Developer")
        let web = tmp.mkdir("Developer/web")
        try makeGitRepo(web)
        try commit(web, message: "initial")
        tmp.write("Developer/web/.vercel/project.json", #"{"projectId":"prj_web","orgId":"team_1"}"#)
        tmp.write("Developer/web/package.json", #"{"dependencies":{"next":"15"}}"#)

        // Fake Vercel API whose state the test controls.
        let deploymentsJSON = LockedString(#"{"deployments":[{"uid":"d1","state":"READY","target":"production","url":"web.vercel.app","inspectorUrl":"https://vercel.com/acme/web/d1","created":1000}]}"#)
        let http = FakeHTTP { _ in HTTPResponse(status: 200, body: Data(deploymentsJSON.value.utf8)) }

        let transport = LoopbackTransport()
        let effects = RecordingEffects()
        let notifier = RecordingNotifier()
        let store = SettingsStore(url: nil)
        try store.save(FlipDeckSettings(projectRoots: [root]))
        let engine = FlipDeckEngine(dependencies: EngineDependencies(
            http: http,
            secrets: InMemorySecretStore([.vercelToken: "tok"]),
            effects: effects,
            transport: transport,
            notifier: notifier,
            logSink: NullLogSink(),
            settingsStore: store,
            activityLog: ActivityLog(),
            hostName: "TestMac"
        ))
        let snapshots = SnapshotBox()
        await engine.setObserver { snapshots.set($0) }
        await engine.startSession()

        // Discovery + git.
        await engine.discoverProjects()
        var state = await engine.state
        XCTAssertEqual(state.projects.map(\.name), ["web"])
        XCTAssertEqual(state.projects[0].git?.branch, "main")
        XCTAssertEqual(state.projects[0].git?.lastCommit?.subject, "initial")
        XCTAssertEqual(state.projects[0].runtime.framework, "Next.js")
        XCTAssertEqual(state.projects[0].vercel?.projectID, "prj_web")

        // Baselines: processes and deployments produce no events on first sight.
        await engine.scanProcesses()
        await engine.refreshIntegrations()
        XCTAssertTrue(snapshots.value?.events.isEmpty ?? false, "\(snapshots.value?.events.map(\.title) ?? [])")

        // A dev server starts inside the project.
        let portFile = tmp.path + "/port"
        let server = Process()
        server.executableURL = URL(fileURLWithPath: ToolLocator.find("perl")!)
        server.arguments = ["-MIO::Socket::INET", "-e", "my $s = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Listen => 5) or die; open(my $f, '>', '\(portFile)'); print $f $s->sockport; close $f; sleep 30"]
        server.currentDirectoryURL = URL(fileURLWithPath: web)
        try server.run()
        // SIGKILL: the XCTest runner can hand children an ignored SIGTERM.
        defer { kill(server.processIdentifier, SIGKILL) }
        for _ in 0..<50 where !FileManager.default.fileExists(atPath: portFile) { try await Task.sleep(nanoseconds: 100_000_000) }
        try await Task.sleep(nanoseconds: 100_000_000)

        await engine.scanProcesses()
        state = await engine.state
        let detected = try XCTUnwrap(state.servers.first { $0.pid == server.processIdentifier })
        XCTAssertEqual(detected.projectID, web)
        XCTAssertEqual(snapshots.value?.events.first?.type, .serverStarted)

        // A new commit is noticed.
        try commit(web, message: "second")
        await engine.refreshGit(force: true)
        XCTAssertEqual(snapshots.value?.events.first?.title, "New commit on main")
        XCTAssertEqual(snapshots.value?.events.first?.message, "second")

        // Connect the Flipper.
        transport.emit(.connected(peerName: "FlipDeck Zero", maxChunk: 182))
        try await Task.sleep(nanoseconds: 50_000_000)
        transport.receive(String(decoding: FrameCodec.encode(Frame("HI", ["1", "0.1.0", "0", "boot1"])), as: UTF8.self))
        try await Task.sleep(nanoseconds: 100_000_000)
        let sent = transport.sentFrames()
        XCTAssertTrue(sent.contains { $0.type == "PRJ" && $0.fields[1] == "web" && $0.fields[6] == String(detected.primaryPort) && $0.fields[7] == "r" })
        XCTAssertTrue(sent.contains { $0.type == "SVC" })

        // A production deployment fails → activity, Flipper alert, Mac notification, attention.
        deploymentsJSON.value = #"{"deployments":[{"uid":"d2","state":"ERROR","target":"production","inspectorUrl":"https://vercel.com/acme/web/d2","created":2000,"meta":{"githubCommitMessage":"oops"}},{"uid":"d1","state":"READY","target":"production","url":"web.vercel.app","created":1000}]}"#
        await engine.refreshIntegrations()
        try await Task.sleep(nanoseconds: 100_000_000)
        let snapshot = try XCTUnwrap(snapshots.value)
        XCTAssertEqual(snapshot.events.first?.type, .deploymentFailed)
        XCTAssertEqual(snapshot.attention.map(\.title), ["Production deploy failed"])
        XCTAssertEqual(notifier.posted.all, ["deployment.failed"])
        let alert = try XCTUnwrap(transport.sentFrames().last { $0.type == "ALR" })
        XCTAssertEqual(alert.fields[3], "Production deployment failed")
        XCTAssertEqual(alert.fields[4], "oops")

        // The alert's actions arrived in a snapshot before the alert itself.
        let frames = transport.sentFrames()
        let alertIndex = try XCTUnwrap(frames.lastIndex { $0.type == "ALR" })
        let logsAction = try XCTUnwrap(frames[..<alertIndex].last { $0.type == "ACT" && $0.fields[0] == alert.fields[0] && $0.fields[2] == "LOGS" })

        // The Flipper asks for the logs.
        transport.receive(String(decoding: FrameCodec.encode(Frame("REQ", ["1", logsAction.fields[1], "0"])), as: UTF8.self))
        try await Task.sleep(nanoseconds: 200_000_000)
        XCTAssertTrue(effects.entries.contains("open https://vercel.com/acme/web/d2"), "\(effects.entries)")
        XCTAssertEqual(transport.sentFrames().last { $0.type == "RES" }?.fields, ["1", "1", "Opened logs"])

        // Dismissing it on the Flipper clears the attention item.
        transport.receive(String(decoding: FrameCodec.encode(Frame("SEEN", [alert.fields[0]])), as: UTF8.self))
        try await Task.sleep(nanoseconds: 200_000_000)
        XCTAssertEqual(snapshots.value?.attention.count, 0)

        // The server stops.
        kill(server.processIdentifier, SIGKILL)
        server.waitUntilExit()
        await engine.scanProcesses()
        XCTAssertTrue(snapshots.value?.events.contains { $0.type == .serverStopped } ?? false)
    }
}

final class LockedString: @unchecked Sendable {
    private let lock = NSLock()
    private var stored: String
    init(_ value: String) { stored = value }
    var value: String {
        get { lock.lock(); defer { lock.unlock() }; return stored }
        set { lock.lock(); stored = newValue; lock.unlock() }
    }
}

final class SnapshotBox: @unchecked Sendable {
    private let lock = NSLock()
    private var stored: EngineSnapshot?
    func set(_ snapshot: EngineSnapshot) { lock.lock(); stored = snapshot; lock.unlock() }
    var value: EngineSnapshot? { lock.lock(); defer { lock.unlock() }; return stored }
}
