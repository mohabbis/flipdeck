import type { FlipDeckAction } from "@/types/flipdeck";

const rules = [
  { pattern: /rm\s+-rf/i, label: "Recursive delete", severity: "critical" },
  { pattern: /\bsudo\b/i, label: "Privilege escalation", severity: "warning" },
  { pattern: /(curl|wget).*\|\s*(sh|bash)/i, label: "Remote shell pipe", severity: "critical" },
  { pattern: /\bdd\s+if=/i, label: "Raw disk write", severity: "critical" },
  { pattern: /\bmkfs\b/i, label: "Filesystem format", severity: "critical" },
  { pattern: /(token|api_key|secret|password|private_key)/i, label: "Possible secret", severity: "warning" },
] as const;

export interface CommandRisk {
  label: string;
  pattern: string;
  severity: "critical" | "warning";
  command: string;
}

export function auditCommands(commands: FlipDeckAction[]): CommandRisk[] {
  return commands.flatMap((command) => {
    if (command.type !== "text") return [];
    return rules
      .filter((rule) => rule.pattern.test(command.value))
      .map((rule) => ({
        label: rule.label,
        pattern: rule.pattern.source,
        severity: rule.severity,
        command: command.label,
      }));
  });
}
