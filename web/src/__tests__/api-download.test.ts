import { describe, it, expect } from 'vitest';
import JSZip from 'jszip';
import { GET } from '../app/api/profiles/download/route';

describe('GET /api/profiles/download', () => {
    it('returns a ZIP file with correct headers', async () => {
        const res = await GET();

        expect(res.status).toBe(200);
        expect(res.headers.get('Content-Type')).toBe('application/zip');
        expect(res.headers.get('Content-Disposition')).toBe('attachment; filename=flipdeck-profiles.zip');
    });

    it('returns actual binary ZIP content', async () => {
        const res = await GET();

        const buffer = await res.arrayBuffer();
        expect(buffer.byteLength).toBeGreaterThan(0);

        const bytes = new Uint8Array(buffer);
        expect(bytes[0]).toBe(0x50);
        expect(bytes[1]).toBe(0x4B);
    });

    it('includes every bundled default profile in the ZIP', async () => {
        const res = await GET();
        const zip = await JSZip.loadAsync(await res.arrayBuffer());

        expect(Object.keys(zip.files).sort()).toEqual([
            'aws.json',
            'docker.json',
            'git.json',
            'node.json',
            'presentation.json',
            'python.json',
            'snippets.json',
            'system.json',
            'vscode.json',
            'wifi-devboard.json',
        ]);

        const gitProfile = JSON.parse(await zip.file('git.json')!.async('string'));
        expect(gitProfile.id).toBe('git');
        expect(gitProfile.actions[0].value).toBe('git status\n');
    });
});
