import Foundation

/// Finds projects under the configured roots without hammering the disk:
/// bounded depth, bounded entry count, no symlink following, never descends
/// into a project once found, and skips dependency/build directories.
public struct ProjectScanner: Sendable {
    public struct Limits: Sendable {
        public var maxDepth: Int
        public var maxProjects: Int
        public var maxDirectoriesVisited: Int

        public init(maxDepth: Int = 3, maxProjects: Int = 300, maxDirectoriesVisited: Int = 20_000) {
            self.maxDepth = maxDepth
            self.maxProjects = maxProjects
            self.maxDirectoriesVisited = maxDirectoriesVisited
        }
    }

    public struct Found: Hashable, Sendable {
        public let path: String
        public let isGitRepository: Bool
    }

    static let skippedDirectoryNames: Set<String> = [
        "node_modules", "bower_components", "vendor", "Pods", "Carthage",
        "build", "dist", "out", "target", "DerivedData", ".build",
        "venv", "__pycache__", "Library", "Applications",
    ]

    static let manifestFiles = [
        "package.json", "Cargo.toml", "go.mod", "pyproject.toml", "Package.swift",
        "Gemfile", "composer.json", "deno.json", "mix.exs", "pom.xml", "build.gradle",
    ]

    public let limits: Limits

    public init(limits: Limits = Limits()) {
        self.limits = limits
    }

    public func scan(roots: [String]) -> [Found] {
        let fileManager = FileManager.default
        var found: [Found] = []
        var seen: Set<String> = []
        var visited = 0

        func visit(_ path: String, depth: Int) {
            guard found.count < limits.maxProjects, visited < limits.maxDirectoriesVisited else { return }
            visited += 1

            if fileManager.fileExists(atPath: path + "/.git") {
                if seen.insert(path).inserted { found.append(Found(path: path, isGitRepository: true)) }
                return
            }
            // Only a root's direct children may be manifest-only (non-git) projects;
            // deeper manifests are usually examples/fixtures inside something else.
            if depth == 1, Self.manifestFiles.contains(where: { fileManager.fileExists(atPath: path + "/" + $0) }) {
                if seen.insert(path).inserted { found.append(Found(path: path, isGitRepository: false)) }
                return
            }
            guard depth < limits.maxDepth else { return }

            let url = URL(fileURLWithPath: path, isDirectory: true)
            let keys: [URLResourceKey] = [.isDirectoryKey, .isSymbolicLinkKey]
            guard let children = try? fileManager.contentsOfDirectory(at: url, includingPropertiesForKeys: keys, options: []) else { return }
            for child in children.sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) {
                let name = child.lastPathComponent
                if name.hasPrefix(".") || Self.skippedDirectoryNames.contains(name) || name.hasSuffix(".app") { continue }
                guard let values = try? child.resourceValues(forKeys: Set(keys)),
                      values.isDirectory == true, values.isSymbolicLink != true else { continue }
                visit(child.path, depth: depth + 1)
            }
        }

        for root in roots {
            let standardized = Self.standardize(root)
            var isDirectory: ObjCBool = false
            guard fileManager.fileExists(atPath: standardized, isDirectory: &isDirectory), isDirectory.boolValue else { continue }
            if fileManager.fileExists(atPath: standardized + "/.git") {
                if seen.insert(standardized).inserted { found.append(Found(path: standardized, isGitRepository: true)) }
                continue
            }
            visit(standardized, depth: 0)
        }
        return found
    }

    /// Expands `~` and removes trailing slashes / `..` segments.
    public static func standardize(_ path: String) -> String {
        let expanded = (path as NSString).expandingTildeInPath
        var standardized = URL(fileURLWithPath: expanded).standardizedFileURL.path
        while standardized.count > 1 && standardized.hasSuffix("/") { standardized.removeLast() }
        return standardized
    }
}

/// Best-effort framework / runtime / package-manager detection from files at
/// the project root. Only reports what the files actually say.
public enum FrameworkDetector {
    public static func detect(at path: String, fileManager: FileManager = .default) -> ProjectRuntime {
        func exists(_ name: String) -> Bool { fileManager.fileExists(atPath: path + "/" + name) }
        var runtime = ProjectRuntime()

        if exists("package.json") {
            runtime.runtime = "Node"
            if exists("bun.lockb") || exists("bun.lock") { runtime.packageManager = "bun"; runtime.runtime = "Bun" }
            else if exists("pnpm-lock.yaml") { runtime.packageManager = "pnpm" }
            else if exists("yarn.lock") { runtime.packageManager = "yarn" }
            else if exists("package-lock.json") { runtime.packageManager = "npm" }

            if let data = fileManager.contents(atPath: path + "/package.json"),
               let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
                var deps: [String: Any] = [:]
                for key in ["dependencies", "devDependencies"] {
                    if let section = json[key] as? [String: Any] { deps.merge(section) { a, _ in a } }
                }
                runtime.framework = nodeFramework(dependencies: Set(deps.keys))
                if runtime.packageManager == nil, let declared = json["packageManager"] as? String {
                    runtime.packageManager = declared.split(separator: "@").first.map(String.init)
                }
            }
        } else if exists("deno.json") || exists("deno.jsonc") {
            runtime.runtime = "Deno"
        } else if exists("Cargo.toml") {
            runtime.runtime = "Rust"
            runtime.packageManager = "cargo"
        } else if exists("go.mod") {
            runtime.runtime = "Go"
        } else if exists("Package.swift") {
            runtime.runtime = "Swift"
            runtime.packageManager = "swiftpm"
        } else if exists("pyproject.toml") || exists("requirements.txt") || exists("setup.py") {
            runtime.runtime = "Python"
            if exists("uv.lock") { runtime.packageManager = "uv" }
            else if exists("poetry.lock") { runtime.packageManager = "poetry" }
            else if exists("requirements.txt") { runtime.packageManager = "pip" }
            if exists("manage.py") { runtime.framework = "Django" }
        } else if exists("Gemfile") {
            runtime.runtime = "Ruby"
            runtime.packageManager = "bundler"
            if exists("config/application.rb") { runtime.framework = "Rails" }
        } else if exists("composer.json") {
            runtime.runtime = "PHP"
            runtime.packageManager = "composer"
            if exists("artisan") { runtime.framework = "Laravel" }
        }
        return runtime
    }

    static func nodeFramework(dependencies: Set<String>) -> String? {
        let ordered: [(String, String)] = [
            ("next", "Next.js"), ("@remix-run/react", "Remix"), ("@react-router/dev", "React Router"),
            ("nuxt", "Nuxt"), ("@sveltejs/kit", "SvelteKit"), ("astro", "Astro"),
            ("gatsby", "Gatsby"), ("expo", "Expo"), ("@angular/core", "Angular"),
            ("@nestjs/core", "NestJS"), ("vite", "Vite"), ("react-scripts", "Create React App"),
            ("electron", "Electron"), ("express", "Express"), ("fastify", "Fastify"), ("hono", "Hono"),
        ]
        return ordered.first { dependencies.contains($0.0) }?.1
    }
}
