import XCTest
@testable import FlipDeckCore

final class FrameCodecTests: XCTestCase {
    func testCRCCheckValue() {
        // The standard CRC-16/CCITT-FALSE check value.
        XCTAssertEqual(CRC16.ccittFalse(Array("123456789".utf8)), 0x29B1)
    }

    func testRoundTrip() throws {
        let frame = Frame("PRJ", ["p1", "flipdeck", "main", "c", "0", "0", "3000", "r", "0"])
        let data = FrameCodec.encode(frame)
        XCTAssertEqual(data.last, 0x0A)
        var decoder = FrameDecoder()
        XCTAssertEqual(try decoder.feed(data).map { try $0.get() }, [frame])
    }

    func testEmptyFieldsSurvive() throws {
        let frame = Frame("ACT", ["p1", "a1", "OPEN", "", ""])
        var decoder = FrameDecoder()
        XCTAssertEqual(try decoder.feed(FrameCodec.encode(frame)).first?.get(), frame)
    }

    func testSanitize() {
        XCTAssertEqual(FrameCodec.sanitize("Café | déjà vu * 🚀\nnext", maxLength: 100), "Cafe / deja vu + ? next")
        XCTAssertEqual(FrameCodec.sanitize("  lots   of\t\tspace  ", maxLength: 100), "lots of space")
        XCTAssertEqual(FrameCodec.sanitize("abcdefghij", maxLength: 4), "abcd")
        XCTAssertEqual(FrameCodec.sanitize("“smart” – quotes…", maxLength: 100), "\"smart\" - quotes.")
    }

    func testEncoderShortensLongestFieldToFit() throws {
        let frame = Frame("ALR", ["e1", "e", "proj", String(repeating: "t", count: 100), String(repeating: "m", count: 200)])
        let data = FrameCodec.encode(frame)
        XCTAssertLessThanOrEqual(data.count, FDP.maxFrameLength)
        var decoder = FrameDecoder()
        let decoded = try XCTUnwrap(decoder.feed(data).first).get()
        XCTAssertEqual(decoded.fields[3].count, 100)
        XCTAssertTrue(decoded.fields[4].hasPrefix("mmm"))
    }

    func testDecoderHandlesArbitraryChunking() throws {
        let frames = (0..<20).map { Frame("EVT", ["e\($0)", "i", "12:0\($0 % 10)", "proj", "Event number \($0)"]) }
        let stream = frames.map(FrameCodec.encode).reduce(Data(), +)
        for chunkSize in [1, 3, 20, 185, 243, stream.count] {
            var decoder = FrameDecoder()
            var decoded: [Frame] = []
            var offset = 0
            while offset < stream.count {
                let end = min(offset + chunkSize, stream.count)
                decoded += try decoder.feed(stream.subdata(in: offset..<end)).map { try $0.get() }
                offset = end
            }
            XCTAssertEqual(decoded, frames, "chunk size \(chunkSize)")
        }
    }

    func testDecoderRejectsCorruptionAndResynchronizes() {
        var decoder = FrameDecoder()
        let good = FrameCodec.encode(Frame("PING", ["1", "2"]))
        var corrupted = good
        corrupted[2] = UInt8(ascii: "X")
        let overlong = Data(repeating: UInt8(ascii: "A"), count: 300) + Data("\n".utf8)
        let binary = Data([0x50, 0x00, 0x01, 0x0A])
        let noCRC = Data("PING|1|2\n".utf8)
        let lowercaseType = Data("ping|1*0000\n".utf8)

        let results = decoder.feed(corrupted + overlong + binary + noCRC + lowercaseType + good)
        let errors = results.compactMap { result -> FrameError? in
            if case .failure(let error) = result { return error }
            return nil
        }
        // The CRC is checked before the type, so a bad-CRC lowercase frame is a checksum error.
        XCTAssertEqual(errors, [.badChecksum, .tooLong, .nonPrintable, .malformed, .badChecksum])
        XCTAssertEqual(try results.last?.get(), Frame("PING", ["1", "2"]))
    }

    /// Golden vectors shared with the Flipper's C implementation.
    func testGoldenVectors() throws {
        let url = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .appendingPathComponent("docs/protocol-vectors.txt")
        let text = try String(contentsOf: url, encoding: .utf8)
        var checked = 0
        for line in text.split(separator: "\n") where !line.hasPrefix("#") && !line.isEmpty {
            // "OK <frame>" or "BAD <reason> <frame>"
            let parts = line.split(separator: " ", maxSplits: 1)
            let kind = parts[0]
            let rest = String(parts[1])
            if kind == "OK" {
                let result = FrameCodec.decodeLine(Array(rest.utf8))
                let frame = try result.get()
                XCTAssertEqual(FrameCodec.encode(frame), Data((rest + "\n").utf8), rest)
            } else {
                let reasonAndFrame = rest.split(separator: " ", maxSplits: 1)
                let bytes: [UInt8] = reasonAndFrame.count > 1 ? Array(reasonAndFrame[1].utf8) : []
                let result = FrameCodec.decodeLine(bytes)
                guard case .failure(let error) = result else { XCTFail("expected failure: \(line)"); continue }
                XCTAssertEqual(String(describing: error), String(reasonAndFrame[0]), String(line))
            }
            checked += 1
        }
        XCTAssertGreaterThan(checked, 5)
    }
}

final class SnapshotBuilderTests: XCTestCase {
    func bigState() -> EngineState {
        var state = EngineState()
        for i in 0..<30 {
            var p = project("/dev/project-\(i)", name: "project-\(i)-with-a-rather-long-name")
            p.git = GitStatus(branch: "feature/very-long-branch-name-\(i)-and-more", headOID: "x", ahead: i, behind: 0, changedCount: i % 2,
                              lastCommit: CommitInfo(oid: "x", subject: "s", date: .at(TimeInterval(i))))
            state.projects.append(p)
        }
        state.servers = (0..<12).map { server(pid: Int32(100 + $0), port: 3000 + $0, projectID: "/dev/project-\($0 + 5)") }
        state.agents = (0..<9).map { AgentSession(providerID: "codex", providerName: "Codex", pid: Int32(500 + $0), startedAt: .at(1), projectID: "/dev/project-1", command: "codex") }
        return state
    }

    func testRespectsLimitsAndFrameSize() {
        let state = bigState()
        let events = (0..<30).map { FDEvent(id: "ev\($0)", timestamp: .at(TimeInterval($0) * 60), source: .process, projectName: "project-\($0)", type: .serverStarted, severity: .info, title: String(repeating: "Title ", count: 20), actions: ActionCatalog.actions(for: state.servers[0])) }
        let snapshot = FlipperSnapshotBuilder.build(state: state, attention: [], events: events, timeZone: TimeZone(identifier: "UTC")!)
        let counts = Dictionary(grouping: snapshot.records, by: \.type).mapValues(\.count)
        XCTAssertEqual(counts["SUM"], 1)
        XCTAssertEqual(counts["PRJ"], FlipperLimits.projects)
        XCTAssertEqual(counts["SVC"], FlipperLimits.services)
        XCTAssertEqual(counts["AGT"], FlipperLimits.agents)
        XCTAssertEqual(counts["EVT"], FlipperLimits.events)
        XCTAssertLessThanOrEqual(counts["ACT"] ?? 0, FlipperLimits.actions)
        XCTAssertEqual(snapshot.records.first { $0.type == "SUM" }?.fields, ["0", "30", "12", "9"])
        for record in snapshot.records {
            XCTAssertLessThanOrEqual(FrameCodec.encode(record).count, FDP.maxFrameLength)
        }
        XCTAssertEqual(snapshot.actions.count, counts["ACT"])
        // No Mac-only actions on the wire.
        XCTAssertFalse(snapshot.actions.values.contains { !$0.kind.availableRemotely })
        XCTAssertEqual(snapshot.records.first { $0.type == "EVT" }?.fields[2], "00:00")
    }

    func testProjectOrderingPutsAttentionThenActiveFirst() {
        let state = bigState()
        let attention = [AttentionItem(id: "x", severity: .error, projectID: "/dev/project-0", projectName: "p0", title: "Deploy failed", eventID: nil, actions: [])]
        let snapshot = FlipperSnapshotBuilder.build(state: state, attention: attention, events: [])
        let projects = snapshot.records.filter { $0.type == "PRJ" }
        XCTAssertEqual(projects[0].fields[1], FrameCodec.sanitize("project-0-with-a-rather-long-name", maxLength: FlipperLimits.name))
        XCTAssertEqual(projects[0].fields[8], "1")
        // Then active projects (servers/agents) by most recent commit: project-16, project-15, ...
        XCTAssertTrue(projects[1].fields[1].hasPrefix("project-16"))
        XCTAssertEqual(projects[1].fields[6], "3011")
        XCTAssertTrue(projects[2].fields[1].hasPrefix("project-15"))
        // Idle projects never displace active ones.
        XCTAssertFalse(projects.contains { $0.fields[1].hasPrefix("project-29") })
    }

    func testIDsAreStableAcrossBuilds() {
        let state = bigState()
        let a = FlipperSnapshotBuilder.build(state: state, attention: [], events: [])
        let b = FlipperSnapshotBuilder.build(state: state, attention: [], events: [])
        XCTAssertEqual(a, b)
        XCTAssertTrue(a.records.allSatisfy { $0.type == "SUM" || ($0.fields.first?.count ?? 0) <= FlipperLimits.id })
    }

    func testMachineFrameQuantizes() {
        let machine = MachineStatus(hostName: "Mo’s MacBook Pro", osVersion: "15", cpuUsage: 0.234, memoryUsedBytes: 6, memoryTotalBytes: 8,
                                    battery: BatteryStatus(percent: 81, charging: true, onAC: true), networkReachable: true, bootTime: .at(1000))
        XCTAssertEqual(FlipperSnapshotBuilder.machineFrame(machine).fields, ["Mo's MacBook Pro", "25", "75", "81", "1", "1", "1000"])
        XCTAssertEqual(FlipperSnapshotBuilder.machineFrame(MachineStatus(hostName: "h", osVersion: "")).fields, ["h", "-", "-", "-", "-", "-", "-"])
    }
}
