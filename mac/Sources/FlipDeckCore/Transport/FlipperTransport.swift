import Foundation

public enum TransportStatus: Equatable, Sendable {
    /// Can't operate (Bluetooth off, permission denied, unsupported).
    case unavailable(String)
    case idle
    case scanning
    case connecting(String)
    case connected(String)

    public var label: String {
        switch self {
        case .unavailable(let reason): return reason
        case .idle: return "Idle"
        case .scanning: return "Searching for Flipper…"
        case .connecting(let name): return "Connecting to \(name)…"
        case .connected(let name): return "Connected to \(name)"
        }
    }
}

public enum TransportEvent: Sendable {
    case status(TransportStatus)
    /// The byte pipe is ready. `maxChunk` is the largest write the peer can
    /// receive in one piece (BLE: ATT MTU − 3).
    case connected(peerName: String, maxChunk: Int)
    case disconnected(reason: String?)
    case received(Data)
}

/// A reliable, ordered byte stream to one Flipper. The session doesn't know
/// or care whether it's BLE, USB serial, or an in-memory pipe.
public protocol FlipperTransport: AnyObject, Sendable {
    /// Single-consumer event stream; call once.
    func events() -> AsyncStream<TransportEvent>
    func start()
    func stop()
    /// Enqueues bytes for sending. Order across calls is preserved. Delivery
    /// failures surface as `.disconnected`.
    func send(_ data: Data)
}

/// In-memory transport for tests and the headless demo: bytes the session
/// sends are handed to `onSend`; tests inject inbound bytes with `receive`.
public final class LoopbackTransport: FlipperTransport, @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: AsyncStream<TransportEvent>.Continuation?
    private var pending: [TransportEvent] = []
    private var sent: [Data] = []
    public var onSend: (@Sendable (Data) -> Void)?

    public init() {}

    public func events() -> AsyncStream<TransportEvent> {
        AsyncStream { continuation in
            lock.lock()
            self.continuation = continuation
            let backlog = pending
            pending.removeAll()
            lock.unlock()
            for event in backlog { continuation.yield(event) }
        }
    }

    public func start() {}
    public func stop() {}

    public func send(_ data: Data) {
        lock.lock()
        sent.append(data)
        let handler = onSend
        lock.unlock()
        handler?(data)
    }

    public func emit(_ event: TransportEvent) {
        lock.lock()
        if let continuation {
            lock.unlock()
            continuation.yield(event)
        } else {
            pending.append(event)
            lock.unlock()
        }
    }

    public func receive(_ string: String) { emit(.received(Data(string.utf8))) }

    /// Everything sent so far, as decoded frames (drops malformed ones).
    public func sentFrames() -> [Frame] {
        lock.lock()
        let all = sent.reduce(Data(), +)
        lock.unlock()
        var decoder = FrameDecoder()
        return decoder.feed(all).compactMap { try? $0.get() }
    }

    public func clearSent() {
        lock.lock()
        sent.removeAll()
        lock.unlock()
    }
}
