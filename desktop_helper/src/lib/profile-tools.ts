import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "fs";
import { homedir } from "os";
import { basename, dirname, join, resolve } from "path";
import axios from "axios";
import qrcode from "qrcode-terminal";
import { Command } from "commander";
import { createInterface } from "readline/promises";
import { stdin as input, stdout as output } from "process";
import { formatProfile, loadProfileFile, NormalizedProfile, normalizeProfile, ProfileIcon, profileIcons, validateProfileFile } from "./schema";
import { validateProfile } from "../validation";

const CONFIG_DIR = join(homedir(), ".flipdeck");
const CONFIG_FILE = join(CONFIG_DIR, "config.json");

interface FlipDeckConfig {
    gistId?: string;
    shareBaseUrl?: string;
}

function readConfig(): FlipDeckConfig {
    if (!existsSync(CONFIG_FILE)) return {};
    return JSON.parse(readFileSync(CONFIG_FILE, "utf-8")) as FlipDeckConfig;
}

function writeConfig(config: FlipDeckConfig) {
    mkdirSync(CONFIG_DIR, { recursive: true });
    writeFileSync(CONFIG_FILE, `${JSON.stringify(config, null, 2)}\n`);
}

function requireProfilePath(file: string): string {
    const filePath = resolve(file);
    if (!existsSync(filePath)) {
        throw new Error(`Profile file not found: ${file}`);
    }
    return filePath;
}

function renderFlipperPreview(profile: NormalizedProfile): string {
    const width = 34;
    const inner = width - 2;
    const line = `+${"-".repeat(inner)}+`;
    const title = profile.name.slice(0, inner).padEnd(inner);
    const rows = profile.commands.slice(0, 5).map((command, index) => {
        const prefix = index === 0 ? ">" : " ";
        const label = `${prefix} ${command.label}`.slice(0, inner - 7).padEnd(inner - 7);
        const type = command.type === "key_combo" ? "combo" : command.type;
        return `|${label}${type.slice(0, 5).padStart(5)} |`;
    });

    while (rows.length < 5) rows.push(`|${" ".repeat(inner)}|`);

    return [
        line,
        `|${title}|`,
        `|${"-".repeat(inner)}|`,
        ...rows,
        `|${`${profile.commands.length} commands`.padEnd(inner)}|`,
        line,
    ].join("\n");
}

function printValidation(filePath: string): NormalizedProfile {
    const result = validateProfileFile(filePath);
    if (!result.profile) {
        for (const error of result.errors) {
            console.error(`✗ ${error.path}: ${error.message}`);
        }
        process.exit(1);
    }

    const audit = validateProfile({ actions: result.profile.actions });
    for (const issue of audit.issues) {
        const kind = issue.kind === "dangerous" ? "dangerous command" : "possible credential";
        console.error(`! ${kind} in "${issue.label}": matched "${issue.pattern}"`);
    }

    if (!audit.safe) {
        console.error("✗ Profile schema is valid, but safety audit found risky commands.");
        process.exit(1);
    }

    console.log(`✓ ${basename(filePath)} is valid (${result.profile.commands.length} commands)`);
    return result.profile;
}

async function promptForProfile(): Promise<NormalizedProfile> {
    const rl = createInterface({ input, output });
    try {
        const name = (await rl.question("Profile name: ")).trim();
        const id = (await rl.question("Profile id/file name: ")).trim().toLowerCase();
        const description = (await rl.question("Description: ")).trim();
        const iconAnswer = (await rl.question(`Icon (${profileIcons.join(", ")}): `)).trim();
        const icon: ProfileIcon = profileIcons.includes(iconAnswer as ProfileIcon) ? (iconAnswer as ProfileIcon) : "system";
        const commands: NormalizedProfile["commands"] = [];

        do {
            const label = (await rl.question("Command label: ")).trim();
            const typeAnswer = (await rl.question("Type (text/key/key_combo) [text]: ")).trim();
            const type = typeAnswer === "key" || typeAnswer === "key_combo" ? typeAnswer : "text";
            const value = await rl.question("Value: ");
            const confirmAnswer = (await rl.question("Require confirmation? [Y/n]: ")).trim().toLowerCase();
            commands.push({
                label,
                type,
                value,
                delay_ms: 100,
                confirmation_required: confirmAnswer !== "n",
            });
        } while ((await rl.question("Add another command? [y/N]: ")).trim().toLowerCase() === "y");

        return normalizeProfile({ name, id, description, icon, commands }, `${id}.json`);
    } finally {
        rl.close();
    }
}

async function pushProfiles(dir: string, token: string, gistId?: string) {
    const files: Record<string, { content: string }> = {};
    for (const file of readdirSync(dir).filter((name) => name.endsWith(".json"))) {
        const filePath = join(dir, file);
        const profile = loadProfileFile(filePath);
        files[file] = { content: await formatProfile(profile) };
    }

    const body = { description: "FlipDeck profile collection", public: false, files };
    const headers = { Authorization: `token ${token}` };
    const response = gistId
        ? await axios.patch(`https://api.github.com/gists/${gistId}`, body, { headers })
        : await axios.post("https://api.github.com/gists", body, { headers });

    const config = readConfig();
    config.gistId = response.data.id;
    writeConfig(config);
    console.log(`✓ Synced profiles to ${response.data.html_url}`);
}

async function pullProfiles(dir: string, token: string, gistId: string) {
    mkdirSync(dir, { recursive: true });
    const response = await axios.get(`https://api.github.com/gists/${gistId}`, {
        headers: { Authorization: `token ${token}` },
    });
    const files = response.data.files as Record<string, { content: string }>;
    for (const [name, file] of Object.entries(files)) {
        if (name.endsWith(".json")) {
            writeFileSync(join(dir, name), file.content.endsWith("\n") ? file.content : `${file.content}\n`);
            console.log(`✓ Pulled ${name}`);
        }
    }
}

export function registerProfileCommands(program: Command, profilesDir: string) {
    const profile = program.command("profile").description("Create, validate, preview, audit, and sync profiles");

    const validateAction = (file: string) => {
        printValidation(requireProfilePath(file));
    };

    const newAction = async (options: { interactive?: boolean; output?: string }) => {
        const newProfile = options.interactive
            ? await promptForProfile()
            : normalizeProfile({
                  name: "New Profile",
                  id: "new-profile",
                  description: "",
                  icon: "system",
                  commands: [
                      {
                          label: "Hello",
                          type: "text",
                          value: "echo hello\n",
                          delay_ms: 100,
                          confirmation_required: true,
                      },
                  ],
              });
        const outputFile = resolve(options.output ?? join(profilesDir, `${newProfile.id}.json`));
        mkdirSync(dirname(outputFile), { recursive: true });
        writeFileSync(outputFile, await formatProfile(newProfile));
        console.log(`✓ Wrote ${outputFile}`);
    };

    const editAction = async (file: string) => {
        const filePath = requireProfilePath(file);
        const loadedProfile = printValidation(filePath);
        writeFileSync(filePath, await formatProfile(loadedProfile));
        console.log(`✓ Formatted ${filePath}`);
    };

    const previewAction = (file: string) => {
        const loadedProfile = loadProfileFile(requireProfilePath(file));
        console.log(renderFlipperPreview(loadedProfile));
    };

    const auditAction = (dir: string) => {
        const profileDir = resolve(dir);
        let failures = 0;
        for (const file of readdirSync(profileDir).filter((name) => name.endsWith(".json"))) {
            const filePath = join(profileDir, file);
            const result = validateProfileFile(filePath);
            if (!result.profile) {
                failures += 1;
                console.error(`✗ ${file}: ${result.errors.map((error) => error.message).join("; ")}`);
                continue;
            }
            const audit = validateProfile({ actions: result.profile.actions });
            if (!audit.safe) {
                failures += 1;
                console.error(`✗ ${file}`);
                for (const issue of audit.issues) {
                    console.error(`  - ${issue.label}: ${issue.pattern}`);
                }
            } else {
                console.log(`✓ ${file}`);
            }
        }
        if (failures) process.exit(1);
    };

    const migrateAction = async (options: { from: string; dir: string }) => {
        if (options.from !== "v1") {
            console.error(`Unsupported migration source: ${options.from}`);
            process.exit(1);
        }
        for (const file of readdirSync(options.dir).filter((name) => name.endsWith(".json"))) {
            const filePath = join(options.dir, file);
            const loadedProfile = loadProfileFile(filePath);
            writeFileSync(filePath, await formatProfile(loadedProfile));
            console.log(`✓ Migrated ${file}`);
        }
    };

    const shareAction = (file: string, options: { url?: string }) => {
        const filePath = requireProfilePath(file);
        const loadedProfile = loadProfileFile(filePath);
        const config = readConfig();
        const baseUrl = options.url ?? config.shareBaseUrl ?? "https://flipdeck-production.up.railway.app/preview";
        const url = `${baseUrl}/${encodeURIComponent(loadedProfile.id)}`;
        console.log(url);
        qrcode.generate(url, { small: true });
    };

    const syncAction = async (options: { push?: boolean; pull?: string; dir: string; token?: string }) => {
        const token = options.token ?? process.env.GITHUB_TOKEN;
        if (!token) {
            console.error("GitHub token required. Use --token or set GITHUB_TOKEN.");
            process.exit(1);
        }
        if (options.push) {
            await pushProfiles(resolve(options.dir), token, readConfig().gistId);
        } else if (options.pull) {
            await pullProfiles(resolve(options.dir), token, options.pull);
        } else {
            console.error("Choose --push or --pull <gist>.");
            process.exit(1);
        }
    };

    profile
        .command("validate <file>")
        .description("Validate a profile file against the FlipDeck schema and safety rules")
        .action(validateAction);

    profile
        .command("new")
        .description("Create a new profile")
        .option("-i, --interactive", "Prompt for profile fields")
        .option("-o, --output <file>", "Output file")
        .action(newAction);

    profile
        .command("edit <file>")
        .description("Validate and normalize a profile file in place")
        .action(editAction);

    profile
        .command("preview <file>")
        .description("Render an ASCII Flipper screen preview")
        .action(previewAction);

    profile
        .command("audit <dir>")
        .description("Scan a directory of profiles for risky text commands")
        .action(auditAction);

    profile
        .command("migrate")
        .description("Migrate legacy actions/confirm profiles to v2 commands")
        .requiredOption("--from <version>", "Source version, currently v1")
        .option("-d, --dir <dir>", "Profile directory", profilesDir)
        .action(migrateAction);

    profile
        .command("share <file>")
        .description("Print a share URL and QR code for a profile")
        .option("-u, --url <baseUrl>", "Base URL for profile previews")
        .action(shareAction);

    profile
        .command("sync")
        .description("Push or pull profile files via GitHub Gist")
        .option("--push", "Upload local profiles to a GitHub Gist")
        .option("--pull <gist>", "Download profiles from a GitHub Gist")
        .option("-d, --dir <dir>", "Profile directory", profilesDir)
        .option("-t, --token <token>", "GitHub token; defaults to GITHUB_TOKEN")
        .action(syncAction);

    program
        .command("profile:validate <file>")
        .description("Validate a profile file against the FlipDeck schema and safety rules")
        .action(validateAction);

    program
        .command("profile:new")
        .description("Create a new profile")
        .option("-i, --interactive", "Prompt for profile fields")
        .option("-o, --output <file>", "Output file")
        .action(newAction);

    program
        .command("profile:edit <file>")
        .description("Validate and normalize a profile file in place")
        .action(editAction);

    program
        .command("profile:preview <file>")
        .description("Render an ASCII Flipper screen preview")
        .action(previewAction);

    program
        .command("profile:audit <dir>")
        .description("Scan a directory of profiles for risky text commands")
        .action(auditAction);

    program
        .command("profile:migrate")
        .description("Migrate legacy actions/confirm profiles to v2 commands")
        .requiredOption("--from <version>", "Source version, currently v1")
        .option("-d, --dir <dir>", "Profile directory", profilesDir)
        .action(migrateAction);

    program
        .command("profile:share <file>")
        .description("Print a share URL and QR code for a profile")
        .option("-u, --url <baseUrl>", "Base URL for profile previews")
        .action(shareAction);

    program
        .command("profile:sync")
        .description("Push or pull profile files via GitHub Gist")
        .option("--push", "Upload local profiles to a GitHub Gist")
        .option("--pull <gist>", "Download profiles from a GitHub Gist")
        .option("-d, --dir <dir>", "Profile directory", profilesDir)
        .option("-t, --token <token>", "GitHub token; defaults to GITHUB_TOKEN")
        .action(syncAction);
}
