"use client";

import { useState } from "react";
import { profileFiles } from "@/lib/profiles";
import { getActionTypeColor } from "@/lib/utils";

const installSteps = [
  {
    number: "1",
    title: "Plug In Your Flipper",
    description: "Connect via USB and unlock the device",
    icon: "🔌",
  },
  {
    number: "2",
    title: "Open qFlipper",
    description: "Browse to the SD card file explorer",
    icon: "📁",
  },
  {
    number: "3",
    title: "Download Pack",
    description: "Get the ready-to-copy ZIP bundle",
    icon: "📥",
  },
  {
    number: "4",
    title: "Drag & Drop",
    description: "Copy apps_data folder to SD card",
    icon: "✨",
  },
  {
    number: "5",
    title: "Launch",
    description: "Run FlipDeck and select a profile",
    icon: "▶️",
  },
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
    <main className="min-h-screen bg-white">
      {/* Hero Section */}
      <div className="relative overflow-hidden">
        {/* Gradient background */}
        <div className="absolute inset-0 bg-gradient-to-br from-slate-900 via-purple-900 to-slate-900" />
        <div className="absolute inset-0 opacity-40">
          <div className="absolute top-0 right-1/4 w-96 h-96 bg-purple-500 rounded-full filter blur-3xl opacity-20" />
          <div className="absolute bottom-0 left-1/4 w-96 h-96 bg-blue-500 rounded-full filter blur-3xl opacity-20" />
        </div>

        <div className="relative z-10 max-w-6xl mx-auto px-4 sm:px-6 lg:px-8 py-20 sm:py-32">
          <div className="grid lg:grid-cols-2 gap-12 items-center">
            <div>
              <div className="inline-block mb-4 px-4 py-2 bg-purple-500/20 border border-purple-400/40 rounded-full">
                <span className="text-purple-300 text-sm font-semibold">✨ USB Command Deck</span>
              </div>
              <h1 className="text-5xl sm:text-6xl lg:text-7xl font-black text-white mb-6 leading-tight">
                Transform Your <span className="bg-clip-text text-transparent bg-gradient-to-r from-purple-400 via-pink-400 to-purple-400">Flipper Zero</span>
              </h1>
              <p className="text-xl text-slate-300 mb-8 leading-relaxed">
                One drag-and-drop install. {profileFiles.length} ready-made profiles. {totalActions} powerful commands at your fingertips.
              </p>
              <div className="flex flex-col sm:flex-row gap-4">
                <a
                  href="/api/install-bundle/download"
                  className="group inline-flex items-center justify-center gap-2 px-8 py-4 bg-gradient-to-r from-purple-600 to-pink-600 hover:from-purple-700 hover:to-pink-700 text-white font-bold rounded-xl shadow-xl hover:shadow-2xl shadow-purple-900/50 transition-all duration-300 transform hover:scale-105"
                >
                  <span className="text-xl">📦</span>
                  Download Install Pack
                  <span className="group-hover:translate-x-1 transition-transform">→</span>
                </a>
                <a
                  href="#steps"
                  className="inline-flex items-center justify-center px-8 py-4 border-2 border-purple-400 text-purple-300 font-bold rounded-xl hover:bg-purple-500/10 transition-all duration-300"
                >
                  Learn More
                </a>
              </div>
            </div>

            {/* Feature cards */}
            <div className="grid grid-cols-2 gap-4">
              <div className="group p-6 bg-white/10 backdrop-blur-md border border-white/20 rounded-2xl hover:bg-white/20 hover:border-white/40 transition-all duration-300 hover:-translate-y-2">
                <div className="text-4xl mb-3">🎯</div>
                <h3 className="text-white font-bold mb-2">9 Profiles</h3>
                <p className="text-slate-400 text-sm">Git, Docker, Node, Python & more</p>
              </div>
              <div className="group p-6 bg-white/10 backdrop-blur-md border border-white/20 rounded-2xl hover:bg-white/20 hover:border-white/40 transition-all duration-300 hover:-translate-y-2">
                <div className="text-4xl mb-3">⚡</div>
                <h3 className="text-white font-bold mb-2">{totalActions} Actions</h3>
                <p className="text-slate-400 text-sm">Pre-built commands & shortcuts</p>
              </div>
              <div className="group p-6 bg-white/10 backdrop-blur-md border border-white/20 rounded-2xl hover:bg-white/20 hover:border-white/40 transition-all duration-300 hover:-translate-y-2">
                <div className="text-4xl mb-3">🔒</div>
                <h3 className="text-white font-bold mb-2">Safe by Default</h3>
                <p className="text-slate-400 text-sm">Confirmation required for all commands</p>
              </div>
              <div className="group p-6 bg-white/10 backdrop-blur-md border border-white/20 rounded-2xl hover:bg-white/20 hover:border-white/40 transition-all duration-300 hover:-translate-y-2">
                <div className="text-4xl mb-3">📱</div>
                <h3 className="text-white font-bold mb-2">No Firmware</h3>
                <p className="text-slate-400 text-sm">Works on stock Flipper Zero</p>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Installation Steps */}
      <section id="steps" className="py-20 sm:py-32 bg-slate-50">
        <div className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="text-center mb-16">
            <h2 className="text-4xl sm:text-5xl font-black text-slate-900 mb-4">
              Install in 5 Simple Steps
            </h2>
            <p className="text-xl text-slate-600">From zero to FlipDeck in minutes</p>
          </div>

          <div className="grid md:grid-cols-5 gap-6">
            {installSteps.map((step, idx) => (
              <div
                key={idx}
                className="group relative p-8 bg-white rounded-2xl shadow-lg hover:shadow-2xl transition-all duration-300 hover:-translate-y-2 border border-slate-200 hover:border-purple-300"
              >
                {/* Number badge */}
                <div className="absolute -top-4 -right-4 w-12 h-12 bg-gradient-to-br from-purple-600 to-pink-600 rounded-full flex items-center justify-center text-white font-bold text-lg shadow-lg">
                  {step.number}
                </div>

                <div className="text-5xl mb-4">{step.icon}</div>
                <h3 className="text-lg font-bold text-slate-900 mb-2">{step.title}</h3>
                <p className="text-slate-600 text-sm">{step.description}</p>

                {idx < installSteps.length - 1 && (
                  <div className="hidden md:block absolute -right-3 top-1/2 -translate-y-1/2 w-6 h-0.5 bg-gradient-to-r from-purple-400 to-transparent" />
                )}
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* Profiles Section */}
      <section className="py-20 sm:py-32 bg-white">
        <div className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="mb-12">
            <h2 className="text-4xl sm:text-5xl font-black text-slate-900 mb-4">
              Choose Your Profiles
            </h2>
            <p className="text-xl text-slate-600 mb-8">
              Download the full pack or customize your selection
            </p>

            {/* Search bar */}
            <div className="relative max-w-2xl">
              <div className="absolute inset-y-0 left-0 pl-4 flex items-center pointer-events-none">
                <span className="text-2xl">🔍</span>
              </div>
              <input
                type="search"
                placeholder="Search profiles..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="w-full pl-14 pr-6 py-4 border-2 border-slate-300 rounded-xl focus:outline-none focus:border-purple-600 focus:ring-4 focus:ring-purple-200 text-lg transition-all duration-200"
              />
            </div>
          </div>

          {/* Profile cards grid */}
          <div className="grid md:grid-cols-2 lg:grid-cols-3 gap-6">
            {filteredProfiles.length > 0 ? (
              filteredProfiles.map(({ fileName, profile }) => (
                <div
                  key={profile.id}
                  className="group h-full flex flex-col p-8 bg-gradient-to-br from-slate-50 to-slate-100 rounded-2xl border-2 border-slate-200 hover:border-purple-400 shadow-md hover:shadow-xl transition-all duration-300 hover:-translate-y-1"
                >
                  <div className="mb-4">
                    <div className="inline-block px-3 py-1 bg-purple-100 text-purple-700 rounded-full text-xs font-bold mb-2">
                      {fileName}
                    </div>
                    <h3 className="text-2xl font-bold text-slate-900 group-hover:text-purple-600 transition-colors">
                      {profile.name}
                    </h3>
                  </div>

                  <p className="text-slate-600 mb-6 flex-grow">{profile.description}</p>

                  <div className="mb-6 pt-6 border-t border-slate-300">
                    <span className="text-sm font-semibold text-slate-500">
                      {profile.actions.length} commands
                    </span>
                  </div>

                  {/* Action types */}
                  <div className="space-y-2 mb-6 max-h-64 overflow-y-auto">
                    {profile.actions.map((action) => (
                      <div
                        key={`${profile.id}-${action.label}`}
                        className="flex items-center justify-between p-3 bg-white rounded-lg border border-slate-200 group-hover:border-purple-200 transition-colors"
                      >
                        <span className="text-sm font-medium text-slate-700">
                          {action.label}
                        </span>
                        <span
                          className={`px-2 py-1 text-xs font-bold rounded-full ${getActionTypeColor(
                            action.type
                          )}`}
                        >
                          {action.type}
                        </span>
                      </div>
                    ))}
                  </div>

                  <a
                    href={`/profiles/${profile.id}.json`}
                    download
                    className="w-full px-4 py-3 bg-gradient-to-r from-purple-600 to-pink-600 text-white font-bold rounded-lg hover:from-purple-700 hover:to-pink-700 shadow-md hover:shadow-lg transition-all duration-300 text-center"
                  >
                    Download JSON
                  </a>
                </div>
              ))
            ) : (
              <div className="col-span-full py-12 text-center">
                <p className="text-xl text-slate-500">
                  No profiles match "{searchQuery}"
                </p>
              </div>
            )}
          </div>

          {/* Bulk download */}
          <div className="mt-12 text-center">
            <a
              href="/api/install-bundle/download"
              className="inline-flex items-center justify-center gap-2 px-8 py-4 bg-slate-900 hover:bg-slate-800 text-white font-bold rounded-xl shadow-lg hover:shadow-xl transition-all duration-300"
            >
              <span>📦</span>
              Download All Profiles
            </a>
          </div>
        </div>
      </section>

      {/* Safety Section */}
      <section className="py-20 sm:py-32 bg-gradient-to-br from-slate-900 to-slate-800">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 text-center">
          <h2 className="text-4xl sm:text-5xl font-black text-white mb-8">
            Built With Safety In Mind
          </h2>
          <p className="text-xl text-slate-300 mb-12 max-w-2xl mx-auto">
            FlipDeck is transparent by design. All commands are visible, reviewable, and require explicit confirmation before execution.
          </p>

          <div className="grid md:grid-cols-3 gap-8">
            {[
              {
                icon: "👁️",
                title: "Visible Commands",
                desc: "All profiles are plain JSON. Inspect before you run.",
              },
              {
                icon: "✅",
                title: "Confirmation Required",
                desc: "Every action needs your approval on the device.",
              },
              {
                icon: "🛡️",
                title: "No Hidden Payloads",
                desc: "No auto-execution, no background processes, no secrets.",
              },
            ].map((item, idx) => (
              <div
                key={idx}
                className="p-8 bg-white/10 backdrop-blur-md border border-white/20 rounded-2xl hover:bg-white/20 transition-all duration-300"
              >
                <div className="text-5xl mb-4">{item.icon}</div>
                <h3 className="text-xl font-bold text-white mb-2">{item.title}</h3>
                <p className="text-slate-300">{item.desc}</p>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* CTA Footer */}
      <section className="py-20 bg-white">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 text-center">
          <h2 className="text-4xl font-black text-slate-900 mb-6">
            Ready to supercharge your Flipper?
          </h2>
          <a
            href="/api/install-bundle/download"
            className="inline-flex items-center justify-center gap-2 px-10 py-5 bg-gradient-to-r from-purple-600 to-pink-600 hover:from-purple-700 hover:to-pink-700 text-white font-bold text-lg rounded-xl shadow-xl hover:shadow-2xl shadow-purple-900/50 transition-all duration-300 transform hover:scale-105"
          >
            <span className="text-2xl">🚀</span>
            Get Started Now
          </a>
        </div>
      </section>
    </main>
  );
}
