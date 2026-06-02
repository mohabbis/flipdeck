import { describe, it, expect } from 'vitest';
import JSZip from 'jszip';
import { GET } from '../app/api/profiles/download/route';

describe('GET /api/profiles/download', () => {
    it('returns a ZIP file with correct headers', async () => {
        const res = await GET();

        expect(res.status).toBe(200);
        expect(res.headers.get('Content-Type')).toBe('application/zip');
        expect(res.headers.get('Content-Disposition')).toContain('attachment');
        expect(res.headers.get('Content-Disposition')).toContain('flipdeck-profiles.zip');
    });

    it('returns actual binary ZIP content', async () => {
        const res = await GET();

        const buffer = await res.arrayBuffer();
        expect(buffer.byteLength).toBeGreaterThan(0);

        // ZIP files start with PK magic bytes (0x50 0x4B)
        const bytes = new Uint8Array(buffer);
        expect(bytes[0]).toBe(0x50); // 'P'
        expect(bytes[1]).toBe(0x4B); // 'K'
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
        ]);

        const gitProfile = JSON.parse(await zip.file('git.json')!.async('string'));
        expect(gitProfile.id).toBe('git');
        expect(gitProfile.actions[0].value).toBe('git status\n');
    });
});
