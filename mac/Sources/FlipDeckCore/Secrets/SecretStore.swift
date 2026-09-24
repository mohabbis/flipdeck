import Foundation

public enum SecretKey: String, Sendable, CaseIterable {
    case vercelToken = "vercel.token"
}

/// Credential storage. The macOS app uses the Keychain; secrets never go
/// into settings files, logs, or anything sent to the Flipper.
public protocol SecretStore: Sendable {
    func get(_ key: SecretKey) -> String?
    func set(_ value: String?, for key: SecretKey) throws
}

public final class InMemorySecretStore: SecretStore, @unchecked Sendable {
    private let lock = NSLock()
    private var values: [SecretKey: String] = [:]

    public init(_ initial: [SecretKey: String] = [:]) {
        values = initial
    }

    public func get(_ key: SecretKey) -> String? {
        lock.lock()
        defer { lock.unlock() }
        return values[key]
    }

    public func set(_ value: String?, for key: SecretKey) throws {
        lock.lock()
        defer { lock.unlock() }
        values[key] = value
    }
}

/// Reads secrets from environment variables (headless/CI use only).
public struct EnvironmentSecretStore: SecretStore {
    public init() {}

    public func get(_ key: SecretKey) -> String? {
        switch key {
        case .vercelToken: return ProcessInfo.processInfo.environment["VERCEL_TOKEN"]
        }
    }

    public func set(_ value: String?, for key: SecretKey) throws {}
}
