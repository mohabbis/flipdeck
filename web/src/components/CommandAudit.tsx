import type { FlipDeckAction } from "@/types/flipdeck";
import { auditCommands } from "@/lib/safety-check";

interface CommandAuditProps {
  commands: FlipDeckAction[];
}

export function CommandAudit({ commands }: CommandAuditProps) {
  const risks = auditCommands(commands);

  return (
    <section className="rounded-lg border border-amber-300/25 bg-[linear-gradient(180deg,rgba(251,191,36,0.14),rgba(15,23,42,0.70))] p-4 shadow-2xl shadow-black/20 backdrop-blur-md">
      <div className="flex items-center justify-between gap-4">
        <div>
          <h2 className="text-lg font-semibold text-white">Safety Audit</h2>
          <p className="text-sm text-slate-400">Flags risky shell patterns before download.</p>
        </div>
        <span
          className={`rounded-md border px-2 py-1 text-xs ${
            risks.length
              ? "border-amber-400/30 bg-amber-400/10 text-amber-200"
              : "border-[#00D4AA]/40 bg-[#00D4AA]/15 text-[#C7FFF1]"
          }`}
        >
          {risks.length ? `${risks.length} warning${risks.length > 1 ? "s" : ""}` : "clear"}
        </span>
      </div>

      <div className="mt-4 space-y-2">
        {risks.length ? (
          risks.map((risk) => (
            <div
              key={`${risk.command}-${risk.label}`}
              className="rounded-md border border-amber-400/20 bg-amber-400/10 p-3"
            >
              <div className="text-sm font-medium text-amber-100">{risk.label}</div>
              <div className="mt-1 text-sm text-amber-200/80">{risk.command}</div>
            </div>
          ))
        ) : (
          <div className="rounded-md border border-[#00D4AA]/25 bg-[#00D4AA]/10 p-3 text-sm text-[#C7FFF1]">
            No high-risk shell patterns detected in this profile.
          </div>
        )}
      </div>
    </section>
  );
}
