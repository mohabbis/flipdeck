import Foundation

/// Short, stable identifiers for things sent to the Flipper. They must be the
/// same across snapshots (so an action id seen a moment ago still resolves)
/// and short (every byte crosses BLE).
public enum StableID {
    /// 64-bit FNV-1a; deterministic across launches, unlike `Hasher`.
    public static func fnv1a(_ string: String) -> UInt64 {
        var hash: UInt64 = 0xcbf2_9ce4_8422_2325
        for byte in string.utf8 {
            hash ^= UInt64(byte)
            hash = hash &* 0x0000_0100_0000_01B3
        }
        return hash
    }

    /// `prefix` + 6 base-36 characters (~2.2 billion values).
    public static func short(_ prefix: String, _ key: String) -> String {
        let value = fnv1a(key) % 2_176_782_336 // 36^6
        var digits = String(value, radix: 36)
        while digits.count < 6 { digits = "0" + digits }
        return prefix + digits
    }
}
