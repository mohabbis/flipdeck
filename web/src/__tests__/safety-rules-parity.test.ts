import { readFileSync } from "node:fs";
import { join } from "node:path";
import { describe, expect, it } from "vitest";
import { RULES } from "../lib/safety-check";

// The web RULES array must stay byte-for-byte aligned with the canonical
// repo-root safety-rules.json shared across the desktop helper and C app.
describe("safety-rules parity", () => {
  it("matches the canonical safety-rules.json", () => {
    const canonicalPath = join(__dirname, "../../../safety-rules.json");
    const canonical = JSON.parse(readFileSync(canonicalPath, "utf8"));
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
