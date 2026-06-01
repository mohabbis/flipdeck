import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('fs', () => ({
    readFileSync: vi.fn(),
    readdirSync: vi.fn(),
}));

import * as fs from 'fs';
import { GET } from '../app/api/profiles/route';

const mockReaddirSync = vi.mocked(fs.readdirSync);
const mockReadFileSync = vi.mocked(fs.readFileSync);
const profileFiles = (...files: string[]) => files as ReturnType<typeof fs.readdirSync>;

const gitProfile = {
    id: 'git',
    name: 'Git',
    description: 'Git workflow shortcuts',
    actions: [{ label: 'Git Status', type: 'text', value: 'git status\n', confirm: true }],
};

const nodeProfile = {
    id: 'node',
    name: 'Node.js',
    description: 'Node.js commands',
    actions: [],
};

beforeEach(() => {
    vi.clearAllMocks();
});

describe('GET /api/profiles', () => {
    it('returns 200 and a map of profiles keyed by id', async () => {
        mockReaddirSync.mockReturnValue(profileFiles('git.json', 'node.json'));
        mockReadFileSync
            .mockReturnValueOnce(JSON.stringify(gitProfile))
            .mockReturnValueOnce(JSON.stringify(nodeProfile));

        const res = await GET();

        expect(res.status).toBe(200);
        const data = await res.json();
        expect(data).toHaveProperty('git');
        expect(data).toHaveProperty('node');
        expect(data.git.name).toBe('Git');
        expect(data.node.name).toBe('Node.js');
    });

    it('only includes .json files', async () => {
        mockReaddirSync.mockReturnValue(profileFiles('.DS_Store', 'README.md', 'git.json'));
        mockReadFileSync.mockReturnValue(JSON.stringify(gitProfile));

        const res = await GET();

        expect(res.status).toBe(200);
        const data = await res.json();
        const keys = Object.keys(data);
        expect(keys).toHaveLength(1);
        expect(keys[0]).toBe('git');
        expect(mockReadFileSync).toHaveBeenCalledTimes(1);
    });

    it('returns 200 with empty object when no profiles exist', async () => {
        mockReaddirSync.mockReturnValue(profileFiles());

        const res = await GET();

        expect(res.status).toBe(200);
        const data = await res.json();
        expect(data).toEqual({});
    });

    it('returns 500 when the profiles directory cannot be read', async () => {
        mockReaddirSync.mockImplementation(() => { throw new Error('ENOENT'); });

        const res = await GET();

        expect(res.status).toBe(500);
        const data = await res.json();
        expect(data.error).toBe('Failed to load profiles');
    });

    it('returns 500 when a profile file cannot be parsed', async () => {
        mockReaddirSync.mockReturnValue(profileFiles('bad.json'));
        mockReadFileSync.mockReturnValue('not valid json');

        const res = await GET();

        expect(res.status).toBe(500);
    });
});
