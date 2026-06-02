import { mkdtempSync, rmSync, writeFileSync } from "fs";
import { jest } from "@jest/globals";
import { tmpdir } from "os";
import { join } from "path";

jest.mock("prettier", () => ({
    format: jest.fn((source: string) => source),
}));

import {
    loadProfileFile,
    normalizeProfile,
    parseJsonWithLocation,
    validateProfileFile,
} from "../lib/schema";

describe("profile schema", () => {
    it("applies v2 command defaults for delay and confirmation", () => {
        const profile = normalizeProfile(
            {
                name: "Git",
                icon: "git",
                commands: [
                    {
                        label: "Status",
                        type: "text",
                        value: "git status\n",
                    },
                ],
            },
            "git.json",
        );

        expect(profile.commands).toEqual([
            {
                label: "Status",
                type: "text",
                value: "git status\n",
                delay_ms: 100,
                confirmation_required: true,
            },
        ]);
        expect(profile.actions[0]).toMatchObject({
            label: "Status",
            type: "text",
            value: "git status\n",
            confirm: true,
        });
    });

    it("normalizes legacy actions into v2 commands", () => {
        const profile = normalizeProfile(
            {
                name: "VSCode",
                actions: [
                    {
                        label: "Palette",
                        type: "key_combo",
                        value: "CTRL+SHIFT+P",
                        confirm: false,
                    },
                    {
                        label: "Terminal",
                        type: "key_combo",
                        value: "CTRL+`",
                    },
                ],
            },
            "vscode.json",
        );

        expect(profile.icon).toBe("vscode");
        expect(profile.commands).toEqual([
            {
                label: "Palette",
                type: "key_combo",
                value: "CTRL+SHIFT+P",
                delay_ms: 100,
                confirmation_required: false,
            },
            {
                label: "Terminal",
                type: "key_combo",
                value: "CTRL+`",
                delay_ms: 100,
                confirmation_required: true,
            },
        ]);
        expect(profile.actions).toEqual([
            {
                label: "Palette",
                type: "key_combo",
                value: "CTRL+SHIFT+P",
                confirm: false,
            },
            {
                label: "Terminal",
                type: "key_combo",
                value: "CTRL+`",
                confirm: true,
            },
        ]);
    });

    it("includes line and column numbers for position-bearing invalid JSON errors", () => {
        const source = '{\n  "name": "Broken",\n  bad\n}';
        const position = source.indexOf("bad");
        const parseSpy = jest.spyOn(JSON, "parse").mockImplementation(() => {
            throw new SyntaxError(`Unexpected token b in JSON at position ${position}`);
        });

        try {
            expect(() => parseJsonWithLocation(source, "broken.json")).toThrow(/broken\.json:3:3/);
        } finally {
            parseSpy.mockRestore();
        }
    });

    it("surfaces schema error paths and messages", () => {
        const dir = mkdtempSync(join(tmpdir(), "flipdeck-schema-"));
        const file = join(dir, "bad.json");

        try {
            writeFileSync(
                file,
                JSON.stringify({
                    name: "",
                    icon: "invalid",
                    commands: [
                        {
                            label: "Open",
                            type: "macro",
                            value: "open .",
                        },
                    ],
                }),
            );

            const result = validateProfileFile(file);

            expect(result.profile).toBeUndefined();
            expect(result.errors).toEqual(
                expect.arrayContaining([
                    expect.objectContaining({
                        path: "name",
                        message: "Profile name is required",
                    }),
                    expect.objectContaining({
                        path: "icon",
                    }),
                    expect.objectContaining({
                        path: "commands.0.type",
                    }),
                ]),
            );
        } finally {
            rmSync(dir, { recursive: true, force: true });
        }
    });

    it("merges inherited commands before child commands", () => {
        const dir = mkdtempSync(join(tmpdir(), "flipdeck-extends-"));
        const base = join(dir, "base-git.json");
        const child = join(dir, "feature.json");

        try {
            writeFileSync(
                base,
                JSON.stringify({
                    name: "Base Git",
                    icon: "git",
                    commands: [
                        {
                            label: "Status",
                            type: "text",
                            value: "git status\n",
                        },
                    ],
                }),
            );
            writeFileSync(
                child,
                JSON.stringify({
                    name: "Feature Work",
                    extends: "base-git.json",
                    commands: [
                        {
                            label: "Checkout",
                            type: "text",
                            value: "git checkout -b ",
                            confirmation_required: false,
                        },
                    ],
                }),
            );

            const profile = loadProfileFile(child);

            expect(profile.name).toBe("Feature Work");
            expect(profile.extends).toBe("base-git.json");
            expect(profile.commands.map((command) => command.label)).toEqual(["Status", "Checkout"]);
            expect(profile.commands[0]).toMatchObject({
                delay_ms: 100,
                confirmation_required: true,
            });
            expect(profile.commands[1]).toMatchObject({
                delay_ms: 100,
                confirmation_required: false,
            });
            expect(profile.actions.map((action) => action.label)).toEqual(["Status", "Checkout"]);
        } finally {
            rmSync(dir, { recursive: true, force: true });
        }
    });
});
