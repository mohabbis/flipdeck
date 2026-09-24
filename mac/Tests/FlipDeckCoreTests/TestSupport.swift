import Foundation
import XCTest
@testable import FlipDeckCore

/// Temporary directory removed at deinit.
final class TempDir {
    let path: String

    init() {
        let base = FileManager.default.temporaryDirectory.appendingPathComponent("flipdeck-tests-\(UUID().uuidString)")
        try! FileManager.default.createDirectory(at: base, withIntermediateDirectories: true)
        // Resolve symlinks (/tmp → /private/tmp on macOS) so paths match what tools report.
        path = base.resolvingSymlinksInPath().path
    }

    deinit { try? FileManager.default.removeItem(atPath: path) }

    @discardableResult
    func mkdir(_ relative: String) -> String {
        let full = path + "/" + relative
        try! FileManager.default.createDirectory(atPath: full, withIntermediateDirectories: true)
        return full
    }

    func write(_ relative: String, _ contents: String) {
        let full = path + "/" + relative
        try! FileManager.default.createDirectory(atPath: (full as NSString).deletingLastPathComponent, withIntermediateDirectories: true)
        try! contents.write(toFile: full, atomically: true, encoding: .utf8)
    }
}

/// Runs real commands synchronously for test setup.
@discardableResult
func sh(_ command: String, cwd: String? = nil) throws -> String {
    let result = try ProcessCommandRunner.runSync("/bin/sh", ["-c", command], cwd: cwd, timeout: 30)
    if !result.succeeded {
        throw NSError(domain: "sh", code: Int(result.exitCode), userInfo: [NSLocalizedDescriptionKey: "\(command): \(result.stderr)"])
    }
    return result.stdout
}

func makeGitRepo(_ path: String) throws {
    try sh("git init -q -b main . && git config user.email t@example.com && git config user.name Test && git config commit.gpgsign false", cwd: path)
}

func commit(_ path: String, file: String = "README.md", message: String = "commit") throws {
    try sh("echo \(UUID().uuidString) >> \(file) && git add -A && git commit -q -m '\(message)'", cwd: path)
}

/// Fake command runner keyed by the executable's basename + first argument.
final class FakeRunner: CommandRunner, @unchecked Sendable {
    var responses: [String: CommandResult] = [:]
    var calls: [[String]] = []
    private let lock = NSLock()

    private func recordCall(_ call: [String]) {
        lock.lock()
        calls.append(call)
        lock.unlock()
    }

    func run(_ executable: String, _ arguments: [String], cwd: String?, timeout: TimeInterval) async throws -> CommandResult {
        recordCall([executable] + arguments)
        let name = URL(fileURLWithPath: executable).lastPathComponent
        return responses[name] ?? CommandResult(exitCode: 1, stdout: "", stderr: "no fake for \(name)")
    }
}

/// Records effects instead of performing them.
final class RecordingEffects: SystemEffects, @unchecked Sendable {
    private let lock = NSLock()
    private(set) var log: [String] = []

    private func record(_ entry: String) {
        lock.lock()
        log.append(entry)
        lock.unlock()
    }

    var entries: [String] {
        lock.lock()
        defer { lock.unlock() }
        return log
    }

    func openURL(_ url: URL) async throws { record("open \(url.absoluteString)") }
    func openProject(at path: String, editorBundleID: String?) async throws -> String {
        record("project \(path)")
        return "Editor"
    }
    func openTerminal(at path: String) async throws { record("terminal \(path)") }
    func revealInFinder(_ path: String) async throws { record("finder \(path)") }
    func copyToClipboard(_ string: String) async { record("copy \(string)") }
    func terminate(pid: Int32) throws { record("terminate \(pid)") }
}

/// Fake HTTP client returning canned responses per path.
final class FakeHTTP: HTTPClient, @unchecked Sendable {
    var handler: (URL) -> HTTPResponse
    private(set) var requests: [(URL, [String: String])] = []
    private let lock = NSLock()

    init(handler: @escaping (URL) -> HTTPResponse) { self.handler = handler }

    private func recordRequest(_ url: URL, _ headers: [String: String]) -> (URL) -> HTTPResponse {
        lock.lock()
        defer { lock.unlock() }
        requests.append((url, headers))
        return handler
    }

    func get(_ url: URL, headers: [String: String]) async throws -> HTTPResponse {
        recordRequest(url, headers)(url)
    }
}

extension Date {
    static func at(_ seconds: TimeInterval) -> Date { Date(timeIntervalSince1970: seconds) }
}

func project(_ path: String, name: String? = nil, git: GitStatus? = GitStatus(branch: "main", headOID: "aaa")) -> Project {
    Project(path: path, name: name ?? URL(fileURLWithPath: path).lastPathComponent, isGitRepository: true, git: git)
}

func server(pid: Int32, port: Int, projectID: String? = nil, startedAt: Date? = .at(1_000)) -> DevServer {
    DevServer(pid: pid, ports: [port], processName: "node", command: "node vite", framework: "Vite", startedAt: startedAt, projectID: projectID)
}
