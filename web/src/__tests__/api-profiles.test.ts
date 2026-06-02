import { describe, it, expect } from 'vitest';
import { GET } from '../app/api/profiles/route';

describe('GET /api/profiles', () => {
    it('returns 200 and a map of profiles keyed by id', async () => {
        const res = GET();

        expect(res.status).toBe(200);
        const data = await res.json();
        expect(data).toHaveProperty('git');
        expect(data).toHaveProperty('node');
        expect(data.git.name).toBe('Git');
        expect(data.node.name).toBe('Node');
    });

    it('includes all bundled default profiles', async () => {
        const res = GET();

        const data = await res.json();
        expect(Object.keys(data).sort()).toEqual([
            'aws',
            'docker',
            'git',
            'node',
            'presentation',
            'python',
            'snippets',
            'system',
            'vscode',
        ]);
    });

    it('returns profile actions with the expected action shape', async () => {
        const res = GET();

        const data = await res.json();
        expect(data.git.actions[0]).toMatchObject({
            label: 'Git Status',
            type: 'text',
            value: 'git status\n',
            confirm: true,
        });
    });
});
