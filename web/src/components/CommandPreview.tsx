import type { Profile } from "@/types/flipdeck";
import { renderFlipperAscii } from "@/lib/flipper-mockup";
import { getProfileCommands } from "@/lib/profiles";

interface CommandPreviewProps {
  profile: Profile;
  renderAs?: "flipper-screen" | "ascii";
}

export function CommandPreview({ profile }: CommandPreviewProps) {
  const commands = getProfileCommands(profile);

  return (
    <section className="rounded-lg border border-orange-300/30 bg-[linear-gradient(180deg,rgba(251,146,60,0.18),rgba(15,23,42,0.72))] p-4 shadow-2xl shadow-orange-950/30 backdrop-blur-md">
      <div className="flex items-start justify-between gap-4">
        <div>
          <h2 className="text-lg font-semibold text-white">Live Preview</h2>
          <p className="text-sm text-orange-100/70">128x64 orange-backlit menu approximation.</p>
        </div>
        <span className="rounded-md border border-orange-300/40 bg-orange-300/15 px-2 py-1 text-xs text-orange-100">
          {profile.icon ?? profile.id}
        </span>
      </div>

      <div className="mt-4 rounded-[28px] border border-slate-200 bg-[#f7f2e8] p-5 shadow-[inset_0_0_0_2px_rgba(255,255,255,0.8),0_24px_60px_rgba(0,0,0,0.35)]">
        <div className="mx-auto max-w-[320px] rounded-[22px] border border-orange-200 bg-[#fffaf0] p-4">
          <div className="rounded-lg border-2 border-[#2b2118] bg-[#ff9d2e] p-3 text-[#1f1306] shadow-[inset_0_0_20px_rgba(80,38,0,0.22)]">
            <pre className="overflow-hidden whitespace-pre font-mono text-[11px] font-semibold leading-4 sm:text-xs">
              {renderFlipperAscii(profile)}
            </pre>
          </div>
          <div className="mt-4 grid grid-cols-[1fr_auto_1fr] items-center gap-3">
            <div className="h-8 rounded-full bg-[#ff7a00]" />
            <div className="grid h-16 w-16 place-items-center rounded-full bg-slate-900">
              <div className="h-7 w-7 rounded-full border border-slate-500 bg-slate-700" />
            </div>
            <div className="h-8 rounded-full bg-[#ff7a00]" />
          </div>
        </div>
      </div>

      <div className="mt-4 max-h-64 overflow-y-auto rounded-lg border border-orange-300/15 bg-slate-950/70">
        {commands.map((command) => (
          <div
            key={`${profile.id}-${command.label}`}
            className="grid grid-cols-[1fr_auto] gap-3 border-b border-orange-300/10 px-3 py-2 last:border-b-0"
          >
            <span className="min-w-0">
              <span className="block truncate text-sm font-medium text-white">{command.label}</span>
              <span className="block truncate font-mono text-xs text-orange-100/55">{command.value}</span>
            </span>
            <span className="self-center rounded border border-[#00D4AA]/25 bg-[#00D4AA]/10 px-2 py-1 text-xs text-[#9FF5DF]">
              {command.type}
            </span>
          </div>
        ))}
      </div>
    </section>
  );
}
