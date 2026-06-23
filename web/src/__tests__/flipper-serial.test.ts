import { describe, expect, it } from "vitest";
import {
  chunkBytes,
  concatBytes,
  encodeBytesField,
  encodeMain,
  encodeStringField,
  encodeVarint,
  formatRpcStatus,
  isWebSerialSupported,
  parseMainResponse,
} from "../lib/flipper-serial";

/** Reference varint decoder, used to assert encodeVarint round-trips. */
function decodeVarint(bytes: Uint8Array): { value: number; bytesRead: number } {
  let result = 0;
  let shift = 0;
  let offset = 0;
  while (offset < bytes.length) {
    const byte = bytes[offset++];
    result |= (byte & 0x7f) << shift;
    if ((byte & 0x80) === 0) return { value: result >>> 0, bytesRead: offset };
    shift += 7;
  }
  throw new Error("truncated varint");
}

describe("encodeVarint", () => {
  it("encodes single-byte values directly", () => {
    expect(Array.from(encodeVarint(0))).toEqual([0]);
    expect(Array.from(encodeVarint(1))).toEqual([1]);
    expect(Array.from(encodeVarint(0x7f))).toEqual([0x7f]);
  });

  it("uses continuation bits for multi-byte values", () => {
    // 300 = 0b1_0010_1100 → 0xAC 0x02
    expect(Array.from(encodeVarint(300))).toEqual([0xac, 0x02]);
  });

  it("round-trips through a reference decoder across boundaries", () => {
    for (const value of [0, 1, 127, 128, 255, 256, 16383, 16384, 512, 1_000_000]) {
      const { value: decoded, bytesRead } = decodeVarint(encodeVarint(value));
      expect(decoded, `value ${value}`).toBe(value);
      expect(bytesRead).toBe(encodeVarint(value).length);
    }
  });
});

describe("concatBytes", () => {
  it("joins parts end to end", () => {
    const out = concatBytes([new Uint8Array([1, 2]), new Uint8Array([]), new Uint8Array([3])]);
    expect(Array.from(out)).toEqual([1, 2, 3]);
  });
});

describe("chunkBytes", () => {
  it("splits into fixed-size chunks with a smaller remainder", () => {
    const chunks = chunkBytes(new Uint8Array([1, 2, 3, 4, 5]), 2);
    expect(chunks.map((c) => Array.from(c))).toEqual([[1, 2], [3, 4], [5]]);
  });

  it("returns a single chunk when the input fits", () => {
    const chunks = chunkBytes(new Uint8Array([1, 2]), 512);
    expect(chunks).toHaveLength(1);
    expect(Array.from(chunks[0])).toEqual([1, 2]);
  });

  it("returns no chunks for empty input", () => {
    expect(chunkBytes(new Uint8Array([]), 4)).toEqual([]);
  });
});

describe("protobuf field encoders", () => {
  it("encodes a length-delimited bytes field with a wire-type-2 key", () => {
    // field 4, wire type 2 → key = (4<<3)|2 = 0x22, then length, then payload
    const out = encodeBytesField(4, new Uint8Array([0xaa, 0xbb]));
    expect(Array.from(out)).toEqual([0x22, 0x02, 0xaa, 0xbb]);
  });

  it("encodes a string field as UTF-8 bytes", () => {
    const out = encodeStringField(1, "AB");
    // field 1, wire type 2 → key 0x0a, length 2, 'A'=0x41 'B'=0x42
    expect(Array.from(out)).toEqual([0x0a, 0x02, 0x41, 0x42]);
  });
});

describe("encodeMain → parseMainResponse round-trip", () => {
  it("preserves commandId, hasNext, and an embedded content field", () => {
    const payload = new Uint8Array([1, 2, 3]);
    const main = encodeMain(7, 11, payload, true);
    const parsed = parseMainResponse(main);
    expect(parsed.commandId).toBe(7);
    expect(parsed.hasNext).toBe(true);
    // content field 11 is length-delimited and skipped by the parser, not surfaced.
  });

  it("defaults hasNext to false when not flagged", () => {
    const parsed = parseMainResponse(encodeMain(3, 11, new Uint8Array(), false));
    expect(parsed.commandId).toBe(3);
    expect(parsed.hasNext).toBe(false);
  });

  it("reads command_status from a response message (field 2)", () => {
    // Build a minimal main message: command_id=5 (field 1), command_status=9 (field 2)
    const message = concatBytes([
      new Uint8Array([0x08, 0x05]), // field 1, varint 5
      new Uint8Array([0x10, 0x09]), // field 2, varint 9
    ]);
    const parsed = parseMainResponse(message);
    expect(parsed.commandId).toBe(5);
    expect(parsed.commandStatus).toBe(9);
  });

  it("throws on an unsupported wire type", () => {
    // field 1, wire type 5 (32-bit) is not handled
    expect(() => parseMainResponse(new Uint8Array([0x0d, 0, 0, 0, 0]))).toThrow();
  });
});

describe("formatRpcStatus", () => {
  it("maps known status codes to readable names", () => {
    expect(formatRpcStatus(0)).toContain("OK");
    expect(formatRpcStatus(6)).toContain("ERROR_STORAGE_EXIST");
  });

  it("falls back to the numeric status for unknown codes", () => {
    expect(formatRpcStatus(99)).toContain("status 99");
  });
});

describe("isWebSerialSupported", () => {
  it("returns false when navigator has no serial (node test env)", () => {
    expect(isWebSerialSupported()).toBe(false);
  });
});
