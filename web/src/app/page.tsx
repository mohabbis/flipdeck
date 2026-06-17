"use client";

import { useMemo, useState } from "react";
import { CommandAudit } from "@/components/CommandAudit";
import { CommandPreview } from "@/components/CommandPreview";
import { ProfileSelector } from "@/components/ProfileSelector";
import { SmartInstallButton } from "@/components/SmartInstallButton";
import { getProfileCommands, normalizeProfile, profileFiles } from "@/lib/profiles";
import { auditCommands } from "@/lib/safety-check";

const installSteps = [
  "Plug in Flipper via USB",
  "Connect & pick your profiles",
  "One click installs the app + profiles",
  "Launch FlipDeck from Apps → Tools",
];

export default function Home() {
  const profiles = useMemo(
    () => profileFiles.map(({ fileName, profile }) => normalizeProfile(profile, fileName)),
    []
  );
  const [activeProfileId, setActiveProfileId] = useState("git");
  const [selectedIds, setSelectedIds] = useState(() =>
    profiles.map((profile) => profile.id ?? profile.name.toLowerCase())
  );

  const activeProfile =
    profiles.find((profile) => profile.id === activeProfileId) ??
    profiles.find((profile) => selectedIds.includes(profile.id ?? "")) ??
    profiles[0];
  const selectedIdSet = useMemo(() => new Set(selectedIds), [selectedIds]);
  const selectedProfiles = profiles.filter((profile) => selectedIdSet.has(profile.id ?? ""));
  const selectedCommands = selectedProfiles.flatMap((profile) => getProfileCommands(profile));
  const totalCommands = profiles.reduce((sum, profile) => sum + getProfileCommands(profile).length, 0);
  const selectedCommandCount = selectedCommands.length;
  const hasCriticalRisk = auditCommands(selectedCommands).some((risk) => risk.severity === "critical");

  function toggleProfile(profileId: string) {
    setActiveProfileId(profileId);
    setSelectedIds((current) =>
      current.includes(profileId)
        ? current.filter((id) => id !== profileId)
        : [...current, profileId]
    );
  }

  return (
    <main className="min-h-screen bg-background text-foreground">
      {/* Hero Section */}
      <section className="relative overflow-hidden border-b border-border bg-gradient-to-b from-background to-card">
        <div className="f1-checker pointer-events-none absolute inset-0" aria-hidden="true" />
        <div className="f1-stripes pointer-events-none absolute inset-0" aria-hidden="true" />
        <div className="relative mx-auto grid max-w-7xl gap-8 px-4 py-8 sm:px-6 lg:grid-cols-[minmax(0,1fr)_440px] lg:px-8">
          <div className="flex flex-col justify-between gap-8 py-2">
            <div className="space-y-6">
              <header className="flex flex-wrap items-center justify-between gap-3 text-sm">
                <a
                  href="https://github.com/mohabbis/flipdeck"
                  className="group flex items-center gap-2 font-bold text-xl text-foreground transition-colors hover:text-accent"
                >
                  <span className="text-accent">⚡</span>
                  FlipDeck
                </a>
                <span className="rounded-full border border-border bg-card px-3 py-1 text-xs font-medium text-muted-foreground shadow-sm">
                  Web Serial installer
                </span>
              </header>

              <div>
                <div className="mb-4 flex flex-wrap items-center gap-3">
                  <p className="inline-flex items-center gap-2 rounded-full border border-accent/30 bg-accent/10 px-4 py-1.5 text-sm font-semibold text-accent">
                    <span className="inline-block h-2 w-2 animate-pulse rounded-full bg-accent"></span>
                    USB HID command deck for Flipper Zero
                  </p>
                  <span className="f1-lights" aria-hidden="true">
                    <span></span>
                    <span></span>
                    <span></span>
                    <span></span>
                    <span></span>
                  </span>
                </div>
                <h1 className="max-w-3xl bg-gradient-to-r from-foreground via-foreground to-accent bg-clip-text text-5xl font-black uppercase tracking-tight text-transparent sm:text-6xl lg:text-7xl">
                  Flip
                  <span className="italic" style={{ WebkitTextFillColor: "var(--accent-primary)" }}>
                    Deck
                  </span>
                </h1>
                <p className="mt-5 max-w-2xl text-lg leading-relaxed text-muted-foreground">
                  Pick trusted developer profiles, inspect every command, then connect your Flipper
                  over USB and install the app and profiles directly from your browser — no
                  qFlipper or SD card juggling required.
                </p>
                <p className="mt-3 max-w-2xl text-sm leading-relaxed text-muted-foreground">
                  New here? A <strong className="text-foreground">profile</strong> is a set of
                  commands (like <code className="rounded bg-card px-1 py-0.5">git status</code>)
                  that your Flipper Zero can type into a computer over USB with one button press.
                  Check the boxes below for the workflows you want, then install.
                </p>
              </div>

              <SmartInstallButton
                selectedIds={selectedIds}
                selectedCount={selectedCommandCount}
                blocked={hasCriticalRisk}
              />
            </div>

            <dl className="grid grid-cols-3 gap-4 rounded-xl border border-border bg-card/50 p-4 backdrop-blur-sm">
              <div className="col-span-3 mb-1 flex items-center gap-2">
                <span className="f1-live-dot" aria-hidden="true"></span>
                <span className="text-xs font-bold uppercase tracking-[0.2em] text-muted-foreground">
                  Live telemetry
                </span>
              </div>
              <div className="text-center sm:text-left">
                <dt className="text-xs font-medium uppercase tracking-wide text-muted-foreground">
                  Profiles
                </dt>
                <dd className="mt-1 font-mono text-3xl font-bold tabular-nums text-foreground">
                  {selectedIds.length}/{profiles.length}
                </dd>
              </div>
              <div className="text-center sm:text-left">
                <dt className="text-xs font-medium uppercase tracking-wide text-muted-foreground">
                  Commands
                </dt>
                <dd className="mt-1 font-mono text-3xl font-bold tabular-nums text-accent">
                  {selectedCommandCount}
                </dd>
              </div>
              <div className="text-center sm:text-left">
                <dt className="text-xs font-medium uppercase tracking-wide text-muted-foreground">
                  Available
                </dt>
                <dd className="mt-1 font-mono text-3xl font-bold tabular-nums text-foreground">
                  {totalCommands}
                </dd>
              </div>
            </dl>
          </div>

          {activeProfile && <CommandPreview profile={activeProfile} renderAs="flipper-screen" />}
        </div>
      </section>

      {/* Main Content */}
      <section className="mx-auto grid max-w-7xl gap-6 px-4 py-8 sm:px-6 lg:grid-cols-[minmax(0,1fr)_380px] lg:px-8">
        <div className="space-y-6">
          <ProfileSelector
            profiles={profiles}
            activeId={activeProfile?.id ?? ""}
            selectedIds={selectedIds}
            onToggle={toggleProfile}
            onPreview={setActiveProfileId}
            onSelectAll={() =>
              setSelectedIds(profiles.map((profile) => profile.id ?? profile.name.toLowerCase()))
            }
            onClear={() => setSelectedIds([])}
            filters={["dev", "cloud", "system", "wifi"]}
            previewOnHover
          />

          {/* Installation Steps */}
          <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
            <div className="border-b border-border bg-gradient-to-r from-card to-card/80 px-4 py-3">
              <div className="flex items-center justify-between">
                <h2 className="text-lg font-bold uppercase tracking-wide text-foreground">🚀 How to Install</h2>
                <span className="rounded-full bg-accent/10 px-2 py-0.5 text-xs font-bold uppercase tracking-[0.15em] text-accent">
                  one-click via browser
                </span>
              </div>
            </div>
            <div className="grid grid-cols-2 gap-px bg-border sm:grid-cols-4">
              {installSteps.map((step, index) => (
                <div
                  key={step}
                  className="group relative bg-card p-4 transition-colors hover:bg-card/80"
                >
                  <div className="mb-2 inline-flex h-8 w-8 items-center justify-center rounded-full bg-gradient-to-br from-accent to-accent-primary-hover font-mono text-sm font-black italic text-white shadow-md">
                    {String(index + 1).padStart(2, "0")}
                  </div>
                  <div className="text-sm font-semibold uppercase tracking-wide text-foreground">{step}</div>
                  <div className="absolute inset-x-0 bottom-0 h-0.5 bg-gradient-to-r from-accent to-transparent opacity-0 transition-opacity group-hover:opacity-100"></div>
                </div>
              ))}
            </div>
          </section>
        </div>

        <aside className="space-y-6">
          <CommandAudit commands={selectedCommands} />
          <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
            <div className="border-b border-border bg-gradient-to-r from-card to-card/80 px-4 py-3">
              <h2 className="text-lg font-semibold text-foreground">📄 Active Profile JSON</h2>
              <p className="mt-0.5 text-sm text-muted-foreground">{activeProfile?.name}</p>
            </div>
            <div className="p-4">
              <a
                href={`/profiles/${activeProfile?.id}.json`}
                download
                className="group flex items-center justify-between rounded-lg border border-border bg-card/50 px-4 py-3 text-sm font-medium text-foreground transition-all hover:border-accent/50 hover:bg-accent/10 hover:text-accent"
              >
                <span>Download profile JSON</span>
                <span className="transition-transform group-hover:translate-x-1">→</span>
              </a>
            </div>
          </section>
        </aside>
      </section>
    </main>
  );
}
