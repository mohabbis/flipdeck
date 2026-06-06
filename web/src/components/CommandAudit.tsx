import type { FlipDeckAction } from "@/types/flipdeck";
import { auditCommands } from "@/lib/safety-check";

interface CommandAuditProps {
  commands: FlipDeckAction[];
}

export function CommandAudit({ commands }: CommandAuditProps) {
  const risks = auditCommands(commands);

  return (
    <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
      <div className="flex items-center justify-between gap-4 border-b border-border bg-gradient-to-r from-card to-card/80 px-4 py-3">
        <div>
          <h2 className="text-lg font-semibold text-foreground">🛡️ Pack Safety</h2>
          <p className="text-sm text-muted-foreground">{commands.length} commands checked</p>
        </div>
        <span
          className={`rounded-full px-3 py-1 text-xs font-semibold ${
            risks.length
              ? "border border-accent-warning/30 bg-accent-warning/10 text-accent-warning"
              : "border border-accent-success/30 bg-accent-success/10 text-accent-success"
          }`}
        >
          {risks.length ? `${risks.length} warning${risks.length > 1 ? "s" : ""}` : "✓ All clear"}
        </span>
      </div>

      <div className="p-4">
        <div className="space-y-2">
          {risks.length ? (
            risks.map((risk) => (
              <div
                key={`${risk.command}-${risk.label}`}
                className="overflow-hidden rounded-lg border border-accent-warning/25 bg-accent-warning/10 p-3"
              >
                <div className="text-sm font-medium text-accent-warning">{risk.label}</div>
                <div className="mt-1 break-all font-mono text-xs text-accent-warning/80">
                  {risk.command}
                </div>
              </div>
            ))
          ) : (
            <div className="rounded-lg border border-accent-success/25 bg-accent-success/10 p-3 text-sm text-accent-success">
              ✓ No high-risk shell patterns detected in the selected pack.
            </div>
          )}
        </div>
      </div>
    </section>
  );
}
