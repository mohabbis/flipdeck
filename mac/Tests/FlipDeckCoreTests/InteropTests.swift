import XCTest
@testable import FlipDeckCore
#if canImport(Glibc)
import Glibc
#endif

/// The Mac's Swift session against the Flipper's real C protocol/state code
/// (src/fd_state.c, compiled into src/tests/host/fd_sim.c) over pipes.
final class InteropTests: XCTestCase {
    static var repoRoot: URL {
        URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent()
    }

    func buildSimulator() throws -> String {
        guard let cc = ToolLocator.find("cc") ?? ToolLocator.find("clang") ?? ToolLocator.find("gcc") else {
            throw XCTSkip("no C compiler")
        }
        let src = Self.repoRoot.appendingPathComponent("src").path
        let output = FileManager.default.temporaryDirectory.appendingPathComponent("fd_sim-\(UUID().uuidString)").path
        let result = try ProcessCommandRunner.runSync(cc, [
            "-std=c11", "-O1", "-I", src, "-o", output,
            src + "/tests/host/fd_sim.c", src + "/fd_state.c", src + "/fd_proto.c",
        ], cwd: nil, timeout: 60)
        guard result.succeeded else { throw XCTSkip("couldn't build fd_sim: \(result.stderr)") }
        return output
    }

    func testSwiftSessionAgainstFlipperStateMachine() async throws {
        let simulator = try buildSimulator()
        defer { try? FileManager.default.removeItem(atPath: simulator) }

        let process = Process()
        process.executableURL = URL(fileURLWithPath: simulator)
        let stdin = Pipe(), stdout = Pipe(), stderr = Pipe()
        process.standardInput = stdin
        process.standardOutput = stdout
        process.standardError = stderr
        try process.run()

        let transport = LoopbackTransport()
        transport.onSend = { data in stdin.fileHandleForWriting.write(data) }
        stdout.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            if !data.isEmpty { transport.emit(.received(data)) }
        }
        let report = LockedString("")
        stderr.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            if !data.isEmpty { report.value += String(decoding: data, as: UTF8.self) }
        }

        let performed = Counter()
        let seen = Counter()
        let session = FlipperSession(transport: transport, hostName: "Studio", log: Logger("t", sink: NullLogSink()))
        let openAction = FDAction(kind: .openOnMac, target: .project(path: "/dev/flipdeck"), source: .system)
        await session.setHandlers(
            action: { action in
                performed.add(action.id)
                return .success("Opened in Cursor")
            },
            seen: { seen.add($0) },
            status: nil
        )
        await session.start()

        var state = EngineState()
        state.projects = [project("/dev/flipdeck", name: "flipdeck")]
        state.servers = [server(pid: 42, port: 5173, projectID: "/dev/flipdeck")]
        let event = FDEvent(id: "ev-1", timestamp: Date(), source: .vercel, projectID: "/dev/flipdeck", projectName: "flipdeck",
                            type: .deploymentFailed, severity: .error, title: "Production deployment failed", message: "Type error",
                            actions: [FDAction(kind: .openLogs, target: .url("https://vercel.com/a/b/c"), source: .vercel)])
        let snapshot = FlipperSnapshotBuilder.build(state: state, attention: [], events: [event])
        await session.update(snapshot: snapshot, machine: MachineStatus(hostName: "Studio", osVersion: "15", cpuUsage: 0.25))

        transport.emit(.connected(peerName: "sim", maxChunk: 182))

        // Handshake → snapshot → Flipper commits → Flipper requests OPEN → Mac executes → RES.
        try await waitFor(report, contains: "RESULT=1|Opened in Cursor")
        XCTAssertEqual(performed.all, [openAction.id])
        XCTAssertTrue(report.value.contains("COMMITTED=1 PROJECTS=1"), report.value)
        XCTAssertTrue(report.value.contains("FIRST=flipdeck"), report.value)

        // Alert → Flipper shows it and dismisses it → Mac acknowledges the event.
        await session.alert(event)
        try await waitFor(report, contains: "ALERT=")
        try await waitFor({ seen.all == ["ev-1"] })

        let status = await session.status
        XCTAssertEqual(status.link, .ready(appVersion: "0.1.0"))
        XCTAssertEqual(status.framesDropped, 0)

        stdin.fileHandleForWriting.closeFile()
        process.waitUntilExit()
        stdout.fileHandleForReading.readabilityHandler = nil
        stderr.fileHandleForReading.readabilityHandler = nil
        XCTAssertTrue(report.value.contains("FRAMES_BAD=0"), report.value)
    }

    func waitFor(_ report: LockedString, contains text: String, timeout: TimeInterval = 10) async throws {
        try await waitFor({ report.value.contains(text) }, timeout: timeout, message: "waiting for \(text); got:\n\(report.value)")
    }

    func waitFor(_ condition: @escaping () -> Bool, timeout: TimeInterval = 10, message: String = "condition") async throws {
        let deadline = Date().addingTimeInterval(timeout)
        while !condition() {
            if Date() > deadline { XCTFail("timed out \(message)"); return }
            try await Task.sleep(nanoseconds: 20_000_000)
        }
    }
}
