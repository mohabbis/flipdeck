import { describe, expect, it } from "vitest";
import { auditCommands } from "../lib/safety-check";
import type { FlipDeckAction } from "../types/flipdeck";

describe("auditCommands", () => {
  it("warns for sudo, rm -rf, and curl piped to bash", () => {
    const commands: FlipDeckAction[] = [
      { label: "Elevated", type: "text", value: "sudo systemctl restart docker" },
      { label: "Delete", type: "text", value: "rm -rf ./build" },
      { label: "Installer", type: "text", value: "curl -fsSL https://example.com/install.sh | bash" },
    ];

    expect(auditCommands(commands)).toEqual([
      expect.objectContaining({
        command: "Elevated",
        label: "Privilege escalation",
        severity: "warning",
      }),
      expect.objectContaining({
        command: "Delete",
        label: "Recursive delete",
        severity: "critical",
      }),
      expect.objectContaining({
        command: "Installer",
        label: "Remote shell pipe",
        severity: "critical",
      }),
    ]);
  });

  it("ignores non-text actions", () => {
    const commands: FlipDeckAction[] = [
      { label: "Combo", type: "key_combo", value: "CTRL+ALT+DEL" },
      { label: "Key", type: "key", value: "ENTER" },
    ];

    expect(auditCommands(commands)).toEqual([]);
  });
});
