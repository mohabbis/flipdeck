import type { FlipDeckAction } from "@/types/flipdeck";
import { auditCommands, auditSnippets, type CommandRisk } from "@/lib/safety-check";

interface CommandAuditProps {
  commands: FlipDeckAction[];
  snippets?: Record<string, string>;
}

function RiskRow({ risk, tone }: { risk: CommandRisk; tone: "critical" | "warning" }) {
  const styles =
    tone === "critical"
      ? { box: "border-red-500/30 bg-red-500/10", title: "text-red-400", body: "text-red-400/80", icon: "🚫" }
      : {
          box: "border-accent-warning/25 bg-accent-warning/10",
          title: "text-accent-warning",
          body: "text-accent-warning/80",
          icon: "⚠️"
        };
  return (
    <div className={`overflow-hidden rounded-lg border p-3 ${styles.box}`}>
      <div className={`flex items-center justify-between gap-2 text-sm font-medium ${styles.title}`}>
        <span>
          {styles.icon} {risk.label}
        </span>
        <span className="shrink-0 text-xs font-normal opacity-70">{risk.command}</span>
      </div>
      <pre className={`mt-1 overflow-x-auto whitespace-pre-wrap break-all font-mono text-xs ${styles.body}`}>
        {risk.value.trim()}
      </pre>
    </div>
  );
}

export function CommandAudit({ commands, snippets }: CommandAuditProps) {
  const risks = [...auditCommands(commands), ...(snippets ? auditSnippets(snippets) : [])];
  const criticalRisks = risks.filter((risk) => risk.severity === "critical");
  const warningRisks = risks.filter((risk) => risk.severity === "warning");
  const snippetCount = snippets ? Object.keys(snippets).length : 0;
  const checkedLabel = snippets
    ? `${commands.length} commands + ${snippetCount} snippets checked`
    : `${commands.length} commands checked`;

  return (
    <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
      <div className="flex items-center justify-between gap-4 border-b border-border bg-gradient-to-r from-card to-card/80 px-4 py-3">
        <div>
          <h2 className="text-lg font-semibold text-foreground">🛡️ Pack Safety</h2>
          <p className="text-sm text-muted-foreground">{checkedLabel}</p>
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
          {criticalRisks.map((risk, index) => (
            <RiskRow key={`crit-${risk.label}-${risk.command}-${index}`} risk={risk} tone="critical" />
          ))}
          {warningRisks.map((risk, index) => (
            <RiskRow key={`warn-${risk.label}-${risk.command}-${index}`} risk={risk} tone="warning" />
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
