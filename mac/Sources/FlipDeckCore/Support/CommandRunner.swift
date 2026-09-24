import Foundation

public struct CommandResult: Sendable, Equatable {
    public let exitCode: Int32
    public let stdout: String
    public let stderr: String

    public init(exitCode: Int32, stdout: String, stderr: String) {
        self.exitCode = exitCode
        self.stdout = stdout
        self.stderr = stderr
    }

    public var succeeded: Bool { exitCode == 0 }
}

public enum CommandError: Error, Equatable, CustomStringConvertible {
    case toolNotFound(String)
    case launchFailed(String)
    case timedOut(String)

    public var description: String {
        switch self {
        case .toolNotFound(let tool): return "\(tool) not found"
        case .launchFailed(let reason): return "failed to launch: \(reason)"
        case .timedOut(let command): return "\(command) timed out"
        }
    }
}

/// Runs external tools. Everything FlipDeck learns from `git`, `ps` and `lsof`
/// goes through this seam so it can be faked in tests.
public protocol CommandRunner: Sendable {
    func run(_ executable: String, _ arguments: [String], cwd: String?, timeout: TimeInterval) async throws -> CommandResult
}

extension CommandRunner {
    public func run(_ executable: String, _ arguments: [String], cwd: String? = nil) async throws -> CommandResult {
        try await run(executable, arguments, cwd: cwd, timeout: 10)
    }
}

public struct ProcessCommandRunner: CommandRunner {
    public init() {}

    public func run(_ executable: String, _ arguments: [String], cwd: String?, timeout: TimeInterval) async throws -> CommandResult {
        try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.global(qos: .utility).async {
                do {
                    continuation.resume(returning: try Self.runSync(executable, arguments, cwd: cwd, timeout: timeout))
                } catch {
                    continuation.resume(throwing: error)
                }
            }
        }
    }

    private final class Collected: @unchecked Sendable {
        private let lock = NSLock()
        private var stdout = Data()
        private var stderr = Data()
        func set(stdout data: Data) { lock.lock(); stdout = data; lock.unlock() }
        func set(stderr data: Data) { lock.lock(); stderr = data; lock.unlock() }
        func get() -> (Data, Data) { lock.lock(); defer { lock.unlock() }; return (stdout, stderr) }
    }

    static func runSync(_ executable: String, _ arguments: [String], cwd: String?, timeout: TimeInterval) throws -> CommandResult {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments
        if let cwd { process.currentDirectoryURL = URL(fileURLWithPath: cwd) }

        var environment = ProcessInfo.processInfo.environment
        environment["LC_ALL"] = "C"
        environment["LANG"] = "C"
        // Never block on credential prompts, and never take the index lock
        // (so FlipDeck can't collide with the user's own git commands).
        environment["GIT_TERMINAL_PROMPT"] = "0"
        environment["GIT_OPTIONAL_LOCKS"] = "0"
        process.environment = environment

        let out = Pipe()
        let err = Pipe()
        process.standardOutput = out
        process.standardError = err
        process.standardInput = FileHandle.nullDevice

        do {
            try process.run()
        } catch {
            throw CommandError.launchFailed("\(executable): \(error.localizedDescription)")
        }

        // Drain both pipes concurrently; a full pipe buffer would otherwise
        // deadlock a chatty child (ps on a busy machine easily exceeds 64 KB).
        let collected = Collected()
        let group = DispatchGroup()
        group.enter()
        DispatchQueue.global(qos: .utility).async {
            collected.set(stdout: out.fileHandleForReading.readDataToEndOfFile())
            group.leave()
        }
        group.enter()
        DispatchQueue.global(qos: .utility).async {
            collected.set(stderr: err.fileHandleForReading.readDataToEndOfFile())
            group.leave()
        }

        if group.wait(timeout: .now() + timeout) == .timedOut {
            process.terminate()
            _ = group.wait(timeout: .now() + 2)
            throw CommandError.timedOut(([executable] + arguments.prefix(2)).joined(separator: " "))
        }
        process.waitUntilExit()

        let (stdoutData, stderrData) = collected.get()
        return CommandResult(
            exitCode: process.terminationStatus,
            stdout: String(decoding: stdoutData, as: UTF8.self),
            stderr: String(decoding: stderrData, as: UTF8.self)
        )
    }
}

/// Finds system tools without depending on the (often minimal) PATH a GUI app
/// inherits from launchd.
public enum ToolLocator {
    static let searchPaths = ["/usr/bin", "/bin", "/usr/sbin", "/sbin", "/opt/homebrew/bin", "/usr/local/bin"]

    public static func find(_ tool: String, fileManager: FileManager = .default) -> String? {
        for directory in searchPaths {
            let candidate = directory + "/" + tool
            if fileManager.isExecutableFile(atPath: candidate) { return candidate }
        }
        return nil
    }
}
