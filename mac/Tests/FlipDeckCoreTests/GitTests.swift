import XCTest
@testable import FlipDeckCore

final class GitStatusParserTests: XCTestCase {
    func testParsesBranchUpstreamAndCounts() {
        let output = [
            "# branch.oid 1234567890abcdef1234567890abcdef12345678",
            "# branch.head main",
            "# branch.upstream origin/main",
            "# branch.ab +2 -3",
            "1 .M N... 100644 100644 100644 abc abc src/a.swift",
            "1 A. N... 000000 100644 100644 000 abc src/b.swift",
            "u UU N... 100644 100644 100644 100644 a b c conflict.txt",
            "? new file with spaces.txt",
            "? other.txt",
            "",
        ].joined(separator: "\0")
        let status = GitStatusParser.parse(output)
        XCTAssertEqual(status.branch, "main")
        XCTAssertEqual(status.headOID, "1234567890abcdef1234567890abcdef12345678")
        XCTAssertEqual(status.upstream, "origin/main")
        XCTAssertEqual(status.ahead, 2)
        XCTAssertEqual(status.behind, 3)
        XCTAssertEqual(status.changedCount, 2)
        XCTAssertEqual(status.conflictedCount, 1)
        XCTAssertEqual(status.untrackedCount, 2)
        XCTAssertTrue(status.isDirty)
    }

    func testDetachedAndInitial() {
        let status = GitStatusParser.parse("# branch.oid (initial)\0# branch.head (detached)\0")
        XCTAssertNil(status.branch)
        XCTAssertNil(status.headOID)
        XCTAssertNil(status.ahead)
        XCTAssertFalse(status.isDirty)
    }

    func testLastCommit() {
        let commit = GitStatusParser.parseLastCommit("abc123\u{0}1700000000\u{0}Fix: the | thing\n")
        XCTAssertEqual(commit?.oid, "abc123")
        XCTAssertEqual(commit?.subject, "Fix: the | thing")
        XCTAssertEqual(commit?.date, Date(timeIntervalSince1970: 1_700_000_000))
        XCTAssertNil(GitStatusParser.parseLastCommit(""))
    }

    func testRemoteURLCredentialsAreStripped() {
        XCTAssertEqual(GitStatusParser.sanitizeRemoteURL("https://user:ghp_secret@github.com/o/r.git\n"), "https://github.com/o/r.git")
        XCTAssertEqual(GitStatusParser.sanitizeRemoteURL("https://github.com/o/r.git"), "https://github.com/o/r.git")
        XCTAssertEqual(GitStatusParser.sanitizeRemoteURL("git@github.com:o/r.git"), "git@github.com:o/r.git")
        // An @ in the path (not userinfo) must be left alone.
        XCTAssertEqual(GitStatusParser.sanitizeRemoteURL("https://host/a@b/r.git"), "https://host/a@b/r.git")
    }
}

/// Runs against real `git` in temporary repositories.
final class GitClientIntegrationTests: XCTestCase {
    let client = GitClient(runner: ProcessCommandRunner())

    func testCleanDirtyAheadBehind() async throws {
        let tmp = TempDir()
        let remote = tmp.mkdir("remote.git")
        try sh("git init -q --bare -b main .", cwd: remote)
        let repo = tmp.mkdir("repo")
        try makeGitRepo(repo)
        try commit(repo, message: "first")
        try sh("git remote add origin '\(remote)' && git push -q -u origin main", cwd: repo)

        var status = try await client.status(at: repo)
        XCTAssertEqual(status.branch, "main")
        XCTAssertEqual(status.upstream, "origin/main")
        XCTAssertEqual(status.ahead, 0)
        XCTAssertEqual(status.behind, 0)
        XCTAssertFalse(status.isDirty)

        try commit(repo, message: "second")
        try sh("echo x > untracked.txt && echo y >> README.md", cwd: repo)
        status = try await client.status(at: repo)
        XCTAssertEqual(status.ahead, 1)
        XCTAssertEqual(status.untrackedCount, 1)
        XCTAssertEqual(status.changedCount, 1)
        XCTAssertTrue(status.isDirty)

        let last = try await client.lastCommit(at: repo)
        XCTAssertEqual(last?.subject, "second")
        XCTAssertEqual(last?.oid, status.headOID)

        let url = try await client.remoteURL(at: repo)
        XCTAssertEqual(url, remote)
    }

    func testEmptyRepositoryHasNoCommit() async throws {
        let tmp = TempDir()
        let repo = tmp.mkdir("empty")
        try makeGitRepo(repo)
        let status = try await client.status(at: repo)
        XCTAssertEqual(status.branch, "main")
        XCTAssertNil(status.headOID)
        let last = try await client.lastCommit(at: repo)
        XCTAssertNil(last)
    }

    func testNotARepositoryThrows() async {
        let tmp = TempDir()
        do {
            _ = try await client.status(at: tmp.mkdir("plain"))
            XCTFail("expected failure")
        } catch {
            XCTAssertTrue(String(describing: error).lowercased().contains("not a git repository"))
        }
    }
}

final class ProjectScannerTests: XCTestCase {
    func testFindsReposWithinDepthAndSkipsNoise() throws {
        let tmp = TempDir()
        let root = tmp.mkdir("Developer")
        tmp.mkdir("Developer/app/.git")
        tmp.mkdir("Developer/app/packages/inner/.git")          // inside a project: not descended
        tmp.mkdir("Developer/clients/acme/site/.git")          // depth 3: found
        tmp.mkdir("Developer/a/b/c/too-deep/.git")             // depth 4: not found
        tmp.mkdir("Developer/node_modules/pkg/.git")           // skipped directory
        tmp.mkdir("Developer/.hidden/repo/.git")               // hidden: skipped
        tmp.write("Developer/scratch/package.json", "{}")      // manifest-only project at depth 1
        tmp.write("Developer/clients/notes/package.json", "{}") // manifest-only below depth 1: ignored
        try FileManager.default.createSymbolicLink(atPath: root + "/loop", withDestinationPath: root)

        let found = ProjectScanner().scan(roots: [root])
        let relative = Set(found.map { String($0.path.dropFirst(root.count + 1)) })
        XCTAssertEqual(relative, ["app", "clients/acme/site", "scratch"])
        XCTAssertEqual(found.first { $0.path.hasSuffix("scratch") }?.isGitRepository, false)
    }

    func testRootThatIsARepoAndDuplicateRoots() {
        let tmp = TempDir()
        let repo = tmp.mkdir("repo")
        tmp.mkdir("repo/.git")
        let found = ProjectScanner().scan(roots: [repo, repo + "/", tmp.path + "/missing"])
        XCTAssertEqual(found.map(\.path), [repo])
    }

    func testLimits() {
        let tmp = TempDir()
        for i in 0..<10 { tmp.mkdir("root/p\(i)/.git") }
        let scanner = ProjectScanner(limits: .init(maxDepth: 3, maxProjects: 4, maxDirectoriesVisited: 1000))
        XCTAssertEqual(scanner.scan(roots: [tmp.path + "/root"]).count, 4)
    }

    func testFrameworkDetection() {
        let tmp = TempDir()
        tmp.write("next/package.json", #"{"dependencies":{"next":"15","react":"19"}}"#)
        tmp.write("next/pnpm-lock.yaml", "")
        tmp.write("vite/package.json", #"{"devDependencies":{"vite":"6"},"packageManager":"yarn@4.1.0"}"#)
        tmp.write("rust/Cargo.toml", "")
        tmp.write("django/pyproject.toml", "")
        tmp.write("django/manage.py", "")
        tmp.write("broken/package.json", "{not json")

        XCTAssertEqual(FrameworkDetector.detect(at: tmp.path + "/next"), ProjectRuntime(framework: "Next.js", runtime: "Node", packageManager: "pnpm"))
        XCTAssertEqual(FrameworkDetector.detect(at: tmp.path + "/vite"), ProjectRuntime(framework: "Vite", runtime: "Node", packageManager: "yarn"))
        XCTAssertEqual(FrameworkDetector.detect(at: tmp.path + "/rust"), ProjectRuntime(framework: nil, runtime: "Rust", packageManager: "cargo"))
        XCTAssertEqual(FrameworkDetector.detect(at: tmp.path + "/django").framework, "Django")
        XCTAssertEqual(FrameworkDetector.detect(at: tmp.path + "/broken"), ProjectRuntime(framework: nil, runtime: "Node", packageManager: nil))
    }
}
