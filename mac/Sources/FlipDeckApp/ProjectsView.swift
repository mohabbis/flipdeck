#if os(macOS)
import SwiftUI
import FlipDeckCore

struct ProjectsView: View {
    @EnvironmentObject private var model: AppModel
    @State private var search = ""

    var body: some View {
        let state = model.state
        let projects = state.projects
            .filter { search.isEmpty || $0.name.localizedCaseInsensitiveContains(search) || $0.path.localizedCaseInsensitiveContains(search) }
            .sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }

        if state.projects.isEmpty {
            EmptyState(
                title: model.snapshot?.settings.projectRoots.isEmpty ?? true ? "No project folders" : "No projects found",
                symbol: "folder.badge.questionmark",
                message: "Add the folders where you keep code (for example ~/Developer) in Settings. FlipDeck finds Git repositories up to three levels deep."
            )
            .toolbar { Button("Open Settings") { model.section = .settings } }
        } else {
            HSplitView {
                List(selection: $model.selectedProjectID) {
                    ForEach(projects) { project in
                        ProjectRow(project: project, state: state).tag(project.id)
                    }
                }
                .frame(minWidth: 240, idealWidth: 290, maxWidth: 380)
                .searchable(text: $search, placement: .sidebar, prompt: "Filter projects")

                Group {
                    if let project = state.project(id: model.selectedProjectID) {
                        ProjectDetail(project: project, state: state)
                    } else {
                        EmptyState(title: "Select a project", symbol: "sidebar.left", message: "\(state.projects.count) projects discovered.")
                    }
                }
                .frame(minWidth: 420, maxWidth: .infinity, maxHeight: .infinity)
            }
        }
    }
}

struct ProjectRow: View {
    let project: Project
    let state: EngineState

    var body: some View {
        HStack(spacing: 8) {
            VStack(alignment: .leading, spacing: 1) {
                Text(project.name).fontWeight(.medium)
                Text(project.git?.branch ?? (project.isGitRepository ? "detached" : "not a Git repo"))
                    .font(.caption.monospaced())
                    .foregroundStyle(.secondary)
            }
            Spacer()
            if !state.servers(for: project.id).isEmpty {
                Image(systemName: "server.rack").foregroundStyle(.green).help("Dev server running")
            }
            if !state.agents(for: project.id).isEmpty {
                Image(systemName: "sparkles").foregroundStyle(.purple).help("Agent running")
            }
            if let deployment = state.latestDeployment(for: project.id) {
                StatusDot(color: deployment.state.color).help("Deployment: \(deployment.state.label)")
            }
            if project.git?.isDirty == true {
                Text("M").font(.caption.weight(.bold)).foregroundStyle(.orange).help("Uncommitted changes")
            }
        }
        .padding(.vertical, 2)
    }
}

struct ProjectDetail: View {
    @EnvironmentObject private var model: AppModel
    let project: Project
    let state: EngineState

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(project.name).font(.title2.weight(.semibold))
                    Text(project.path).font(.callout.monospaced()).foregroundStyle(.secondary).textSelection(.enabled)
                }
                ActionButtons(actions: ActionCatalog.actions(for: project))

                SectionHeader(title: "Git")
                gitSection

                SectionHeader(title: "Stack")
                Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 4) {
                    row("Framework", project.runtime.framework ?? "—")
                    row("Runtime", project.runtime.runtime ?? "—")
                    row("Package manager", project.runtime.packageManager ?? "—")
                }

                let servers = state.servers(for: project.id)
                SectionHeader(title: "Dev servers", detail: servers.isEmpty ? nil : "\(servers.count)")
                if servers.isEmpty {
                    Text("None running").foregroundStyle(.secondary)
                }
                ForEach(servers) { server in
                    ActiveRow(symbol: "server.rack", color: .green, title: server.displayName,
                              detail: server.ports.map { ":\($0)" }.joined(separator: " "), project: nil,
                              since: server.startedAt, actions: ActionCatalog.actions(for: server))
                }

                let agents = state.agents(for: project.id)
                let processes = state.processes.filter { $0.projectID == project.id }
                if !agents.isEmpty || !processes.isEmpty {
                    SectionHeader(title: "Active")
                    ForEach(agents) { agent in
                        ActiveRow(symbol: "sparkles", color: .purple, title: agent.providerName, detail: "pid \(agent.pid)", project: nil, since: agent.startedAt, actions: [])
                    }
                    ForEach(processes) { process in
                        ActiveRow(symbol: "hammer", color: .blue, title: process.label, detail: "pid \(process.pid)", project: nil, since: process.startedAt, actions: [])
                    }
                }

                SectionHeader(title: "Deployment")
                deploymentSection
            }
            .padding(20)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    @ViewBuilder
    private var gitSection: some View {
        if let git = project.git {
            Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 4) {
                row("Branch", git.branch ?? "detached HEAD", mono: true)
                row("Working tree", git.isDirty
                    ? "\(git.changedCount) changed, \(git.untrackedCount) untracked" + (git.conflictedCount > 0 ? ", \(git.conflictedCount) conflicted" : "")
                    : "Clean")
                if let upstream = git.upstream {
                    let ahead = git.ahead ?? 0
                    let behind = git.behind ?? 0
                    row("Remote", ahead == 0 && behind == 0 ? "Up to date with \(upstream)" : "↑\(ahead) ↓\(behind) vs \(upstream)")
                } else {
                    row("Remote", "No upstream branch")
                }
                if let commit = git.lastCommit {
                    row("Last commit", "\(commit.subject) · \(dayFormatter.string(from: commit.date))")
                }
                if let remote = git.remoteURL { row("Origin", remote, mono: true) }
            }
        } else if let error = project.gitError {
            Text("Couldn't read Git state: \(error)").foregroundStyle(.red)
        } else if !project.isGitRepository {
            Text("Not a Git repository").foregroundStyle(.secondary)
        } else {
            Text("Reading…").foregroundStyle(.secondary)
        }
    }

    @ViewBuilder
    private var deploymentSection: some View {
        if project.vercel == nil {
            Text("Not linked. Run `vercel link` in this project to connect it to Vercel.").foregroundStyle(.secondary)
        } else if model.snapshot?.hasVercelToken != true {
            Text("Linked to Vercel. Add a Vercel token in Settings to track deployments.").foregroundStyle(.secondary)
        } else if let deployments = state.deployments[project.id], !deployments.isEmpty {
            ForEach(deployments.prefix(5)) { deployment in
                HStack(spacing: 10) {
                    StatusDot(color: deployment.state.color)
                    Text(deployment.state.label).fontWeight(.medium).frame(width: 70, alignment: .leading)
                    Text(deployment.targetLabel).foregroundStyle(.secondary).frame(width: 80, alignment: .leading)
                    Text(deployment.commitMessage ?? deployment.branch ?? deployment.id).lineLimit(1)
                    Spacer()
                    Text(dayFormatter.string(from: deployment.createdAt)).font(.callout).foregroundStyle(.secondary)
                    ActionButtons(actions: ActionCatalog.actions(for: deployment), compact: true)
                }
                if let error = deployment.errorMessage, deployment.state == .error {
                    Text(error).font(.callout).foregroundStyle(.red).padding(.leading, 17)
                }
            }
        } else {
            Text("No deployments yet.").foregroundStyle(.secondary)
        }
    }

    private func row(_ label: String, _ value: String, mono: Bool = false) -> some View {
        GridRow {
            Text(label).foregroundStyle(.secondary)
            Text(value).font(mono ? .body.monospaced() : .body).textSelection(.enabled)
        }
    }
}
#endif
