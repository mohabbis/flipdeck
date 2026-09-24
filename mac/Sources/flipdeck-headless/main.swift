import Foundation
import FlipDeckCore

// flipdeck-headless: runs the FlipDeck engine without a UI and prints what it
// sees. Useful for debugging detection on a real machine, and on Linux where
// the SwiftUI app can't run.
//
//   flipdeck-headless [--root DIR]... [--once] [--frames] [--verbose]
//
//   --root DIR   project root to scan (repeatable; default: settings file)
//   --once       one full pass, print a summary, exit
//   --frames     print the FDP/1 frames a Flipper would receive
//   VERCEL_TOKEN environment variable enables the Vercel integration.

struct Options {
    var roots: [String] = []
    var once = false
    var frames = false
    var verbose = false
}

var options = Options()
var arguments = CommandLine.arguments.dropFirst().makeIterator()
while let argument = arguments.next() {
    switch argument {
    case "--root": if let root = arguments.next() { options.roots.append(root) }
    case "--once": options.once = true
    case "--frames": options.frames = true
    case "--verbose": options.verbose = true
    case "-h", "--help":
        print("usage: flipdeck-headless [--root DIR]... [--once] [--frames] [--verbose]")
        exit(0)
    default:
        FileHandle.standardError.write(Data("unknown argument: \(argument)\n".utf8))
        exit(2)
    }
}

/// Prints instead of acting: the headless runner never opens or kills anything.
struct DryRunEffects: SystemEffects {
    func openURL(_ url: URL) async throws { print("[dry-run] open \(url)") }
    func openProject(at path: String, editorBundleID: String?) async throws -> String { print("[dry-run] open project \(path)"); return "dry-run" }
    func openTerminal(at path: String) async throws { print("[dry-run] terminal \(path)") }
    func revealInFinder(_ path: String) async throws { print("[dry-run] reveal \(path)") }
    func copyToClipboard(_ string: String) async { print("[dry-run] copy \(string)") }
    func terminate(pid: Int32) throws { print("[dry-run] SIGTERM \(pid)") }
}

let support = AppPaths.supportDirectory()
let store = SettingsStore(url: options.roots.isEmpty ? support.appendingPathComponent("settings.json") : nil)
if !options.roots.isEmpty {
    var settings = store.settings
    settings.projectRoots = options.roots
    try? store.save(settings)
}

let transport = LoopbackTransport()
if options.frames {
    transport.onSend = { data in
        for line in String(decoding: data, as: UTF8.self).split(separator: "\n") { print("  → \(line)") }
    }
}

let engine = FlipDeckEngine(dependencies: EngineDependencies(
    secrets: EnvironmentSecretStore(),
    effects: DryRunEffects(),
    transport: options.frames ? transport : nil,
    logSink: StderrLogSink(minimum: options.verbose ? .debug : .warning),
    settingsStore: store,
    activityLog: ActivityLog(),
    hostName: ProcessInfo.processInfo.hostName
))

func summary(_ snapshot: EngineSnapshot) -> String {
    let state = snapshot.state
    var lines: [String] = []
    if let machine = state.machine {
        lines.append("MACHINE  \(machine.hostName) · \(machine.osVersion)")
    }
    lines.append("PROJECTS \(state.projects.count)")
    for project in state.projects {
        var parts = ["  \(project.name)"]
        if let git = project.git {
            parts.append(git.branch ?? "(detached)")
            parts.append(git.isDirty ? "dirty(\(git.changedCount + git.untrackedCount))" : "clean")
            if let ahead = git.ahead, let behind = git.behind { parts.append("↑\(ahead) ↓\(behind)") }
        } else if !project.isGitRepository {
            parts.append("no git")
        } else if let error = project.gitError {
            parts.append("git error: \(error)")
        }
        if let framework = project.runtime.framework ?? project.runtime.runtime { parts.append(framework) }
        if project.vercel != nil { parts.append("vercel-linked") }
        if let deployment = state.latestDeployment(for: project.id) { parts.append("deploy:\(deployment.state.rawValue)") }
        lines.append(parts.joined(separator: " · "))
    }
    lines.append("SERVERS  \(state.servers.count)")
    for server in state.servers {
        lines.append("  :\(server.primaryPort) \(server.displayName) pid \(server.pid) → \(state.project(id: server.projectID)?.name ?? "unassociated")")
    }
    lines.append("AGENTS   \(state.agents.count)")
    for agent in state.agents {
        lines.append("  \(agent.providerName) pid \(agent.pid) → \(state.project(id: agent.projectID)?.name ?? agent.cwd ?? "?")")
    }
    lines.append("ACTIVE   \(state.processes.count)")
    for process in state.processes {
        lines.append("  \(process.kind.rawValue) \(process.label) pid \(process.pid) → \(state.project(id: process.projectID)?.name ?? "?")")
    }
    for integration in state.integrations {
        lines.append("INTEGRATION \(integration.displayName): \(integration.health) (\(integration.linkedProjects) linked)")
    }
    if !snapshot.attention.isEmpty {
        lines.append("ATTENTION \(snapshot.attention.count)")
        for item in snapshot.attention { lines.append("  ! \(item.projectName ?? "") \(item.title)") }
    }
    return lines.joined(separator: "\n")
}

let lastEventID = LockedBox<String?>(nil)
let printEvents = !options.once
await engine.setObserver { snapshot in
    guard printEvents else { return }
    // Print new events as they happen.
    let events = snapshot.events
    let known = lastEventID.value
    let fresh = events.prefix { $0.id != known }
    for event in fresh.reversed() {
        print("\(ISO8601DateFormatter().string(from: event.timestamp)) [\(event.severity.rawValue)] \(event.type.rawValue) \(event.projectName ?? "-"): \(event.title) \(event.message)")
    }
    if let first = events.first { lastEventID.value = first.id }
}

final class LockedBox<T>: @unchecked Sendable {
    private let lock = NSLock()
    private var stored: T
    init(_ value: T) { stored = value }
    var value: T {
        get { lock.lock(); defer { lock.unlock() }; return stored }
        set { lock.lock(); stored = newValue; lock.unlock() }
    }
}

if options.once {
    let box = LockedBox<EngineSnapshot?>(nil)
    await engine.setObserver { box.value = $0 }
    // Runs the real scheduler for one cycle of every provider.
    await engine.start()
    if options.frames {
        // Pretend a Flipper connected, to show exactly what it would receive.
        transport.emit(.connected(peerName: "headless", maxChunk: 182))
        transport.receive(String(decoding: FrameCodec.encode(Frame("HI", ["1", "headless", "0", "cli"])), as: UTF8.self))
    }
    try? await Task.sleep(nanoseconds: 3_000_000_000)
    if let snapshot = box.value { print(summary(snapshot)) }
    await engine.stop()
    exit(0)
}

signal(SIGINT, SIG_IGN)
let interrupt = DispatchSource.makeSignalSource(signal: SIGINT, queue: .main)
interrupt.setEventHandler {
    Task {
        await engine.stop()
        exit(0)
    }
}
interrupt.resume()

await engine.start()
print("FlipDeck headless running. Ctrl-C to stop.")
while true { try? await Task.sleep(nanoseconds: 60_000_000_000) }
