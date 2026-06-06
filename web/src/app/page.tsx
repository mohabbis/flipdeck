"use client";

import { useMemo, useState } from "react";
import { CommandAudit } from "@/components/CommandAudit";
import { CommandPreview } from "@/components/CommandPreview";
import { ProfileSelector } from "@/components/ProfileSelector";
import { SmartInstallButton } from "@/components/SmartInstallButton";
import { getProfileCommands, normalizeProfile, profileFiles } from "@/lib/profiles";

const installSteps = [
  "Plug in Flipper",
  "Keep microSD inserted",
  "Install to Flipper",
  "Launch FlipDeck",
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
      <section className="border-b border-border bg-gradient-to-b from-background to-card">
        <div className="mx-auto grid max-w-7xl gap-8 px-4 py-8 sm:px-6 lg:grid-cols-[minmax(0,1fr)_440px] lg:px-8">
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
                  Railway installer
                </span>
              </header>

              <div>
                <p className="mb-4 inline-flex items-center gap-2 rounded-full border border-accent/30 bg-accent/10 px-4 py-1.5 text-sm font-semibold text-accent">
                  <span className="inline-block h-2 w-2 animate-pulse rounded-full bg-accent"></span>
                  USB HID command deck for Flipper Zero
                </p>
                <h1 className="max-w-3xl bg-gradient-to-r from-foreground via-foreground to-accent bg-clip-text text-5xl font-bold tracking-tight text-transparent sm:text-6xl lg:text-7xl">
                  Flip<span className="text-accent">Deck</span>
                </h1>
                <p className="mt-5 max-w-2xl text-lg leading-relaxed text-muted-foreground">
                  Build a Flipper-ready SD card pack from trusted developer profiles, inspect every
                  command, and download a qFlipper-friendly ZIP.
                </p>
              </div>

              <SmartInstallButton selectedIds={selectedIds} selectedCount={selectedCommandCount} />
            </div>

            <dl className="grid grid-cols-3 gap-4 rounded-xl border border-border bg-card/50 p-4 backdrop-blur-sm">
              <div className="text-center sm:text-left">
                <dt className="text-xs font-medium uppercase tracking-wide text-muted-foreground">
                  Profiles
                </dt>
                <dd className="mt-1 font-mono text-3xl font-bold text-foreground">
                  {selectedIds.length}/{profiles.length}
                </dd>
              </div>
              <div className="text-center sm:text-left">
                <dt className="text-xs font-medium uppercase tracking-wide text-muted-foreground">
                  Commands
                </dt>
                <dd className="mt-1 font-mono text-3xl font-bold text-accent">
                  {selectedCommandCount}
                </dd>
              </div>
              <div className="text-center sm:text-left">
                <dt className="text-xs font-medium uppercase tracking-wide text-muted-foreground">
                  Available
                </dt>
                <dd className="mt-1 font-mono text-3xl font-bold text-foreground">
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
            filters={["dev", "cloud", "system"]}
            previewOnHover
          />

          {/* Installation Steps */}
          <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
            <div className="border-b border-border bg-gradient-to-r from-card to-card/80 px-4 py-3">
              <div className="flex items-center justify-between">
                <h2 className="text-lg font-semibold text-foreground">🚀 qFlipper Handoff</h2>
                <span className="rounded-full bg-accent/10 px-2 py-0.5 text-xs font-medium text-accent">
                  ZIP contains apps_data/flipdeck
                </span>
              </div>
            </div>
            <div className="grid grid-cols-2 gap-px bg-border sm:grid-cols-4">
              {installSteps.map((step, index) => (
                <div
                  key={step}
                  className="group relative bg-card p-4 transition-colors hover:bg-card/80"
                >
                  <div className="mb-2 inline-flex h-8 w-8 items-center justify-center rounded-full bg-gradient-to-br from-accent to-accent-primary-hover font-mono text-xs font-bold text-white shadow-md">
                    {String(index + 1).padStart(2, "0")}
                  </div>
                  <div className="text-sm font-semibold text-foreground">{step}</div>
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
