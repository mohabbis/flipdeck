import { isValueSafe, validateProfile, DANGEROUS_PATTERNS, CREDENTIAL_PATTERNS } from '../validation';

describe('isValueSafe', () => {
    describe('dangerous patterns', () => {
        test.each(DANGEROUS_PATTERNS as unknown as string[])(
            'blocks "%s"',
            (pattern) => {
                expect(isValueSafe(`prefix ${pattern} suffix`)).toBe(false);
            }
        );

        it('blocks rm -rf at start of string', () => {
            expect(isValueSafe('rm -rf /')).toBe(false);
        });

        it('blocks sudo in middle of command', () => {
            expect(isValueSafe('echo foo && sudo rm bar')).toBe(false);
        });

        it('blocks curl | sh piped execution (exact pattern)', () => {
            // Pattern matches the literal substring "curl | sh"
            expect(isValueSafe('curl | sh')).toBe(false);
        });

        it('blocks wget | sh piped execution (exact pattern)', () => {
            expect(isValueSafe('wget | sh')).toBe(false);
        });

        it('blocks mkfs filesystem formatting', () => {
            expect(isValueSafe('mkfs.ext4 /dev/sda')).toBe(false);
        });

        it('blocks dd disk operations', () => {
            expect(isValueSafe('dd if=/dev/zero of=/dev/sda')).toBe(false);
        });

        it('blocks fork bomb', () => {
            expect(isValueSafe(':(){ :|:& };:')).toBe(false);
        });

        it('blocks disk write via redirection', () => {
            expect(isValueSafe('echo x > /dev/sda')).toBe(false);
        });

        it('blocks chmod 777', () => {
            expect(isValueSafe('chmod 777 /etc/passwd')).toBe(false);
        });

        it('blocks chown root', () => {
            expect(isValueSafe('chown root /etc')).toBe(false);
        });
    });

    describe('credential patterns (case-insensitive)', () => {
        it('blocks PASSWORD (uppercase)', () => {
            expect(isValueSafe('export PASSWORD=secret')).toBe(false);
        });

        it('blocks password (lowercase)', () => {
            expect(isValueSafe('export password=secret')).toBe(false);
        });

        it('blocks TOKEN', () => {
            expect(isValueSafe('echo TOKEN=abc')).toBe(false);
        });

        it('blocks token (lowercase)', () => {
            expect(isValueSafe('my_token=abc')).toBe(false);
        });

        it('blocks API_KEY', () => {
            expect(isValueSafe('API_KEY=xyz')).toBe(false);
        });

        it('blocks api_key (lowercase)', () => {
            expect(isValueSafe('api_key=xyz')).toBe(false);
        });

        it('blocks SECRET', () => {
            expect(isValueSafe('export SECRET=foo')).toBe(false);
        });

        it('blocks secret (lowercase)', () => {
            expect(isValueSafe('export secret=foo')).toBe(false);
        });

        it('blocks PRIVATE_KEY', () => {
            expect(isValueSafe('PRIVATE_KEY=bar')).toBe(false);
        });

        it('blocks private_key (lowercase)', () => {
            expect(isValueSafe('private_key=bar')).toBe(false);
        });
    });

    describe('safe values', () => {
        it('allows git commands', () => {
            expect(isValueSafe('git status')).toBe(true);
            expect(isValueSafe('git add .\n')).toBe(true);
            expect(isValueSafe('git commit -m "fix: update README"\n')).toBe(true);
            expect(isValueSafe('git push origin\n')).toBe(true);
        });

        it('allows npm commands', () => {
            expect(isValueSafe('npm run dev\n')).toBe(true);
            expect(isValueSafe('npm test\n')).toBe(true);
            expect(isValueSafe('npm install\n')).toBe(true);
        });

        it('allows python commands', () => {
            expect(isValueSafe('python\n')).toBe(true);
            expect(isValueSafe('pip install flask\n')).toBe(true);
        });

        it('allows docker commands', () => {
            expect(isValueSafe('docker ps\n')).toBe(true);
            expect(isValueSafe('docker images\n')).toBe(true);
        });

        it('allows common system commands', () => {
            expect(isValueSafe('ls -la\n')).toBe(true);
            expect(isValueSafe('pwd\n')).toBe(true);
            expect(isValueSafe('clear\n')).toBe(true);
            expect(isValueSafe('whoami\n')).toBe(true);
        });

        it('allows empty string', () => {
            expect(isValueSafe('')).toBe(true);
        });

        it('allows key combo strings', () => {
            expect(isValueSafe('CTRL+SHIFT+P')).toBe(true);
        });
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
        // Even if a key/key_combo action has a suspicious value, it is not flagged
        const result = validateProfile({
            actions: [
                { label: 'Tricky Key', type: 'key', value: 'rm -rf' },
                { label: 'Tricky Combo', type: 'key_combo', value: 'sudo+SHIFT+P' },
            ],
        });
        expect(result.safe).toBe(true);
        expect(result.issues).toHaveLength(0);
    });

    it('flags dangerous text actions', () => {
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
            pattern: 'rm -rf',
            kind: 'dangerous',
        });
    });

    it('flags credential text actions', () => {
        const result = validateProfile({
            actions: [
                { label: 'Set Env', type: 'text', value: 'export PASSWORD=hunter2' },
            ],
        });
        expect(result.safe).toBe(false);
        expect(result.issues[0].kind).toBe('credential');
        expect(result.issues[0].label).toBe('Set Env');
    });

    it('reports multiple issues across multiple actions', () => {
        const result = validateProfile({
            actions: [
                { label: 'A', type: 'text', value: 'rm -rf /' },
                { label: 'B', type: 'text', value: 'export TOKEN=abc' },
            ],
        });
        expect(result.safe).toBe(false);
        expect(result.issues).toHaveLength(2);
    });

    it('handles a profile with no actions', () => {
        const result = validateProfile({ actions: [] });
        expect(result.safe).toBe(true);
        expect(result.issues).toHaveLength(0);
    });
});
