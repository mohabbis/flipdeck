import type { FlipDeckAction } from "@/types/flipdeck";

export type RiskSeverity = "critical" | "warning";
export type RiskKind = "dangerous" | "credential";

export interface SafetyRule {
  id: string;
  label: string;
  regex: string;
  flags: string;
  severity: RiskSeverity;
  kind: RiskKind;
}

/**
 * Canonical safety rules. Must stay byte-for-byte in sync with the repo-root
 * `safety-rules.json` (the single source of truth shared with the desktop
 * helper and the on-device C app). The parity test in
 * `src/__tests__/safety-rules-parity.test.ts` enforces this.
 *
 * `critical` rules block installation; `warning` rules are surfaced but allowed.
 */
export const RULES: SafetyRule[] = [
  { id: "recursive_delete", label: "Recursive delete", regex: "rm\\s+-rf", flags: "i", severity: "critical", kind: "dangerous" },
  { id: "remote_shell_pipe", label: "Remote shell pipe", regex: "(curl|wget).*\\|\\s*(sh|bash)", flags: "i", severity: "critical", kind: "dangerous" },
  { id: "raw_disk_write", label: "Raw disk write", regex: "dd\\s+if=", flags: "i", severity: "critical", kind: "dangerous" },
  { id: "disk_redirect", label: "Raw disk redirect", regex: ">\\s*/dev/sd", flags: "i", severity: "critical", kind: "dangerous" },
  { id: "filesystem_format", label: "Filesystem format", regex: "mkfs", flags: "i", severity: "critical", kind: "dangerous" },
  { id: "fork_bomb", label: "Fork bomb", regex: ":\\(\\)\\s*\\{[^}]*:\\|:&", flags: "i", severity: "critical", kind: "dangerous" },
  { id: "privilege_escalation", label: "Privilege escalation", regex: "\\bsudo\\b", flags: "i", severity: "warning", kind: "dangerous" },
  { id: "world_writable", label: "Loose permissions (chmod 777)", regex: "chmod\\s+777", flags: "i", severity: "warning", kind: "dangerous" },
  { id: "chown_root", label: "Ownership change to root", regex: "chown\\s+root", flags: "i", severity: "warning", kind: "dangerous" },
  { id: "credential", label: "Possible credential", regex: "(PASSWORD|TOKEN|API_KEY|SECRET|PRIVATE_KEY)\\s*=", flags: "i", severity: "warning", kind: "credential" }
];

const compiledRules = RULES.map((rule) => ({
  ...rule,
  matcher: new RegExp(rule.regex, rule.flags)
}));

export interface CommandRisk {
  /** Rule that matched, e.g. "Recursive delete". */
  label: string;
  /** Raw regex source of the rule that matched. */
  pattern: string;
  severity: RiskSeverity;
  kind: RiskKind;
  /** Human label of the offending command (for context). */
  command: string;
  /** The actual command text that triggered the rule. */
  value: string;
}

/** Audit a single text value against every rule. */
export function auditValue(label: string, value: string): CommandRisk[] {
  return compiledRules
    .filter((rule) => rule.matcher.test(value))
    .map((rule) => ({
      label: rule.label,
      pattern: rule.regex,
      severity: rule.severity,
      kind: rule.kind,
      command: label,
      value
    }));
}

export function auditCommands(commands: FlipDeckAction[]): CommandRisk[] {
  return commands.flatMap((command) => {
    if (command.type !== "text") return [];
    return auditValue(command.label, command.value);
  });
}

/** Audit raw snippet bodies (keyed by file name). */
export function auditSnippets(snippets: Record<string, string>): CommandRisk[] {
  return Object.entries(snippets).flatMap(([name, body]) => auditValue(name, body));
}

export function hasCriticalRisk(risks: CommandRisk[]): boolean {
  return risks.some((risk) => risk.severity === "critical");
}
