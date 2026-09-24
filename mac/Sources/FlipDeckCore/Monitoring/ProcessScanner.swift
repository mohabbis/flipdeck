import Foundation
#if canImport(Glibc)
import Glibc
#elseif canImport(Darwin)
import Darwin
#endif

public struct ProcessRecord: Hashable, Sendable {
    public let pid: Int32
    public let ppid: Int32
    public let uid: UInt32
    public let startedAt: Date?
    /// Full argument vector as `ps` reports it.
    public let args: String

    public init(pid: Int32, ppid: Int32, uid: UInt32, startedAt: Date?, args: String) {
        self.pid = pid
        self.ppid = ppid
        self.uid = uid
        self.startedAt = startedAt
        self.args = args
    }

    /// Whitespace-split arguments. Paths containing spaces get split too; the
    /// classifiers only look for well-known tokens, so that's acceptable.
    public var tokens: [Substring] { args.split(separator: " ", omittingEmptySubsequences: true) }

    public var executableName: String {
        guard let first = tokens.first else { return "" }
        return String(first.split(separator: "/").last ?? first)
    }
}

/// Parses `ps -axww -o pid=,ppid=,uid=,lstart=,args=`. The same invocation
/// works on macOS and Linux (procps).
public enum PSParser {
    public static let arguments = ["-axww", "-o", "pid=,ppid=,uid=,lstart=,args="]

    private static let formatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone.current
        formatter.dateFormat = "EEE MMM d HH:mm:ss yyyy"
        return formatter
    }()
    private static let formatterLock = NSLock()

    public static func parse(_ output: String) -> [ProcessRecord] {
        var records: [ProcessRecord] = []
        records.reserveCapacity(512)
        for line in output.split(separator: "\n") {
            if let record = parseLine(line) { records.append(record) }
        }
        return records
    }

    static func parseLine(_ line: Substring) -> ProcessRecord? {
        // pid ppid uid DOW MON DAY HH:MM:SS YEAR args...
        var fields: [Substring] = []
        var index = line.startIndex
        while fields.count < 8 {
            while index < line.endIndex, line[index] == " " { index = line.index(after: index) }
            guard index < line.endIndex else { return nil }
            let start = index
            while index < line.endIndex, line[index] != " " { index = line.index(after: index) }
            fields.append(line[start..<index])
        }
        while index < line.endIndex, line[index] == " " { index = line.index(after: index) }
        let args = String(line[index...])

        guard let pid = Int32(fields[0]), let ppid = Int32(fields[1]), let uid = UInt32(fields[2]) else { return nil }
        let dateString = fields[3...7].joined(separator: " ")
        formatterLock.lock()
        let startedAt = formatter.date(from: dateString)
        formatterLock.unlock()
        return ProcessRecord(pid: pid, ppid: ppid, uid: uid, startedAt: startedAt, args: args)
    }
}

public struct Listener: Hashable, Sendable {
    public let pid: Int32
    public let command: String
    public let host: String
    public let port: Int
}

/// Parses `lsof -F` field output (`p<pid>`, `c<command>`, `n<name>`, ...).
public enum LsofParser {
    public static let listenArguments = ["-nP", "-iTCP", "-sTCP:LISTEN", "-F", "pcn"]

    public static func cwdArguments(pids: [Int32]) -> [String] {
        ["-a", "-d", "cwd", "-p", pids.map(String.init).joined(separator: ","), "-F", "pn"]
    }

    public static func parseListeners(_ output: String) -> [Listener] {
        var listeners: [Listener] = []
        var seen: Set<String> = []
        var pid: Int32?
        var command = ""
        for line in output.split(separator: "\n") {
            guard let tag = line.first else { continue }
            let value = line.dropFirst()
            switch tag {
            case "p":
                pid = Int32(value)
                command = ""
            case "c":
                command = String(value)
            case "n":
                guard let pid, let (host, port) = splitHostPort(value) else { continue }
                // Dual-stack servers show up once per address family; one row is enough.
                if seen.insert("\(pid):\(port)").inserted {
                    listeners.append(Listener(pid: pid, command: command, host: host, port: port))
                }
            default:
                continue
            }
        }
        return listeners
    }

    /// "*:3000", "127.0.0.1:5173", "[::1]:8080" → (host, port)
    static func splitHostPort(_ name: Substring) -> (String, Int)? {
        guard let colon = name.lastIndex(of: ":"), let port = Int(name[name.index(after: colon)...]) else { return nil }
        var host = String(name[..<colon])
        if host.hasPrefix("[") && host.hasSuffix("]") { host = String(host.dropFirst().dropLast()) }
        return (host, port)
    }

    public static func parseCwds(_ output: String) -> [Int32: String] {
        var result: [Int32: String] = [:]
        var pid: Int32?
        for line in output.split(separator: "\n") {
            guard let tag = line.first else { continue }
            let value = line.dropFirst()
            if tag == "p" { pid = Int32(value) }
            else if tag == "n", let pid { result[pid] = String(value) }
        }
        return result
    }
}

public struct ProcessTableSnapshot: Sendable {
    public let records: [ProcessRecord]
    public let listeners: [Listener]
    public let cwds: [Int32: String]
    public let takenAt: Date
}

/// Runs `ps` and `lsof`. Cwd lookups are batched and only done for the few
/// processes FlipDeck actually cares about.
public struct ProcessScanner: Sendable {
    let runner: CommandRunner
    let psPath: String?
    let lsofPath: String?

    public init(runner: CommandRunner, psPath: String? = ToolLocator.find("ps"), lsofPath: String? = ToolLocator.find("lsof")) {
        self.runner = runner
        self.psPath = psPath
        self.lsofPath = lsofPath
    }

    public var currentUID: UInt32 { UInt32(getuid()) }

    public func processTable() async throws -> [ProcessRecord] {
        guard let psPath else { throw CommandError.toolNotFound("ps") }
        let result = try await runner.run(psPath, PSParser.arguments, cwd: nil, timeout: 10)
        guard result.succeeded else { throw CommandError.launchFailed("ps exited \(result.exitCode)") }
        return PSParser.parse(result.stdout)
    }

    public func listeners() async throws -> [Listener] {
        guard let lsofPath else { throw CommandError.toolNotFound("lsof") }
        let result = try await runner.run(lsofPath, LsofParser.listenArguments, cwd: nil, timeout: 10)
        // lsof exits 1 when nothing matches; output is still authoritative.
        return LsofParser.parseListeners(result.stdout)
    }

    public func cwds(for pids: [Int32]) async -> [Int32: String] {
        guard let lsofPath, !pids.isEmpty else { return [:] }
        guard let result = try? await runner.run(lsofPath, LsofParser.cwdArguments(pids: pids), cwd: nil, timeout: 10) else { return [:] }
        return LsofParser.parseCwds(result.stdout)
    }
}
