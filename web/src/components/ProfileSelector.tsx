"use client";

import { useMemo, useState } from "react";
import type { Profile } from "@/types/flipdeck";
import { getProfileCommands } from "@/lib/profiles";

interface ProfileSelectorProps {
  profiles: Profile[];
  selectedId: string;
  onSelect: (profileId: string) => void;
  onHover?: (profileId: string) => void;
  search?: boolean;
  filters?: string[];
  previewOnHover?: boolean;
}

function profileCategory(profile: Profile): string {
  const id = profile.id ?? "";
  if (["aws", "docker"].includes(id)) return "cloud";
  if (["system", "presentation"].includes(id)) return "system";
  return "dev";
}

function categoryTone(category: string, selected: boolean) {
  const tones: Record<string, string> = {
    dev: selected
      ? "border-[#00D4AA] bg-[#00D4AA]/15"
      : "border-[#00D4AA]/20 bg-[#00D4AA]/8 hover:border-[#00D4AA]/45",
    cloud: selected
      ? "border-sky-300 bg-sky-400/15"
      : "border-sky-400/20 bg-sky-400/8 hover:border-sky-300/45",
    system: selected
      ? "border-amber-300 bg-amber-300/15"
      : "border-amber-300/20 bg-amber-300/8 hover:border-amber-300/45",
  };
  return tones[category] ?? tones.dev;
}

export function ProfileSelector({
  profiles,
  selectedId,
  onSelect,
  onHover,
  search = true,
  filters = ["dev", "cloud", "system"],
  previewOnHover = true,
}: ProfileSelectorProps) {
  const [query, setQuery] = useState("");
  const [activeFilter, setActiveFilter] = useState("all");

  const filteredProfiles = useMemo(() => {
    const normalizedQuery = query.trim().toLowerCase();
    return profiles.filter((profile) => {
      const category = profileCategory(profile);
      const matchesFilter = activeFilter === "all" || category === activeFilter;
      const matchesQuery =
        !normalizedQuery ||
        profile.name.toLowerCase().includes(normalizedQuery) ||
        (profile.description ?? "").toLowerCase().includes(normalizedQuery);
      return matchesFilter && matchesQuery;
    });
  }, [activeFilter, profiles, query]);

  return (
    <section className="rounded-lg border border-[#00D4AA]/20 bg-[linear-gradient(180deg,rgba(0,212,170,0.12),rgba(15,23,42,0.62))] p-4 shadow-2xl shadow-black/20 backdrop-blur-md">
      <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
        <div>
          <h2 className="text-lg font-semibold text-white">Profiles</h2>
          <p className="text-sm text-slate-400">Choose the command sets to include.</p>
        </div>
        {search && (
          <input
            type="search"
            value={query}
            onChange={(event) => setQuery(event.target.value)}
            placeholder="Search"
            className="h-10 w-full rounded-md border border-white/10 bg-slate-950/70 px-3 text-sm text-white outline-none ring-0 placeholder:text-slate-500 focus:border-[#00D4AA] sm:w-56"
          />
        )}
      </div>

      <div className="mt-4 flex flex-wrap gap-2">
        {["all", ...filters].map((filter) => (
          <button
            key={filter}
            type="button"
            onClick={() => setActiveFilter(filter)}
            className={`h-9 rounded-md border px-3 text-sm capitalize transition ${
              activeFilter === filter
                ? "border-[#00D4AA] bg-[#00D4AA]/20 text-[#C7FFF1]"
                : "border-white/10 bg-slate-950/50 text-slate-300 hover:border-white/25"
            }`}
          >
            {filter}
          </button>
        ))}
      </div>

      <div className="mt-4 grid gap-3">
        {filteredProfiles.map((profile) => {
          const profileId = profile.id ?? profile.name.toLowerCase();
          const selected = selectedId === profileId;
          const category = profileCategory(profile);
          return (
            <button
              key={profileId}
              type="button"
              onClick={() => onSelect(profileId)}
              onMouseEnter={() => previewOnHover && onHover?.(profileId)}
              className={`grid min-h-24 grid-cols-[1fr_auto] gap-3 rounded-lg border p-4 text-left transition ${categoryTone(category, selected)}`}
            >
              <span>
                <span className="block font-semibold text-white">{profile.name}</span>
                <span className="mt-1 block text-sm leading-5 text-slate-400">
                  {profile.description}
                </span>
              </span>
              <span className="rounded-md border border-white/10 bg-slate-950/40 px-2 py-1 text-xs text-slate-200">
                {getProfileCommands(profile).length}
              </span>
            </button>
          );
        })}
      </div>
    </section>
  );
}
