import { describe, expect, it } from "vitest";
import { auditCommands, auditSnippets, hasCriticalRisk } from "../lib/safety-check";
import type { FlipDeckAction } from "../types/flipdeck";

function riskFor(value: string) {
  return auditCommands([{ label: "Cmd", type: "text", value }]);
}

describe("auditCommands", () => {
  it("flags rm -rf, real curl|sh, dd, mkfs, fork bomb, disk redirect as critical", () => {
    const criticalValues = [
      "rm -rf ./build",
      "curl -fsSL https://example.com/install.sh | bash",
      "dd if=/dev/zero of=/dev/sda",
      "mkfs.ext4 /dev/sda1",
      ":(){ :|:& };:",
      "echo x > /dev/sda",
    ];
    for (const value of criticalValues) {
      const risks = riskFor(value);
      expect(risks.length, value).toBeGreaterThan(0);
      expect(risks[0].severity, value).toBe("critical");
    }
  });

  it("matches dangerous patterns case-insensitively", () => {
    const risks = riskFor("RM -RF /");
    expect(risks).toHaveLength(1);
    expect(risks[0].severity).toBe("critical");
  });

  it("treats sudo and chmod 777 as warnings, not blocks", () => {
    const sudo = riskFor("sudo systemctl restart docker");
    expect(sudo[0].severity).toBe("warning");
    const chmod = riskFor("chmod 777 ./script.sh");
    expect(chmod[0].severity).toBe("warning");
    expect(hasCriticalRisk([...sudo, ...chmod])).toBe(false);
  });

  it("flags credential assignments as warnings but ignores benign mentions", () => {
    expect(riskFor("export API_KEY=abc123")[0]?.severity).toBe("warning");
    expect(riskFor("my_token=xyz")[0]?.severity).toBe("warning");
    // No assignment → not a credential match
    expect(riskFor("cat /etc/secrets/app.conf")).toHaveLength(0);
    expect(riskFor("git push origin main")).toHaveLength(0);
  });

  it("carries the actual offending command text, not just the label", () => {
    const risks = riskFor("rm -rf /tmp/cache");
    expect(risks[0].command).toBe("Cmd");
    expect(risks[0].value).toBe("rm -rf /tmp/cache");
    expect(risks[0].label).toBe("Recursive delete");
  });

  it("ignores non-text actions", () => {
    const commands: FlipDeckAction[] = [
      { label: "Combo", type: "key_combo", value: "CTRL+ALT+DEL" },
      { label: "Key", type: "key", value: "ENTER" },
    ];
    expect(auditCommands(commands)).toEqual([]);
  });
});

describe("auditSnippets", () => {
  it("audits snippet bodies and reports the file name as the source", () => {
    const risks = auditSnippets({ "danger.txt": "rm -rf /\n" });
    expect(risks).toHaveLength(1);
    expect(risks[0].command).toBe("danger.txt");
    expect(risks[0].severity).toBe("critical");
  });

  it("returns nothing for benign snippets", () => {
    expect(auditSnippets({ "ok.txt": "console.log('hello')" })).toEqual([]);
  });
});
