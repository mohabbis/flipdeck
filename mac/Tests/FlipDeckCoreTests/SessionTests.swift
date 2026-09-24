import XCTest
@testable import FlipDeckCore

final class Clock: @unchecked Sendable {
    private let lock = NSLock()
    private var value: Date
    init(_ start: Date) { value = start }
    var now: Date { lock.lock(); defer { lock.unlock() }; return value }
    func advance(_ seconds: TimeInterval) { lock.lock(); value += seconds; lock.unlock() }
}

final class Counter: @unchecked Sendable {
    private let lock = NSLock()
    private var items: [String] = []
    func add(_ item: String) { lock.lock(); items.append(item); lock.unlock() }
    var all: [String] { lock.lock(); defer { lock.unlock() }; return items }
}

final class FlipperSessionTests: XCTestCase {
    var transport: LoopbackTransport!
    var clock: Clock!
    var session: FlipperSession!
    var performed: Counter!
    var seen: Counter!

    let openAction = FDAction(kind: .openOnMac, target: .project(path: "/dev/web"), source: .system)

    override func setUp() async throws {
        transport = LoopbackTransport()
        clock = Clock(.at(1_000_000))
        let clock = self.clock!
        session = FlipperSession(transport: transport, hostName: "Studio", log: Logger("t", sink: NullLogSink()), now: { clock.now })
        performed = Counter()
        seen = Counter()
        let performed = self.performed!
        let seen = self.seen!
        await session.setHandlers(
            action: { action in
                performed.add(action.id)
                return .success("done")
            },
            seen: { id in seen.add(id) },
            status: nil
        )
        await session.start()
    }

    func snapshot(_ title: String = "web") -> FlipperSnapshot {
        FlipperSnapshot(
            records: [Frame("SUM", ["0", "1", "0", "0"]), Frame("PRJ", ["p1", title, "main", "c", "0", "0", "0", "-", "0"]), Frame("ACT", ["p1", "a1", "OPEN", "", ""])],
            actions: ["a1": openAction],
            eventOwners: ["ev-1": "e1"]
        )
    }

    /// Lets the session's transport-event task catch up.
    func settle() async {
        for _ in 0..<20 { await Task.yield() }
        try? await Task.sleep(nanoseconds: 20_000_000)
    }

    func frames(_ type: String) -> [Frame] { transport.sentFrames().filter { $0.type == type } }

    func connectAndHandshake(nonce: String = "n1") async {
        transport.emit(.connected(peerName: "FlipDeck Zero", maxChunk: 182))
        await settle()
        transport.receive(String(decoding: FrameCodec.encode(Frame("HI", ["1", "0.1.0", "0", nonce])), as: UTF8.self))
        await settle()
    }

    func send(_ frame: Frame) async {
        transport.emit(.received(FrameCodec.encode(frame)))
        await settle()
    }

    func testHandshakeThenSnapshot() async {
        await session.update(snapshot: snapshot(), machine: nil)
        XCTAssertTrue(transport.sentFrames().isEmpty, "nothing is sent before the link is up")

        transport.emit(.connected(peerName: "FlipDeck Zero", maxChunk: 182))
        await settle()
        let hello = frames("HELLO")
        XCTAssertEqual(hello.count, 1)
        XCTAssertEqual(hello[0].fields[0], "1")
        XCTAssertEqual(hello[0].fields[2], "Studio")
        XCTAssertEqual(hello[0].fields[3], "182")
        XCTAssertTrue(frames("SNAP").isEmpty, "no snapshot before HI")

        await send(Frame("HI", ["1", "0.1.0", "0", "n1"]))
        let sent = transport.sentFrames().map(\.type)
        XCTAssertEqual(Array(sent.drop { $0 != "SNAP" }.prefix(5)), ["SNAP", "SUM", "PRJ", "ACT", "END"])
        XCTAssertEqual(frames("SNAP").first?.fields, ["1", "3"])
        XCTAssertEqual(frames("END").first?.fields, ["1"])
        let status = await session.status
        XCTAssertEqual(status.link, .ready(appVersion: "0.1.0"))
    }

    func testSnapshotsOnlyWhenChangedAndRateLimited() async {
        await session.update(snapshot: snapshot(), machine: nil)
        await connectAndHandshake()
        transport.clearSent()

        await session.update(snapshot: snapshot(), machine: nil)
        XCTAssertTrue(frames("SNAP").isEmpty, "identical snapshot not resent")

        await session.update(snapshot: snapshot("renamed"), machine: nil)
        XCTAssertTrue(frames("SNAP").isEmpty, "rate limited to 1/s")
        clock.advance(1.1)
        await session.tick()
        XCTAssertEqual(frames("SNAP").map { $0.fields[0] }, ["2"])
    }

    func testHeartbeatAndResyncOnGenerationMismatch() async {
        await session.update(snapshot: snapshot(), machine: nil)
        await connectAndHandshake()
        transport.clearSent()
        clock.advance(5)
        await session.tick()
        XCTAssertEqual(frames("PING").first?.fields.first, "1")

        await send(Frame("PONG", ["1"]))
        XCTAssertTrue(frames("SNAP").isEmpty, "matching generation: no resend")

        clock.advance(3)
        await send(Frame("PONG", ["0"]))
        XCTAssertEqual(frames("SNAP").count, 1, "Flipper didn't commit: resend")

        await send(Frame("SYNC", ["0"]))
        XCTAssertEqual(frames("SNAP").count, 1, "resync storms are rate limited")
        clock.advance(2.5)
        await send(Frame("SYNC", ["0"]))
        XCTAssertEqual(frames("SNAP").count, 2)
    }

    func testActionRequestsAreExecutedOnceAndDuplicatesGetCachedResult() async {
        await session.update(snapshot: snapshot(), machine: nil)
        await connectAndHandshake()
        transport.clearSent()

        await send(Frame("REQ", ["7", "a1", "1"]))
        await send(Frame("REQ", ["7", "a1", "1"]))
        XCTAssertEqual(performed.all, [openAction.id])
        XCTAssertEqual(frames("RES").map(\.fields), [["7", "1", "done"], ["7", "1", "done"]])

        await send(Frame("REQ", ["8", "zzz", "1"]))
        XCTAssertEqual(frames("RES").last?.fields, ["8", "0", "Action expired; refresh"])
        XCTAssertEqual(performed.all.count, 1)
    }

    func testNewFlipperInstanceClearsRequestCache() async {
        await session.update(snapshot: snapshot(), machine: nil)
        await connectAndHandshake(nonce: "boot-A")
        await send(Frame("REQ", ["1", "a1", "1"]))
        transport.emit(.disconnected(reason: "app restarted"))
        await settle()
        await connectAndHandshake(nonce: "boot-B")
        await send(Frame("REQ", ["1", "a1", "1"]))
        XCTAssertEqual(performed.all.count, 2, "req 1 from a new app instance is a new request")
    }

    func testRequestsIgnoredUntilReady() async {
        await session.update(snapshot: snapshot(), machine: nil)
        transport.emit(.connected(peerName: "f", maxChunk: 20))
        await settle()
        await send(Frame("REQ", ["1", "a1", "1"]))
        XCTAssertTrue(performed.all.isEmpty)
        XCTAssertTrue(frames("RES").isEmpty)
    }

    func testIncompatibleVersion() async {
        await session.update(snapshot: snapshot(), machine: nil)
        transport.emit(.connected(peerName: "f", maxChunk: 20))
        await settle()
        await send(Frame("HI", ["2", "9.0.0", "0", "n"]))
        let status = await session.status
        XCTAssertEqual(status.link, .incompatible(peerProtocol: 2))
        transport.clearSent()
        clock.advance(6)
        await session.tick()
        XCTAssertEqual(transport.sentFrames().map(\.type), ["HELLO"], "only HELLO while incompatible")
        await send(Frame("REQ", ["1", "a1", "1"]))
        XCTAssertTrue(performed.all.isEmpty)
    }

    func testAlertsDeliveredReplayedAndSeen() async {
        await session.update(snapshot: snapshot(), machine: nil)
        let event = FDEvent(id: "ev-1", timestamp: clock.now, source: .vercel, projectName: "web", type: .deploymentFailed, severity: .error, title: "Production deployment failed", message: "Type error")
        await session.alert(event)
        XCTAssertTrue(frames("ALR").isEmpty, "not connected yet")

        await connectAndHandshake()
        XCTAssertEqual(frames("ALR").map(\.fields), [["e1", "e", "web", "Production deployment failed", "Type error"]])

        // Reconnect: replayed (the Flipper de-duplicates by id).
        transport.emit(.disconnected(reason: nil))
        await connectAndHandshake()
        XCTAssertEqual(frames("ALR").count, 2)

        await send(Frame("SEEN", ["e1"]))
        XCTAssertEqual(seen.all, ["ev-1"])
        transport.emit(.disconnected(reason: nil))
        await connectAndHandshake()
        XCTAssertEqual(frames("ALR").count, 2, "seen alerts aren't replayed")
    }

    func testStaleAlertsAreNotReplayed() async {
        let event = FDEvent(id: "old", timestamp: clock.now, source: .agent, type: .agentExited, severity: .info, title: "Codex finished")
        await session.alert(event)
        clock.advance(31 * 60)
        await connectAndHandshake()
        XCTAssertTrue(frames("ALR").isEmpty)
    }

    func testMalformedInputIsCountedNotFatal() async {
        await session.update(snapshot: snapshot(), machine: nil)
        await connectAndHandshake()
        transport.receive("garbage\nREQ|1|a1|1*FFFF\n")
        await settle()
        let status = await session.status
        XCTAssertEqual(status.framesDropped, 2)
        XCTAssertTrue(performed.all.isEmpty)
        await send(Frame("REQ", ["2", "a1", "1"]))
        XCTAssertEqual(performed.all.count, 1)
    }

    func testMachineFrameSentOnlyOnChange() async {
        let machine = MachineStatus(hostName: "Studio", osVersion: "15", cpuUsage: 0.1)
        await session.update(snapshot: snapshot(), machine: machine)
        await connectAndHandshake()
        XCTAssertEqual(frames("MAC").count, 1)
        await session.update(snapshot: snapshot(), machine: MachineStatus(hostName: "Studio", osVersion: "15", cpuUsage: 0.11))
        XCTAssertEqual(frames("MAC").count, 1, "quantized to the same 10%")
        await session.update(snapshot: snapshot(), machine: MachineStatus(hostName: "Studio", osVersion: "15", cpuUsage: 0.5))
        XCTAssertEqual(frames("MAC").count, 2)
    }
}
