import Foundation

/// Maps a working directory or argument vector to a discovered project.
public struct ProjectAssociator: Sendable {
    /// Longest paths first so nested projects win over their parents.
    let projectPaths: [String]

    public init(projects: [Project]) {
        projectPaths = projects.map(\.path).sorted { $0.count > $1.count }
    }

    public func projectID(cwd: String?, args: String) -> String? {
        if let cwd, let match = projectPaths.first(where: { Self.isInside(cwd, $0) }) {
            return match
        }
        // Fallback: a script path inside a project (`node ~/dev/app/node_modules/.bin/vite`).
        for path in projectPaths {
            if let range = args.range(of: path) {
                let after = args[range.upperBound...]
                if after.isEmpty || after.first == "/" || after.first == " " { return path }
            }
        }
        return nil
    }

    public func contains(_ path: String) -> Bool {
        projectPaths.contains { Self.isInside(path, $0) }
    }

    static func isInside(_ path: String, _ root: String) -> Bool {
        path == root || path.hasPrefix(root.hasSuffix("/") ? root : root + "/")
    }
}

/// Recognizes developer-relevant processes. Deliberately conservative: an
/// unrecognized process is ignored rather than guessed at.
public enum ProcessClassifier {
    static let interpreters: Set<String> = ["node", "bun", "deno", "python", "python3", "ruby", "php", "java", "npx", "pnpx", "bunx"]
    static let packageManagers: Set<String> = ["npm", "pnpm", "yarn", "bun"]

    /// Runtimes whose listening sockets are dev servers by default.
    static let serverRuntimes: Set<String> = [
        "node", "bun", "deno", "python", "python3", "ruby", "php", "java", "dotnet", "beam.smp",
        "hugo", "jekyll", "puma", "rails", "gunicorn", "uvicorn", "flask", "vite", "next-server",
        "esbuild", "workerd", "wrangler", "caddy", "com.docker.backend", "vpnkit-bridge",
    ]

    public struct Match: Equatable {
        public let kind: DevProcessKind
        public let label: String
    }

    /// Lowercased tokens with interpreter paths reduced to basenames.
    static func normalizedTokens(_ record: ProcessRecord) -> [String] {
        record.tokens.map { token -> String in
            let lower = token.lowercased()
            if lower.contains("/") { return String(lower.split(separator: "/").last ?? Substring(lower)) }
            return lower
        }
    }

    public static func classify(_ record: ProcessRecord) -> Match? {
        let tokens = normalizedTokens(record)
        guard let exe = tokens.first else { return nil }
        let joined = " " + tokens.joined(separator: " ") + " "

        func has(_ phrase: String) -> Bool { joined.contains(" \(phrase) ") }

        // Package-manager script invocations: `npm test`, `pnpm run build`, `yarn dev`.
        if packageManagers.contains(exe), tokens.count >= 2 {
            let sub = tokens[1] == "run" && tokens.count >= 3 ? tokens[2] : tokens[1]
            switch sub {
            case "test", "test:unit", "test:e2e", "e2e": return Match(kind: .testRunner, label: "\(exe) \(sub)")
            case "build": return Match(kind: .build, label: "\(exe) build")
            case "install", "i", "ci", "add": return Match(kind: .packageManager, label: "\(exe) \(sub)")
            default: break
            }
        }

        let testRunners: [(String, String)] = [
            ("vitest", "vitest"), ("jest", "jest"), ("mocha", "mocha"), ("pytest", "pytest"),
            ("playwright test", "playwright"), ("cypress run", "cypress"), ("rspec", "rspec"), ("phpunit", "phpunit"),
        ]
        for (phrase, label) in testRunners where has(phrase) || tokens.contains(phrase) {
            return Match(kind: .testRunner, label: label)
        }
        if exe == "go" && has("go test") { return Match(kind: .testRunner, label: "go test") }
        if exe == "cargo" && has("cargo test") { return Match(kind: .testRunner, label: "cargo test") }
        if exe == "swift" && has("swift test") { return Match(kind: .testRunner, label: "swift test") }
        if exe == "bun" && has("bun test") { return Match(kind: .testRunner, label: "bun test") }
        if exe == "deno" && has("deno test") { return Match(kind: .testRunner, label: "deno test") }
        if exe == "xcodebuild" { return Match(kind: tokens.contains("test") ? .testRunner : .build, label: "xcodebuild") }

        if has("next build") { return Match(kind: .build, label: "next build") }
        if has("vite build") { return Match(kind: .build, label: "vite build") }
        if exe == "cargo" && has("cargo build") { return Match(kind: .build, label: "cargo build") }
        if exe == "swift" && has("swift build") { return Match(kind: .build, label: "swift build") }
        if exe == "go" && has("go build") { return Match(kind: .build, label: "go build") }
        if tokens.contains("tsc") && (tokens.contains("--watch") || tokens.contains("-w")) { return Match(kind: .build, label: "tsc --watch") }
        if exe == "gradle" || exe == "gradlew" { return Match(kind: .build, label: "gradle") }

        if (exe == "docker" && (has("docker compose up") || has("docker run"))) || exe == "docker-compose" {
            return Match(kind: .container, label: "docker")
        }

        return nil
    }

    public static func isServerRuntime(_ record: ProcessRecord) -> Bool {
        let exe = record.executableName.lowercased()
        if exe.hasPrefix("python") { return true }
        if exe.hasPrefix("next-server") { return true }
        return serverRuntimes.contains(exe)
    }

    /// A readable name for what's serving on a port.
    public static func serverFramework(_ record: ProcessRecord) -> String? {
        let tokens = normalizedTokens(record)
        let joined = " " + tokens.joined(separator: " ") + " "
        func has(_ phrase: String) -> Bool { joined.contains(" \(phrase) ") || joined.contains(" \(phrase)") }

        let exe = record.executableName.lowercased()
        if exe.hasPrefix("next-server") || has("next dev") || has("next start") || has("next-server") { return "Next.js" }
        let known: [(String, String)] = [
            ("vite", "Vite"), ("astro", "Astro"), ("nuxi", "Nuxt"), ("nuxt", "Nuxt"), ("remix", "Remix"),
            ("react-router", "React Router"), ("react-scripts", "Create React App"), ("webpack-dev-server", "webpack"),
            ("webpack serve", "webpack"), ("storybook", "Storybook"), ("expo", "Expo"), ("wrangler", "Wrangler"),
            ("workerd", "Wrangler"), ("docusaurus", "Docusaurus"), ("ng serve", "Angular"), ("nodemon", "nodemon"),
            ("tsx watch", "tsx"), ("manage.py runserver", "Django"), ("flask", "Flask"), ("uvicorn", "Uvicorn"),
            ("gunicorn", "Gunicorn"), ("fastapi", "FastAPI"), ("rails server", "Rails"), ("puma", "Rails"),
            ("artisan serve", "Laravel"), ("hugo", "Hugo"), ("jekyll", "Jekyll"), ("http.server", "http.server"),
        ]
        for (phrase, name) in known where has(phrase) { return name }
        if exe.hasPrefix("com.docker") || exe.hasPrefix("vpnkit") { return "Docker" }
        switch exe {
        case "node": return "Node"
        case "bun": return "Bun"
        case "deno": return "Deno"
        case "ruby": return "Ruby"
        case "php": return "PHP"
        case "java": return "Java"
        default: return exe.hasPrefix("python") ? "Python" : nil
        }
    }

    /// Drops ancestors that are just wrappers of a matched descendant of the
    /// same kind (`npm test` → `node vitest`): keeps the most specific one.
    public static func collapseWrappers(_ matches: [(ProcessRecord, Match)]) -> [(ProcessRecord, Match)] {
        let byPID = Dictionary(uniqueKeysWithValues: matches.map { ($0.0.pid, $0) })
        var dropped: Set<Int32> = []
        for (record, match) in matches {
            var parent = record.ppid
            var hops = 0
            while let ancestor = byPID[parent], hops < 8 {
                if ancestor.1.kind == match.kind { dropped.insert(ancestor.0.pid) }
                parent = ancestor.0.ppid
                hops += 1
            }
        }
        return matches.filter { !dropped.contains($0.0.pid) }
    }
}

/// Turns a process table + listening sockets into dev servers.
public enum DevServerDetector {
    public static func detect(
        records: [ProcessRecord],
        listeners: [Listener],
        cwds: [Int32: String],
        currentUID: UInt32,
        associator: ProjectAssociator
    ) -> [DevServer] {
        let byPID = Dictionary(records.map { ($0.pid, $0) }, uniquingKeysWith: { a, _ in a })
        let grouped = Dictionary(grouping: listeners.filter { $0.port >= 1024 }, by: \.pid)

        var servers: [DevServer] = []
        for (pid, socketList) in grouped {
            guard let record = byPID[pid], record.uid == currentUID else { continue }
            let cwd = cwds[pid]
            let projectID = associator.projectID(cwd: cwd, args: record.args)
            // A listener counts if it's a known dev runtime, or anything (e.g. a
            // compiled Go/Rust binary) running out of a project directory.
            guard ProcessClassifier.isServerRuntime(record) || projectID != nil else { continue }
            servers.append(DevServer(
                pid: pid,
                ports: socketList.map(\.port),
                processName: record.executableName,
                command: String(record.args.prefix(300)),
                framework: ProcessClassifier.serverFramework(record),
                startedAt: record.startedAt,
                cwd: cwd,
                projectID: projectID
            ))
        }
        return servers.sorted { ($0.primaryPort, $0.pid) < ($1.primaryPort, $1.pid) }
    }
}
