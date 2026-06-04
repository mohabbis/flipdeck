"use client";

import { useMemo, useState } from "react";
import { CommandAudit } from "@/components/CommandAudit";
import { CommandPreview } from "@/components/CommandPreview";
import { ProfileSelector } from "@/components/ProfileSelector";
import { SmartInstallButton } from "@/components/SmartInstallButton";
import { getActionTypeColor } from "@/lib/utils";
import { getProfileCommands, normalizeProfile, profileFiles } from "@/lib/profiles";

const installSteps = [
  {
    title: "Download",
    body: "Get a ZIP with the apps_data folder already in the right structure.",
  },
  {
    title: "Open qFlipper",
    body: "Use the SD card browser. WebUSB is optional, not a blocker.",
  },
  {
    title: "Copy apps_data",
    body: "Drop the folder onto the SD root and merge when prompted.",
  },
  {
    title: "Launch",
    body: "Open FlipDeck, choose a profile, and confirm before sending.",
  },
];

const navItems = ["Install", "Profiles", "Safety", "Docs"];
const trustItems = ["Plain JSON", "No firmware flash", "Review before run", "qFlipper ready"];

export default function Home() {
  const profiles = useMemo(
    () => profileFiles.map(({ fileName, profile }) => normalizeProfile(profile, fileName)),
    []
  );
  const [selectedId, setSelectedId] = useState(profiles[0]?.id ?? "git");

  const selectedProfile = profiles.find((profile) => profile.id === selectedId) ?? profiles[0];
  const selectedCommands = selectedProfile ? getProfileCommands(selectedProfile) : [];
  const totalCommands = profiles.reduce((sum, profile) => sum + getProfileCommands(profile).length, 0);
  const selectedRiskCount = selectedProfile?.metadata?.risk_count ?? 0;
  const firstCommand = selectedCommands[0];

  return (
    <main className="min-h-screen overflow-hidden bg-[#070b10] text-zinc-100">
      <div className="pointer-events-none fixed inset-0 bg-[radial-gradient(circle_at_20%_10%,rgba(255,122,0,0.18),transparent_32%),radial-gradient(circle_at_82%_18%,rgba(0,212,170,0.14),transparent_30%),linear-gradient(180deg,rgba(255,255,255,0.04),transparent_22%)]" />
      <div className="pointer-events-none fixed inset-0 bg-[linear-gradient(rgba(255,255,255,0.035)_1px,transparent_1px),linear-gradient(90deg,rgba(255,255,255,0.035)_1px,transparent_1px)] bg-[size:48px_48px] opacity-20" />

      <div className="relative mx-auto grid max-w-[1440px] gap-6 px-4 py-4 sm:px-6 lg:grid-cols-[240px_minmax(0,1fr)] lg:px-8">
        <aside className="hidden lg:block">
          <div className="sticky top-4 rounded-[28px] border border-white/10 bg-white/[0.045] p-4 shadow-2xl shadow-black/40 backdrop-blur-xl">
            <a href="#" className="flex items-center gap-3">
              <span className="grid h-10 w-10 place-items-center rounded-2xl border border-[#ff7a00]/40 bg-[#ff7a00]/15 font-mono text-sm text-[#ffd7a3]">FD</span>
              <span>
                <span className="block text-sm font-semibold text-white">FlipDeck</span>
                <span className="block text-xs text-zinc-500">USB command deck</span>
              </span>
            </a>

            <nav className="mt-8 space-y-1">
              {navItems.map((item) => (
                <a key={item} href={`#${item.toLowerCase()}`} className="flex h-10 items-center rounded-xl px-3 text-sm text-zinc-400 transition hover:bg-white/8 hover:text-white">
                  {item}
                </a>
              ))}
            </nav>

            <div className="mt-8 rounded-2xl border border-[#00D4AA]/20 bg-[#00D4AA]/8 p-3">
              <div className="font-mono text-xs uppercase tracking-[0.22em] text-[#9ff5df]">Pack status</div>
              <div className="mt-2 text-sm text-zinc-300">{selectedProfile?.name ?? "Profile"} selected</div>
              <div className="mt-3 h-1.5 overflow-hidden rounded-full bg-white/10">
                <div className="h-full w-3/4 rounded-full bg-[#00D4AA]" />
              </div>
            </div>
          </div>
        </aside>

        <div className="space-y-6">
          <header className="sticky top-0 z-20 -mx-4 border-b border-white/10 bg-[#070b10]/80 px-4 py-3 backdrop-blur-xl sm:-mx-6 sm:px-6 lg:hidden">
            <div className="flex items-center justify-between">
              <div className="font-semibold">FlipDeck</div>
              <a href="/api/pack" className="rounded-full bg-[#ff7a00] px-4 py-2 text-sm font-semibold text-white">Download</a>
            </div>
          </header>

          <section id="install" className="rounded-[34px] border border-white/10 bg-[#0b1118]/85 p-4 shadow-2xl shadow-black/40 backdrop-blur-xl sm:p-6 lg:p-8">
            <div className="grid gap-8 lg:grid-cols-[minmax(0,1fr)_440px]">
              <div className="flex min-h-[520px] flex-col justify-between gap-10">
                <div>
                  <div className="inline-flex items-center gap-2 rounded-full border border-[#00D4AA]/30 bg-[#00D4AA]/10 px-3 py-1 text-xs font-semibold uppercase tracking-[0.18em] text-[#9ff5df]">Flipper Zero web installer</div>
                  <h1 className="mt-6 max-w-4xl text-5xl font-semibold tracking-[-0.05em] text-white sm:text-6xl lg:text-7xl">A cleaner command deck for your Flipper.</h1>
                  <p className="mt-6 max-w-2xl text-lg leading-8 text-zinc-300">Build a Flipper-ready SD card pack, inspect every command, and install through qFlipper without flashing firmware or trusting mystery payloads from the void.</p>
                  <div className="mt-8 flex flex-col gap-3 sm:flex-row">
                    <a href="/api/pack" className="inline-flex h-12 items-center justify-center rounded-2xl bg-[#ff7a00] px-6 text-sm font-bold text-white shadow-lg shadow-orange-950/40 transition hover:-translate-y-0.5 hover:bg-[#ff922b]">Download full ZIP pack</a>
                    <a href="#profiles" className="inline-flex h-12 items-center justify-center rounded-2xl border border-white/12 bg-white/6 px-6 text-sm font-bold text-zinc-100 transition hover:-translate-y-0.5 hover:border-[#00D4AA]/40 hover:bg-[#00D4AA]/10">Inspect profiles</a>
                  </div>
                </div>

                <div className="grid gap-3 sm:grid-cols-3">
                  {[["Profiles", profiles.length], ["Commands", totalCommands], ["Default delay", "100ms"]].map(([label, value]) => (
                    <div key={label} className="rounded-3xl border border-white/10 bg-white/[0.055] p-4">
                      <div className="font-mono text-3xl font-semibold text-white">{value}</div>
                      <div className="mt-1 text-sm text-zinc-500">{label}</div>
                    </div>
                  ))}
                </div>
              </div>

              <div className="space-y-4">
                {selectedProfile && <CommandPreview profile={selectedProfile} renderAs="flipper-screen" />}
                <div className="grid grid-cols-2 gap-3">
                  {trustItems.map((item) => (
                    <div key={item} className="rounded-2xl border border-white/10 bg-white/[0.045] px-3 py-3 text-sm text-zinc-300">
                      <span className="mr-2 text-[#00D4AA]">*</span>{item}
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </section>

          <section className="grid gap-4 md:grid-cols-4">
            {installSteps.map((step, index) => (
              <div key={step.title} className="rounded-3xl border border-white/10 bg-white/[0.045] p-5 backdrop-blur-xl">
                <div className="font-mono text-sm text-[#ffb067]">{String(index + 1).padStart(2, "0")}</div>
                <h2 className="mt-4 text-base font-semibold text-white">{step.title}</h2>
                <p className="mt-2 text-sm leading-6 text-zinc-400">{step.body}</p>
              </div>
            ))}
          </section>

          <section id="profiles" className="grid gap-6 lg:grid-cols-[minmax(0,1fr)_420px]">
            <div className="space-y-6">
              <ProfileSelector profiles={profiles} selectedId={selectedId} onSelect={setSelectedId} onHover={setSelectedId} filters={["dev", "cloud", "system"]} previewOnHover />

              <section className="rounded-[28px] border border-white/10 bg-white/[0.045] p-5 backdrop-blur-xl">
                <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
                  <div>
                    <p className="font-mono text-xs uppercase tracking-[0.22em] text-[#9ff5df]">Command inspector</p>
                    <h2 className="mt-2 text-2xl font-semibold tracking-tight text-white">{selectedProfile?.name ?? "Profile"} commands</h2>
                    <p className="mt-2 max-w-2xl text-sm leading-6 text-zinc-400">The exact command text is visible before download. Revolutionary, apparently.</p>
                  </div>
                  <span className="rounded-full border border-white/10 bg-white/6 px-3 py-1 text-sm text-zinc-300">{selectedCommands.length} actions</span>
                </div>

                <div className="mt-5 overflow-hidden rounded-2xl border border-white/10">
                  {selectedCommands.slice(0, 6).map((command) => (
                    <div key={`${selectedProfile?.id}-${command.label}`} className="grid gap-3 border-b border-white/10 bg-[#070b10]/70 p-4 last:border-b-0 md:grid-cols-[220px_minmax(0,1fr)_90px]">
                      <div className="font-medium text-white">{command.label}</div>
                      <code className="truncate rounded-xl bg-black/35 px-3 py-2 font-mono text-xs text-[#ffd7a3]">{command.value}</code>
                      <span className={`w-fit rounded-full border px-2 py-1 text-xs ${getActionTypeColor(command.type)}`}>{command.type}</span>
                    </div>
                  ))}
                </div>
              </section>
            </div>

            <aside id="safety" className="space-y-6">
              <CommandAudit commands={selectedCommands} />
              <SmartInstallButton selectedIds={selectedProfile?.id ? [selectedProfile.id] : []} />

              <section className="rounded-[28px] border border-white/10 bg-white/[0.045] p-5 backdrop-blur-xl">
                <p className="font-mono text-xs uppercase tracking-[0.22em] text-[#ffb067]">Selected profile</p>
                <h2 className="mt-2 text-xl font-semibold text-white">{selectedProfile?.name}</h2>
                <p className="mt-2 text-sm leading-6 text-zinc-400">{selectedProfile?.description}</p>
                <div className="mt-5 grid grid-cols-2 gap-3">
                  <div className="rounded-2xl bg-black/25 p-3"><div className="font-mono text-2xl text-white">{selectedCommands.length}</div><div className="text-xs text-zinc-500">Commands</div></div>
                  <div className="rounded-2xl bg-black/25 p-3"><div className="font-mono text-2xl text-white">{selectedRiskCount}</div><div className="text-xs text-zinc-500">Risk flags</div></div>
                </div>
                {firstCommand && (
                  <div className="mt-5 rounded-2xl border border-white/10 bg-black/25 p-3">
                    <div className="text-xs uppercase tracking-[0.18em] text-zinc-500">First action</div>
                    <div className="mt-2 text-sm font-medium text-white">{firstCommand.label}</div>
                    <code className="mt-2 block truncate font-mono text-xs text-[#9ff5df]">{firstCommand.value}</code>
                  </div>
                )}
                <a href={`/profiles/${selectedProfile?.id}.json`} download className="mt-5 inline-flex h-11 w-full items-center justify-center rounded-2xl border border-white/10 bg-white/6 text-sm font-semibold text-zinc-100 transition hover:border-[#00D4AA]/40 hover:bg-[#00D4AA]/10">Download profile JSON</a>
              </section>
            </aside>
          </section>

          <section id="docs" className="rounded-[34px] border border-white/10 bg-[#0b1118]/85 p-6 backdrop-blur-xl lg:p-8">
            <div className="grid gap-8 lg:grid-cols-[0.9fr_1.1fr]">
              <div>
                <p className="font-mono text-xs uppercase tracking-[0.22em] text-[#9ff5df]">File layout</p>
                <h2 className="mt-3 text-3xl font-semibold tracking-tight text-white">Copy one folder. Done.</h2>
                <p className="mt-3 text-sm leading-6 text-zinc-400">The ZIP contains the Flipper SD card structure, so installation is just qFlipper plus drag-and-drop. No firmware flash. No ritual sacrifice to build tools.</p>
              </div>
              <pre className="overflow-x-auto rounded-3xl border border-white/10 bg-black/35 p-5 font-mono text-sm leading-7 text-zinc-300">{`SD Card
` + `|-- apps_data
` + `    |-- flipdeck
` + `        |-- settings.json
` + `        |-- profiles
` + `        |   |-- git.json
` + `        |   |-- docker.json
` + `        |   |-- python.json
` + `        |-- snippets`}</pre>
            </div>
          </section>
        </div>
      </div>
    </main>
  );
}

