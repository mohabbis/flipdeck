#if os(macOS)
import CoreBluetooth
import Foundation
import FlipDeckCore

/// FDP/1 over the Flipper's BLE serial GATT service, exposed by the FlipDeck
/// .fap under its own profile (see ARCHITECTURE.md → Bluetooth details).
///
/// - RX (…62fe0000): we write. Requires a paired, encrypted link; the first
///   authenticated access makes macOS show the pairing prompt and the Flipper
///   show the PIN.
/// - TX (…61fe0000): the Flipper indicates.
/// - Flow control (…63fe0000): uint32 big-endian number of bytes the Flipper can
///   accept. We write *exactly up to* the credit; the Flipper re-arms it only
///   after the credit hits zero and its buffer is drained.
public final class BLESerialTransport: NSObject, FlipperTransport, @unchecked Sendable {
    static let serviceUUID = CBUUID(string: "8FE5B3D5-2E7F-4A98-2A48-7ACC60FE0000")
    static let rxUUID = CBUUID(string: "19ED82AE-ED21-4C9D-4145-228E62FE0000")
    static let txUUID = CBUUID(string: "19ED82AE-ED21-4C9D-4145-228E61FE0000")
    static let flowUUID = CBUUID(string: "19ED82AE-ED21-4C9D-4145-228E63FE0000")
    static let namePrefix = "FlipDeck"

    private let queue = DispatchQueue(label: "flipdeck.ble")
    private var central: CBCentralManager!
    private var continuation: AsyncStream<TransportEvent>.Continuation?

    private var knownPeripheralID: UUID?
    private var peripheral: CBPeripheral?
    private var rx: CBCharacteristic?
    private var tx: CBCharacteristic?
    private var flow: CBCharacteristic?
    private var running = false
    private var linkReady = false

    private var outbound = Data()
    private var credit = 0
    private var writeInFlight = false
    private var creditKnown = false
    private var notifyReady = false

    /// Called on the BLE queue when a FlipDeck Flipper is first identified,
    /// so the app can remember it and reconnect directly next time.
    public var onPeripheralIdentified: (@Sendable (UUID) -> Void)?

    public init(knownPeripheralID: UUID?) {
        self.knownPeripheralID = knownPeripheralID
        super.init()
    }

    public func events() -> AsyncStream<TransportEvent> {
        AsyncStream { continuation in
            queue.async { self.continuation = continuation }
        }
    }

    public func start() {
        queue.async {
            self.running = true
            if self.central == nil {
                self.central = CBCentralManager(delegate: self, queue: self.queue)
            } else {
                self.connectOrScan()
            }
        }
    }

    public func stop() {
        queue.async {
            self.running = false
            self.central?.stopScan()
            if let peripheral = self.peripheral { self.central?.cancelPeripheralConnection(peripheral) }
            self.resetLink()
            self.emit(.status(.idle))
        }
    }

    /// Drops the remembered Flipper and searches again (e.g. to pair another).
    public func forget() {
        queue.async {
            self.knownPeripheralID = nil
            if let peripheral = self.peripheral { self.central?.cancelPeripheralConnection(peripheral) }
            self.peripheral = nil
            self.resetLink()
            if self.running { self.connectOrScan() }
        }
    }

    public func send(_ data: Data) {
        queue.async {
            guard self.linkReady else { return }
            self.outbound.append(data)
            self.pump()
        }
    }

    // MARK: - Internals (BLE queue only)

    private func emit(_ event: TransportEvent) {
        continuation?.yield(event)
    }

    private func resetLink(reason: String? = nil) {
        let wasReady = linkReady
        rx = nil
        tx = nil
        flow = nil
        outbound.removeAll()
        credit = 0
        creditKnown = false
        notifyReady = false
        writeInFlight = false
        linkReady = false
        if wasReady { emit(.disconnected(reason: reason)) }
    }

    private func connectOrScan() {
        guard running, central.state == .poweredOn else { return }
        if let id = knownPeripheralID, let known = central.retrievePeripherals(withIdentifiers: [id]).first {
            peripheral = known
            known.delegate = self
            emit(.status(.connecting(known.name ?? "Flipper")))
            // A pending connect has no timeout: CoreBluetooth completes it
            // whenever the Flipper comes into range and advertises.
            central.connect(known, options: nil)
        } else {
            emit(.status(.scanning))
            central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
        }
    }

    private func checkReady() {
        guard !linkReady, notifyReady, creditKnown, rx != nil, let peripheral else { return }
        linkReady = true
        let chunk = min(243, peripheral.maximumWriteValueLength(for: .withoutResponse))
        let name = peripheral.name ?? "Flipper"
        emit(.status(.connected(name)))
        emit(.connected(peerName: name, maxChunk: chunk))
        pump()
    }

    private func pump() {
        guard linkReady, !writeInFlight, credit > 0, !outbound.isEmpty, let peripheral, let rx else { return }
        let maxWrite = min(243, peripheral.maximumWriteValueLength(for: .withoutResponse))
        let size = min(outbound.count, credit, maxWrite)
        let chunk = outbound.prefix(size)
        outbound.removeFirst(size)
        credit -= size
        writeInFlight = true
        // With-response writes: ordered, acknowledged, and they surface auth
        // errors (which is what triggers pairing on first use).
        peripheral.writeValue(Data(chunk), for: rx, type: .withResponse)
    }
}

extension BLESerialTransport: CBCentralManagerDelegate {
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            connectOrScan()
        case .poweredOff:
            resetLink()
            emit(.status(.unavailable("Bluetooth is off")))
        case .unauthorized:
            emit(.status(.unavailable("Bluetooth permission denied (System Settings → Privacy & Security → Bluetooth)")))
        case .unsupported:
            emit(.status(.unavailable("Bluetooth LE is not supported on this Mac")))
        default:
            emit(.status(.unavailable("Bluetooth is unavailable")))
        }
    }

    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let advertisedName = advertisementData[CBAdvertisementDataLocalNameKey] as? String
        guard let name = advertisedName ?? peripheral.name, name.hasPrefix(Self.namePrefix) else { return }
        central.stopScan()
        self.peripheral = peripheral
        peripheral.delegate = self
        emit(.status(.connecting(name)))
        central.connect(peripheral, options: nil)
    }

    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        if knownPeripheralID != peripheral.identifier {
            knownPeripheralID = peripheral.identifier
            onPeripheralIdentified?(peripheral.identifier)
        }
        peripheral.discoverServices([Self.serviceUUID])
    }

    public func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        resetLink()
        emit(.status(.unavailable("Couldn't connect: \(error?.localizedDescription ?? "unknown error")")))
        queue.asyncAfter(deadline: .now() + 5) { [weak self] in self?.connectOrScan() }
    }

    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        resetLink(reason: error?.localizedDescription ?? "disconnected")
        guard running else { return }
        emit(.status(.connecting(peripheral.name ?? "Flipper")))
        // Re-arm immediately; completes when the Flipper is back in range.
        central.connect(peripheral, options: nil)
    }
}

extension BLESerialTransport: CBPeripheralDelegate {
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == Self.serviceUUID }) else {
            emit(.status(.unavailable("Flipper isn't running FlipDeck (serial service missing)")))
            central.cancelPeripheralConnection(peripheral)
            return
        }
        peripheral.discoverCharacteristics([Self.rxUUID, Self.txUUID, Self.flowUUID], for: service)
    }

    public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        for characteristic in service.characteristics ?? [] {
            switch characteristic.uuid {
            case Self.rxUUID: rx = characteristic
            case Self.txUUID:
                tx = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            case Self.flowUUID:
                flow = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
            default: break
            }
        }
        if rx == nil || tx == nil || flow == nil {
            emit(.status(.unavailable("Flipper serial service is incomplete")))
            central.cancelPeripheralConnection(peripheral)
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        if let error {
            // Typically "insufficient authentication" while pairing is in progress;
            // macOS retries after the user enters the PIN shown on the Flipper.
            emit(.status(.connecting("Pairing… enter the PIN shown on the Flipper (\(error.localizedDescription))")))
            return
        }
        if characteristic.uuid == Self.txUUID, characteristic.isNotifying {
            notifyReady = true
            checkReady()
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, let value = characteristic.value else { return }
        switch characteristic.uuid {
        case Self.txUUID:
            if linkReady { emit(.received(value)) }
        case Self.flowUUID where value.count >= 4:
            credit = value.prefix(4).reduce(0) { ($0 << 8) | Int($1) }
            creditKnown = true
            checkReady()
            pump()
        default:
            break
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        writeInFlight = false
        if let error {
            emit(.status(.unavailable("Write failed: \(error.localizedDescription)")))
            central.cancelPeripheralConnection(peripheral)
            return
        }
        pump()
    }
}
#endif
