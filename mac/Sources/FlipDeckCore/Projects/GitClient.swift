import Foundation

/// Parses `git status --porcelain=v2 --branch -z --no-renames`.
public enum GitStatusParser {
    public static func parse(_ output: String) -> GitStatus {
        var status = GitStatus()
        for entry in output.split(separator: "\0", omittingEmptySubsequences: true) {
            if entry.hasPrefix("# ") {
                parseHeader(entry.dropFirst(2), into: &status)
                continue
            }
            switch entry.first {
            case "1", "2": status.changedCount += 1
            case "u": status.conflictedCount += 1
            case "?": status.untrackedCount += 1
            default: break
            }
        }
        return status
    }

    private static func parseHeader(_ header: Substring, into status: inout GitStatus) {
        let parts = header.split(separator: " ", maxSplits: 1, omittingEmptySubsequences: false)
        guard parts.count == 2 else { return }
        let value = String(parts[1])
        switch parts[0] {
        case "branch.oid":
            status.headOID = value == "(initial)" ? nil : value
        case "branch.head":
            status.branch = value == "(detached)" ? nil : value
        case "branch.upstream":
            status.upstream = value
        case "branch.ab":
            // "+<ahead> -<behind>"
            let numbers = value.split(separator: " ").compactMap { Int($0.dropFirst()) }
            if numbers.count == 2 {
                status.ahead = numbers[0]
                status.behind = numbers[1]
            }
        default:
            break
        }
    }

    /// Parses `git log -1 --format=%H%x00%ct%x00%s`.
    public static func parseLastCommit(_ output: String) -> CommitInfo? {
        let parts = output.trimmingCharacters(in: .newlines).split(separator: "\0", maxSplits: 2, omittingEmptySubsequences: false)
        guard parts.count == 3, let seconds = TimeInterval(parts[1]) else { return nil }
        return CommitInfo(oid: String(parts[0]), subject: String(parts[2]), date: Date(timeIntervalSince1970: seconds))
    }

    /// Removes credentials from remote URLs (`https://user:token@host/...`)
    /// so they never reach the UI, logs, or the Flipper.
    public static func sanitizeRemoteURL(_ url: String) -> String {
        let trimmed = url.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let schemeRange = trimmed.range(of: "://") else { return trimmed }
        let afterScheme = trimmed[schemeRange.upperBound...]
        guard let at = afterScheme.firstIndex(of: "@") else { return trimmed }
        let slash = afterScheme.firstIndex(of: "/") ?? afterScheme.endIndex
        guard at < slash else { return trimmed }
        return String(trimmed[..<schemeRange.upperBound]) + String(afterScheme[afterScheme.index(after: at)...])
    }
}

public enum GitError: Error, CustomStringConvertible {
    case gitUnavailable
    case failed(String)

    public var description: String {
        switch self {
        case .gitUnavailable: return "git is not installed"
        case .failed(let message): return message
        }
    }
}

public struct GitClient: Sendable {
    let runner: CommandRunner
    let gitPath: String?
    let timeout: TimeInterval

    public init(runner: CommandRunner, gitPath: String? = ToolLocator.find("git"), timeout: TimeInterval = 8) {
        self.runner = runner
        self.gitPath = gitPath
        self.timeout = timeout
    }

    private func git(_ arguments: [String], in path: String) async throws -> CommandResult {
        guard let gitPath else { throw GitError.gitUnavailable }
        return try await runner.run(gitPath, ["-C", path] + arguments, cwd: nil, timeout: timeout)
    }

    public func status(at path: String) async throws -> GitStatus {
        let result = try await git(["status", "--porcelain=v2", "--branch", "-z", "--no-renames"], in: path)
        guard result.succeeded else {
            throw GitError.failed(result.stderr.trimmingCharacters(in: .whitespacesAndNewlines))
        }
        return GitStatusParser.parse(result.stdout)
    }

    public func lastCommit(at path: String) async throws -> CommitInfo? {
        let result = try await git(["log", "-1", "--format=%H%x00%ct%x00%s"], in: path)
        // An empty repository has no commits; that's not an error.
        guard result.succeeded else { return nil }
        return GitStatusParser.parseLastCommit(result.stdout)
    }

    public func remoteURL(at path: String) async throws -> String? {
        let result = try await git(["config", "--get", "remote.origin.url"], in: path)
        guard result.succeeded else { return nil }
        let url = GitStatusParser.sanitizeRemoteURL(result.stdout)
        return url.isEmpty ? nil : url
    }
}
