import Foundation

public enum LogLevel: Int, Sendable, Comparable {
    case debug, info, warning, error

    public static func < (lhs: LogLevel, rhs: LogLevel) -> Bool { lhs.rawValue < rhs.rawValue }
}

public protocol LogSink: Sendable {
    func log(_ level: LogLevel, _ category: String, _ message: String)
}

/// Writes to stderr. The macOS app swaps in an `os.Logger` sink.
public struct StderrLogSink: LogSink {
    public let minimum: LogLevel
    public init(minimum: LogLevel = .info) { self.minimum = minimum }

    public func log(_ level: LogLevel, _ category: String, _ message: String) {
        guard level >= minimum else { return }
        let line = "[\(level)] \(category): \(message)\n"
        FileHandle.standardError.write(Data(line.utf8))
    }
}

public struct NullLogSink: LogSink {
    public init() {}
    public func log(_ level: LogLevel, _ category: String, _ message: String) {}
}

public struct Logger: Sendable {
    public let category: String
    public let sink: LogSink

    public init(_ category: String, sink: LogSink) {
        self.category = category
        self.sink = sink
    }

    public func debug(_ message: @autoclosure () -> String) { sink.log(.debug, category, message()) }
    public func info(_ message: @autoclosure () -> String) { sink.log(.info, category, message()) }
    public func warning(_ message: @autoclosure () -> String) { sink.log(.warning, category, message()) }
    public func error(_ message: @autoclosure () -> String) { sink.log(.error, category, message()) }
}

/// Helpers that keep secrets out of logs and error messages.
public enum Redact {
    /// Replaces every occurrence of `secret` in `text`. Use on any string that
    /// might echo a request (error bodies, URLs) before logging it.
    public static func removing(_ secret: String?, from text: String) -> String {
        guard let secret, secret.count >= 4 else { return text }
        return text.replacingOccurrences(of: secret, with: "<redacted>")
    }

    /// Drops query strings (which may carry tokens) from a URL for logging.
    public static func url(_ url: URL) -> String {
        guard var components = URLComponents(url: url, resolvingAgainstBaseURL: false) else { return "<url>" }
        if components.query != nil { components.query = "…" }
        components.user = nil
        components.password = nil
        return components.string ?? "<url>"
    }
}
