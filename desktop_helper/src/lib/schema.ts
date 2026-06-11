import { existsSync, readFileSync } from "fs";
import { basename, dirname, join } from "path";
import { z } from "zod";

export const profileIcons = [
    "git",
    "docker",
    "node",
    "python",
    "aws",
    "vscode",
    "system",
    "snippets",
    "presentation",
    "wifi",
] as const;

export const commandTypes = ["text", "key", "key_combo"] as const;
export const commandTargets = ["usb_hid", "wifi_uart"] as const;

const profileIconSchema = z.enum(profileIcons);
const commandTypeSchema = z.enum(commandTypes);
const commandTargetSchema = z.enum(commandTargets);

const commandSchema = z.object({
    label: z.string().min(1, "Command label is required").max(40, "Command label must be 40 characters or fewer"),
    type: commandTypeSchema,
    value: z.string().min(1, "Command value is required"),
    delay_ms: z.number().int().min(0).max(5000).default(100),
    description: z.string().max(120, "Command description must be 120 characters or fewer").optional(),
    confirmation_required: z.boolean().default(true),
    target: commandTargetSchema.default("usb_hid"),
});

const legacyActionSchema = z.object({
    label: z.string().min(1).max(40),
    type: commandTypeSchema,
    value: z.string().min(1),
    confirm: z.boolean().default(true),
    target: commandTargetSchema.default("usb_hid"),
});

const rawProfileSchema = z
    .object({
        name: z.string().min(1, "Profile name is required").max(32, "Profile name must be 32 characters or fewer"),
        id: z.string().min(1).max(32).regex(/^[a-z0-9_-]+$/i, "Profile id may only contain letters, numbers, underscores, and dashes").optional(),
        extends: z.string().min(1).max(120).optional(),
        description: z.string().max(120, "Profile description must be 120 characters or fewer").optional().default(""),
        icon: profileIconSchema.optional(),
        commands: z.array(commandSchema).optional(),
        actions: z.array(legacyActionSchema).optional(),
    })
    .superRefine((profile, ctx) => {
        if (!profile.commands && !profile.actions) {
            ctx.addIssue({
                code: "custom",
                path: ["commands"],
                message: "Profile must define commands (v2) or actions (legacy).",
            });
        }
    });

export type ProfileIcon = (typeof profileIcons)[number];
export type CommandType = (typeof commandTypes)[number];
export type CommandTarget = (typeof commandTargets)[number];

export interface ProfileCommand {
    label: string;
    type: CommandType;
    value: string;
    delay_ms: number;
    description?: string;
    confirmation_required: boolean;
    target: CommandTarget;
}

export interface NormalizedProfile {
    name: string;
    id: string;
    description: string;
    icon: ProfileIcon;
    extends?: string;
    commands: ProfileCommand[];
    actions: Array<{
        label: string;
        type: CommandType;
        value: string;
        confirm: boolean;
    }>;
}

export interface ParsedJsonError extends Error {
    line?: number;
    column?: number;
}

export interface ProfileValidationError {
    path: string;
    message: string;
}

export interface ProfileLoadResult {
    profile?: NormalizedProfile;
    errors: ProfileValidationError[];
}

function getJsonLocation(source: string, position: number): { line: number; column: number } {
    const before = source.slice(0, position);
    const lines = before.split(/\r\n|\r|\n/);
    return {
        line: lines.length,
        column: lines[lines.length - 1].length + 1,
    };
}

export function parseJsonWithLocation(source: string, filePath: string): unknown {
    try {
        return JSON.parse(source);
    } catch (error) {
        const err = error as ParsedJsonError;
        const match = /position (\d+)/.exec(err.message);
        if (match) {
            const location = getJsonLocation(source, Number(match[1]));
            err.line = location.line;
            err.column = location.column;
            err.message = `${filePath}:${location.line}:${location.column} ${err.message}`;
        } else {
            err.message = `${filePath}: ${err.message}`;
        }
        throw err;
    }
}

function defaultIconForId(id: string): ProfileIcon {
    return profileIcons.includes(id as ProfileIcon) ? (id as ProfileIcon) : "system";
}

function toCommands(profile: z.infer<typeof rawProfileSchema>): ProfileCommand[] {
    if (profile.commands) return profile.commands;
    return (profile.actions ?? []).map((action) => ({
        label: action.label,
        type: action.type,
        value: action.value,
        delay_ms: 100,
        confirmation_required: action.confirm,
        target: action.target,
    }));
}

export function normalizeProfile(raw: unknown, fileName = "profile.json"): NormalizedProfile {
    const parsed = rawProfileSchema.parse(raw);
    const id = parsed.id ?? basename(fileName, ".json");
    const commands = toCommands(parsed);

    return {
        name: parsed.name,
        id,
        description: parsed.description,
        icon: parsed.icon ?? defaultIconForId(id),
        extends: parsed.extends,
        commands,
        actions: commands.map((command) => ({
            label: command.label,
            type: command.type,
            value: command.value,
            confirm: command.confirmation_required,
        })),
    };
}

function mergeProfiles(base: NormalizedProfile, child: NormalizedProfile): NormalizedProfile {
    const commands = [...base.commands, ...child.commands];
    return {
        ...base,
        ...child,
        commands,
        actions: commands.map((command) => ({
            label: command.label,
            type: command.type,
            value: command.value,
            confirm: command.confirmation_required,
        })),
    };
}

export function loadProfileFile(filePath: string, seen = new Set<string>()): NormalizedProfile {
    if (seen.has(filePath)) {
        throw new Error(`${filePath}: circular profile inheritance detected`);
    }
    seen.add(filePath);

    const source = readFileSync(filePath, "utf-8");
    const raw = parseJsonWithLocation(source, filePath);
    const profile = normalizeProfile(raw, basename(filePath));

    if (!profile.extends) return profile;

    const basePath = join(dirname(filePath), profile.extends);
    if (!existsSync(basePath)) {
        throw new Error(`${filePath}: extends target not found: ${profile.extends}`);
    }

    return mergeProfiles(loadProfileFile(basePath, seen), profile);
}

export function validateProfileFile(filePath: string): ProfileLoadResult {
    try {
        return { profile: loadProfileFile(filePath), errors: [] };
    } catch (error) {
        if (error instanceof z.ZodError) {
            return {
                errors: error.issues.map((issue) => ({
                    path: issue.path.length ? issue.path.join(".") : "(root)",
                    message: issue.message,
                })),
            };
        }

        const message = error instanceof Error ? error.message : String(error);
        return { errors: [{ path: filePath, message }] };
    }
}

export async function formatProfile(profile: NormalizedProfile): Promise<string> {
    const serializable = {
        name: profile.name,
        id: profile.id,
        description: profile.description,
        icon: profile.icon,
        commands: profile.commands,
    };
    const source = JSON.stringify(serializable, null, 2);
    const prettier = await import("prettier");
    return prettier.format(source, { parser: "json" });
}
