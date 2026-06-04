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
    <main className="min-h-screen bg-[#f5f1e8] text-[#171717]">
      <section className="border-b border-black/10 bg-[#f5f1e8]">
        <div className="mx-auto grid max-w-7xl gap-8 px-4 py-6 sm:px-6 lg:grid-cols-[minmax(0,1fr)_440px] lg:px-8">
          <div className="flex flex-col justify-between gap-8 py-2">
            <div className="space-y-7">
              <header className="flex flex-wrap items-center justify-between gap-3 text-sm">
                <a href="https://github.com/mohabbis/flipdeck" className="font-semibold text-[#171717]">
                  FlipDeck
                </a>
                <span className="rounded-md border border-black/10 bg-white/65 px-3 py-1 font-medium text-[#4b5563]">
                  Railway installer
                </span>
              </header>

              <div>
                <p className="mb-4 inline-flex rounded-md border border-[#ff7a00]/30 bg-[#fff5e8] px-3 py-1 text-sm font-semibold text-[#9a4a00]">
                  USB HID command deck for Flipper Zero
                </p>
                <h1 className="max-w-3xl text-5xl font-semibold tracking-normal text-[#111111] sm:text-6xl lg:text-7xl">
                  Flip<span className="text-[#ff6b00]">Deck</span>
                </h1>
                <p className="mt-5 max-w-2xl text-lg leading-8 text-[#374151]">
                  Build a Flipper-ready SD card pack from trusted developer profiles, inspect every
                  command, and download a qFlipper-friendly ZIP.
                </p>
              </div>

              <SmartInstallButton selectedIds={selectedIds} selectedCount={selectedCommandCount} />
            </div>

            <dl className="grid gap-3 border-t border-black/10 pt-5 sm:grid-cols-3">
              <div>
                <dt className="text-sm font-medium text-[#6b7280]">Included profiles</dt>
                <dd className="mt-1 font-mono text-3xl font-semibold text-[#171717]">
                  {selectedIds.length}/{profiles.length}
                </dd>
              </div>
              <div>
                <dt className="text-sm font-medium text-[#6b7280]">Selected commands</dt>
                <dd className="mt-1 font-mono text-3xl font-semibold text-[#171717]">
                  {selectedCommandCount}
                </dd>
              </div>
              <div>
                <dt className="text-sm font-medium text-[#6b7280]">Available commands</dt>
                <dd className="mt-1 font-mono text-3xl font-semibold text-[#171717]">
                  {totalCommands}
                </dd>
              </div>
            </dl>
          </div>

          {activeProfile && <CommandPreview profile={activeProfile} renderAs="flipper-screen" />}
        </div>
      </section>

      <section className="mx-auto grid max-w-7xl gap-6 px-4 py-6 sm:px-6 lg:grid-cols-[minmax(0,1fr)_380px] lg:px-8">
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

          <section className="rounded-lg border border-black/10 bg-white p-4 shadow-sm">
            <div className="flex flex-col gap-1 sm:flex-row sm:items-end sm:justify-between">
              <h2 className="text-lg font-semibold text-[#171717]">qFlipper Handoff</h2>
              <span className="text-sm text-[#6b7280]">ZIP contains apps_data/flipdeck</span>
            </div>
            <div className="mt-4 grid gap-2 sm:grid-cols-4">
              {installSteps.map((step, index) => (
                <div key={step} className="border-l-2 border-[#ff7a00] bg-[#fff8ed] px-3 py-2">
                  <div className="font-mono text-xs font-semibold text-[#9a4a00]">
                    {String(index + 1).padStart(2, "0")}
                  </div>
                  <div className="mt-1 text-sm font-semibold text-[#171717]">{step}</div>
                </div>
              ))}
            </div>
          </section>
        </div>

        <aside className="space-y-6">
          <CommandAudit commands={selectedCommands} />
          <section className="rounded-lg border border-black/10 bg-white p-4 shadow-sm">
            <h2 className="text-lg font-semibold text-[#171717]">Active Profile JSON</h2>
            <p className="mt-1 text-sm text-[#6b7280]">{activeProfile?.name}</p>
            <a
              href={`/profiles/${activeProfile?.id}.json`}
              download
              className="mt-4 inline-flex h-10 items-center rounded-md border border-black/10 px-3 text-sm font-semibold text-[#171717] transition hover:border-[#ff7a00]/50 hover:text-[#9a4a00]"
            >
              Download profile JSON
            </a>
          </section>
        </aside>
      </section>
    </main>
  );
}
