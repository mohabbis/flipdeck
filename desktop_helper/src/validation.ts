export type RiskSeverity = 'critical' | 'warning';
export type RiskKind = 'dangerous' | 'credential';

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
 * `safety-rules.json` (the single source of truth shared with the web installer
 * and the on-device C app). The parity test in
 * `src/__tests__/safety-rules-parity.test.ts` enforces this.
 *
 * `critical` rules fail validation (block install); `warning` rules are reported
 * but do not fail — the Flipper still requires explicit OK before sending.
 */
export const RULES: SafetyRule[] = [
    { id: 'recursive_delete', label: 'Recursive delete', regex: 'rm\\s+-rf', flags: 'i', severity: 'critical', kind: 'dangerous' },
    { id: 'remote_shell_pipe', label: 'Remote shell pipe', regex: '(curl|wget).*\\|\\s*(sh|bash)', flags: 'i', severity: 'critical', kind: 'dangerous' },
    { id: 'raw_disk_write', label: 'Raw disk write', regex: 'dd\\s+if=', flags: 'i', severity: 'critical', kind: 'dangerous' },
    { id: 'disk_redirect', label: 'Raw disk redirect', regex: '>\\s*/dev/sd', flags: 'i', severity: 'critical', kind: 'dangerous' },
    { id: 'filesystem_format', label: 'Filesystem format', regex: 'mkfs', flags: 'i', severity: 'critical', kind: 'dangerous' },
    { id: 'fork_bomb', label: 'Fork bomb', regex: ':\\(\\)\\s*\\{[^}]*:\\|:&', flags: 'i', severity: 'critical', kind: 'dangerous' },
    { id: 'privilege_escalation', label: 'Privilege escalation', regex: '\\bsudo\\b', flags: 'i', severity: 'warning', kind: 'dangerous' },
    { id: 'world_writable', label: 'Loose permissions (chmod 777)', regex: 'chmod\\s+777', flags: 'i', severity: 'warning', kind: 'dangerous' },
    { id: 'chown_root', label: 'Ownership change to root', regex: 'chown\\s+root', flags: 'i', severity: 'warning', kind: 'dangerous' },
    { id: 'credential', label: 'Possible credential', regex: '(PASSWORD|TOKEN|API_KEY|SECRET|PRIVATE_KEY)\\s*=', flags: 'i', severity: 'warning', kind: 'credential' }
];

const compiledRules = RULES.map((rule) => ({ ...rule, matcher: new RegExp(rule.regex, rule.flags) }));

export interface ValidationIssue {
    label: string;
    pattern: string;
    kind: RiskKind;
    severity: RiskSeverity;
    value: string;
}

export interface ValidationResult {
    safe: boolean;
    issues: ValidationIssue[];
}

export interface ValidatableAction {
    label: string;
    type: 'text' | 'key' | 'key_combo';
    value: string;
}

export interface ValidatableProfile {
    actions: ValidatableAction[];
}

/** Returns every rule that matches the value. */
export function auditValue(label: string, value: string): ValidationIssue[] {
    return compiledRules
        .filter((rule) => rule.matcher.test(value))
        .map((rule) => ({
            label,
            pattern: rule.regex,
            kind: rule.kind,
            severity: rule.severity,
            value
        }));
}

/**
 * A value is "safe" when it contains no *critical* patterns. Warnings (sudo,
 * chmod 777, credential assignments) do not make a value unsafe — they are
 * surfaced but allowed, matching the web installer and the on-device model.
 */
export function isValueSafe(value: string): boolean {
    return !compiledRules.some((rule) => rule.severity === 'critical' && rule.matcher.test(value));
}

export function validateProfile(profile: ValidatableProfile): ValidationResult {
    const issues: ValidationIssue[] = [];

    for (const action of profile.actions) {
        if (action.type !== 'text') continue;
        issues.push(...auditValue(action.label, action.value));
    }

    const safe = !issues.some((issue) => issue.severity === 'critical');
    return { safe, issues };
}
