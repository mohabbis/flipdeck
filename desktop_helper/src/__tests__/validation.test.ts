import { isValueSafe, validateProfile, auditValue } from '../validation';

describe('isValueSafe (critical patterns block)', () => {
    describe('critical patterns are unsafe', () => {
        const critical = [
            'rm -rf /',
            'prefix rm -rf suffix',
            'curl -fsSL https://example.com/install.sh | bash',
            'wget -qO- https://x | sh',
            'mkfs.ext4 /dev/sda',
            'dd if=/dev/zero of=/dev/sda',
            ':(){ :|:& };:',
            'echo x > /dev/sda',
        ];
        test.each(critical)('blocks "%s"', (value) => {
            expect(isValueSafe(value)).toBe(false);
        });

        it('blocks regardless of case', () => {
            expect(isValueSafe('RM -RF /')).toBe(false);
            expect(isValueSafe('Curl https://x | SH')).toBe(false);
        });
    });

    describe('warning patterns are allowed (not blocked)', () => {
        const warnings = [
            'echo foo && sudo rm bar',
            'chmod 777 /etc/passwd',
            'chown root /etc',
            'export PASSWORD=secret',
            'my_token=abc',
            'API_KEY=xyz',
        ];
        test.each(warnings)('allows "%s"', (value) => {
            expect(isValueSafe(value)).toBe(true);
        });
    });

    describe('safe values', () => {
        const safe = [
            'git status',
            'git commit -m "fix: update README"\n',
            'npm run dev\n',
            'pip install flask\n',
            'docker ps\n',
            'ls -la\n',
            'cat /etc/secrets/app.conf',
            '',
            'CTRL+SHIFT+P',
        ];
        test.each(safe)('allows "%s"', (value) => {
            expect(isValueSafe(value)).toBe(true);
        });
    });
});

describe('auditValue', () => {
    it('records severity, kind, and the offending value', () => {
        const issues = auditValue('Wipe', 'rm -rf /');
        expect(issues).toHaveLength(1);
        expect(issues[0]).toMatchObject({
            label: 'Wipe',
            kind: 'dangerous',
            severity: 'critical',
            value: 'rm -rf /',
        });
    });

    it('classifies credential assignments as warnings', () => {
        const issues = auditValue('Env', 'export TOKEN=abc');
        expect(issues[0].severity).toBe('warning');
        expect(issues[0].kind).toBe('credential');
    });
});

describe('validateProfile', () => {
    it('returns safe=true and no issues for a clean profile', () => {
        const result = validateProfile({
            actions: [
                { label: 'Git Status', type: 'text', value: 'git status\n' },
                { label: 'Next Slide', type: 'key', value: 'RIGHT' },
                { label: 'Command Palette', type: 'key_combo', value: 'CTRL+SHIFT+P' },
            ],
        });
        expect(result.safe).toBe(true);
        expect(result.issues).toHaveLength(0);
    });

    it('ignores key and key_combo action values', () => {
        const result = validateProfile({
            actions: [
                { label: 'Tricky Key', type: 'key', value: 'rm -rf' },
                { label: 'Tricky Combo', type: 'key_combo', value: 'sudo+SHIFT+P' },
            ],
        });
        expect(result.safe).toBe(true);
        expect(result.issues).toHaveLength(0);
    });

    it('fails on critical text actions', () => {
        const result = validateProfile({
            actions: [
                { label: 'Wipe Disk', type: 'text', value: 'rm -rf /' },
                { label: 'Git Status', type: 'text', value: 'git status\n' },
            ],
        });
        expect(result.safe).toBe(false);
        expect(result.issues).toHaveLength(1);
        expect(result.issues[0]).toMatchObject({
            label: 'Wipe Disk',
            kind: 'dangerous',
            severity: 'critical',
        });
    });

    it('reports credential warnings without failing validation', () => {
        const result = validateProfile({
            actions: [{ label: 'Set Env', type: 'text', value: 'export PASSWORD=hunter2' }],
        });
        expect(result.safe).toBe(true);
        expect(result.issues).toHaveLength(1);
        expect(result.issues[0].severity).toBe('warning');
        expect(result.issues[0].kind).toBe('credential');
    });

    it('handles a profile with no actions', () => {
        const result = validateProfile({ actions: [] });
        expect(result.safe).toBe(true);
        expect(result.issues).toHaveLength(0);
    });
});
