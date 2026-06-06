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
    <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
      <div className="flex items-start justify-between gap-4 border-b border-border bg-gradient-to-r from-card to-card/80 px-4 py-3">
        <div>
          <h2 className="text-lg font-semibold text-foreground">📱 Live Preview</h2>
          <p className="text-sm text-muted-foreground">{profile.name}</p>
        </div>
        <span className="rounded-full border border-accent/35 bg-accent/15 px-2 py-1 text-xs font-semibold text-accent">
          {profile.icon ?? profile.id}
        </span>
      </div>

      <div className="p-4">
        {/* Flipper Device Mockup */}
        <div className="mx-auto max-w-[340px] rounded-[28px] border border-border bg-gradient-to-br from-card to-background p-5 shadow-2xl">
          <div className="rounded-[22px] border border-accent/30 bg-gradient-to-br from-accent/20 to-accent/5 p-4">
            {/* Screen */}
            <div className="overflow-hidden rounded-lg border-2 border-foreground/20 bg-flipper-screen-bg p-3 shadow-inner">
              <pre className="overflow-hidden whitespace-pre font-mono text-[10px] font-semibold leading-4 text-flipper-screen sm:text-xs">
                {renderFlipperAscii(profile)}
              </pre>
            </div>
            
            {/* Buttons */}
            <div className="mt-4 grid grid-cols-[1fr_auto_1fr] items-center gap-3">
              <div className="h-8 rounded-full bg-gradient-to-br from-accent to-accent-primary-hover shadow-md" />
              <div className="grid h-16 w-16 place-items-center rounded-full bg-gradient-to-br from-foreground/90 to-foreground shadow-lg">
                <div className="h-7 w-7 rounded-full border border-border bg-card shadow-inner" />
              </div>
              <div className="h-8 rounded-full bg-gradient-to-br from-accent to-accent-primary-hover shadow-md" />
            </div>
          </div>
        </div>

        {/* Commands List */}
        <div className="mt-4 max-h-64 overflow-y-auto rounded-lg border border-border bg-background/50">
          {commands.map((command) => (
            <div
              key={`${profile.id}-${command.label}`}
              className="grid grid-cols-[1fr_auto] gap-3 border-b border-border px-3 py-2.5 last:border-b-0 hover:bg-accent/5"
            >
              <span className="min-w-0">
                <span className="block truncate text-sm font-medium text-foreground">{command.label}</span>
                <span className="block truncate font-mono text-xs text-muted-foreground">{command.value}</span>
              </span>
              <span className="self-center rounded-full border border-accent-success/30 bg-accent-success/15 px-2 py-1 text-xs font-semibold text-accent-success">
                {command.type}
              </span>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
