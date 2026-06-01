export const DANGEROUS_PATTERNS = [
    'rm -rf',
    'sudo',
    'curl | sh',
    'wget | sh',
    'curl.ssh',
    'mkfs',
    'dd if=',
    ':(){ :|:& };:',
    '> /dev/sd',
    'chmod 777',
    'chown root',
] as const;

export const CREDENTIAL_PATTERNS = [
    'PASSWORD',
    'TOKEN',
    'API_KEY',
    'SECRET',
    'PRIVATE_KEY',
] as const;

export interface ValidationIssue {
    label: string;
    pattern: string;
    kind: 'dangerous' | 'credential';
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

export function isValueSafe(value: string): boolean {
    for (const pattern of DANGEROUS_PATTERNS) {
        if (value.includes(pattern)) return false;
    }
    const upper = value.toUpperCase();
    for (const pattern of CREDENTIAL_PATTERNS) {
        if (upper.includes(pattern)) return false;
    }
    return true;
}

export function validateProfile(profile: ValidatableProfile): ValidationResult {
    const issues: ValidationIssue[] = [];

    for (const action of profile.actions) {
        if (action.type !== 'text') continue;

        const upper = action.value.toUpperCase();
        for (const pattern of DANGEROUS_PATTERNS) {
            if (action.value.includes(pattern)) {
                issues.push({ label: action.label, pattern, kind: 'dangerous' });
            }
        }
        for (const pattern of CREDENTIAL_PATTERNS) {
            if (upper.includes(pattern)) {
                issues.push({ label: action.label, pattern, kind: 'credential' });
            }
        }
    }

    return { safe: issues.length === 0, issues };
}
