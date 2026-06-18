import { readFileSync } from 'fs';
import { join } from 'path';
import { RULES } from '../validation';

// The desktop RULES array must stay byte-for-byte aligned with the canonical
// repo-root safety-rules.json shared across the web installer and C app.
describe('safety-rules parity', () => {
    it('matches the canonical safety-rules.json', () => {
        const canonicalPath = join(__dirname, '../../../safety-rules.json');
        const canonical = JSON.parse(readFileSync(canonicalPath, 'utf8'));
        const canonicalRules = canonical.rules.map(
            ({ id, label, regex, flags, severity, kind }: Record<string, string>) => ({
                id,
                label,
                regex,
                flags,
                severity,
                kind,
            })
        );
        expect(RULES).toEqual(canonicalRules);
    });
});
