// Minimal Web Serial type declarations for Flipper Zero CLI communication
interface FlipperSerialPort {
  open(options: { baudRate: number; dataBits: number; stopBits: number; parity: string }): Promise<void>;
  close(): Promise<void>;
  readonly readable: ReadableStream<Uint8Array>;
  readonly writable: WritableStream<Uint8Array>;
}
interface FlipperSerialApi {
  requestPort(options: { filters: unknown[] }): Promise<FlipperSerialPort>;
}
interface NavigatorWithSerial extends Navigator {
  serial: FlipperSerialApi;
}

const BAUD_RATE = 230400;
const CLI_PROMPT = ">: ";
const RPC_CHUNK_SIZE = 512;

const COMMAND_STATUS: Record<number, string> = {
  0: "OK",
  1: "ERROR",
  2: "ERROR_DECODE",
  3: "ERROR_NOT_IMPLEMENTED",
  4: "ERROR_BUSY",
  5: "ERROR_STORAGE_NOT_READY",
  6: "ERROR_STORAGE_EXIST",
  7: "ERROR_STORAGE_NOT_EXIST",
  8: "ERROR_STORAGE_INVALID_PARAMETER",
  9: "ERROR_STORAGE_DENIED",
  10: "ERROR_STORAGE_INVALID_NAME",
  11: "ERROR_STORAGE_INTERNAL",
  12: "ERROR_STORAGE_NOT_IMPLEMENTED",
  13: "ERROR_STORAGE_ALREADY_OPEN",
  14: "ERROR_CONTINUOUS_COMMAND_INTERRUPTED",
  15: "ERROR_INVALID_PARAMETERS",
  18: "ERROR_STORAGE_DIR_NOT_EMPTY",
};

export type ProgressCallback = (message: string, fraction: number) => void;

export class FlipperSerialError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "FlipperSerialError";
  }
}

export function isWebSerialSupported(): boolean {
  return typeof navigator !== "undefined" && "serial" in navigator;
}

export class FlipperSerial {
  private port: FlipperSerialPort | null = null;
  private reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
  private writer: WritableStreamDefaultWriter<Uint8Array> | null = null;
  private inputBuffer = "";
  private rpcBuffer: Uint8Array<ArrayBufferLike> = new Uint8Array();
  private commandId = 0;
  private inRpcSession = false;
  private readonly decoder = new TextDecoder();
  private readonly encoder = new TextEncoder();

  get connected() {
    return this.port !== null;
  }

  async connect(): Promise<void> {
    const nav = navigator as NavigatorWithSerial;
    const port = await nav.serial.requestPort({ filters: [] });
    await port.open({ baudRate: BAUD_RATE, dataBits: 8, stopBits: 1, parity: "none" });
    this.port = port;
    this.reader = port.readable.getReader();
    this.writer = port.writable.getWriter();
    // Wake CLI in case Flipper is showing a banner
    await this.send("\r\n");
    await this.waitForPrompt(8000);
  }

  async disconnect(): Promise<void> {
    try {
      this.reader?.releaseLock();
      this.writer?.releaseLock();
    } finally {
      try {
        await this.port?.close();
      } catch {
        // Already closed
      }
      this.reader = null;
      this.writer = null;
      this.port = null;
      this.inputBuffer = "";
      this.rpcBuffer = new Uint8Array();
      this.inRpcSession = false;
    }
  }

  private async send(text: string): Promise<void> {
    if (!this.writer) throw new FlipperSerialError("Not connected");
    await this.writer.write(this.encoder.encode(text));
  }

  private async sendBytes(bytes: Uint8Array): Promise<void> {
    if (!this.writer) throw new FlipperSerialError("Not connected");
    await this.writer.write(bytes);
  }

  private async readMore(): Promise<void> {
    if (!this.reader) throw new FlipperSerialError("Not connected");
    const { value, done } = await this.reader.read();
    if (done) throw new FlipperSerialError("Serial port closed unexpectedly");
    this.inputBuffer += this.decoder.decode(value, { stream: true });
  }

  private waitForPrompt(timeoutMs: number): Promise<string> {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        reject(
          new FlipperSerialError(
            "Timed out — make sure the Flipper is unlocked, not in an app, and plugged in via USB"
          )
        );
      }, timeoutMs);

      const poll = async () => {
        try {
          while (true) {
            const idx = this.inputBuffer.indexOf(CLI_PROMPT);
            if (idx !== -1) {
              clearTimeout(timer);
              const output = this.inputBuffer.slice(0, idx);
              this.inputBuffer = this.inputBuffer.slice(idx + CLI_PROMPT.length);
              resolve(output);
              return;
            }
            await this.readMore();
          }
        } catch (err) {
          clearTimeout(timer);
          reject(err);
        }
      };

      poll();
    });
  }

  private async cmd(command: string, timeoutMs = 12_000): Promise<void> {
    await this.send(command + "\r\n");
    await this.waitForPrompt(timeoutMs);
  }

  private async readTextUntil(marker: string, timeoutMs: number): Promise<string> {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        reject(new FlipperSerialError("Timed out starting Flipper RPC session"));
      }, timeoutMs);

      const poll = async () => {
        try {
          while (true) {
            const idx = this.inputBuffer.indexOf(marker);
            if (idx !== -1) {
              clearTimeout(timer);
              const output = this.inputBuffer.slice(0, idx + marker.length);
              this.inputBuffer = this.inputBuffer.slice(idx + marker.length);
              resolve(output);
              return;
            }
            await this.readMore();
          }
        } catch (err) {
          clearTimeout(timer);
          reject(err);
        }
      };

      poll();
    });
  }

  private nextCommandId(): number {
    this.commandId += 1;
    return this.commandId;
  }

  private async startRpcSession(): Promise<void> {
    if (this.inRpcSession) return;
    await this.send("start_rpc_session\r");
    await this.readTextUntil("\n", 8_000);
    this.inputBuffer = "";
    this.rpcBuffer = new Uint8Array();
    this.inRpcSession = true;
  }

  private async readRpcMore(timeoutMs: number): Promise<void> {
    if (!this.reader) throw new FlipperSerialError("Not connected");

    const read = this.reader.read();
    const timeout = new Promise<never>((_, reject) => {
      setTimeout(() => reject(new FlipperSerialError("Timed out waiting for Flipper RPC response")), timeoutMs);
    });

    const { value, done } = await Promise.race([read, timeout]);
    if (done || !value) throw new FlipperSerialError("Serial port closed unexpectedly");
    this.rpcBuffer = concatBytes([this.rpcBuffer, value]);
  }

  private async readRpcByte(timeoutMs: number): Promise<number> {
    while (this.rpcBuffer.length === 0) {
      await this.readRpcMore(timeoutMs);
    }
    const byte = this.rpcBuffer[0];
    this.rpcBuffer = this.rpcBuffer.slice(1);
    return byte;
  }

  private async readRpcVarint(timeoutMs: number): Promise<number> {
    let result = 0;
    let shift = 0;

    while (shift < 64) {
      const byte = await this.readRpcByte(timeoutMs);
      result |= (byte & 0x7f) << shift;
      if ((byte & 0x80) === 0) return result >>> 0;
      shift += 7;
    }

    throw new FlipperSerialError("Invalid Flipper RPC varint response");
  }

  private async readRpcBytes(length: number, timeoutMs: number): Promise<Uint8Array> {
    while (this.rpcBuffer.length < length) {
      await this.readRpcMore(timeoutMs);
    }

    const bytes = this.rpcBuffer.slice(0, length);
    this.rpcBuffer = this.rpcBuffer.slice(length);
    return bytes;
  }

  private async readRpcMain(commandId: number, timeoutMs = 20_000): Promise<RpcMainResponse> {
    while (true) {
      const length = await this.readRpcVarint(timeoutMs);
      const message = await this.readRpcBytes(length, timeoutMs);
      const response = parseMainResponse(message);
      if (response.commandId === commandId) return response;
    }
  }

  private async sendRpcFrame(contentField: number, payload: Uint8Array, options: RpcFrameOptions = {}) {
    const commandId = options.commandId ?? this.nextCommandId();
    const main = encodeMain(commandId, contentField, payload, options.hasNext ?? false);
    await this.sendBytes(concatBytes([encodeVarint(main.length), main]));
    return commandId;
  }

  private async sendRpcRequest(
    contentField: number,
    payload: Uint8Array,
    acceptedStatuses = new Set([0])
  ): Promise<void> {
    const commandId = await this.sendRpcFrame(contentField, payload);
    const response = await this.readRpcMain(commandId);
    if (!acceptedStatuses.has(response.commandStatus)) {
      throw new FlipperSerialError(formatRpcStatus(response.commandStatus));
    }
  }

  async ensureDir(path: string): Promise<void> {
    await this.cmd(`storage mkdir ${path}`);
  }

  async writeFile(path: string, content: string): Promise<void> {
    // storage write reads until Ctrl+C (0x03 = ETX = EOF signal)
    await this.send(`storage write ${path}\r\n`);
    await this.send(content);
    await this.send("\x03");
    await this.waitForPrompt(20_000);
  }

  private async ensureDirRpc(path: string): Promise<void> {
    await this.sendRpcRequest(13, encodeStringField(1, path), new Set([0, 6]));
  }

  private async deleteFileRpc(path: string): Promise<void> {
    await this.sendRpcRequest(12, encodeStringField(1, path), new Set([0, 7]));
  }

  private async writeFileRpc(path: string, content: Uint8Array): Promise<void> {
    await this.deleteFileRpc(path);

    const commandId = this.nextCommandId();
    const chunks = content.length ? chunkBytes(content, RPC_CHUNK_SIZE) : [new Uint8Array()];

    for (let index = 0; index < chunks.length; index += 1) {
      const hasNext = index < chunks.length - 1;
      const file = encodeBytesField(4, chunks[index]);
      const request = concatBytes([encodeStringField(1, path), encodeMessageField(2, file)]);
      await this.sendRpcFrame(11, request, { commandId, hasNext });
    }

    const response = await this.readRpcMain(commandId, 30_000);
    if (response.commandStatus !== 0) {
      throw new FlipperSerialError(formatRpcStatus(response.commandStatus));
    }
  }

  async install(
    profiles: Array<{ id: string; json: string }>,
    snippets: Array<{ name: string; content: string }>,
    settingsJson: string,
    appBinary: Uint8Array,
    onProgress: ProgressCallback
  ): Promise<void> {
    const total = 7 + profiles.length + (snippets.length ? 1 + snippets.length : 0);
    let step = 0;
    const tick = (msg: string) => onProgress(msg, step++ / total);

    tick("Creating directory…");
    await this.ensureDir("/ext/apps_data/flipdeck");

    tick("Creating profiles directory…");
    await this.ensureDir("/ext/apps_data/flipdeck/profiles");

    tick("Writing settings…");
    await this.writeFile("/ext/apps_data/flipdeck/settings.json", settingsJson);

    for (const { id, json } of profiles) {
      tick(`Writing ${id}.json…`);
      await this.writeFile(`/ext/apps_data/flipdeck/profiles/${id}.json`, json);
    }

    if (snippets.length) {
      tick("Creating snippets directory…");
      await this.ensureDir("/ext/apps_data/flipdeck/snippets");
      for (const { name, content } of snippets) {
        tick(`Writing snippet ${name}…`);
        await this.writeFile(`/ext/apps_data/flipdeck/snippets/${name}`, content);
      }
    }

    tick("Starting binary transfer…");
    await this.startRpcSession();

    tick("Creating apps directory…");
    await this.ensureDirRpc("/ext/apps");

    tick("Creating tools directory…");
    await this.ensureDirRpc("/ext/apps/Tools");

    tick("Installing flipdeck.fap…");
    await this.writeFileRpc("/ext/apps/Tools/flipdeck.fap", appBinary);

    onProgress("Done!", 1);
  }
}

interface RpcFrameOptions {
  commandId?: number;
  hasNext?: boolean;
}

interface RpcMainResponse {
  commandId: number;
  commandStatus: number;
  hasNext: boolean;
}

function formatRpcStatus(status: number): string {
  return `Flipper RPC failed: ${COMMAND_STATUS[status] ?? `status ${status}`}`;
}

function concatBytes(parts: Array<Uint8Array<ArrayBufferLike>>): Uint8Array {
  const length = parts.reduce((sum, part) => sum + part.length, 0);
  const out = new Uint8Array(length);
  let offset = 0;
  for (const part of parts) {
    out.set(part, offset);
    offset += part.length;
  }
  return out;
}

function chunkBytes(bytes: Uint8Array<ArrayBufferLike>, size: number): Uint8Array[] {
  const chunks: Uint8Array[] = [];
  for (let offset = 0; offset < bytes.length; offset += size) {
    chunks.push(bytes.slice(offset, offset + size));
  }
  return chunks;
}

function encodeVarint(value: number): Uint8Array {
  const bytes: number[] = [];
  let current = value >>> 0;

  while (current > 0x7f) {
    bytes.push((current & 0x7f) | 0x80);
    current >>>= 7;
  }
  bytes.push(current);

  return new Uint8Array(bytes);
}

function encodeKey(fieldNumber: number, wireType: number): Uint8Array {
  return encodeVarint((fieldNumber << 3) | wireType);
}

function encodeVarintField(fieldNumber: number, value: number): Uint8Array {
  return concatBytes([encodeKey(fieldNumber, 0), encodeVarint(value)]);
}

function encodeBytesField(fieldNumber: number, value: Uint8Array): Uint8Array {
  return concatBytes([encodeKey(fieldNumber, 2), encodeVarint(value.length), value]);
}

function encodeStringField(fieldNumber: number, value: string): Uint8Array {
  return encodeBytesField(fieldNumber, new TextEncoder().encode(value));
}

function encodeMessageField(fieldNumber: number, value: Uint8Array): Uint8Array {
  return encodeBytesField(fieldNumber, value);
}

function encodeMain(
  commandId: number,
  contentField: number,
  payload: Uint8Array,
  hasNext: boolean
): Uint8Array {
  const fields = [encodeVarintField(1, commandId)];
  if (hasNext) fields.push(encodeVarintField(3, 1));
  fields.push(encodeMessageField(contentField, payload));
  return concatBytes(fields);
}

function parseMainResponse(message: Uint8Array): RpcMainResponse {
  let offset = 0;
  const response: RpcMainResponse = { commandId: 0, commandStatus: 0, hasNext: false };

  const readVarint = () => {
    let result = 0;
    let shift = 0;

    while (offset < message.length && shift < 64) {
      const byte = message[offset++];
      result |= (byte & 0x7f) << shift;
      if ((byte & 0x80) === 0) return result >>> 0;
      shift += 7;
    }

    throw new FlipperSerialError("Invalid Flipper RPC response");
  };

  while (offset < message.length) {
    const key = readVarint();
    const fieldNumber = key >>> 3;
    const wireType = key & 0x07;

    if (wireType === 0) {
      const value = readVarint();
      if (fieldNumber === 1) response.commandId = value;
      if (fieldNumber === 2) response.commandStatus = value;
      if (fieldNumber === 3) response.hasNext = value !== 0;
      continue;
    }

    if (wireType === 2) {
      const length = readVarint();
      offset += length;
      continue;
    }

    throw new FlipperSerialError(`Unsupported Flipper RPC wire type ${wireType}`);
  }

  return response;
}
