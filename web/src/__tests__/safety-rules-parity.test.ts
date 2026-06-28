import { readFileSync } from "fs";
import { describe, it, expect } from "vitest";
import { RULES } from "@/lib/safety-check";
import path from "path";

/**
 * Safety rules parity test: ensures the safety rules in web/src/lib/safety-check.ts
 * stay perfectly in sync with the canonical repo-root safety-rules.json.
 *
 * The canonical file is the single source of truth shared with:
 * - Web installer (this test)
 * - Desktop helper (desktop_helper/src/__tests__/safety-rules-parity.test.ts)
 * - On-device C app (src/profile_manager.c — checked by hand)
 */

describe("safety-rules parity", () => {
  it("web RULES match canonical safety-rules.json", () => {
    // Load canonical rules from repo root
    const canonicalPath = path.resolve(__dirname, "../../../safety-rules.json");
    const canonicalJson = JSON.parse(readFileSync(canonicalPath, "utf-8"));
    const canonicalRules = canonicalJson.rules as typeof RULES;

    // Compare count
    expect(RULES).toHaveLength(
      canonicalRules.length,
      `Rule count mismatch: RULES has ${RULES.length}, canonical has ${canonicalRules.length}`
    );

    // Compare each rule field-by-field
    for (let i = 0; i < RULES.length; i++) {
      const rule = RULES[i];
      const canonical = canonicalRules[i];

      expect(rule.id).toBe(canonical.id, `Rule ${i}: id mismatch`);
      expect(rule.label).toBe(canonical.label, `Rule ${i}: label mismatch`);
      expect(rule.regex).toBe(canonical.regex, `Rule ${i}: regex mismatch`);
      expect(rule.flags).toBe(canonical.flags, `Rule ${i}: flags mismatch`);
      expect(rule.severity).toBe(canonical.severity, `Rule ${i}: severity mismatch`);
      expect(rule.kind).toBe(canonical.kind, `Rule ${i}: kind mismatch`);
    }
  });
});
