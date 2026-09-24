import XCTest
@testable import FlipDeckCore

final class ParserTests: XCTestCase {
    func testPSParsing() {
        let output = """
          501   300     1 Wed Sep 24 03:53:00 2026     /opt/homebrew/bin/node /Users/me/Developer/web/node_modules/.bin/vite --port 5173
          502   501   501 Thu Sep  4 11:02:09 2026     claude
            1     0     0 Mon Sep  1 00:00:00 2026     /sbin/launchd
        garbage line
        """
        let records = PSParser.parse(output)
        XCTAssertEqual(records.count, 3)
        XCTAssertEqual(records[0].pid, 501)
        XCTAssertEqual(records[0].ppid, 300)
        XCTAssertEqual(records[0].uid, 1)
        XCTAssertEqual(records[0].args, "/opt/homebrew/bin/node /Users/me/Developer/web/node_modules/.bin/vite --port 5173")
        XCTAssertEqual(records[0].executableName, "node")
        XCTAssertEqual(records[1].executableName, "claude")

        var components = DateComponents()
        (components.year, components.month, components.day, components.hour, components.minute, components.second) = (2026, 9, 4, 11, 2, 9)
        XCTAssertEqual(records[1].startedAt, Calendar.current.date(from: components))
    }

    func testLsofListenerParsing() {
        let output = """
        p501
        f22
        cnode
        n*:5173
        f23
        n[::1]:5173
        p777
        f4
        cpython3.12
        n127.0.0.1:8000
        p900
        cControlCe
        n*:7000
        """
        let listeners = LsofParser.parseListeners(output)
        XCTAssertEqual(listeners.map(\.port), [5173, 8000, 7000])
        XCTAssertEqual(listeners[0].host, "*")
        XCTAssertEqual(listeners[1].command, "python3.12")
        XCTAssertEqual(listeners[1].host, "127.0.0.1")
    }

    func testLsofCwdParsing() {
        let cwds = LsofParser.parseCwds("p10\nfcwd\nn/Users/me/Developer/web\np11\nfcwd\nn/\n")
        XCTAssertEqual(cwds, [10: "/Users/me/Developer/web", 11: "/"])
    }
}

final class ClassificationTests: XCTestCase {
    func record(_ pid: Int32, _ args: String, ppid: Int32 = 1, uid: UInt32 = 501) -> ProcessRecord {
        ProcessRecord(pid: pid, ppid: ppid, uid: uid, startedAt: .at(1_000), args: args)
    }

    func testClassifiesDevProcesses() {
        let cases: [(String, DevProcessKind?, String?)] = [
            ("node /p/node_modules/.bin/vitest run", .testRunner, "vitest"),
            ("npm test", .testRunner, "npm test"),
            ("pnpm run build", .build, "pnpm build"),
            ("python3 -m pytest -q", .testRunner, "pytest"),
            ("cargo test --all", .testRunner, "cargo test"),
            ("/usr/bin/swift test", .testRunner, "swift test"),
            ("node /p/node_modules/next/dist/bin/next build", .build, "next build"),
            ("tsc --watch", .build, "tsc --watch"),
            ("docker compose up", .container, "docker"),
            ("npm install", .packageManager, "npm install"),
            ("/Applications/Safari.app/Contents/MacOS/Safari", nil, nil),
            ("node server.js", nil, nil),
            ("npm run dev", nil, nil),
        ]
        for (args, kind, label) in cases {
            let match = ProcessClassifier.classify(record(1, args))
            XCTAssertEqual(match?.kind, kind, args)
            XCTAssertEqual(match?.label, label, args)
        }
    }

    func testWrapperCollapseKeepsMostSpecific() {
        let parent = record(10, "npm test")
        let child = record(11, "node /p/node_modules/.bin/vitest", ppid: 10)
        let other = record(20, "cargo build")
        let matches = [parent, child, other].compactMap { r in ProcessClassifier.classify(r).map { (r, $0) } }
        XCTAssertEqual(ProcessClassifier.collapseWrappers(matches).map(\.0.pid), [11, 20])
    }

    func testServerFramework() {
        XCTAssertEqual(ProcessClassifier.serverFramework(record(1, "node /p/node_modules/.bin/vite")), "Vite")
        XCTAssertEqual(ProcessClassifier.serverFramework(record(1, "next-server (v15.1.0)")), "Next.js")
        XCTAssertEqual(ProcessClassifier.serverFramework(record(1, "python3 manage.py runserver")), "Django")
        XCTAssertEqual(ProcessClassifier.serverFramework(record(1, "/usr/local/bin/python3 -m http.server 8000")), "http.server")
        XCTAssertEqual(ProcessClassifier.serverFramework(record(1, "node dist/index.js")), "Node")
        XCTAssertEqual(ProcessClassifier.serverFramework(record(1, "/Applications/Docker.app/Contents/MacOS/com.docker.backend")), "Docker")
    }

    func testDevServerDetection() {
        let records = [
            record(100, "node /Users/me/dev/web/node_modules/.bin/vite"),
            record(101, "/Users/me/dev/api/target/debug/api"),               // unknown binary, but runs from a project
            record(102, "/System/Library/CoreServices/ControlCenter.app/Contents/MacOS/ControlCenter"),
            record(103, "node other-users-server.js", uid: 999),              // someone else's process
            record(104, "node privileged.js"),
        ]
        let listeners = [
            Listener(pid: 100, command: "node", host: "*", port: 5173),
            Listener(pid: 100, command: "node", host: "*", port: 24678),
            Listener(pid: 101, command: "api", host: "127.0.0.1", port: 8080),
            Listener(pid: 102, command: "ControlCe", host: "*", port: 7000),
            Listener(pid: 103, command: "node", host: "*", port: 3000),
            Listener(pid: 104, command: "node", host: "*", port: 80),
        ]
        let cwds: [Int32: String] = [100: "/Users/me/dev/web", 101: "/Users/me/dev/api", 102: "/"]
        let projects = [project("/Users/me/dev/web"), project("/Users/me/dev/api")]
        let servers = DevServerDetector.detect(records: records, listeners: listeners, cwds: cwds, currentUID: 501, associator: ProjectAssociator(projects: projects))

        XCTAssertEqual(servers.map(\.pid), [100, 101])
        XCTAssertEqual(servers[0].ports, [5173, 24678])
        XCTAssertEqual(servers[0].primaryPort, 5173)
        XCTAssertEqual(servers[0].framework, "Vite")
        XCTAssertEqual(servers[0].projectID, "/Users/me/dev/web")
        XCTAssertEqual(servers[1].projectID, "/Users/me/dev/api")
        XCTAssertEqual(servers[0].url, "http://localhost:5173")
    }

    func testProjectAssociationPrefersNestedAndChecksBoundaries() {
        let associator = ProjectAssociator(projects: [project("/dev/mono"), project("/dev/mono/apps/web"), project("/dev/app")])
        XCTAssertEqual(associator.projectID(cwd: "/dev/mono/apps/web/src", args: ""), "/dev/mono/apps/web")
        XCTAssertEqual(associator.projectID(cwd: "/dev/mono/packages", args: ""), "/dev/mono")
        XCTAssertNil(associator.projectID(cwd: "/dev/application", args: ""))   // prefix but not a path boundary
        XCTAssertEqual(associator.projectID(cwd: "/", args: "node /dev/app/server.js"), "/dev/app")
        XCTAssertNil(associator.projectID(cwd: nil, args: "node /dev/apple/x.js"))
    }
}

final class AgentProviderTests: XCTestCase {
    func record(_ pid: Int32, _ args: String, ppid: Int32 = 1, uid: UInt32 = 501) -> ProcessRecord {
        ProcessRecord(pid: pid, ppid: ppid, uid: uid, startedAt: .at(1_000), args: args)
    }

    func testClaudeCodeDetection() {
        let records = [
            record(10, "claude"),
            record(11, "node /opt/homebrew/lib/node_modules/@anthropic-ai/claude-code/cli.js --resume"),
            record(12, "/Users/me/.local/share/claude/versions/2.0.14 -p hi"),
            record(13, "/Applications/Claude.app/Contents/MacOS/Claude"),
            record(14, "/Applications/Claude.app/Contents/Frameworks/claude"),
            record(15, "claude", uid: 999),
            record(16, "claude-helper --x"),
            record(17, "node mcp-server.js", ppid: 10),
        ]
        let sessions = AgentProviders.claudeCode.sessions(in: records, currentUID: 501)
        XCTAssertEqual(sessions.map(\.pid), [10, 11, 12])
    }

    func testCodexLauncherAndBinaryAreOneSession() {
        let records = [
            record(20, "node /opt/homebrew/lib/node_modules/@openai/codex/bin/codex.js"),
            record(21, "/opt/homebrew/lib/node_modules/@openai/codex/vendor/aarch64-apple-darwin/codex/codex", ppid: 20),
            record(30, "codex exec 'fix tests'"),
        ]
        XCTAssertEqual(AgentProviders.codex.sessions(in: records, currentUID: 501).map(\.pid), [20, 30])
    }

    func testCapabilitiesAreHonest() {
        for provider in AgentProviders.all {
            XCTAssertTrue(provider.capabilities.contains(.running))
            XCTAssertFalse(provider.capabilities.contains(.exitStatus), "\(provider.id) can't know exit status from ps")
            XCTAssertFalse(provider.capabilities.contains(.waitingState))
        }
    }
}

/// Real processes: a listening socket running from inside a project, and a
/// fake `claude` binary. Requires `ps`, `lsof` and `perl`.
final class ProcessScanIntegrationTests: XCTestCase {
    func testDetectsRealListenerAndAgent() async throws {
        guard ToolLocator.find("lsof") != nil, ToolLocator.find("perl") != nil else {
            throw XCTSkip("lsof/perl not available")
        }
        let tmp = TempDir()
        let projectPath = tmp.mkdir("myapp")
        let bin = tmp.mkdir("bin")
        try sh("cp \"$(command -v sleep)\" '\(bin)/claude'")

        let portFile = tmp.path + "/port"
        let server = Process()
        server.executableURL = URL(fileURLWithPath: ToolLocator.find("perl")!)
        server.arguments = ["-MIO::Socket::INET", "-e",
            "my $s = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Listen => 5) or die; open(my $f, '>', '\(portFile)'); print $f $s->sockport; close $f; sleep 30"]
        server.currentDirectoryURL = URL(fileURLWithPath: projectPath)
        let agent = Process()
        agent.executableURL = URL(fileURLWithPath: bin + "/claude")
        agent.arguments = ["30"]
        agent.currentDirectoryURL = URL(fileURLWithPath: projectPath)
        try server.run()
        try agent.run()
        defer { kill(server.processIdentifier, SIGKILL); kill(agent.processIdentifier, SIGKILL) }

        var port: Int?
        for _ in 0..<50 where port == nil {
            port = (try? String(contentsOfFile: portFile, encoding: .utf8)).flatMap { Int($0) }
            if port == nil { try await Task.sleep(nanoseconds: 100_000_000) }
        }
        let expectedPort = try XCTUnwrap(port)

        let scanner = ProcessScanner(runner: ProcessCommandRunner())
        let records = try await scanner.processTable()
        let listeners = try await scanner.listeners()
        XCTAssertTrue(listeners.contains { $0.pid == server.processIdentifier && $0.port == expectedPort })

        let cwds = await scanner.cwds(for: [server.processIdentifier, agent.processIdentifier])
        XCTAssertEqual(cwds[server.processIdentifier], projectPath)

        let associator = ProjectAssociator(projects: [project(projectPath)])
        let servers = DevServerDetector.detect(records: records, listeners: listeners, cwds: cwds, currentUID: scanner.currentUID, associator: associator)
        let detected = try XCTUnwrap(servers.first { $0.pid == server.processIdentifier })
        XCTAssertEqual(detected.primaryPort, expectedPort)
        XCTAssertEqual(detected.projectID, projectPath)
        XCTAssertNotNil(detected.startedAt)

        let sessions = AgentProviders.claudeCode.sessions(in: records, currentUID: scanner.currentUID)
        XCTAssertTrue(sessions.contains { $0.pid == agent.processIdentifier })
    }
}
