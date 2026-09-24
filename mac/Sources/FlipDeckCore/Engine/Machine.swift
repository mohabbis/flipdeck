import Foundation

public protocol MachineMetricsProvider: Sendable {
    func sample() async -> MachineStatus
}

/// Portable baseline: only what Foundation can report reliably everywhere.
/// CPU, used memory, battery and network are left nil (unknown) rather than
/// guessed; the macOS platform module supplies real values.
public struct BasicMachineMetrics: MachineMetricsProvider {
    public init() {}

    public func sample() async -> MachineStatus {
        let info = ProcessInfo.processInfo
        return MachineStatus(
            hostName: info.hostName,
            osVersion: info.operatingSystemVersionString,
            memoryTotalBytes: info.physicalMemory,
            bootTime: Date().addingTimeInterval(-info.systemUptime)
        )
    }
}

/// Posts Mac notifications (UserNotifications on macOS).
public protocol MacNotifier: Sendable {
    func post(_ event: FDEvent)
}
