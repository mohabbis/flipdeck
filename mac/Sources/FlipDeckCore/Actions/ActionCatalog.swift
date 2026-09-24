import Foundation

/// Decides which actions are offered for each entity. Only actions that can
/// actually be carried out are listed (e.g. no "Open logs" without a logs URL).
public enum ActionCatalog {
    public static func actions(for project: Project) -> [FDAction] {
        [
            FDAction(kind: .openOnMac, target: .project(path: project.path), source: .system),
            FDAction(kind: .openTerminal, target: .project(path: project.path), source: .system),
            FDAction(kind: .openInFinder, target: .project(path: project.path), source: .system),
        ]
    }

    public static func actions(for server: DevServer) -> [FDAction] {
        let process = ActionTarget.process(pid: server.pid, startedAt: server.startedAt, port: server.primaryPort)
        return [
            FDAction(kind: .openLocalhost, target: process, source: .process),
            FDAction(kind: .copyURL, target: .url(server.url), source: .process),
            FDAction(kind: .stopServer, target: process, source: .process),
        ]
    }

    public static func actions(for deployment: Deployment) -> [FDAction] {
        var actions: [FDAction] = []
        if let inspector = deployment.inspectorURL {
            actions.append(FDAction(kind: .openLogs, target: .url(inspector), source: .vercel))
        }
        if deployment.state == .ready, let url = deployment.url {
            actions.append(FDAction(kind: .openDeployment, target: .url(url), source: .vercel))
        }
        return actions
    }
}
