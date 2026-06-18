#!/usr/bin/env node
/**
 * @file src/index.ts
 * @brief FlipDeck Desktop Helper - CLI Application
 *
 * Companion CLI app for FlipDeck Flipper Zero application.
 * Provides profile management, GitHub sync, and automation.
 */

import { program } from 'commander';
import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'fs';
import { join, dirname } from 'path';
import * as fsExtra from 'fs-extra';
import axios from 'axios';
import { validateProfile } from './validation';
import { registerProfileCommands } from './lib/profile-tools';

interface FlipDeckAction {
    label: string;
    type: 'text' | 'key' | 'key_combo';
    value: string;
    confirm: boolean;
}

interface Profile {
    name: string;
    id: string;
    description: string;
    actions: FlipDeckAction[];
}

interface Settings {
    send_delay_ms: number;
    confirm_before_send: boolean;
    auto_detect_usb: boolean;
    show_icons: boolean;
    show_descriptions: boolean;
    startup_category: string;
}

const DATA_DIR = join(__dirname, '../../sd_card/apps_data/flipdeck');
const PROFILES_DIR = join(DATA_DIR, 'profiles');
const SETTINGS_FILE = join(DATA_DIR, 'settings.json');
const SNIPPETS_DIR = join(DATA_DIR, 'snippets');

// Ensure directories exist
export function ensureDirectories() {
    fsExtra.mkdirsSync(PROFILES_DIR);
    fsExtra.mkdirsSync(SNIPPETS_DIR);
    fsExtra.mkdirsSync(dirname(SETTINGS_FILE));
}

// Load all profiles
export function loadProfiles(): Profile[] {
    const profiles: Profile[] = [];
    
    try {
        const files = require('fs').readdirSync(PROFILES_DIR);
        for (const file of files) {
            if (file.endsWith('.json')) {
                const content = readFileSync(join(PROFILES_DIR, file), 'utf-8');
                profiles.push(JSON.parse(content));
            }
        }
    } catch (e) {
        console.log('No profiles found, using defaults');
        loadDefaultProfiles();
    }
    
    return profiles;
}

// Load default profiles
export function loadDefaultProfiles(): void {
    const defaults: Profile[] = [
        {
            name: 'Git',
            id: 'git',
            description: 'Git workflow shortcuts',
            actions: [
                { label: 'Git Status', type: 'text', value: 'git status\n', confirm: true },
                { label: 'Git Add All', type: 'text', value: 'git add .\n', confirm: true },
                { label: 'Git Commit', type: 'text', value: 'git commit -m ""\n', confirm: true },
                { label: 'Git Push', type: 'text', value: 'git push origin\n', confirm: true },
                { label: 'Git Pull', type: 'text', value: 'git pull origin\n', confirm: true },
            ]
        },
        {
            name: 'Node.js',
            id: 'node',
            description: 'Node.js development commands',
            actions: [
                { label: 'Run dev server', type: 'text', value: 'npm run dev\n', confirm: true },
                { label: 'Run tests', type: 'text', value: 'npm test\n', confirm: true },
                { label: 'Build', type: 'text', value: 'npm run build\n', confirm: true },
                { label: 'Install', type: 'text', value: 'npm install\n', confirm: true },
            ]
        },
        {
            name: 'Python',
            id: 'python',
            description: 'Python development commands',
            actions: [
                { label: 'Run Python', type: 'text', value: 'python\n', confirm: true },
                { label: 'Run script', type: 'text', value: 'python ', confirm: true },
                { label: 'Pip Install', type: 'text', value: 'pip install ', confirm: true },
                { label: 'Pip List', type: 'text', value: 'pip list\n', confirm: true },
            ]
        },
        {
            name: 'VSCode',
            id: 'vscode',
            description: 'VSCode keyboard shortcuts',
            actions: [
                { label: 'Command Palette', type: 'key_combo', value: 'CTRL+SHIFT+P', confirm: false },
                { label: 'File Explorer', type: 'key_combo', value: 'CTRL+SHIFT+E', confirm: false },
                { label: 'Search', type: 'key_combo', value: 'CTRL+SHIFT+F', confirm: false },
                { label: 'Terminal', type: 'key_combo', value: 'CTRL+`', confirm: false },
            ]
        },
        {
            name: 'Presentation',
            id: 'presentation',
            description: 'Presentation remote control',
            actions: [
                { label: 'Next Slide', type: 'key', value: 'RIGHT', confirm: false },
                { label: 'Previous Slide', type: 'key', value: 'LEFT', confirm: false },
                { label: 'Start Slideshow', type: 'key', value: 'F5', confirm: false },
                { label: 'Exit Presentation', type: 'key', value: 'ESCAPE', confirm: false },
            ]
        },
    ];

    for (const profile of defaults) {
        writeFileSync(join(PROFILES_DIR, `${profile.id}.json`), JSON.stringify(profile, null, 2));
    }
}

// Initialize profiles and settings
function initialize(): void {
    ensureDirectories();
    
    // Create default settings if not exists
    if (!existsSync(SETTINGS_FILE)) {
        const defaultSettings: Settings = {
            send_delay_ms: 100,
            confirm_before_send: true,
            auto_detect_usb: true,
            show_icons: true,
            show_descriptions: true,
            startup_category: '',
        };
        writeFileSync(SETTINGS_FILE, JSON.stringify(defaultSettings, null, 2));
    }
    
    // Create default profiles if none exist
    const profiles = loadProfiles();
    if (profiles.length === 0) {
        loadDefaultProfiles();
    }
}

// CLI Commands
program
    .name('flipdeck')
    .description('CLI tool for FlipDeck - USB Command Deck for Flipper Zero')
    .version('1.0.0');

registerProfileCommands(program, PROFILES_DIR);

program
    .command('init')
    .description('Initialize FlipDeck directory structure')
    .action(() => {
        initialize();
        console.log('✓ FlipDeck initialized');
        console.log(`  Profiles: ${PROFILES_DIR}`);
        console.log(`  Settings: ${SETTINGS_FILE}`);
    });

program
    .command('list')
    .description('List all profile categories')
    .action(() => {
        const profiles = loadProfiles();
        console.log('\nAvailable profile categories:');
        for (const p of profiles) {
            console.log(`  - ${p.id}.json: ${p.name} (${p.actions.length} actions)`);
        }
    });

program
    .command('show <category>')
    .description('Show a profile category')
    .action((category: string) => {
        const profiles = loadProfiles();
        const profile = profiles.find(p => p.id === category);
        if (!profile) {
            console.error(`Profile "${category}" not found`);
            process.exit(1);
        }
        console.log(JSON.stringify(profile, null, 2));
    });

program
    .command('create <category> <name>')
    .description('Create a new profile category')
    .action((category: string, name: string) => {
        const existing = loadProfiles();
        if (existing.find(p => p.id === category)) {
            console.error(`Profile "${category}" already exists`);
            process.exit(1);
        }
        const profile: Profile = {
            name: name,
            id: category,
            description: '',
            actions: [],
        };
        writeFileSync(join(PROFILES_DIR, `${category}.json`), JSON.stringify(profile, null, 2));
        console.log(`✓ Created profile: ${category}.json`);
    });

program
    .command('add-action <category> <label>')
    .description('Add an action to a profile')
    .option('-t, --type <type>', 'Action type (text, key, key_combo)', 'text')
    .option('-v, --value <value>', 'Action value', '')
    .option('-c, --confirm', 'Require confirmation', false)
    .action((category: string, label: string, options: { type: string, value: string, confirm: boolean }) => {
        const profiles = loadProfiles();
        const profile = profiles.find(p => p.id === category);
        if (!profile) {
            console.error(`Profile "${category}" not found`);
            process.exit(1);
        }
        profile.actions.push({
            label,
            type: options.type as 'text' | 'key' | 'key_combo',
            value: options.value,
            confirm: options.confirm,
        });
        writeFileSync(join(PROFILES_DIR, `${category}.json`), JSON.stringify(profile, null, 2));
        console.log(`✓ Added action "${label}" to ${category}.json`);
    });

program
    .command('validate <category>')
    .description('Validate a profile for safety')
    .action((category: string) => {
        const profiles = loadProfiles();
        const profile = profiles.find(p => p.id === category);
        if (!profile) {
            console.error(`Profile "${category}" not found`);
            process.exit(1);
        }
        
        const result = validateProfile(profile);
        for (const issue of result.issues) {
            const kindLabel = issue.kind === 'dangerous' ? 'Dangerous pattern' : 'Credential pattern';
            const marker = issue.severity === 'critical' ? '✗' : '!';
            console.error(`  ${marker} [${issue.severity}] ${kindLabel} in "${issue.label}": /${issue.pattern}/`);
        }
        if (!result.safe) {
            console.error('\n⚠ Validation failed - the commands above are blocked');
            process.exit(1);
        } else {
            console.log('✓ Profile is safe');
        }
    });

program
    .command('export <category> <output>')
    .description('Export a profile to a file')
    .action((category: string, output: string) => {
        const profiles = loadProfiles();
        const profile = profiles.find(p => p.id === category);
        if (!profile) {
            console.error(`Profile "${category}" not found`);
            process.exit(1);
        }
        writeFileSync(output, JSON.stringify(profile, null, 2));
        console.log(`✓ Exported ${category}.json to ${output}`);
    });

program
    .command('import <input> [category]')
    .description('Import a profile from a file')
    .action((input: string, category?: string) => {
        if (!existsSync(input)) {
            console.error(`File not found: ${input}`);
            process.exit(1);
        }
        const content = readFileSync(input, 'utf-8');
        const profile: Profile = JSON.parse(content);
        const categoryId = category || profile.id;
        writeFileSync(join(PROFILES_DIR, `${categoryId}.json`), JSON.stringify(profile, null, 2));
        console.log(`✓ Imported profile to ${categoryId}.json`);
    });

program
    .command('settings')
    .description('Show current settings')
    .action(() => {
        const content = readFileSync(SETTINGS_FILE, 'utf-8');
        console.log(content);
    });

program
    .command('set <key> <value>')
    .description('Update a setting')
    .action((key: string, value: string) => {
        const content = readFileSync(SETTINGS_FILE, 'utf-8');
        const settings: Settings = JSON.parse(content);
        
        let typedValue: any = value;
        if (value === 'true' || value === 'false') {
            typedValue = value === 'true';
        } else if (!isNaN(Number(value))) {
            typedValue = Number(value);
        }
        
        (settings as any)[key] = typedValue;
        writeFileSync(SETTINGS_FILE, JSON.stringify(settings, null, 2));
        console.log(`✓ Updated ${key} = ${value}`);
    });

// GitHub Gist sync
interface GistProfile {
    filename: string;
    content: string;
}

program
    .command('gist-push')
    .description('Push profiles to a GitHub Gist')
    .option('-g, --github-token <token>', 'GitHub personal access token')
    .option('-p, --gist-id <id>', 'Gist ID for updating')
    .action(async (options: { githubToken?: string, gistId?: string }) => {
        const token = options.githubToken || process.env.GITHUB_TOKEN;
        if (!token) {
            console.error('GitHub token required. Use -g <token> or set GITHUB_TOKEN env var');
            process.exit(1);
        }
        
        const profiles = loadProfiles();
        const files: Record<string, { content: string }> = {};
        
        for (const profile of profiles) {
            files[`${profile.id}.json`] = { content: JSON.stringify(profile, null, 2) };
        }
        
        const gistData: any = {
            description: 'FlipDeck profile collection',
            public: true,
            files,
        };
        
        try {
            let response;
            if (options.gistId) {
                // Update existing gist
                response = await axios.patch(
                    `https://api.github.com/gists/${options.gistId}`,
                    gistData,
                    { headers: { Authorization: `token ${token}` } }
                );
                console.log(`✓ Updated Gist: ${response.data.html_url}`);
            } else {
                // Create new gist
                response = await axios.post(
                    'https://api.github.com/gists',
                    gistData,
                    { headers: { Authorization: `token ${token}` } }
                );
                console.log(`✓ Created Gist: ${response.data.html_url}`);
                console.log(`  Gist ID: ${response.data.id}`);
            }
        } catch (error: any) {
            console.error('Failed to sync:', error.response?.data?.message || error.message);
            process.exit(1);
        }
    });

program
    .command('gist-pull')
    .description('Pull profiles from a GitHub Gist')
    .option('-g, --github-token <token>', 'GitHub personal access token')
    .option('-p, --gist-id <id>', 'Gist ID to pull from (required)')
    .action(async (options: { githubToken?: string, gistId?: string }) => {
        const token = options.githubToken || process.env.GITHUB_TOKEN;
        const gistId = options.gistId || process.env.FLIPDECK_GIST_ID;
        
        if (!token) {
            console.error('GitHub token required. Use -g <token> or set GITHUB_TOKEN env var');
            process.exit(1);
        }
        if (!gistId) {
            console.error('Gist ID required. Use -p <id> or set FLIPDECK_GIST_ID env var');
            process.exit(1);
        }
        
        try {
            const response = await axios.get(
                `https://api.github.com/gists/${gistId}`,
                { headers: { Authorization: `token ${token}` } }
            );
            
            const files = response.data.files as Record<string, { content: string }>;
            for (const [filename, fileData] of Object.entries(files)) {
                if (filename.endsWith('.json')) {
                    const filepath = join(PROFILES_DIR, filename);
                    writeFileSync(filepath, fileData.content);
                    console.log(`✓ Pulled: ${filename}`);
                }
            }
            console.log(`✓ Imported profiles from Gist: ${response.data.html_url}`);
        } catch (error: any) {
            console.error('Failed to pull:', error.response?.data?.message || error.message);
            process.exit(1);
        }
    });

program
    .command('gist-list')
    .description('List Gists accessible with the token')
    .option('-g, --github-token <token>', 'GitHub personal access token')
    .action(async (options: { githubToken?: string }) => {
        const token = options.githubToken || process.env.GITHUB_TOKEN;
        if (!token) {
            console.error('GitHub token required. Use -g <token> or set GITHUB_TOKEN env var');
            process.exit(1);
        }
        
        try {
            const response = await axios.get(
                'https://api.github.com/gists/public',
                { headers: { Authorization: `token ${token}` } }
            );
            
            console.log('Recent public gists:');
            for (const gist of response.data) {
                const profileFiles = Object.keys(gist.files).filter(f => f.endsWith('.json'));
                if (profileFiles.length > 0) {
                    console.log(`  - ${gist.id}: ${gist.description || 'No description'} (${profileFiles.length} files)`);
                }
            }
        } catch (error: any) {
            console.error('Failed to list gists:', error.response?.data?.message || error.message);
            process.exit(1);
        }
    });

if (require.main === module) {
    initialize();
    program.parse();
}
