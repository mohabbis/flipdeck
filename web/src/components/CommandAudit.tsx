import type { FlipDeckAction } from "@/types/flipdeck";
import { auditCommands } from "@/lib/safety-check";

interface CommandAuditProps {
  commands: FlipDeckAction[];
}

export function CommandAudit({ commands }: CommandAuditProps) {
  const risks = auditCommands(commands);

  return (
    <section className="rounded-lg border border-black/10 bg-white p-4 shadow-sm">
      <div className="flex items-center justify-between gap-4">
        <div>
          <h2 className="text-lg font-semibold text-[#171717]">Pack Safety</h2>
          <p className="text-sm text-[#6b7280]">{commands.length} commands checked</p>
        </div>
        <span
          className={`rounded-md border px-2 py-1 text-xs font-semibold ${
            risks.length
              ? "border-[#ff7a00]/30 bg-[#fff3df] text-[#9a4a00]"
              : "border-[#00a88a]/30 bg-[#e7fbf5] text-[#006b59]"
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
              className="rounded-md border border-[#ff7a00]/25 bg-[#fff3df] p-3"
            >
              <div className="text-sm font-medium text-[#7c2d12]">{risk.label}</div>
              <div className="mt-1 break-all font-mono text-xs text-[#9a4a00]">{risk.command}</div>
            </div>
          ))
        ) : (
          <div className="rounded-md border border-[#00a88a]/25 bg-[#e7fbf5] p-3 text-sm text-[#006b59]">
            No high-risk shell patterns detected in the selected pack.
          </div>
        )}
      </div>
    </section>
  );
}
