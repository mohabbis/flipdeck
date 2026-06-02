import { profileFiles } from "@/lib/profiles";
import { getActionTypeColor } from "@/lib/utils";

const installSteps = [
  {
    title: "1. Plug in your Flipper Zero",
    body: "Use the USB cable, unlock the Flipper, and open the official qFlipper app on your computer.",
  },
  {
    title: "2. Open the SD card",
    body: "In qFlipper, choose the file browser / SD card view so you can copy files onto the Flipper.",
  },
  {
    title: "3. Download this ready-to-copy pack",
    body: "Grab the ZIP below. It already contains the exact apps_data/flipdeck folders your Flipper needs.",
  },
  {
    title: "4. Drag apps_data onto the SD card",
    body: "Merge it with the existing apps_data folder. When prompted, keep/replace the FlipDeck files from this pack.",
  },
  {
    title: "5. Launch FlipDeck",
    body: "On the Flipper, go to Apps → GPIO / USB or Apps → Tools, open FlipDeck, pick a profile, and press OK.",
  },
];

const quickChecks = [
  "No custom firmware required for the bundled profiles.",
  "Commands require confirmation before text is sent.",
  "Profiles live at /apps_data/flipdeck/profiles/ on the SD card.",
];

export default function Home() {
  const totalActions = profileFiles.reduce(
    (sum, { profile }) => sum + profile.actions.length,
    0
  );

  return (
    <main className="min-h-screen bg-orange-50 text-zinc-950 dark:bg-zinc-950 dark:text-zinc-50">
      <section className="mx-auto flex w-full max-w-6xl flex-col gap-10 px-4 py-8 sm:px-6 lg:px-8">
        <div className="rounded-[2rem] border border-orange-200 bg-white p-6 shadow-xl shadow-orange-950/5 dark:border-orange-900/60 dark:bg-zinc-900 sm:p-10">
          <div className="grid gap-8 lg:grid-cols-[1.05fr_0.95fr] lg:items-center">
            <div>
              <p className="mb-4 inline-flex rounded-full bg-orange-100 px-3 py-1 text-sm font-semibold text-orange-800 dark:bg-orange-950 dark:text-orange-200">
                Flipper Zero setup made easy
              </p>
              <h1 className="text-4xl font-black tracking-tight text-zinc-950 dark:text-white sm:text-6xl">
                Install FlipDeck in one SD card drag-and-drop.
              </h1>
              <p className="mt-5 max-w-2xl text-lg leading-8 text-zinc-700 dark:text-zinc-300">
                Plug in your Flipper, open qFlipper, download the install pack,
                and copy one folder. The pack includes {profileFiles.length} default
                profiles with {totalActions} ready-to-use commands and shortcuts.
              </p>
              <div className="mt-8 flex flex-col gap-3 sm:flex-row">
                <a
                  href="/api/install-bundle/download"
                  className="inline-flex items-center justify-center rounded-xl bg-orange-600 px-6 py-3 text-base font-bold text-white shadow-lg shadow-orange-600/25 transition hover:bg-orange-700 focus:outline-none focus:ring-4 focus:ring-orange-300"
                >
                  Download Flipper install pack
                </a>
                <a
                  href="#install"
                  className="inline-flex items-center justify-center rounded-xl border border-zinc-300 bg-white px-6 py-3 text-base font-bold text-zinc-800 transition hover:bg-zinc-100 focus:outline-none focus:ring-4 focus:ring-zinc-200 dark:border-zinc-700 dark:bg-zinc-900 dark:text-zinc-100 dark:hover:bg-zinc-800"
                >
                  Show step-by-step guide
                </a>
              </div>
            </div>

            <div className="rounded-3xl bg-zinc-950 p-5 text-zinc-100 shadow-2xl dark:bg-black">
              <div className="mb-4 flex items-center gap-2">
                <span className="h-3 w-3 rounded-full bg-red-400" />
                <span className="h-3 w-3 rounded-full bg-yellow-400" />
                <span className="h-3 w-3 rounded-full bg-green-400" />
                <span className="ml-2 text-sm text-zinc-400">SD card layout</span>
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
        </div>

        <section
          id="install"
          className="rounded-[2rem] border border-orange-200 bg-white p-6 shadow-lg shadow-orange-950/5 dark:border-orange-900/60 dark:bg-zinc-900 sm:p-8"
        >
          <div className="flex flex-col gap-4 md:flex-row md:items-end md:justify-between">
            <div>
              <p className="text-sm font-bold uppercase tracking-widest text-orange-700 dark:text-orange-300">
                Beginner install guide
              </p>
              <h2 className="mt-2 text-3xl font-black text-zinc-950 dark:text-white">
                Start when your Flipper is plugged in
              </h2>
            </div>
            <a
              href="/api/install-bundle/download"
              className="rounded-xl bg-zinc-950 px-5 py-3 text-center text-sm font-bold text-white transition hover:bg-zinc-800 dark:bg-white dark:text-zinc-950 dark:hover:bg-zinc-200"
            >
              Download the ready-to-copy ZIP
            </a>
          </div>

          <ol className="mt-8 grid gap-4 md:grid-cols-2 lg:grid-cols-5">
            {installSteps.map((step) => (
              <li
                key={step.title}
                className="rounded-2xl border border-zinc-200 bg-orange-50 p-4 dark:border-zinc-700 dark:bg-zinc-950"
              >
                <h3 className="font-bold text-zinc-950 dark:text-white">
                  {step.title}
                </h3>
                <p className="mt-2 text-sm leading-6 text-zinc-700 dark:text-zinc-300">
                  {step.body}
                </p>
              </li>
            ))}
          </ol>

          <div className="mt-8 rounded-2xl border border-amber-200 bg-amber-50 p-5 text-sm leading-6 text-amber-950 dark:border-amber-900 dark:bg-amber-950/40 dark:text-amber-100">
            <strong>Important:</strong> FlipDeck is intentionally visible and
            confirmation-based. Review each command before sending it, and only
            use profiles you trust on computers you own or administer.
          </div>
        </section>

        <section className="rounded-[2rem] border border-zinc-200 bg-white p-6 shadow-lg shadow-orange-950/5 dark:border-zinc-800 dark:bg-zinc-900 sm:p-8">
          <div className="mb-6 flex flex-col gap-4 md:flex-row md:items-end md:justify-between">
            <div>
              <p className="text-sm font-bold uppercase tracking-widest text-orange-700 dark:text-orange-300">
                Included profiles
              </p>
              <h2 className="mt-2 text-3xl font-black text-zinc-950 dark:text-white">
                Download everything or just one profile
              </h2>
            </div>
            <a
              href="/api/profiles/download"
              className="rounded-xl border border-orange-200 bg-orange-100 px-5 py-3 text-center text-sm font-bold text-orange-900 transition hover:bg-orange-200 dark:border-orange-900 dark:bg-orange-950 dark:text-orange-100 dark:hover:bg-orange-900"
            >
              Download profiles only
            </a>
          </div>

          <div className="grid w-full grid-cols-1 gap-5 md:grid-cols-2 lg:grid-cols-3">
            {profileFiles.map(({ fileName, profile }) => (
              <article
                key={profile.id}
                className="rounded-2xl border border-zinc-200 p-4 transition hover:-translate-y-0.5 hover:shadow-md dark:border-zinc-700"
              >
                <div className="mb-3 flex items-center justify-between gap-3">
                  <h3 className="text-xl font-bold text-zinc-900 dark:text-zinc-100">
                    {profile.name}
                  </h3>
                  <span className="rounded-full bg-zinc-100 px-2 py-1 text-xs font-semibold text-zinc-600 dark:bg-zinc-800 dark:text-zinc-300">
                    {fileName}
                  </span>
                </div>
                <p className="mb-4 text-sm text-zinc-600 dark:text-zinc-400">
                  {profile.description}
                </p>
                <div className="mb-3 flex items-center justify-between">
                  <span className="text-xs font-semibold text-zinc-500 dark:text-zinc-400">
                    {profile.actions.length} actions
                  </span>
                  <a
                    href={`/profiles/${profile.id}.json`}
                    download
                    className="rounded-lg bg-orange-100 px-3 py-1 text-xs font-bold text-orange-800 transition hover:bg-orange-200 dark:bg-orange-950 dark:text-orange-100 dark:hover:bg-orange-900"
                  >
                    Download JSON
                  </a>
                </div>
                <div className="max-h-48 space-y-2 overflow-y-auto pr-1">
                  {profile.actions.map((action) => (
                    <div
                      key={`${profile.id}-${action.label}`}
                      className="flex items-center justify-between gap-3 rounded-xl bg-zinc-50 p-2 dark:bg-zinc-800"
                    >
                      <span className="text-sm font-medium text-zinc-700 dark:text-zinc-300">
                        {action.label}
                      </span>
                      <span
                        className={`rounded-full px-2 py-0.5 text-xs font-semibold ${getActionTypeColor(
                          action.type
                        )}`}
                      >
                        {action.type}
                      </span>
                    </div>
                  ))}
                </div>
              </article>
            ))}
          </div>
        </section>
      </section>
    </main>
  );
}
