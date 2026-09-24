import Foundation

/// FDP/1 framing: `TYPE|f1|…|fN*CCCC\n`. See docs/protocol.md.
public enum FDP {
    public static let protocolVersion = 1
    public static let maxFrameLength = 240
}

public struct Frame: Equatable, Hashable, Sendable {
    public let type: String
    public let fields: [String]

    public init(_ type: String, _ fields: [String] = []) {
        self.type = type
        self.fields = fields
    }

    public subscript(index: Int) -> String? {
        index < fields.count ? fields[index] : nil
    }
}

public enum FrameError: Error, Equatable, Sendable {
    case tooLong
    case badChecksum
    case nonPrintable
    case malformed
}

public enum CRC16 {
    /// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0.
    public static func ccittFalse<S: Sequence>(_ bytes: S) -> UInt16 where S.Element == UInt8 {
        var crc: UInt16 = 0xFFFF
        for byte in bytes {
            crc ^= UInt16(byte) << 8
            for _ in 0..<8 {
                crc = (crc & 0x8000) != 0 ? (crc << 1) ^ 0x1021 : crc << 1
            }
        }
        return crc
    }
}

public enum FrameCodec {
    /// Makes arbitrary text safe for a field: ASCII only, no separators, no
    /// control characters, at most `maxLength` bytes.
    public static func sanitize(_ text: String, maxLength: Int) -> String {
        let folded = text.folding(options: [.diacriticInsensitive, .widthInsensitive], locale: Locale(identifier: "en_US_POSIX"))
        var output = ""
        output.reserveCapacity(min(folded.utf8.count, maxLength))
        var lastWasSpace = false
        for scalar in folded.unicodeScalars {
            if output.utf8.count >= maxLength { break }
            var character: Character
            switch scalar.value {
            case 0x7C: character = "/"            // |
            case 0x2A: character = "+"            // *
            case 0x09, 0x0A, 0x0D, 0x20: character = " "
            case 0x2026: character = "."          // …
            case 0x2018, 0x2019: character = "'"
            case 0x201C, 0x201D: character = "\""
            case 0x2013, 0x2014: character = "-"
            case 0x21...0x7E: character = Character(scalar)
            default: character = "?"
            }
            if character == " " {
                if lastWasSpace || output.isEmpty { continue }
                lastWasSpace = true
            } else {
                lastWasSpace = false
            }
            output.append(character)
        }
        while output.hasSuffix(" ") { output.removeLast() }
        return output
    }

    /// Encodes a frame. Fields must already be sanitized; if the frame is
    /// still too long, the longest field is shortened until it fits.
    public static func encode(_ frame: Frame) -> Data {
        var fields = frame.fields
        var payload = ([frame.type] + fields).joined(separator: "|")
        // "*CCCC\n" is 6 bytes.
        while payload.utf8.count + 6 > FDP.maxFrameLength,
              let longest = fields.indices.max(by: { fields[$0].utf8.count < fields[$1].utf8.count }),
              !fields[longest].isEmpty {
            let excess = payload.utf8.count + 6 - FDP.maxFrameLength
            fields[longest] = String(fields[longest].utf8.prefix(max(0, fields[longest].utf8.count - excess)))!
            payload = ([frame.type] + fields).joined(separator: "|")
        }
        let crc = CRC16.ccittFalse(payload.utf8)
        return Data((payload + "*" + hex4(crc) + "\n").utf8)
    }

    static func hex4(_ value: UInt16) -> String {
        let digits = Array("0123456789ABCDEF")
        return String([digits[Int(value >> 12)], digits[Int((value >> 8) & 0xF)], digits[Int((value >> 4) & 0xF)], digits[Int(value & 0xF)]])
    }

    /// Decodes one line (without the trailing `\n`).
    public static func decodeLine<C: Collection>(_ line: C) -> Result<Frame, FrameError> where C.Element == UInt8 {
        let bytes = Array(line.filter { $0 != 0x0D })
        guard bytes.count + 1 <= FDP.maxFrameLength else { return .failure(.tooLong) }
        guard bytes.allSatisfy({ $0 >= 0x20 && $0 <= 0x7E }) else { return .failure(.nonPrintable) }
        guard let star = bytes.lastIndex(of: 0x2A), bytes.count - star == 5 else { return .failure(.malformed) }
        let payload = bytes[..<star]
        let hexDigits = bytes[(star + 1)...]
        guard hexDigits.allSatisfy({ ($0 >= 0x30 && $0 <= 0x39) || ($0 >= 0x41 && $0 <= 0x46) || ($0 >= 0x61 && $0 <= 0x66) }),
              let expected = UInt16(String(decoding: hexDigits, as: UTF8.self), radix: 16) else {
            return .failure(.malformed)
        }
        guard CRC16.ccittFalse(payload) == expected else { return .failure(.badChecksum) }
        let parts = String(decoding: payload, as: UTF8.self).split(separator: "|", omittingEmptySubsequences: false).map(String.init)
        guard let type = parts.first, (1...7).contains(type.count), type.allSatisfy({ $0 >= "A" && $0 <= "Z" }) else {
            return .failure(.malformed)
        }
        return .success(Frame(type, Array(parts.dropFirst())))
    }
}

/// Reassembles frames from an arbitrary chunked byte stream.
public struct FrameDecoder: Sendable {
    private var buffer: [UInt8] = []
    /// After an overlong line, drop bytes until the next newline so the
    /// stream re-synchronizes on a frame boundary.
    private var discarding = false

    public init() {}

    public mutating func feed(_ data: Data) -> [Result<Frame, FrameError>] {
        var results: [Result<Frame, FrameError>] = []
        for byte in data {
            if byte == 0x0A {
                if discarding {
                    discarding = false
                } else if !buffer.isEmpty {
                    results.append(FrameCodec.decodeLine(buffer))
                }
                buffer.removeAll(keepingCapacity: true)
                continue
            }
            if discarding { continue }
            buffer.append(byte)
            if buffer.count >= FDP.maxFrameLength {
                results.append(.failure(.tooLong))
                buffer.removeAll(keepingCapacity: true)
                discarding = true
            }
        }
        return results
    }

    public mutating func reset() {
        buffer.removeAll()
        discarding = false
    }
}
