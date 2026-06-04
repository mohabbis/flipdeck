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
    <section className="rounded-lg border border-black/10 bg-[#202124] p-4 text-white shadow-xl shadow-black/10">
      <div className="flex items-start justify-between gap-4">
        <div>
          <h2 className="text-lg font-semibold">Live Preview</h2>
          <p className="text-sm text-[#d1d5db]">{profile.name}</p>
        </div>
        <span className="rounded-md border border-[#ff7a00]/35 bg-[#ff7a00]/15 px-2 py-1 text-xs font-semibold text-[#ffd8ad]">
          {profile.icon ?? profile.id}
        </span>
      </div>

      <div className="mt-4 rounded-[28px] border border-[#e5e0d6] bg-[#f4efe4] p-5 shadow-[inset_0_0_0_2px_rgba(255,255,255,0.75),0_24px_50px_rgba(0,0,0,0.25)]">
        <div className="mx-auto max-w-[320px] rounded-[22px] border border-[#f5c16f] bg-[#fff8ec] p-4">
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

      <div className="mt-4 max-h-64 overflow-y-auto rounded-lg border border-white/10 bg-black/25">
        {commands.map((command) => (
          <div
            key={`${profile.id}-${command.label}`}
            className="grid grid-cols-[1fr_auto] gap-3 border-b border-white/10 px-3 py-2 last:border-b-0"
          >
            <span className="min-w-0">
              <span className="block truncate text-sm font-medium text-white">{command.label}</span>
              <span className="block truncate font-mono text-xs text-[#d1d5db]">{command.value}</span>
            </span>
            <span className="self-center rounded border border-[#00a88a]/30 bg-[#00a88a]/15 px-2 py-1 text-xs font-semibold text-[#b9fff0]">
              {command.type}
            </span>
          </div>
        ))}
      </div>
    </section>
  );
}
