#if os(macOS)
import Darwin
import Foundation
import IOKit.ps
import Network
import SystemConfiguration
import FlipDeckCore

public enum MacHost {
    /// The user-facing computer name ("Mo's MacBook Pro"). Unlike
    /// `Host.current()`, this never blocks on DNS.
    public static func computerName() -> String {
        (SCDynamicStoreCopyComputerName(nil, nil) as String?) ?? ProcessInfo.processInfo.hostName
    }
}

/// Real machine metrics from Mach, sysctl, IOKit and Network.framework.
public final class MacMachineMetrics: MachineMetricsProvider, @unchecked Sendable {
    private let lock = NSLock()
    private var previousTicks: (busy: UInt64, total: UInt64)?
    private var networkReachable: Bool?
    private var networkInterface: String?
    private let monitor = NWPathMonitor()
    private let hostName: String

    public init() {
        hostName = MacHost.computerName()
        monitor.pathUpdateHandler = { [weak self] path in
            let interface: String?
            if path.usesInterfaceType(.wifi) { interface = "Wi-Fi" }
            else if path.usesInterfaceType(.wiredEthernet) { interface = "Ethernet" }
            else if path.usesInterfaceType(.cellular) { interface = "Cellular" }
            else { interface = nil }
            self?.setNetwork(path.status == .satisfied, interface)
        }
        monitor.start(queue: DispatchQueue(label: "flipdeck.network"))
    }

    deinit { monitor.cancel() }

    private func setNetwork(_ reachable: Bool, _ interface: String?) {
        lock.lock()
        networkReachable = reachable
        networkInterface = interface
        lock.unlock()
    }

    public func sample() async -> MachineStatus {
        let (used, total) = memory()
        lock.lock()
        let reachable = networkReachable
        let interface = networkInterface
        lock.unlock()
        return MachineStatus(
            hostName: hostName,
            osVersion: ProcessInfo.processInfo.operatingSystemVersionString,
            cpuUsage: cpuUsage(),
            memoryUsedBytes: used,
            memoryTotalBytes: total,
            battery: battery(),
            networkReachable: reachable,
            networkInterface: interface,
            bootTime: bootTime()
        )
    }

    /// Fraction of non-idle CPU ticks since the previous sample.
    func cpuUsage() -> Double? {
        var info = host_cpu_load_info()
        var count = mach_msg_type_number_t(MemoryLayout<host_cpu_load_info_data_t>.stride / MemoryLayout<integer_t>.stride)
        let result = withUnsafeMutablePointer(to: &info) { pointer in
            pointer.withMemoryRebound(to: integer_t.self, capacity: Int(count)) {
                host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, $0, &count)
            }
        }
        guard result == KERN_SUCCESS else { return nil }
        let user = UInt64(info.cpu_ticks.0)
        let system = UInt64(info.cpu_ticks.1)
        let idle = UInt64(info.cpu_ticks.2)
        let nice = UInt64(info.cpu_ticks.3)
        let busy = user + system + nice
        let total = busy + idle

        lock.lock()
        defer { lock.unlock() }
        defer { previousTicks = (busy, total) }
        guard let previous = previousTicks, total > previous.total else { return nil }
        return Double(busy - previous.busy) / Double(total - previous.total)
    }

    /// "Memory Used" as Activity Monitor reports it: app + wired + compressed.
    func memory() -> (UInt64?, UInt64) {
        let total = ProcessInfo.processInfo.physicalMemory
        var stats = vm_statistics64()
        var count = mach_msg_type_number_t(MemoryLayout<vm_statistics64_data_t>.stride / MemoryLayout<integer_t>.stride)
        let result = withUnsafeMutablePointer(to: &stats) { pointer in
            pointer.withMemoryRebound(to: integer_t.self, capacity: Int(count)) {
                host_statistics64(mach_host_self(), HOST_VM_INFO64, $0, &count)
            }
        }
        guard result == KERN_SUCCESS else { return (nil, total) }
        let pageSize = UInt64(getpagesize())
        let appPages = UInt64(stats.internal_page_count) - min(UInt64(stats.internal_page_count), UInt64(stats.purgeable_count))
        let used = (appPages + UInt64(stats.wire_count) + UInt64(stats.compressor_page_count)) * pageSize
        return (min(used, total), total)
    }

    func battery() -> BatteryStatus? {
        guard let blob = IOPSCopyPowerSourcesInfo()?.takeRetainedValue(),
              let sources = IOPSCopyPowerSourcesList(blob)?.takeRetainedValue() as? [CFTypeRef] else { return nil }
        for source in sources {
            guard let description = IOPSGetPowerSourceDescription(blob, source)?.takeUnretainedValue() as? [String: Any],
                  (description[kIOPSTypeKey] as? String) == kIOPSInternalBatteryType,
                  let current = description[kIOPSCurrentCapacityKey] as? Int,
                  let maximum = description[kIOPSMaxCapacityKey] as? Int, maximum > 0 else { continue }
            return BatteryStatus(
                percent: Int((Double(current) / Double(maximum) * 100).rounded()),
                charging: description[kIOPSIsChargingKey] as? Bool ?? false,
                onAC: (description[kIOPSPowerSourceStateKey] as? String) == kIOPSACPowerValue
            )
        }
        return nil
    }

    func bootTime() -> Date? {
        var mib: [Int32] = [CTL_KERN, KERN_BOOTTIME]
        var value = timeval()
        var size = MemoryLayout<timeval>.stride
        guard sysctl(&mib, 2, &value, &size, nil, 0) == 0 else { return nil }
        return Date(timeIntervalSince1970: TimeInterval(value.tv_sec) + TimeInterval(value.tv_usec) / 1_000_000)
    }
}
#endif
