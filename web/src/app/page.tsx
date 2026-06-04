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
  const [selectedId, setSelectedId] = useState(profiles[0]?.id ?? "git");

  const selectedProfile = profiles.find((profile) => profile.id === selectedId) ?? profiles[0];
  const selectedCommands = selectedProfile ? getProfileCommands(selectedProfile) : [];
  const totalCommands = profiles.reduce((sum, profile) => sum + getProfileCommands(profile).length, 0);

  return (
    <main className="min-h-screen bg-[#0F172A] text-slate-100">
      <section className="border-b border-[#00D4AA]/20 bg-[linear-gradient(135deg,#07111F_0%,#0F172A_38%,#123C35_72%,#101827_100%)]">
        <div className="mx-auto grid max-w-7xl gap-8 px-4 py-8 sm:px-6 lg:grid-cols-[1fr_420px] lg:px-8">
          <div className="flex min-h-[360px] flex-col justify-between gap-8">
            <div>
              <div className="mb-5 inline-flex rounded-md border border-[#00D4AA]/50 bg-[#00D4AA]/20 px-3 py-1 text-sm font-medium text-[#C7FFF1] shadow-[0_0_30px_rgba(0,212,170,0.18)]">
                USB HID command deck for Flipper Zero
              </div>
              <h1 className="max-w-3xl text-4xl font-semibold tracking-normal text-white sm:text-5xl lg:text-6xl">
                Flip<span className="text-[#00D4AA]">Deck</span>
              </h1>
              <p className="mt-5 max-w-2xl text-lg leading-8 text-slate-300">
                Plug in your Flipper Zero with its microSD card inserted, inspect every
                command, then install the FlipDeck pack directly to the Flipper.
              </p>
            </div>

            <div className="grid gap-3 sm:grid-cols-3">
              <div className="rounded-lg border border-[#00D4AA]/30 bg-[#00D4AA]/12 p-4 backdrop-blur-md">
                <div className="font-mono text-3xl font-semibold text-[#9FF5DF]">{profiles.length}</div>
                <div className="mt-1 text-sm text-slate-400">Profiles</div>
              </div>
              <div className="rounded-lg border border-sky-400/30 bg-sky-400/12 p-4 backdrop-blur-md">
                <div className="font-mono text-3xl font-semibold text-sky-200">{totalCommands}</div>
                <div className="mt-1 text-sm text-slate-400">Commands</div>
              </div>
              <div className="rounded-lg border border-amber-300/30 bg-amber-300/12 p-4 backdrop-blur-md">
                <div className="font-mono text-3xl font-semibold text-amber-200">100ms</div>
                <div className="mt-1 text-sm text-slate-400">Default delay</div>
              </div>
            </div>
          </div>

          {selectedProfile && <CommandPreview profile={selectedProfile} renderAs="flipper-screen" />}
        </div>
      </section>

      <section className="mx-auto grid max-w-7xl gap-6 px-4 py-6 sm:px-6 lg:grid-cols-[minmax(0,1fr)_420px] lg:px-8">
        <div className="space-y-6">
          <ProfileSelector
            profiles={profiles}
            selectedId={selectedId}
            onSelect={setSelectedId}
            onHover={setSelectedId}
            filters={["dev", "cloud", "system"]}
            previewOnHover
          />

          <section className="rounded-lg border border-white/10 bg-[linear-gradient(135deg,rgba(14,165,233,0.16),rgba(0,212,170,0.09),rgba(251,191,36,0.10))] p-4 backdrop-blur-md">
            <div className="flex flex-col gap-1 sm:flex-row sm:items-end sm:justify-between">
              <div>
                <h2 className="text-lg font-semibold text-white">Primary install flow</h2>
                <p className="mt-1 text-sm text-slate-300">
                  Public users should leave the microSD card inside the Flipper and use USB/qFlipper
                  to copy the pack to the card mounted inside the device.
                </p>
              </div>
            </div>
            <div className="mt-4 grid gap-3 sm:grid-cols-4">
              {installSteps.map((step, index) => (
                <div key={step} className="rounded-lg border border-white/10 bg-slate-950/55 p-3">
                  <div className="font-mono text-sm text-[#9FF5DF]">
                    {String(index + 1).padStart(2, "0")}
                  </div>
                  <div className="mt-2 text-sm font-medium text-white">{step}</div>
                </div>
              ))}
            </div>
          </section>
        </div>

        <aside className="space-y-6">
          <CommandAudit commands={selectedCommands} />
          <SmartInstallButton selectedIds={selectedProfile?.id ? [selectedProfile.id] : []} />
          <section className="rounded-lg border border-white/10 bg-white/[0.07] p-4 backdrop-blur-md">
            <h2 className="text-lg font-semibold text-white">Manual fallback</h2>
            <p className="mt-2 text-sm leading-6 text-slate-300">
              If browser USB detection is unavailable, download the ZIP and copy its
              <code className="mx-1 rounded bg-slate-950/70 px-1.5 py-0.5 text-[#9FF5DF]">apps_data</code>
              folder to the Flipper SD card with qFlipper. Developer testing can use an external
              SD-card reader, but that is not the public install path.
            </p>
            <a
              href={`/profiles/${selectedProfile?.id}.json`}
              download
              className="mt-4 inline-flex h-10 items-center rounded-md border border-white/10 px-3 text-sm font-semibold text-slate-200 transition hover:border-[#00D4AA]/50 hover:text-[#9FF5DF]"
            >
              Download selected profile JSON
            </a>
          </section>
        </aside>
      </section>
    </main>
  );
}
