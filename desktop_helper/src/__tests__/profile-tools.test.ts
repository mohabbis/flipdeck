import { mkdtempSync, readFileSync, writeFileSync, existsSync, rmSync } from "fs";
import { tmpdir } from "os";
import { join } from "path";
import { Command } from "commander";
import { registerProfileCommands } from "../lib/profile-tools";

// formatProfile() runs the input through prettier; the real engine performs its
// own internal dynamic import of parser plugins, which jest's module VM cannot
// execute. The serialized profile is already valid JSON, so a pass-through mock
// preserves the bytes we assert on without invoking that machinery.
jest.mock("prettier", () => ({ format: async (source: string) => source }));

/**
 * Drives the registered `profile` subcommands end-to-end through a fresh
 * Commander program, the same way the real CLI does. process.exit is trapped
 * so failure paths can be asserted instead of killing the test runner.
 */

class ExitError extends Error {
    constructor(public code: number) {
        super(`process.exit(${code})`);
    }
}

let tmp: string;
let exitSpy: jest.SpiedFunction<typeof process.exit>;
let logSpy: jest.SpiedFunction<typeof console.log>;
let errSpy: jest.SpiedFunction<typeof console.error>;

beforeEach(() => {
    tmp = mkdtempSync(join(tmpdir(), "flipdeck-cli-"));
    exitSpy = jest.spyOn(process, "exit").mockImplementation(((code?: number) => {
        throw new ExitError(code ?? 0);
    }) as never);
    logSpy = jest.spyOn(console, "log").mockImplementation(() => {});
    errSpy = jest.spyOn(console, "error").mockImplementation(() => {});
});

afterEach(() => {
    jest.restoreAllMocks();
    rmSync(tmp, { recursive: true, force: true });
});

function run(args: string[]): Promise<Command> {
    const program = new Command();
    program.exitOverride();
    registerProfileCommands(program, tmp);
    return program.parseAsync(["profile", ...args], { from: "user" });
}

function writeProfile(name: string, contents: unknown): string {
    const filePath = join(tmp, name);
    writeFileSync(filePath, JSON.stringify(contents, null, 2));
    return filePath;
}

const logged = () => logSpy.mock.calls.map((c) => c.join(" ")).join("\n");
const errored = () => errSpy.mock.calls.map((c) => c.join(" ")).join("\n");

describe("profile validate", () => {
    it("passes a clean v2 profile", async () => {
        const file = writeProfile("git.json", {
            name: "Git",
            id: "git",
            commands: [{ label: "Status", type: "text", value: "git status\n" }],
        });
        await run(["validate", file]);
        expect(exitSpy).not.toHaveBeenCalled();
        expect(logged()).toContain("is valid");
    });

    it("exits non-zero and reports schema errors for an invalid profile", async () => {
        const file = writeProfile("bad.json", { name: "Bad" }); // no commands/actions
        await expect(run(["validate", file])).rejects.toThrow(ExitError);
        expect(exitSpy).toHaveBeenCalledWith(1);
    });

    it("exits non-zero when a critical command is present", async () => {
        const file = writeProfile("danger.json", {
            name: "Danger",
            id: "danger",
            commands: [{ label: "Nuke", type: "text", value: "rm -rf /\n" }],
        });
        await expect(run(["validate", file])).rejects.toThrow(ExitError);
        expect(exitSpy).toHaveBeenCalledWith(1);
        expect(errored()).toContain("BLOCKED");
    });
});

describe("profile migrate (v1 → v2)", () => {
    it("rewrites legacy actions/confirm into commands/confirmation_required", async () => {
        const file = writeProfile("legacy.json", {
            name: "Legacy",
            id: "legacy",
            actions: [{ label: "Deploy", type: "text", value: "make deploy\n", confirm: false }],
        });

        await run(["migrate", "--from", "v1", "--dir", tmp]);

        const migrated = JSON.parse(readFileSync(file, "utf-8"));
        expect(migrated.actions).toBeUndefined();
        expect(migrated.commands).toHaveLength(1);
        expect(migrated.commands[0].confirmation_required).toBe(false);
        expect(migrated.commands[0].delay_ms).toBe(100);
        expect(migrated.commands[0].target).toBe("usb_hid");
    });

    it("rejects an unsupported source version", async () => {
        await expect(run(["migrate", "--from", "v0", "--dir", tmp])).rejects.toThrow(ExitError);
        expect(exitSpy).toHaveBeenCalledWith(1);
    });
});

describe("profile new", () => {
    it("writes a default profile to the requested output path", async () => {
        const out = join(tmp, "out", "fresh.json");
        await run(["new", "--output", out]);
        expect(existsSync(out)).toBe(true);
        const created = JSON.parse(readFileSync(out, "utf-8"));
        expect(created.name).toBe("New Profile");
        expect(created.commands).toHaveLength(1);
    });
});

describe("profile audit", () => {
    it("reports clean profiles and exits non-zero when one is dangerous", async () => {
        writeProfile("ok.json", {
            name: "Ok",
            id: "ok",
            commands: [{ label: "List", type: "text", value: "ls -la\n" }],
        });
        writeProfile("bad.json", {
            name: "Bad",
            id: "bad",
            commands: [{ label: "Nuke", type: "text", value: "rm -rf /\n" }],
        });
        await expect(run(["audit", tmp])).rejects.toThrow(ExitError);
        expect(exitSpy).toHaveBeenCalledWith(1);
        expect(errored()).toContain("bad.json");
    });
});

describe("profile preview", () => {
    it("renders an ASCII screen with the profile name and command count", async () => {
        const file = writeProfile("git.json", {
            name: "Git",
            id: "git",
            commands: [
                { label: "Status", type: "text", value: "git status\n" },
                { label: "Push", type: "text", value: "git push\n" },
            ],
        });
        await run(["preview", file]);
        const out = logged();
        expect(out).toContain("Git");
        expect(out).toContain("2 commands");
    });
});

describe("profile share", () => {
    it("prints a preview URL built from the profile id", async () => {
        const file = writeProfile("git.json", {
            name: "Git",
            id: "git",
            commands: [{ label: "Status", type: "text", value: "git status\n" }],
        });
        await run(["share", file, "--url", "https://example.test/preview"]);
        expect(logged()).toContain("https://example.test/preview/git");
    });
});
