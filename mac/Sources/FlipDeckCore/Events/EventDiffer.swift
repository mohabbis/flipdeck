import Foundation

/// Derives normalized events by comparing two consecutive engine states.
/// Pure and deterministic: the same pair of states always yields the same
/// events with the same ids, so a repeated diff can be de-duplicated.
public enum EventDiffer {
    public static func diff(old: EngineState, new: EngineState, now: Date) -> [FDEvent] {
        var events: [FDEvent] = []
        if old.observed.processes && new.observed.processes {
            events += diffServers(old: old, new: new, now: now)
            events += diffAgents(old: old, new: new, now: now)
        }
        events += diffGit(old: old, new: new, now: now)
        events += diffDeployments(old: old, new: new, now: now)
        return events
    }

    static func projectName(_ state: EngineState, _ id: String?) -> String? {
        state.project(id: id)?.name
    }

    static func diffServers(old: EngineState, new: EngineState, now: Date) -> [FDEvent] {
        func key(_ server: DevServer) -> String { "\(server.pid):\(server.primaryPort)" }
        let oldKeys = Dictionary(old.servers.map { (key($0), $0) }, uniquingKeysWith: { a, _ in a })
        let newKeys = Dictionary(new.servers.map { (key($0), $0) }, uniquingKeysWith: { a, _ in a })
        var events: [FDEvent] = []

        for server in new.servers where oldKeys[key(server)] == nil {
            let name = projectName(new, server.projectID)
            events.append(FDEvent(
                id: "server.started:\(key(server)):\(Int(server.startedAt?.timeIntervalSince1970 ?? 0))",
                timestamp: now, source: .process, projectID: server.projectID, projectName: name,
                type: .serverStarted, severity: .info,
                title: "\(server.displayName) started on :\(server.primaryPort)",
                message: name.map { "in \($0)" } ?? server.processName,
                actions: ActionCatalog.actions(for: server),
                metadata: ["pid": String(server.pid), "port": String(server.primaryPort)]
            ))
        }
        for server in old.servers where newKeys[key(server)] == nil {
            let name = projectName(old, server.projectID)
            events.append(FDEvent(
                id: "server.stopped:\(key(server)):\(Int(now.timeIntervalSince1970))",
                timestamp: now, source: .process, projectID: server.projectID, projectName: name,
                type: .serverStopped, severity: .info,
                title: "\(server.displayName) stopped (:\(server.primaryPort))",
                message: name.map { "in \($0)" } ?? server.processName,
                metadata: ["pid": String(server.pid), "port": String(server.primaryPort)]
            ))
        }
        return events
    }

    static func diffAgents(old: EngineState, new: EngineState, now: Date) -> [FDEvent] {
        let oldIDs = Set(old.agents.map(\.id))
        let newIDs = Set(new.agents.map(\.id))
        var events: [FDEvent] = []

        for agent in new.agents where !oldIDs.contains(agent.id) {
            let name = projectName(new, agent.projectID)
            events.append(FDEvent(
                id: "agent.started:\(agent.id)",
                timestamp: now, source: .agent, projectID: agent.projectID, projectName: name,
                type: .agentStarted, severity: .info,
                title: "\(agent.providerName) started",
                message: name.map { "in \($0)" } ?? agent.cwd ?? "",
                metadata: ["provider": agent.providerID, "pid": String(agent.pid)]
            ))
        }
        for agent in old.agents where !newIDs.contains(agent.id) {
            let name = projectName(old, agent.projectID)
            var detail = name.map { "in \($0)" } ?? ""
            if let started = agent.startedAt {
                detail += (detail.isEmpty ? "" : " ") + "after \(Format.duration(now.timeIntervalSince(started)))"
            }
            var actions: [FDAction] = []
            if let projectID = agent.projectID, let project = old.project(id: projectID) {
                actions = [FDAction(kind: .openOnMac, target: .project(path: project.path), source: .agent)]
            }
            events.append(FDEvent(
                id: "agent.exited:\(agent.id)",
                timestamp: now, source: .agent, projectID: agent.projectID, projectName: name,
                type: .agentExited, severity: .info,
                title: "\(agent.providerName) finished",
                message: detail + (detail.isEmpty ? "" : " · ") + "exit status not available",
                actions: actions,
                metadata: ["provider": agent.providerID, "pid": String(agent.pid)]
            ))
        }
        return events
    }

    static func diffGit(old: EngineState, new: EngineState, now: Date) -> [FDEvent] {
        let oldProjects = Dictionary(old.projects.map { ($0.id, $0) }, uniquingKeysWith: { a, _ in a })
        var events: [FDEvent] = []
        for project in new.projects {
            // Only compare two real observations; a project's first status is its baseline.
            guard let newGit = project.git, let oldGit = oldProjects[project.id]?.git else { continue }
            if newGit.branch != oldGit.branch {
                let branch = newGit.branch ?? "detached HEAD"
                events.append(FDEvent(
                    id: "git.branch:\(project.id):\(branch):\(newGit.headOID ?? "")",
                    timestamp: now, source: .git, projectID: project.id, projectName: project.name,
                    type: .gitChanged, severity: .info,
                    title: "Switched to \(branch)",
                    message: project.name,
                    metadata: ["change": "branch", "branch": branch]
                ))
            } else if let newOID = newGit.headOID, newOID != oldGit.headOID {
                let subject = newGit.lastCommit?.oid == newOID ? newGit.lastCommit?.subject : nil
                events.append(FDEvent(
                    id: "git.commit:\(project.id):\(newOID)",
                    timestamp: now, source: .git, projectID: project.id, projectName: project.name,
                    type: .gitChanged, severity: .info,
                    title: "New commit on \(newGit.branch ?? "HEAD")",
                    message: subject ?? String(newOID.prefix(8)),
                    metadata: ["change": "commit", "oid": newOID]
                ))
            }
        }
        return events
    }

    static func diffDeployments(old: EngineState, new: EngineState, now: Date) -> [FDEvent] {
        var events: [FDEvent] = []
        for (projectID, deployments) in new.deployments {
            // First successful fetch for a project is the baseline.
            guard old.observed.deploymentsByProject.contains(projectID) else { continue }
            let previous = Dictionary((old.deployments[projectID] ?? []).map { ($0.id, $0.state) }, uniquingKeysWith: { a, _ in a })
            let name = projectName(new, projectID)
            for deployment in deployments.reversed() {
                let before = previous[deployment.id]
                guard before != deployment.state else { continue }
                // Deployments older than what we previously tracked fell out of
                // the window, not newly created: don't announce them.
                if before == nil, let oldest = old.deployments[projectID]?.last, deployment.createdAt < oldest.createdAt { continue }
                guard let event = deploymentEvent(deployment, before: before, projectName: name, now: now) else { continue }
                events.append(event)
            }
        }
        return events
    }

    static func deploymentEvent(_ deployment: Deployment, before: DeploymentState?, projectName: String?, now: Date) -> FDEvent? {
        let target = deployment.targetLabel
        let type: EventType
        let severity: Severity
        let title: String
        var message = [deployment.branch, deployment.commitMessage].compactMap { $0 }.joined(separator: " · ")
        switch deployment.state {
        case .queued, .building:
            // One "started" per deployment, even if it goes queued → building.
            guard before == nil else { return nil }
            type = .deploymentStarted
            severity = .info
            title = "\(target) deployment started"
        case .ready:
            type = .deploymentSucceeded
            severity = .success
            title = "\(target) deployment is live"
        case .error:
            type = .deploymentFailed
            severity = deployment.isProduction ? .error : .warning
            title = "\(target) deployment failed"
            if let error = deployment.errorMessage { message = error }
        case .canceled:
            type = .deploymentCanceled
            severity = .info
            title = "\(target) deployment canceled"
        }
        return FDEvent(
            id: "\(type.rawValue):\(deployment.id)",
            timestamp: now, source: .vercel, projectID: deployment.projectID, projectName: projectName,
            type: type, severity: severity, title: title, message: message,
            actions: ActionCatalog.actions(for: deployment),
            metadata: ["deployment": deployment.id, "target": deployment.target ?? "preview"]
        )
    }
}

public enum Format {
    public static func duration(_ seconds: TimeInterval) -> String {
        let total = max(0, Int(seconds))
        if total < 60 { return "\(total)s" }
        if total < 3600 { return "\(total / 60)m" }
        if total < 86_400 { return "\(total / 3600)h \((total % 3600) / 60)m" }
        return "\(total / 86_400)d \((total % 86_400) / 3600)h"
    }
}
