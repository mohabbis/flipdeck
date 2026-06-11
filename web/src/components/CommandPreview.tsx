"use client";

import { useEffect, useState } from "react";
import type { Profile } from "@/types/flipdeck";
import { renderFlipperAscii, type PreviewScreen } from "@/lib/flipper-mockup";
import { getProfileCommands } from "@/lib/profiles";

interface CommandPreviewProps {
  profile: Profile;
  renderAs?: "flipper-screen" | "ascii";
}

export function CommandPreview({ profile }: CommandPreviewProps) {
  const commands = getProfileCommands(profile);
  const [screen, setScreen] = useState<PreviewScreen>({ kind: "list", selectedIndex: 0 });

  // Reset the on-screen state when the previewed profile changes (adjust during render, not in an effect).
  const [renderedProfileId, setRenderedProfileId] = useState(profile.id);
  if (profile.id !== renderedProfileId) {
    setRenderedProfileId(profile.id);
    setScreen({ kind: "list", selectedIndex: 0 });
  }

  useEffect(() => {
    if (screen.kind !== "sent") return;
    const timer = setTimeout(() => setScreen({ kind: "list", selectedIndex: screen.selectedIndex }), 1100);
    return () => clearTimeout(timer);
  }, [screen]);

  function move(delta: number) {
    if (screen.kind !== "list" || commands.length === 0) return;
    const next = (screen.selectedIndex + delta + commands.length) % commands.length;
    setScreen({ kind: "list", selectedIndex: next });
  }

  function pressOk() {
    if (commands.length === 0) return;
    if (screen.kind === "list") setScreen({ kind: "confirm", selectedIndex: screen.selectedIndex });
    else if (screen.kind === "confirm") setScreen({ kind: "sent", selectedIndex: screen.selectedIndex });
  }

  function pressBack() {
    if (screen.kind === "confirm" || screen.kind === "sent") {
      setScreen({ kind: "list", selectedIndex: screen.selectedIndex });
    } else {
      move(-1);
    }
  }

  const leftGlyph = screen.kind === "list" ? "▲" : "✕";
  const rightGlyph = screen.kind === "list" ? "▼" : "";
  const leftLabel = screen.kind === "list" ? "Up" : "Cancel";
  const rightLabel = screen.kind === "list" ? "Down" : "";
  const okLabel = screen.kind === "confirm" ? "Send" : "OK";

  return (
    <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
      <div className="flex items-start justify-between gap-4 border-b border-border bg-gradient-to-r from-card to-card/80 px-4 py-3">
        <div>
          <h2 className="text-lg font-semibold text-foreground">📱 Interactive Preview</h2>
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
                {renderFlipperAscii(profile, screen)}
              </pre>
            </div>

            {/* Buttons — drive the mock screen like the real device's d-pad */}
            <div className="mt-4 grid grid-cols-[1fr_auto_1fr] items-center gap-3">
              <button
                type="button"
                onClick={pressBack}
                aria-label={leftLabel}
                title={leftLabel}
                className="flex h-8 items-center justify-center rounded-full bg-gradient-to-br from-accent to-accent-primary-hover text-xs font-bold text-white shadow-md transition active:scale-95"
              >
                {leftGlyph}
              </button>
              <button
                type="button"
                onClick={pressOk}
                aria-label={okLabel}
                title={okLabel}
                className="grid h-16 w-16 place-items-center rounded-full bg-gradient-to-br from-foreground/90 to-foreground shadow-lg transition active:scale-95"
              >
                <div className="grid h-7 w-7 place-items-center rounded-full border border-border bg-card text-[9px] font-bold text-foreground shadow-inner">
                  {okLabel}
                </div>
              </button>
              <button
                type="button"
                onClick={() => move(1)}
                aria-label={rightLabel || "unavailable"}
                title={rightLabel || undefined}
                disabled={screen.kind !== "list"}
                className="flex h-8 items-center justify-center rounded-full bg-gradient-to-br from-accent to-accent-primary-hover text-xs font-bold text-white shadow-md transition active:scale-95 disabled:cursor-not-allowed disabled:opacity-30"
              >
                {rightGlyph}
              </button>
            </div>
            <p className="mt-2 text-center text-[11px] text-muted-foreground">
              {screen.kind === "list"
                ? "▲▼ to browse · OK to select"
                : screen.kind === "confirm"
                  ? "OK to send · ✕ to cancel"
                  : "Sent — returning to list…"}
            </p>
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
              <span className="flex items-center gap-1.5 self-center">
                {command.target === "wifi_uart" && (
                  <span className="rounded-full border border-sky-400/30 bg-sky-400/15 px-2 py-1 text-xs font-semibold text-sky-400">
                    WiFi UART
                  </span>
                )}
                <span className="rounded-full border border-accent-success/30 bg-accent-success/15 px-2 py-1 text-xs font-semibold text-accent-success">
                  {command.type}
                </span>
              </span>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
