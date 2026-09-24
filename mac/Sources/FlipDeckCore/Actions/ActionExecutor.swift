import Foundation

/// OS side effects. The macOS implementation uses NSWorkspace/AppKit; tests
/// use a recorder. Nothing here takes free-form commands.
public protocol SystemEffects: Sendable {
    func openURL(_ url: URL) async throws
    /// Opens a directory in the configured editor (bundle id) or Finder.
    /// Returns the name of the application used.
    func openProject(at path: String, editorBundleID: String?) async throws -> String
    func openTerminal(at path: String) async throws
    func revealInFinder(_ path: String) async throws
    func copyToClipboard(_ string: String) async
    /// Sends SIGTERM (never SIGKILL) to a process owned by the current user.
    func terminate(pid: Int32) throws
}

public enum ActionOrigin: String, Sendable {
    case mac
    case flipper
}

public struct ActionPolicy: Sendable {
    public var allowDestructiveFromFlipper: Bool
    public var editorBundleID: String?

    public init(allowDestructiveFromFlipper: Bool = true, editorBundleID: String? = nil) {
        self.allowDestructiveFromFlipper = allowDestructiveFromFlipper
        self.editorBundleID = editorBundleID
    }
}

/// Validates an action against *current* state and performs it. An action
/// built a minute ago may no longer be valid (server stopped, PID reused);
/// the executor re-checks everything instead of trusting the action.
public struct ActionExecutor: Sendable {
    let effects: SystemEffects
    let fileManager: FileManager

    public init(effects: SystemEffects, fileManager: FileManager = .default) {
        self.effects = effects
        self.fileManager = fileManager
    }

    public func perform(_ action: FDAction, origin: ActionOrigin, state: EngineState, policy: ActionPolicy) async -> ActionResult {
        if origin == .flipper {
            guard action.kind.availableRemotely else { return .failure("Not available from Flipper") }
            if action.destructive && !policy.allowDestructiveFromFlipper {
                return .failure("Disabled in Mac settings")
            }
        }
        do {
            switch (action.kind, action.target) {
            case (.openOnMac, .project(let path)):
                try requireProject(path, in: state)
                let app = try await effects.openProject(at: path, editorBundleID: policy.editorBundleID)
                return .success("Opened in \(app)")

            case (.openTerminal, .project(let path)):
                try requireProject(path, in: state)
                try await effects.openTerminal(at: path)
                return .success("Opened Terminal")

            case (.openInFinder, .project(let path)):
                try requireProject(path, in: state)
                try await effects.revealInFinder(path)
                return .success("Revealed in Finder")

            case (.openLocalhost, .process(let pid, let startedAt, let port)):
                let server = try requireServer(pid: pid, startedAt: startedAt, in: state)
                let chosen = port.flatMap { server.ports.contains($0) ? $0 : nil } ?? server.primaryPort
                try await effects.openURL(URL(string: "http://localhost:\(chosen)")!)
                return .success("Opened localhost:\(chosen)")

            case (.stopServer, .process(let pid, let startedAt, _)):
                let server = try requireServer(pid: pid, startedAt: startedAt, in: state)
                try effects.terminate(pid: server.pid)
                return .success("Stopping \(server.displayName) (pid \(server.pid))")

            case (.copyURL, .url(let string)):
                guard knownURLs(in: state).contains(string) else { throw ActionFailure("URL is no longer current") }
                await effects.copyToClipboard(string)
                return .success("Copied \(string)")

            case (.openLogs, .url(let string)), (.openDeployment, .url(let string)):
                guard knownURLs(in: state).contains(string), let url = URL(string: string), url.scheme == "https" else {
                    throw ActionFailure("Link is no longer current")
                }
                try await effects.openURL(url)
                return .success(action.kind == .openLogs ? "Opened logs" : "Opened deployment")

            default:
                return .failure("Unsupported action")
            }
        } catch let failure as ActionFailure {
            return .failure(failure.message)
        } catch {
            return .failure("Failed: \(error.localizedDescription)")
        }
    }

    struct ActionFailure: Error {
        let message: String
        init(_ message: String) { self.message = message }
    }

    func requireProject(_ path: String, in state: EngineState) throws {
        var isDirectory: ObjCBool = false
        guard state.projects.contains(where: { $0.path == path }),
              fileManager.fileExists(atPath: path, isDirectory: &isDirectory), isDirectory.boolValue else {
            throw ActionFailure("Project not found")
        }
    }

    func requireServer(pid: Int32, startedAt: Date?, in state: EngineState) throws -> DevServer {
        guard let server = state.servers.first(where: { $0.pid == pid }) else {
            throw ActionFailure("Server is no longer running")
        }
        // PID reuse guard: the process must be the one the action was made for.
        if let expected = startedAt, let actual = server.startedAt, abs(expected.timeIntervalSince(actual)) > 1 {
            throw ActionFailure("Server changed; refresh and retry")
        }
        return server
    }

    func knownURLs(in state: EngineState) -> Set<String> {
        var urls = Set(state.servers.map(\.url))
        for deployments in state.deployments.values {
            for deployment in deployments {
                if let url = deployment.url { urls.insert(url) }
                if let inspector = deployment.inspectorURL { urls.insert(inspector) }
            }
        }
        return urls
    }
}
