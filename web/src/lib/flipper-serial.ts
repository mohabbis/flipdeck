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
    }
  }

  private async send(text: string): Promise<void> {
    if (!this.writer) throw new FlipperSerialError("Not connected");
    await this.writer.write(this.encoder.encode(text));
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

  async install(
    profiles: Array<{ id: string; json: string }>,
    snippets: Array<{ name: string; content: string }>,
    settingsJson: string,
    onProgress: ProgressCallback
  ): Promise<void> {
    const total = 3 + profiles.length + (snippets.length ? 1 + snippets.length : 0);
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

    onProgress("Done!", 1);
  }
}
