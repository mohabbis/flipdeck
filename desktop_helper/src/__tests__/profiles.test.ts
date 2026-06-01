import { jest } from '@jest/globals';

// Mock fs modules before importing anything that uses them
jest.mock('fs', () => ({
    readFileSync: jest.fn(),
    writeFileSync: jest.fn(),
    existsSync: jest.fn(),
    mkdirSync: jest.fn(),
    readdirSync: jest.fn(),
}));

jest.mock('fs-extra', () => ({
    mkdirsSync: jest.fn(),
}));

// Mock axios to prevent network calls
jest.mock('axios');

import * as fs from 'fs';
import { loadProfiles, loadDefaultProfiles } from '../index';

const mockReadFileSync = fs.readFileSync as jest.MockedFunction<typeof fs.readFileSync>;
const mockWriteFileSync = fs.writeFileSync as jest.MockedFunction<typeof fs.writeFileSync>;
const mockReaddirSync = (fs as any).readdirSync as jest.Mock;

const sampleProfile = {
    name: 'Git',
    id: 'git',
    description: 'Git workflow shortcuts',
    actions: [
        { label: 'Git Status', type: 'text', value: 'git status\n', confirm: true },
    ],
};

beforeEach(() => {
    jest.clearAllMocks();
});

describe('loadProfiles', () => {
    it('reads all .json files from the profiles directory', () => {
        mockReaddirSync.mockReturnValue(['git.json', 'node.json', 'README.md']);
        mockReadFileSync.mockReturnValue(JSON.stringify(sampleProfile));

        const profiles = loadProfiles();

        // Only .json files are loaded (README.md skipped)
        expect(profiles).toHaveLength(2);
        expect(mockReadFileSync).toHaveBeenCalledTimes(2);
    });

    it('parses profile JSON correctly', () => {
        mockReaddirSync.mockReturnValue(['git.json']);
        mockReadFileSync.mockReturnValue(JSON.stringify(sampleProfile));

        const [profile] = loadProfiles();

        expect(profile.name).toBe('Git');
        expect(profile.id).toBe('git');
        expect(profile.actions).toHaveLength(1);
        expect(profile.actions[0].label).toBe('Git Status');
    });

    it('returns empty array and falls back to defaults when directory does not exist', () => {
        mockReaddirSync.mockImplementation(() => { throw new Error('ENOENT'); });
        mockWriteFileSync.mockImplementation(() => {});

        // loadDefaultProfiles is called inside loadProfiles on failure
        const profiles = loadProfiles();
        expect(profiles).toHaveLength(0);
    });

    it('skips non-json files', () => {
        mockReaddirSync.mockReturnValue(['.DS_Store', 'notes.txt', 'git.json']);
        mockReadFileSync.mockReturnValue(JSON.stringify(sampleProfile));

        const profiles = loadProfiles();
        expect(profiles).toHaveLength(1);
    });
});

describe('loadDefaultProfiles', () => {
    it('writes all 5 default profiles to disk', () => {
        mockWriteFileSync.mockImplementation(() => {});

        loadDefaultProfiles();

        expect(mockWriteFileSync).toHaveBeenCalledTimes(5);
    });

    it('writes valid JSON for each profile', () => {
        const written: string[] = [];
        mockWriteFileSync.mockImplementation((_path: any, content: any) => {
            written.push(content as string);
        });

        loadDefaultProfiles();

        for (const content of written) {
            const parsed = JSON.parse(content);
            expect(parsed).toHaveProperty('name');
            expect(parsed).toHaveProperty('id');
            expect(parsed).toHaveProperty('description');
            expect(Array.isArray(parsed.actions)).toBe(true);
        }
    });

    it('creates profiles for expected categories', () => {
        const paths: string[] = [];
        mockWriteFileSync.mockImplementation((path: any) => {
            paths.push(path as string);
        });

        loadDefaultProfiles();

        const fileNames = paths.map((p) => p.split('/').pop());
        expect(fileNames).toContain('git.json');
        expect(fileNames).toContain('node.json');
        expect(fileNames).toContain('python.json');
        expect(fileNames).toContain('vscode.json');
        expect(fileNames).toContain('presentation.json');
    });

    it('all default profile actions have required fields', () => {
        const written: string[] = [];
        mockWriteFileSync.mockImplementation((_path: any, content: any) => {
            written.push(content as string);
        });

        loadDefaultProfiles();

        for (const content of written) {
            const profile = JSON.parse(content);
            for (const action of profile.actions) {
                expect(action).toHaveProperty('label');
                expect(action).toHaveProperty('type');
                expect(['text', 'key', 'key_combo']).toContain(action.type);
                expect(action).toHaveProperty('value');
                expect(action).toHaveProperty('confirm');
            }
        }
    });
});
