import { describe, it, expect } from 'vitest';
import { getActionTypeColor } from '../lib/utils';

describe('getActionTypeColor', () => {
    it('returns blue classes for text type', () => {
        const result = getActionTypeColor('text');
        expect(result).toContain('blue');
    });

    it('returns green classes for key type', () => {
        const result = getActionTypeColor('key');
        expect(result).toContain('green');
    });

    it('returns purple classes for key_combo type', () => {
        const result = getActionTypeColor('key_combo');
        expect(result).toContain('purple');
    });

    it('returns gray fallback for unknown type', () => {
        const result = getActionTypeColor('unknown' as never);
        expect(result).toContain('gray');
    });

    it('returns distinct colors for each type', () => {
        const text = getActionTypeColor('text');
        const key = getActionTypeColor('key');
        const combo = getActionTypeColor('key_combo');
        expect(text).not.toBe(key);
        expect(key).not.toBe(combo);
        expect(text).not.toBe(combo);
    });

    it('includes dark mode classes for text', () => {
        expect(getActionTypeColor('text')).toContain('dark:');
    });

    it('includes dark mode classes for key', () => {
        expect(getActionTypeColor('key')).toContain('dark:');
    });

    it('includes dark mode classes for key_combo', () => {
        expect(getActionTypeColor('key_combo')).toContain('dark:');
    });
});
