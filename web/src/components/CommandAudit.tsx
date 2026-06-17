import type { FlipDeckAction } from "@/types/flipdeck";
import { auditCommands } from "@/lib/safety-check";

interface CommandAuditProps {
  commands: FlipDeckAction[];
}

export function CommandAudit({ commands }: CommandAuditProps) {
  const risks = auditCommands(commands);
  const criticalRisks = risks.filter((risk) => risk.severity === "critical");
  const warningRisks = risks.filter((risk) => risk.severity === "warning");

  return (
    <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
      <div className="flex items-center justify-between gap-4 border-b border-border bg-gradient-to-r from-card to-card/80 px-4 py-3">
        <div>
          <h2 className="text-lg font-semibold text-foreground">🛡️ Pack Safety</h2>
          <p className="text-sm text-muted-foreground">{commands.length} commands checked</p>
        </div>
        <span
          className={`rounded-full px-3 py-1 text-xs font-semibold ${
            criticalRisks.length
              ? "border border-red-500/30 bg-red-500/10 text-red-400"
              : warningRisks.length
                ? "border border-accent-warning/30 bg-accent-warning/10 text-accent-warning"
                : "border border-accent-success/30 bg-accent-success/10 text-accent-success"
          }`}
        >
          {criticalRisks.length
            ? `${criticalRisks.length} blocked`
            : warningRisks.length
              ? `${warningRisks.length} warning${warningRisks.length > 1 ? "s" : ""}`
              : "✓ All clear"}
        </span>
      </div>

      <div className="p-4">
        <div className="space-y-2">
          {criticalRisks.length > 0 && (
            <p className="text-xs text-muted-foreground">
              These commands match patterns that are never safe to send automatically. Deselect
              the profile containing them to continue — they can&apos;t be installed.
            </p>
          )}
          {criticalRisks.map((risk) => (
            <div
              key={`${risk.command}-${risk.label}`}
              className="overflow-hidden rounded-lg border border-red-500/30 bg-red-500/10 p-3"
            >
              <div className="text-sm font-medium text-red-400">🚫 {risk.label}</div>
              <div className="mt-1 break-all font-mono text-xs text-red-400/80">
                {risk.command}
              </div>
            </div>
          ))}
          {warningRisks.map((risk) => (
            <div
              key={`${risk.command}-${risk.label}`}
              className="overflow-hidden rounded-lg border border-accent-warning/25 bg-accent-warning/10 p-3"
            >
              <div className="text-sm font-medium text-accent-warning">⚠️ {risk.label}</div>
              <div className="mt-1 break-all font-mono text-xs text-accent-warning/80">
                {risk.command}
              </div>
            </div>
          ))}
          {!risks.length && (
            <div className="rounded-lg border border-accent-success/25 bg-accent-success/10 p-3 text-sm text-accent-success">
              ✓ No high-risk shell patterns detected in the selected pack.
            </div>
          )}
        </div>
      </div>
    </section>
  );
}
