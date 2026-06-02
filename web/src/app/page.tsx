"use client";

import { useState } from "react";
import { profileFiles } from "@/lib/profiles";
import { getActionTypeColor } from "@/lib/utils";

const navItems = [
  { label: "Install", href: "#install" },
  { label: "Profiles", href: "#profiles" },
  { label: "Safety", href: "#safety" },
];

const installSteps = [
  {
    eyebrow: "Connect",
    title: "Plug In Your Flipper",
    body: "Use USB, unlock the device, and open qFlipper on your computer.",
  },
  {
    eyebrow: "Browse",
    title: "Open the SD Card",
    body: "In qFlipper, open the file browser so the SD card root is visible.",
  },
  {
    eyebrow: "Download",
    title: "Grab the Install Pack",
    body: "Download one ZIP with the exact apps_data/flipdeck layout already prepared.",
  },
  {
    eyebrow: "Copy",
    title: "Drag apps_data Over",
    body: "Drop apps_data onto the SD card root and merge or replace FlipDeck files if prompted.",
  },
  {
    eyebrow: "Launch",
    title: "Open FlipDeck",
    body: "On the Flipper, open FlipDeck, pick a profile, review the action, and press OK.",
  },
];

const featureCards = [
  {
    title: "Drag-and-Drop Install",
    body: "No firmware build, no terminal, no manual JSON juggling. Copy one folder and launch.",
  },
  {
    title: "Ready Profiles",
    body: "Git, Node.js, Python, Docker, VSCode, snippets, presentation controls, and more.",
  },
  {
    title: "Visible by Design",
    body: "Profiles are plain JSON, commands are reviewable, and text actions require confirmation.",
  },
];

const quickChecks = [
  "No custom firmware required for the bundled profiles.",
  "Commands require confirmation before text is sent.",
  "Profiles live at /apps_data/flipdeck/profiles/ on the SD card.",
];

const safetyItems = [
  "Bundled commands are plain text and easy to inspect.",
  "Confirmation stays on for command-style actions.",
  "Use profiles only on computers you own or administer.",
];

export default function Home() {
  const [searchQuery, setSearchQuery] = useState("");

  const totalActions = profileFiles.reduce(
    (sum, { profile }) => sum + profile.actions.length,
    0
  );

  const filteredProfiles = profileFiles.filter(({ profile }) =>
    profile.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    profile.description.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <main
      id="main-content"
      className="relative min-h-screen overflow-x-hidden bg-[#08090f] text-white"
    >
      <div className="pointer-events-none absolute inset-0 -z-0">
        <div className="absolute left-1/2 top-[-18rem] h-[36rem] w-[36rem] -translate-x-1/2 rounded-full bg-orange-500/25 blur-3xl" />
        <div className="absolute right-[-12rem] top-56 h-[30rem] w-[30rem] rounded-full bg-fuchsia-500/20 blur-3xl" />
        <div className="absolute bottom-[-14rem] left-[-10rem] h-[34rem] w-[34rem] rounded-full bg-cyan-500/15 blur-3xl" />
        <div className="absolute inset-0 bg-[radial-gradient(circle_at_top,rgba(255,255,255,0.10),transparent_32rem)]" />
      </div>

      <a
        href="#main-content"
        className="sr-only focus:not-sr-only focus:fixed focus:left-4 focus:top-4 focus:z-50 focus:rounded-full focus:bg-white focus:px-4 focus:py-2 focus:text-sm focus:font-bold focus:text-zinc-950 focus-visible:ring-4 focus-visible:ring-orange-300"
      >
        Skip to Content
      </a>

      <div className="relative z-10 mx-auto grid min-h-screen w-full max-w-7xl gap-8 px-4 py-4 sm:px-6 lg:grid-cols-[17rem_1fr] lg:px-8">
        <aside className="hidden lg:sticky lg:top-4 lg:flex lg:h-[calc(100vh-2rem)] lg:flex-col lg:justify-between lg:rounded-[2rem] lg:border lg:border-white/10 lg:bg-white/[0.055] lg:p-5 lg:shadow-2xl lg:shadow-black/30 lg:backdrop-blur-xl">
          <div>
            <a
              href="#main-content"
              className="group flex items-center gap-3 rounded-2xl p-2 focus-visible:ring-4 focus-visible:ring-orange-300 transition-transform hover:-translate-y-0.5"
            >
              <span className="grid h-12 w-12 place-items-center rounded-2xl bg-gradient-to-br from-orange-400 to-pink-500 text-xl font-black shadow-lg shadow-orange-500/25">
                F
              </span>
              <span>
                <span className="block text-lg font-black tracking-tight">
                  FlipDeck
                </span>
                <span className="block text-xs font-medium text-zinc-400">
                  USB Command Deck
                </span>
              </span>
            </a>

            <nav className="mt-8" aria-label="Main navigation">
              <p className="px-3 text-xs font-bold uppercase tracking-[0.25em] text-zinc-500">
                Main Navigation
              </p>
              <div className="mt-3 space-y-2">
                {navItems.map((item) => (
                  <a
                    key={item.href}
                    href={item.href}
                    className="flex items-center justify-between rounded-2xl px-4 py-3 text-sm font-bold text-zinc-300 transition-colors duration-200 hover:bg-white/10 hover:text-white focus-visible:ring-4 focus-visible:ring-orange-300"
                  >
                    {item.label}
                    <span aria-hidden="true" className="text-orange-300">
                      -&gt;
                    </span>
                  </a>
                ))}
              </div>
            </nav>
          </div>

          <div className="rounded-3xl border border-orange-300/20 bg-orange-400/10 p-4">
            <p className="text-sm font-bold text-orange-100">Railway Ready</p>
            <p className="mt-2 text-sm leading-6 text-zinc-300">
              Deploy from the repo root. The app builds from <code>web/</code>
              and serves the standalone Next.js bundle.
            </p>
          </div>
        </aside>

        <div className="space-y-8 pb-12">
          <header className="flex items-center justify-between rounded-[1.5rem] border border-white/10 bg-white/[0.055] px-4 py-3 backdrop-blur-xl lg:hidden">
            <a
              href="#main-content"
              className="flex items-center gap-3 focus-visible:ring-4 focus-visible:ring-orange-300"
            >
              <span className="grid h-10 w-10 place-items-center rounded-xl bg-gradient-to-br from-orange-400 to-pink-500 font-black">
                F
              </span>
              <span className="font-black">FlipDeck</span>
            </a>
            <a
              href="/api/install-bundle/download"
              className="rounded-full bg-white px-4 py-2 text-sm font-black text-zinc-950 transition-colors duration-200 hover:bg-orange-100 focus-visible:ring-4 focus-visible:ring-orange-300"
            >
              Install
            </a>
          </header>

          <section className="relative overflow-hidden rounded-[2.5rem] border border-white/10 bg-white/[0.065] p-5 shadow-2xl shadow-black/30 backdrop-blur-xl sm:p-8 xl:p-10 transition-all duration-300 hover:shadow-xl hover:shadow-orange-500/10">
            <div className="absolute right-10 top-10 hidden rounded-full border border-white/10 bg-white/10 px-3 py-1 text-xs font-bold text-orange-100 sm:block">
              Stock Flipper Friendly
            </div>

            <div className="grid gap-10 xl:grid-cols-[1fr_27rem] xl:items-center">
              <div className="max-w-3xl">
                <p className="inline-flex rounded-full border border-orange-300/25 bg-orange-400/10 px-3 py-1 text-sm font-bold text-orange-100 shadow-lg shadow-orange-950/20">
                  Flipper Zero Install Pack
                </p>
                <h1 className="mt-6 text-balance text-5xl font-black tracking-tight text-white sm:text-7xl lg:text-8xl bg-clip-text bg-gradient-to-r from-orange-400 to-pink-500 text-transparent">
                  Make FlipDeck Feel One-Click.
                </h1>
                <p className="mt-6 max-w-2xl text-pretty text-lg leading-8 text-zinc-300 sm:text-xl">
                  Give Flipper users a simple path: plug in, open qFlipper,
                  download one pack, drag one folder, and start using {" "}
                  {profileFiles.length} profiles with {totalActions} actions.
                </p>

                <div className="mt-8 flex flex-col gap-3 sm:flex-row">
                  <a
                    href="/api/install-bundle/download"
                    className="group inline-flex items-center justify-center gap-3 rounded-2xl bg-gradient-to-r from-orange-400 via-orange-500 to-pink-500 px-6 py-4 text-base font-black text-white shadow-2xl shadow-orange-500/25 transition-all duration-200 hover:-translate-y-0.5 hover:shadow-2xl hover:shadow-orange-500/40 focus-visible:ring-4 focus-visible:ring-orange-300"
                  >
                    Download Flipper Install Pack
                    <span
                      aria-hidden="true"
                      className="transition-transform duration-200 group-hover:translate-x-1"
                    >
                      -&gt;
                    </span>
                  </a>
                  <a
                    href="#install"
                    className="inline-flex items-center justify-center rounded-2xl border border-white/15 bg-white/10 px-6 py-4 text-base font-black text-white transition-all duration-200 hover:bg-white/15 hover:-translate-y-0.5 focus-visible:ring-4 focus-visible:ring-orange-300"
                  >
                    View Install Steps
                  </a>
                </div>

                <dl className="mt-10 grid max-w-2xl grid-cols-3 gap-3">
                  <div className="rounded-2xl border border-white/10 bg-black/20 p-4 transition-all duration-200 hover:bg-black/30 hover:border-white/20">
                    <dt className="text-xs font-bold uppercase tracking-widest text-zinc-500">
                      Profiles
                    </dt>
                    <dd className="mt-2 text-3xl font-black tabular-nums">
                      {profileFiles.length}
                    </dd>
                  </div>
                  <div className="rounded-2xl border border-white/10 bg-black/20 p-4 transition-all duration-200 hover:bg-black/30 hover:border-white/20">
                    <dt className="text-xs font-bold uppercase tracking-widest text-zinc-500">
                      Actions
                    </dt>
                    <dd className="mt-2 text-3xl font-black tabular-nums">
                      {totalActions}
                    </dd>
                  </div>
                  <div className="rounded-2xl border border-white/10 bg-black/20 p-4 transition-all duration-200 hover:bg-black/30 hover:border-white/20">
                    <dt className="text-xs font-bold uppercase tracking-widest text-zinc-500">
                      Firmware
                    </dt>
                    <dd className="mt-2 text-3xl font-black">Stock</dd>
                  </div>
                </dl>
              </div>

              <div className="rounded-3xl bg-zinc-950 p-5 text-zinc-100 shadow-2xl dark:bg-black border border-white/10">
                <div className="mb-4 flex items-center gap-2">
                  <span className="h-3 w-3 rounded-full bg-red-400" />
                  <span className="h-3 w-3 rounded-full bg-yellow-400" />
                  <span className="h-3 w-3 rounded-full bg-green-400" />
                  <span className="ml-2 text-sm text-zinc-400">
                    SD card layout
                  </span>
                </div>
                <pre className="overflow-x-auto rounded-2xl bg-black/50 p-4 text-sm leading-7 text-orange-100">
{`/apps_data/flipdeck/
├── settings.json
├── profiles/
│   ├── git.json
│   ├── node.json
│   ├── python.json
│   └── ...
└── snippets/
    ├── react_component.txt
    └── debug_log.txt`}
                </pre>
                <ul className="mt-5 space-y-3">
                  {quickChecks.map((check) => (
                    <li key={check} className="flex gap-3 text-sm text-zinc-300">
                      <span className="mt-0.5 text-orange-400">✓</span>
                      <span>{check}</span>
                    </li>
                  ))}
                </ul>
              </div>
            </div>
          </section>

          <section className="grid gap-4 md:grid-cols-3" aria-label="Highlights">
            {featureCards.map((card) => (
              <article
                key={card.title}
                className="rounded-[2rem] border border-white/10 bg-white/[0.055] p-6 shadow-xl shadow-black/20 backdrop-blur-xl transition-all duration-200 hover:bg-white/[0.075] hover:border-white/20 hover:-translate-y-1"
              >
                <h2 className="text-xl font-black text-white">{card.title}</h2>
                <p className="mt-3 text-pretty leading-7 text-zinc-300">
                  {card.body}
                </p>
              </article>
            ))}
          </section>

          <section
            id="install"
            className="scroll-mt-6 rounded-[2.5rem] border border-white/10 bg-white/[0.065] p-5 shadow-2xl shadow-black/25 backdrop-blur-xl sm:p-8"
          >
            <div className="flex flex-col gap-4 md:flex-row md:items-end md:justify-between">
              <div>
                <p className="text-sm font-black uppercase tracking-[0.25em] text-orange-300">
                  Web Installer
                </p>
                <h2 className="mt-3 text-balance text-4xl font-black text-white sm:text-5xl">
                  Start When Your Flipper Is Plugged In
                </h2>
              </div>
              <a
                href="/api/install-bundle/download"
                className="rounded-2xl bg-white px-5 py-3 text-center text-sm font-black text-zinc-950 transition-all duration-200 hover:bg-orange-100 hover:-translate-y-0.5 focus-visible:ring-4 focus-visible:ring-orange-300"
              >
                Download Ready-to-Copy ZIP
              </a>
            </div>

            <ol className="mt-8 grid gap-4 md:grid-cols-2 xl:grid-cols-5">
              {installSteps.map((step, index) => (
                <li
                  key={step.title}
                  className="group rounded-[2rem] border border-white/10 bg-black/20 p-5 transition-all duration-200 hover:-translate-y-1 hover:border-orange-300/50 hover:bg-black/30"
                >
                  <div className="mb-8 flex items-center justify-between">
                    <span className="rounded-full bg-orange-400/10 px-3 py-1 text-xs font-black uppercase tracking-widest text-orange-200">
                      {step.eyebrow}
                    </span>
                    <span className="grid h-10 w-10 place-items-center rounded-full bg-white/10 text-sm font-black text-white tabular-nums group-hover:bg-orange-400/20 group-hover:text-orange-200 transition-colors duration-200">
                      {index + 1}
                    </span>
                  </div>
                  <h3 className="text-2xl font-black text-white">
                    {step.title}
                  </h3>
                  <p className="mt-3 text-pretty leading-7 text-zinc-300">
                    {step.body}
                  </p>
                </li>
              ))}
            </ol>
          </section>

          <section
            id="profiles"
            className="scroll-mt-6 rounded-[2.5rem] border border-white/10 bg-white/[0.065] p-5 shadow-2xl shadow-black/25 backdrop-blur-xl sm:p-8"
          >
            <div className="mb-8 flex flex-col gap-4 md:flex-row md:items-end md:justify-between">
              <div>
                <p className="text-sm font-black uppercase tracking-[0.25em] text-orange-300">
                  Included Profiles
                </p>
                <h2 className="mt-3 text-balance text-4xl font-black text-white sm:text-5xl">
                  Command Sets That Are Ready to Copy
                </h2>
              </div>
              <a
                href="/api/profiles/download"
                className="rounded-2xl border border-white/15 bg-white/10 px-5 py-3 text-center text-sm font-black text-white transition-all duration-200 hover:bg-white/15 hover:-translate-y-0.5 focus-visible:ring-4 focus-visible:ring-orange-300"
              >
                Download Profiles Only
              </a>
            </div>

            {/* Search Bar */}
            <div className="mb-8">
              <input
                type="search"
                placeholder="Search profiles by name or description..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="w-full px-5 py-3 rounded-2xl border border-white/15 bg-white/[0.055] text-white placeholder-zinc-400 focus:outline-none focus:ring-4 focus:ring-orange-300/50 focus:border-orange-300/50 backdrop-blur-xl transition-all duration-200"
              />
            </div>

            <div className="grid w-full grid-cols-1 gap-5 md:grid-cols-2 xl:grid-cols-3">
              {filteredProfiles.length > 0 ? (
                filteredProfiles.map(({ fileName, profile }) => (
                  <article
                    key={profile.id}
                    className="min-w-0 rounded-[2rem] border border-white/10 bg-black/20 p-5 transition-all duration-200 hover:-translate-y-1 hover:border-orange-300/50 hover:bg-black/30"
                  >
                    <div className="mb-4 flex min-w-0 items-center justify-between gap-3">
                      <h3 className="truncate text-2xl font-black text-white">
                        {profile.name}
                      </h3>
                      <span
                        translate="no"
                        className="shrink-0 rounded-full bg-white/10 px-3 py-1 text-xs font-bold text-zinc-300"
                      >
                        {fileName}
                      </span>
                    </div>
                    <p className="mb-5 min-h-12 text-pretty text-sm leading-6 text-zinc-300">
                      {profile.description}
                    </p>
                    <div className="mb-4 flex items-center justify-between">
                      <span className="text-xs font-black uppercase tracking-widest text-zinc-500">
                        {profile.actions.length} Actions
                      </span>
                      <a
                        href={`/profiles/${profile.id}.json`}
                        download
                        className="rounded-full bg-orange-400/10 px-3 py-1 text-xs font-black text-orange-200 transition-all duration-200 hover:bg-orange-400/20 focus-visible:ring-4 focus-visible:ring-orange-300"
                      >
                        Download JSON
                      </a>
                    </div>
                    <div className="max-h-48 space-y-2 overflow-y-auto pr-1 [content-visibility:auto]">
                      {profile.actions.map((action) => (
                        <div
                          key={`${profile.id}-${action.label}`}
                          className="flex min-w-0 items-center justify-between gap-3 rounded-2xl bg-white/[0.055] p-3 transition-colors duration-200 hover:bg-white/[0.075]"
                        >
                          <span className="min-w-0 truncate text-sm font-semibold text-zinc-200">
                            {action.label}
                          </span>
                          <span
                            translate="no"
                            className={`shrink-0 rounded-full px-2 py-0.5 text-xs font-bold ${getActionTypeColor(
                              action.type
                            )}`}
                          >
                            {action.type}
                          </span>
                        </div>
                      ))}
                    </div>
                  </article>
                ))
              ) : (
                <div className="col-span-full flex items-center justify-center py-12">
                  <p className="text-lg text-zinc-400">
                    No profiles match "{searchQuery}"
                  </p>
                </div>
              )}
            </div>
          </section>

          <section
            id="safety"
            className="scroll-mt-6 rounded-[2.5rem] border border-orange-300/20 bg-orange-400/10 p-6 shadow-2xl shadow-black/20 backdrop-blur-xl sm:p-8 transition-all duration-200 hover:shadow-xl hover:shadow-orange-500/10"
          >
            <p className="text-sm font-black uppercase tracking-[0.25em] text-orange-200">
              Safety First
            </p>
            <h2 className="mt-3 text-balance text-4xl font-black text-white">
              FlipDeck Should Never Feel Like a Hidden Payload.
            </h2>
            <ul className="mt-6 grid gap-3 md:grid-cols-3">
              {safetyItems.map((item) => (
                <li
                  key={item}
                  className="flex gap-3 rounded-2xl border border-white/10 bg-black/20 p-4 text-sm leading-6 text-zinc-200 transition-all duration-200 hover:bg-black/30 hover:border-orange-300/50"
                >
                  <span aria-hidden="true" className="text-orange-300 flex-shrink-0">
                    ✓
                  </span>
                  <span>{item}</span>
                </li>
              ))}
            </ul>
          </section>
        </div>
      </div>
    </main>
  );
}
