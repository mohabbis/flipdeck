import Foundation

public enum FlipperLinkState: Equatable, Sendable {
    case disconnected
    case handshaking
    case ready(appVersion: String)
    case incompatible(peerProtocol: Int)
}

public struct FlipperLinkStatus: Equatable, Sendable {
    public var transport: TransportStatus = .idle
    public var link: FlipperLinkState = .disconnected
    public var peerName: String?
    public var lastFrameAt: Date?
    public var committedGeneration: UInt32 = 0
    public var framesReceived = 0
    public var framesDropped = 0
    public var actionsHandled = 0

    public init() {}
}

/// Mac side of FDP/1 (docs/protocol.md): handshake, snapshots, heartbeats,
/// alerts, and action requests, over any `FlipperTransport`.
public actor FlipperSession {
    public typealias ActionHandler = @Sendable (FDAction) async -> ActionResult
    public typealias SeenHandler = @Sendable (_ eventID: String) async -> Void
    public typealias StatusHandler = @Sendable (FlipperLinkStatus) -> Void

    static let heartbeatInterval: TimeInterval = 5
    static let helloRetryInterval: TimeInterval = 3
    static let minSnapshotInterval: TimeInterval = 1
    static let alertReplayWindow: TimeInterval = 30 * 60
    static let maxQueuedAlerts = 3
    static let resultCacheSize = 32

    let transport: FlipperTransport
    let hostName: String
    let log: Logger
    let now: @Sendable () -> Date

    private var actionHandler: ActionHandler?
    private var seenHandler: SeenHandler?
    private var statusHandler: StatusHandler?

    public private(set) var status = FlipperLinkStatus()
    private var decoder = FrameDecoder()
    private var sessionID = ""
    private var maxChunk = 20
    private var eventsTask: Task<Void, Never>?

    // Snapshot bookkeeping.
    private var latest = FlipperSnapshot.empty
    private var sentRecords: [Frame]?
    private var generation: UInt32 = 0
    private var lastSnapshotAt: Date = .distantPast
    private var lastResyncAt: Date = .distantPast
    private var sentActions: [String: FDAction] = [:]
    private var previousActions: [String: FDAction] = [:]
    private var sentEventOwners: [String: String] = [:]

    // Live frames.
    private var machineFrame: Frame?
    private var sentMachineFrame: Frame?
    private var lastPingAt: Date = .distantPast
    private var lastHelloAt: Date = .distantPast

    // Recent alert-worthy events: (event, owner id). Replayed after every
    // handshake (the Flipper de-duplicates by id) until seen or expired.
    private var recentAlerts: [(FDEvent, String)] = []

    // REQ de-duplication, keyed by Flipper instance nonce + request id.
    private var peerNonce = ""
    private var resultCache: [(key: UInt32, result: ActionResult)] = []
    private var inFlight: Set<UInt32> = []

    public init(transport: FlipperTransport, hostName: String, log: Logger, now: @escaping @Sendable () -> Date = { Date() }) {
        self.transport = transport
        self.hostName = hostName
        self.log = log
        self.now = now
    }

    public func setHandlers(action: ActionHandler?, seen: SeenHandler?, status: StatusHandler?) {
        actionHandler = action
        seenHandler = seen
        statusHandler = status
    }

    public func start() {
        guard eventsTask == nil else { return }
        let stream = transport.events()
        eventsTask = Task { [weak self] in
            for await event in stream {
                await self?.handle(event)
            }
        }
        transport.start()
    }

    public func stop() {
        if case .ready = status.link { send(Frame("BYE", ["quit"])) }
        transport.stop()
        eventsTask?.cancel()
        eventsTask = nil
    }

    // MARK: Engine inputs

    public func update(snapshot: FlipperSnapshot, machine: MachineStatus?) {
        latest = snapshot
        machineFrame = machine.map(FlipperSnapshotBuilder.machineFrame)
        tick()
    }

    /// Sends an alert for `event` now if the link is ready; it is also kept
    /// for replay on the next handshake while recent and unseen.
    public func alert(_ event: FDEvent) {
        let owner = latest.eventOwners[event.id] ?? FlipperIDs.event(event.id)
        recentAlerts.removeAll { $0.0.id == event.id }
        recentAlerts.append((event, owner))
        if recentAlerts.count > Self.maxQueuedAlerts { recentAlerts.removeFirst(recentAlerts.count - Self.maxQueuedAlerts) }
        if case .ready = status.link { sendAlert(event, owner: owner) }
    }

    /// Call about once a second: heartbeats and coalesced snapshot sends.
    public func tick() {
        guard case .ready = status.link else {
            let sinceHello = now().timeIntervalSince(lastHelloAt)
            if status.link == .handshaking, sinceHello >= Self.helloRetryInterval {
                // The first HELLO can be lost (e.g. glued to a partial frame
                // left over from a previous connection); keep asking.
                sendHello()
            } else if isIncompatible, sinceHello >= Self.heartbeatInterval {
                // Keep announcing ourselves so the Flipper can show both versions.
                sendHello()
            }
            return
        }
        let time = now()
        if latest.records != sentRecords, time.timeIntervalSince(lastSnapshotAt) >= Self.minSnapshotInterval {
            sendSnapshot()
        }
        if let machineFrame, machineFrame != sentMachineFrame {
            send(machineFrame)
            sentMachineFrame = machineFrame
        }
        if time.timeIntervalSince(lastPingAt) >= Self.heartbeatInterval {
            send(Frame("PING", [String(generation), String(Int(time.timeIntervalSince1970))]))
            lastPingAt = time
        }
    }

    private var isIncompatible: Bool {
        if case .incompatible = status.link { return true }
        return false
    }

    // MARK: Transport events

    func handle(_ event: TransportEvent) async {
        switch event {
        case .status(let transportStatus):
            status.transport = transportStatus
            publishStatus()
        case .connected(let peerName, let chunk):
            decoder.reset()
            status.peerName = peerName
            status.link = .handshaking
            status.transport = .connected(peerName)
            maxChunk = max(20, chunk)
            sentRecords = nil
            sentMachineFrame = nil
            generation = 0
            sessionID = String(format: "%08x", UInt32.random(in: 0...UInt32.max))
            sendHello()
            publishStatus()
        case .disconnected(let reason):
            if let reason { log.info("Flipper disconnected: \(reason)") }
            status.link = .disconnected
            inFlight.removeAll()
            publishStatus()
        case .received(let data):
            for result in decoder.feed(data) {
                switch result {
                case .success(let frame):
                    status.framesReceived += 1
                    status.lastFrameAt = now()
                    await handle(frame)
                case .failure(let error):
                    status.framesDropped += 1
                    log.warning("Dropped malformed frame from Flipper: \(error)")
                }
            }
            publishStatus()
        }
    }

    func handle(_ frame: Frame) async {
        switch frame.type {
        case "HI":
            guard let proto = frame[0].flatMap(Int.init) else { return }
            guard proto == FDP.protocolVersion else {
                log.warning("Flipper speaks FDP/\(proto); this Mac speaks FDP/\(FDP.protocolVersion)")
                status.link = .incompatible(peerProtocol: proto)
                return
            }
            let nonce = frame[3] ?? ""
            if nonce != peerNonce {
                peerNonce = nonce
                resultCache.removeAll()
                inFlight.removeAll()
            }
            status.link = .ready(appVersion: frame[1] ?? "?")
            status.committedGeneration = frame[2].flatMap(UInt32.init) ?? 0
            sentRecords = nil
            lastSnapshotAt = .distantPast
            tick()
            replayAlerts()
        case "PONG":
            guard case .ready = status.link, let gen = frame[0].flatMap(UInt32.init) else { return }
            status.committedGeneration = gen
            if gen != generation, sentRecords != nil { requestResync() }
        case "SYNC":
            guard case .ready = status.link else { return }
            requestResync()
        case "REQ":
            guard case .ready = status.link, let req = frame[0].flatMap(UInt32.init), let actionID = frame[1] else { return }
            await handleRequest(req: req, actionID: actionID)
        case "SEEN":
            guard let owner = frame[0] else { return }
            let eventIDs = Set(sentEventOwners.filter { $0.value == owner }.map(\.key))
            recentAlerts.removeAll { $0.1 == owner }
            for eventID in eventIDs { await seenHandler?(eventID) }
        default:
            break // Unknown types are ignored for forward compatibility.
        }
    }

    private func requestResync() {
        let time = now()
        // Don't let a Flipper that can't commit drive a resend storm.
        guard time.timeIntervalSince(lastResyncAt) >= 2 else { return }
        lastResyncAt = time
        sentRecords = nil
        lastSnapshotAt = .distantPast
        tick()
    }

    private func handleRequest(req: UInt32, actionID: String) async {
        if let cached = resultCache.first(where: { $0.key == req }) {
            sendResult(req: req, cached.result)
            return
        }
        guard !inFlight.contains(req) else { return }
        guard let action = sentActions[actionID] ?? previousActions[actionID] else {
            finish(req: req, .failure("Action expired; refresh"))
            return
        }
        guard let actionHandler else {
            finish(req: req, .failure("Mac is not ready"))
            return
        }
        inFlight.insert(req)
        let result = await actionHandler(action)
        inFlight.remove(req)
        status.actionsHandled += 1
        finish(req: req, result)
    }

    private func finish(req: UInt32, _ result: ActionResult) {
        resultCache.append((req, result))
        if resultCache.count > Self.resultCacheSize { resultCache.removeFirst(resultCache.count - Self.resultCacheSize) }
        sendResult(req: req, result)
    }

    private func sendResult(req: UInt32, _ result: ActionResult) {
        send(Frame("RES", [String(req), result.ok ? "1" : "0", FrameCodec.sanitize(result.message, maxLength: FlipperLimits.resultMessage)]))
    }

    // MARK: Sending

    private func sendHello() {
        lastHelloAt = now()
        send(Frame("HELLO", [
            String(FDP.protocolVersion), sessionID, FrameCodec.sanitize(hostName, maxLength: FlipperLimits.host),
            String(maxChunk), String(Int(now().timeIntervalSince1970)),
        ]))
    }

    private func sendSnapshot() {
        generation &+= 1
        if generation == 0 { generation = 1 }
        var data = FrameCodec.encode(Frame("SNAP", [String(generation), String(latest.records.count)]))
        for record in latest.records { data.append(FrameCodec.encode(record)) }
        data.append(FrameCodec.encode(Frame("END", [String(generation)])))
        // One write keeps the snapshot contiguous on the wire.
        transport.send(data)
        if sentRecords != latest.records {
            previousActions = sentActions
        }
        sentActions = latest.actions
        sentEventOwners.merge(latest.eventOwners) { _, new in new }
        if sentEventOwners.count > 256 { sentEventOwners = latest.eventOwners }
        sentRecords = latest.records
        lastSnapshotAt = now()
    }

    private func replayAlerts() {
        let cutoff = now().addingTimeInterval(-Self.alertReplayWindow)
        recentAlerts.removeAll { $0.0.timestamp < cutoff || $0.0.acknowledged }
        for (event, owner) in recentAlerts { sendAlert(event, owner: owner) }
    }

    private func sendAlert(_ event: FDEvent, owner: String) {
        // The alert's actions live in the snapshot; make sure the Flipper has
        // the latest one first, even if that skips the snapshot rate limit.
        if latest.records != sentRecords { sendSnapshot() }
        sentEventOwners[event.id] = owner
        send(Frame("ALR", [
            owner, event.severity.wireCode,
            FrameCodec.sanitize(event.projectName ?? "", maxLength: FlipperLimits.name),
            FrameCodec.sanitize(event.title, maxLength: FlipperLimits.title),
            FrameCodec.sanitize(event.message, maxLength: FlipperLimits.message),
        ]))
    }

    /// The Mac user acknowledged an event; stop replaying its alert.
    public func acknowledged(eventID: String) {
        recentAlerts.removeAll { $0.0.id == eventID }
    }

    private func send(_ frame: Frame) {
        transport.send(FrameCodec.encode(frame))
    }

    private func publishStatus() {
        statusHandler?(status)
    }
}
