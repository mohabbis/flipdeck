import Foundation

// MARK: - Linking

/// Reads the link that `vercel link` writes into a project. This gives an exact
/// local-project ↔ Vercel-project mapping, with no name guessing.
public enum VercelLinkReader {
    struct ProjectFile: Decodable {
        let projectId: String
        let orgId: String
        let projectName: String?
    }

    struct RepoFile: Decodable {
        struct Entry: Decodable {
            let id: String
            let name: String?
            let directory: String?
        }
        let orgId: String
        let projects: [Entry]
    }

    public static func read(projectPath: String, fileManager: FileManager = .default) -> VercelLink? {
        let decoder = JSONDecoder()
        if let data = fileManager.contents(atPath: projectPath + "/.vercel/project.json"),
           let file = try? decoder.decode(ProjectFile.self, from: data),
           !file.projectId.isEmpty, !file.orgId.isEmpty {
            return VercelLink(projectID: file.projectId, orgID: file.orgId, projectName: file.projectName)
        }
        // Monorepo link (`vercel link --repo`): only unambiguous when the repo
        // root itself is the linked directory, or there is exactly one project.
        if let data = fileManager.contents(atPath: projectPath + "/.vercel/repo.json"),
           let file = try? decoder.decode(RepoFile.self, from: data) {
            let rootEntry = file.projects.first { $0.directory == "." || $0.directory == "" }
            if let entry = rootEntry ?? (file.projects.count == 1 ? file.projects[0] : nil) {
                return VercelLink(projectID: entry.id, orgID: file.orgId, projectName: entry.name)
            }
        }
        return nil
    }
}

// MARK: - API

public enum VercelError: Error, Equatable, CustomStringConvertible {
    case unauthorized
    case rateLimited(retryAfter: TimeInterval?)
    case http(Int)
    case decoding(String)
    case transport(String)

    public var description: String {
        switch self {
        case .unauthorized: return "Vercel rejected the token"
        case .rateLimited: return "Vercel rate limit reached"
        case .http(let status): return "Vercel returned HTTP \(status)"
        case .decoding(let detail): return "Unexpected Vercel response (\(detail))"
        case .transport(let detail): return "Network error: \(detail)"
        }
    }
}

struct VercelDeploymentDTO: Decodable {
    let uid: String
    let url: String?
    let state: String?
    let readyState: String?
    let target: String?
    let inspectorUrl: String?
    let created: Double?
    let createdAt: Double?
    let meta: [String: String]?
    let errorMessage: String?

    enum CodingKeys: String, CodingKey {
        case uid, url, state, readyState, target, inspectorUrl, created, createdAt, meta, errorMessage
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        uid = try container.decode(String.self, forKey: .uid)
        url = try? container.decodeIfPresent(String.self, forKey: .url)
        state = try? container.decodeIfPresent(String.self, forKey: .state)
        readyState = try? container.decodeIfPresent(String.self, forKey: .readyState)
        target = try? container.decodeIfPresent(String.self, forKey: .target)
        inspectorUrl = try? container.decodeIfPresent(String.self, forKey: .inspectorUrl)
        created = try? container.decodeIfPresent(Double.self, forKey: .created)
        createdAt = try? container.decodeIfPresent(Double.self, forKey: .createdAt)
        // meta is documented as string→string, but be lenient: drop non-strings.
        if let raw = try? container.decodeIfPresent([String: LenientString].self, forKey: .meta) {
            meta = raw.compactMapValues(\.value)
        } else {
            meta = nil
        }
        errorMessage = try? container.decodeIfPresent(String.self, forKey: .errorMessage)
    }
}

struct LenientString: Decodable {
    let value: String?
    init(from decoder: Decoder) throws {
        value = try? decoder.singleValueContainer().decode(String.self)
    }
}

struct VercelDeploymentsResponse: Decodable {
    let deployments: [VercelDeploymentDTO]
}

public struct VercelClient: Sendable {
    let token: String
    let http: HTTPClient
    let baseURL: URL

    public init(token: String, http: HTTPClient, baseURL: URL = URL(string: "https://api.vercel.com")!) {
        self.token = token
        self.http = http
        self.baseURL = baseURL
    }

    public func deployments(for link: VercelLink, localProjectID: String, limit: Int = 5) async throws -> [Deployment] {
        var components = URLComponents(url: baseURL.appendingPathComponent("v6/deployments"), resolvingAgainstBaseURL: false)!
        var query = [URLQueryItem(name: "projectId", value: link.projectID), URLQueryItem(name: "limit", value: String(limit))]
        // Personal accounts have a user id as orgId; only team ids go in teamId.
        if link.orgID.hasPrefix("team_") { query.append(URLQueryItem(name: "teamId", value: link.orgID)) }
        components.queryItems = query

        let response: HTTPResponse
        do {
            response = try await http.get(components.url!, headers: ["Authorization": "Bearer \(token)", "Accept": "application/json"])
        } catch {
            throw VercelError.transport(Redact.removing(token, from: error.localizedDescription))
        }

        switch response.status {
        case 200: break
        case 401, 403: throw VercelError.unauthorized
        case 429:
            let reset = response.header("x-ratelimit-reset").flatMap(TimeInterval.init)
            throw VercelError.rateLimited(retryAfter: reset.map { max(0, $0 - Date().timeIntervalSince1970) })
        default: throw VercelError.http(response.status)
        }

        let decoded: VercelDeploymentsResponse
        do {
            decoded = try JSONDecoder().decode(VercelDeploymentsResponse.self, from: response.body)
        } catch {
            throw VercelError.decoding(String(describing: error).prefix(120).description)
        }
        return decoded.deployments.compactMap { Self.map($0, localProjectID: localProjectID) }
    }

    static func map(_ dto: VercelDeploymentDTO, localProjectID: String) -> Deployment? {
        guard let rawState = (dto.state ?? dto.readyState)?.uppercased() else { return nil }
        let state: DeploymentState
        var errorMessage = dto.errorMessage
        switch rawState {
        case "QUEUED", "INITIALIZING": state = .queued
        case "BUILDING": state = .building
        case "READY": state = .ready
        case "ERROR": state = .error
        case "CANCELED": state = .canceled
        case "BLOCKED":
            state = .error
            errorMessage = errorMessage ?? "Deployment blocked"
        default: return nil
        }
        let millis = dto.created ?? dto.createdAt ?? 0
        return Deployment(
            id: dto.uid,
            provider: "vercel",
            projectID: localProjectID,
            state: state,
            target: dto.target,
            url: dto.url.map { $0.hasPrefix("http") ? $0 : "https://\($0)" },
            inspectorURL: dto.inspectorUrl.flatMap(Self.safeVercelURL),
            createdAt: Date(timeIntervalSince1970: millis / 1000),
            branch: dto.meta?["githubCommitRef"] ?? dto.meta?["gitlabCommitRef"] ?? dto.meta?["bitbucketCommitRef"],
            commitMessage: dto.meta?["githubCommitMessage"] ?? dto.meta?["gitlabCommitMessage"] ?? dto.meta?["bitbucketCommitMessage"],
            errorMessage: errorMessage
        )
    }

    /// Only https URLs on vercel.com are opened from API data.
    static func safeVercelURL(_ string: String) -> String? {
        let candidate = string.hasPrefix("http") ? string : "https://\(string)"
        guard let url = URL(string: candidate), url.scheme == "https", let host = url.host,
              host == "vercel.com" || host.hasSuffix(".vercel.com") else { return nil }
        return candidate
    }
}

// MARK: - Integration

public protocol Integration: Sendable {
    var id: String { get }
    var displayName: String { get }
}

public struct IntegrationRefresh: Sendable {
    public let status: IntegrationStatus
    /// Only projects that were successfully fetched this round are present;
    /// others keep their previous data.
    public let deployments: [String: [Deployment]]
}

/// Polls Vercel for linked projects with per-project exponential backoff.
public actor VercelIntegration: Integration {
    public nonisolated let id = "vercel"
    public nonisolated let displayName = "Vercel"

    let http: HTTPClient
    let log: Logger
    private var backoff: [String: (failures: Int, nextAttempt: Date)] = [:]
    private var lastSync: Date?
    private var unauthorizedToken: String?

    public init(http: HTTPClient, log: Logger) {
        self.http = http
        self.log = log
    }

    public func refresh(projects: [Project], token: String?, now: Date = Date()) async -> IntegrationRefresh {
        let linked = projects.filter { $0.vercel != nil }
        guard let token, !token.isEmpty else {
            return IntegrationRefresh(status: IntegrationStatus(id: id, displayName: displayName, health: .notConfigured, linkedProjects: linked.count), deployments: [:])
        }
        if unauthorizedToken == token {
            return IntegrationRefresh(status: IntegrationStatus(id: id, displayName: displayName, health: .unauthorized, lastSync: lastSync, linkedProjects: linked.count), deployments: [:])
        }

        let client = VercelClient(token: token, http: http)
        var results: [String: [Deployment]] = [:]
        var lastError: String?

        for project in linked {
            guard let link = project.vercel else { continue }
            if let entry = backoff[project.id], entry.nextAttempt > now { continue }
            do {
                results[project.id] = try await client.deployments(for: link, localProjectID: project.id)
                backoff[project.id] = nil
            } catch VercelError.unauthorized {
                log.warning("Vercel token rejected; pausing until it changes")
                unauthorizedToken = token
                return IntegrationRefresh(status: IntegrationStatus(id: id, displayName: displayName, health: .unauthorized, lastSync: lastSync, linkedProjects: linked.count), deployments: results)
            } catch {
                let failures = (backoff[project.id]?.failures ?? 0) + 1
                var delay = min(30 * pow(2, Double(failures - 1)), 600)
                if case VercelError.rateLimited(let retryAfter?) = error { delay = max(delay, retryAfter) }
                backoff[project.id] = (failures, now.addingTimeInterval(delay))
                lastError = "\(project.name): \(error)"
                log.warning("Vercel refresh failed for \(project.name): \(error)")
            }
        }

        if !results.isEmpty || linked.isEmpty { lastSync = now }
        let health: IntegrationHealth = lastError.map { .error($0) } ?? .ok
        return IntegrationRefresh(status: IntegrationStatus(id: id, displayName: displayName, health: health, lastSync: lastSync, linkedProjects: linked.count), deployments: results)
    }
}
