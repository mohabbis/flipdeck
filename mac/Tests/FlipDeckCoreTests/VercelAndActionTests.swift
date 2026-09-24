import XCTest
@testable import FlipDeckCore

final class VercelTests: XCTestCase {
    static let sampleResponse = """
    {
      "pagination": {"count": 3, "next": 1, "prev": null},
      "deployments": [
        {"uid": "dpl_3", "name": "web", "url": "web-3.vercel.app", "state": "ERROR", "target": "production",
         "inspectorUrl": "https://vercel.com/acme/web/3", "created": 1700000300000,
         "meta": {"githubCommitRef": "main", "githubCommitMessage": "Break it", "githubPrId": 12}},
        {"uid": "dpl_2", "url": "web-2.vercel.app", "readyState": "READY", "target": null, "createdAt": 1700000200000},
        {"uid": "dpl_1", "state": "SOMETHING_NEW", "created": 1700000100000},
        {"uid": "dpl_0", "state": "BLOCKED", "created": 1700000000000, "inspectorUrl": "https://evil.example.com/x"}
      ]
    }
    """

    func testLinkReader() {
        let tmp = TempDir()
        tmp.write("a/.vercel/project.json", #"{"projectId":"prj_1","orgId":"team_9","projectName":"web"}"#)
        tmp.write("b/.vercel/repo.json", #"{"orgId":"team_9","remoteName":"origin","projects":[{"id":"prj_2","name":"api","directory":"apps/api"}]}"#)
        tmp.write("c/.vercel/repo.json", #"{"orgId":"team_9","projects":[{"id":"prj_3","directory":"apps/a"},{"id":"prj_4","directory":"apps/b"}]}"#)
        tmp.write("d/.vercel/project.json", "{broken")

        XCTAssertEqual(VercelLinkReader.read(projectPath: tmp.path + "/a"), VercelLink(projectID: "prj_1", orgID: "team_9", projectName: "web"))
        XCTAssertEqual(VercelLinkReader.read(projectPath: tmp.path + "/b")?.projectID, "prj_2")
        XCTAssertNil(VercelLinkReader.read(projectPath: tmp.path + "/c"), "ambiguous monorepo link must not guess")
        XCTAssertNil(VercelLinkReader.read(projectPath: tmp.path + "/d"))
        XCTAssertNil(VercelLinkReader.read(projectPath: tmp.path + "/missing"))
    }

    func testClientDecodesAndMaps() async throws {
        let http = FakeHTTP { _ in HTTPResponse(status: 200, body: Data(Self.sampleResponse.utf8)) }
        let client = VercelClient(token: "tok_secret", http: http)
        let deployments = try await client.deployments(for: VercelLink(projectID: "prj_1", orgID: "team_9"), localProjectID: "/dev/web")

        XCTAssertEqual(deployments.map(\.id), ["dpl_3", "dpl_2", "dpl_0"], "unknown states are skipped")
        XCTAssertEqual(deployments[0].state, .error)
        XCTAssertEqual(deployments[0].branch, "main")
        XCTAssertEqual(deployments[0].commitMessage, "Break it")
        XCTAssertEqual(deployments[0].url, "https://web-3.vercel.app")
        XCTAssertEqual(deployments[0].inspectorURL, "https://vercel.com/acme/web/3")
        XCTAssertEqual(deployments[0].createdAt, Date(timeIntervalSince1970: 1_700_000_300))
        XCTAssertEqual(deployments[1].state, .ready)
        XCTAssertEqual(deployments[1].targetLabel, "Preview")
        XCTAssertEqual(deployments[2].state, .error)
        XCTAssertEqual(deployments[2].errorMessage, "Deployment blocked")
        XCTAssertNil(deployments[2].inspectorURL, "non-vercel.com links are dropped")

        let (url, headers) = http.requests[0]
        XCTAssertEqual(url.path, "/v6/deployments")
        XCTAssertTrue(url.query!.contains("projectId=prj_1"))
        XCTAssertTrue(url.query!.contains("teamId=team_9"))
        XCTAssertEqual(headers["Authorization"], "Bearer tok_secret")
    }

    func testPersonalAccountOmitsTeamID() async throws {
        let http = FakeHTTP { _ in HTTPResponse(status: 200, body: Data(#"{"deployments":[]}"#.utf8)) }
        _ = try await VercelClient(token: "t", http: http).deployments(for: VercelLink(projectID: "prj_1", orgID: "abc123"), localProjectID: "x")
        XCTAssertFalse(http.requests[0].0.query!.contains("teamId"))
    }

    func testErrorsMapAndNeverLeakToken() async {
        for (status, expected) in [(401, VercelError.unauthorized), (403, .unauthorized), (500, .http(500))] {
            let http = FakeHTTP { _ in HTTPResponse(status: status, body: Data("tok_secret".utf8)) }
            do {
                _ = try await VercelClient(token: "tok_secret", http: http).deployments(for: VercelLink(projectID: "p", orgID: "o"), localProjectID: "x")
                XCTFail("expected error")
            } catch {
                XCTAssertEqual(error as? VercelError, expected)
                XCTAssertFalse(String(describing: error).contains("tok_secret"))
            }
        }
        let garbage = FakeHTTP { _ in HTTPResponse(status: 200, body: Data("<html>".utf8)) }
        do {
            _ = try await VercelClient(token: "t", http: garbage).deployments(for: VercelLink(projectID: "p", orgID: "o"), localProjectID: "x")
            XCTFail("expected error")
        } catch let error as VercelError {
            if case .decoding = error {} else { XCTFail("\(error)") }
        } catch { XCTFail("\(error)") }
    }

    func testIntegrationStatusBackoffAndUnauthorized() async {
        var linked = project("/dev/web")
        linked.vercel = VercelLink(projectID: "prj_1", orgID: "team_1")
        let log = Logger("t", sink: NullLogSink())

        let noToken = await VercelIntegration(http: FakeHTTP { _ in HTTPResponse(status: 200, body: Data()) }, log: log).refresh(projects: [linked], token: nil)
        XCTAssertEqual(noToken.status.health, .notConfigured)
        XCTAssertEqual(noToken.status.linkedProjects, 1)

        let failing = FakeHTTP { _ in HTTPResponse(status: 502, body: Data()) }
        let integration = VercelIntegration(http: failing, log: log)
        let first = await integration.refresh(projects: [linked], token: "t", now: .at(0))
        if case .error = first.status.health {} else { XCTFail("expected error health") }
        _ = await integration.refresh(projects: [linked], token: "t", now: .at(10))   // within 30s backoff
        XCTAssertEqual(failing.requests.count, 1)
        failing.handler = { _ in HTTPResponse(status: 200, body: Data(#"{"deployments":[]}"#.utf8)) }
        let recovered = await integration.refresh(projects: [linked], token: "t", now: .at(31))
        XCTAssertEqual(recovered.status.health, .ok)
        XCTAssertEqual(recovered.deployments["/dev/web"], [])

        let rejecting = FakeHTTP { _ in HTTPResponse(status: 401, body: Data()) }
        let unauthorized = VercelIntegration(http: rejecting, log: log)
        let rejected = await unauthorized.refresh(projects: [linked], token: "bad")
        XCTAssertEqual(rejected.status.health, .unauthorized)
        _ = await unauthorized.refresh(projects: [linked], token: "bad", now: Date().addingTimeInterval(3600))
        XCTAssertEqual(rejecting.requests.count, 1, "a rejected token is not retried")
        _ = await unauthorized.refresh(projects: [linked], token: "new")
        XCTAssertEqual(rejecting.requests.count, 2, "a new token is tried")
    }
}

final class ActionExecutorTests: XCTestCase {
    func makeState(_ tmp: TempDir) -> EngineState {
        var state = EngineState()
        state.projects = [project(tmp.mkdir("web"))]
        state.servers = [server(pid: 4242, port: 5173, projectID: state.projects[0].id, startedAt: .at(1_000))]
        state.deployments[state.projects[0].id] = [Deployment(
            id: "d1", provider: "vercel", projectID: state.projects[0].id, state: .ready, target: "production",
            url: "https://web.vercel.app", inspectorURL: "https://vercel.com/a/web/d1", createdAt: .at(1), branch: nil, commitMessage: nil)]
        return state
    }

    func testPerformsValidActions() async {
        let tmp = TempDir()
        let state = makeState(tmp)
        let effects = RecordingEffects()
        let executor = ActionExecutor(effects: effects)
        let path = state.projects[0].path
        let policy = ActionPolicy()

        var result = await executor.perform(FDAction(kind: .openOnMac, target: .project(path: path), source: .system), origin: .flipper, state: state, policy: policy)
        XCTAssertEqual(result, .success("Opened in Editor"))
        result = await executor.perform(FDAction(kind: .openLocalhost, target: .process(pid: 4242, startedAt: .at(1_000), port: 5173), source: .process), origin: .flipper, state: state, policy: policy)
        XCTAssertTrue(result.ok)
        result = await executor.perform(FDAction(kind: .stopServer, target: .process(pid: 4242, startedAt: .at(1_000), port: 5173), source: .process), origin: .flipper, state: state, policy: policy)
        XCTAssertTrue(result.ok)
        result = await executor.perform(FDAction(kind: .openLogs, target: .url("https://vercel.com/a/web/d1"), source: .vercel), origin: .flipper, state: state, policy: policy)
        XCTAssertTrue(result.ok)
        XCTAssertEqual(effects.entries, ["project \(path)", "open http://localhost:5173", "terminate 4242", "open https://vercel.com/a/web/d1"])
    }

    func testRejectsStaleUnknownAndDisallowed() async {
        let tmp = TempDir()
        let state = makeState(tmp)
        let effects = RecordingEffects()
        let executor = ActionExecutor(effects: effects)

        // PID reused by a different process (different start time).
        var result = await executor.perform(FDAction(kind: .stopServer, target: .process(pid: 4242, startedAt: .at(5_000), port: 5173), source: .process), origin: .mac, state: state, policy: ActionPolicy())
        XCTAssertEqual(result, .failure("Server changed; refresh and retry"))
        // Not a dev server FlipDeck knows about.
        result = await executor.perform(FDAction(kind: .stopServer, target: .process(pid: 1, startedAt: nil, port: nil), source: .process), origin: .mac, state: state, policy: ActionPolicy())
        XCTAssertEqual(result, .failure("Server is no longer running"))
        // Destructive from Flipper disabled by policy.
        result = await executor.perform(FDAction(kind: .stopServer, target: .process(pid: 4242, startedAt: .at(1_000), port: 5173), source: .process), origin: .flipper, state: state, policy: ActionPolicy(allowDestructiveFromFlipper: false))
        XCTAssertEqual(result, .failure("Disabled in Mac settings"))
        // Arbitrary URL that isn't part of current state.
        result = await executor.perform(FDAction(kind: .openLogs, target: .url("https://vercel.com/somewhere-else"), source: .vercel), origin: .mac, state: state, policy: ActionPolicy())
        XCTAssertFalse(result.ok)
        // Directory that isn't a discovered project.
        result = await executor.perform(FDAction(kind: .openTerminal, target: .project(path: "/etc"), source: .system), origin: .mac, state: state, policy: ActionPolicy())
        XCTAssertEqual(result, .failure("Project not found"))
        // Mac-only kinds from the Flipper.
        result = await executor.perform(FDAction(kind: .copyURL, target: .url("http://localhost:5173"), source: .process), origin: .flipper, state: state, policy: ActionPolicy())
        XCTAssertEqual(result, .failure("Not available from Flipper"))
        // Kind/target mismatch.
        result = await executor.perform(FDAction(kind: .stopServer, target: .project(path: state.projects[0].path), source: .system), origin: .mac, state: state, policy: ActionPolicy())
        XCTAssertEqual(result, .failure("Unsupported action"))

        XCTAssertTrue(effects.entries.isEmpty)
    }

    func testActionIDsAreDeterministic() {
        let a = FDAction(kind: .openOnMac, target: .project(path: "/x"), source: .system)
        let b = FDAction(kind: .openOnMac, target: .project(path: "/x"), source: .agent)
        XCTAssertEqual(a.id, b.id)
        XCTAssertNotEqual(a.id, FDAction(kind: .openTerminal, target: .project(path: "/x"), source: .system).id)
    }
}
