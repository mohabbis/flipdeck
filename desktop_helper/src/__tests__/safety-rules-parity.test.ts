import { readFileSync } from 'fs';
import { describe, it, expect } from '@jest/globals';
import { RULES } from '../validation';
import path from 'path';

/**
 * Safety rules parity test: ensures the safety rules in desktop_helper/src/validation.ts
 * stay perfectly in sync with the canonical repo-root safety-rules.json.
 */

describe('safety-rules parity', () => {
  it('desktop helper RULES match canonical safety-rules.json', () => {
    // Load canonical rules from repo root
    // __dirname = desktop_helper/src/__tests__
    // ../../../ = repo root
    const canonicalPath = path.resolve(__dirname, '../../../safety-rules.json');
    const canonicalJson = JSON.parse(readFileSync(canonicalPath, 'utf-8'));
    const canonicalRules = canonicalJson.rules as typeof RULES;

    // Compare count
    expect(RULES.length).toBe(canonicalRules.length);

    // Compare each rule field-by-field
    for (let i = 0; i < RULES.length; i++) {
      const rule = RULES[i];
      const canonical = canonicalRules[i];

      expect(rule.id).toBe(canonical.id);
      expect(rule.label).toBe(canonical.label);
      expect(rule.regex).toBe(canonical.regex);
      expect(rule.flags).toBe(canonical.flags);
      expect(rule.severity).toBe(canonical.severity);
      expect(rule.kind).toBe(canonical.kind);
    }
  });
});
