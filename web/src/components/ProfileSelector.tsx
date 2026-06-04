"use client";

import { useMemo, useState } from "react";
import type { Profile } from "@/types/flipdeck";
import { getProfileCommands } from "@/lib/profiles";

interface ProfileSelectorProps {
  profiles: Profile[];
  activeId: string;
  selectedIds: string[];
  onToggle: (profileId: string) => void;
  onPreview?: (profileId: string) => void;
  onSelectAll?: () => void;
  onClear?: () => void;
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
      ? "border-[#009f84] bg-[#e7fbf5]"
      : "border-black/10 bg-white hover:border-[#009f84]/45",
    cloud: selected
      ? "border-[#2563eb] bg-[#eff6ff]"
      : "border-black/10 bg-white hover:border-[#2563eb]/45",
    system: selected
      ? "border-[#ff7a00] bg-[#fff3df]"
      : "border-black/10 bg-white hover:border-[#ff7a00]/45",
  };
  return tones[category] ?? tones.dev;
}

export function ProfileSelector({
  profiles,
  activeId,
  selectedIds,
  onToggle,
  onPreview,
  onSelectAll,
  onClear,
  search = true,
  filters = ["dev", "cloud", "system"],
  previewOnHover = true,
}: ProfileSelectorProps) {
  const [query, setQuery] = useState("");
  const [activeFilter, setActiveFilter] = useState("all");
  const selectedIdSet = useMemo(() => new Set(selectedIds), [selectedIds]);

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
    <section className="rounded-lg border border-black/10 bg-white p-4 shadow-sm">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
        <div>
          <h2 className="text-lg font-semibold text-[#171717]">Profiles</h2>
          <p className="text-sm text-[#6b7280]">
            {selectedIds.length} of {profiles.length} included
          </p>
        </div>
        <div className="flex w-full flex-col gap-2 sm:w-auto">
          {search && (
            <input
              type="search"
              value={query}
              onChange={(event) => setQuery(event.target.value)}
              placeholder="Search profiles"
              className="h-10 w-full rounded-md border border-black/10 bg-[#f9fafb] px-3 text-sm text-[#171717] outline-none ring-0 placeholder:text-[#9ca3af] focus:border-[#ff7a00] sm:w-64"
            />
          )}
          <div className="grid grid-cols-2 gap-2 sm:flex sm:justify-end">
            <button
              type="button"
              onClick={onSelectAll}
              className="h-9 rounded-md border border-black/10 px-3 text-sm font-semibold text-[#171717] transition hover:border-[#ff7a00]/50"
            >
              Select all
            </button>
            <button
              type="button"
              onClick={onClear}
              className="h-9 rounded-md border border-black/10 px-3 text-sm font-semibold text-[#6b7280] transition hover:border-[#ff7a00]/50 hover:text-[#171717]"
            >
              Clear
            </button>
          </div>
        </div>
      </div>

      <div className="mt-4 flex flex-wrap gap-2" role="group" aria-label="Profile filters">
        {["all", ...filters].map((filter) => (
          <button
            key={filter}
            type="button"
            onClick={() => setActiveFilter(filter)}
            className={`h-9 rounded-md border px-3 text-sm capitalize transition ${
              activeFilter === filter
                ? "border-[#171717] bg-[#171717] text-white"
                : "border-black/10 bg-[#f9fafb] text-[#4b5563] hover:border-black/25"
            }`}
          >
            {filter}
          </button>
        ))}
      </div>

      <div className="mt-4 grid gap-3">
        {filteredProfiles.map((profile) => {
          const profileId = profile.id ?? profile.name.toLowerCase();
          const selected = selectedIdSet.has(profileId);
          const active = activeId === profileId;
          const category = profileCategory(profile);
          return (
            <label
              key={profileId}
              onMouseEnter={() => previewOnHover && onPreview?.(profileId)}
              onFocus={() => onPreview?.(profileId)}
              className={`grid min-h-24 cursor-pointer grid-cols-[auto_1fr_auto] gap-3 rounded-lg border p-4 text-left transition ${categoryTone(category, selected)} ${
                active ? "ring-2 ring-[#171717]/15" : ""
              }`}
            >
              <input
                type="checkbox"
                checked={selected}
                onChange={() => onToggle(profileId)}
                className="mt-1 h-4 w-4 rounded border-black/20 accent-[#ff7a00]"
              />
              <span className="min-w-0">
                <span className="block font-semibold text-[#171717]">{profile.name}</span>
                <span className="mt-1 block text-sm leading-5 text-[#6b7280]">
                  {profile.description}
                </span>
              </span>
              <span className="self-start rounded-md border border-black/10 bg-white/80 px-2 py-1 text-xs font-semibold text-[#4b5563]">
                {getProfileCommands(profile).length}
              </span>
            </label>
          );
        })}
        {!filteredProfiles.length && (
          <div className="rounded-lg border border-dashed border-black/15 bg-[#f9fafb] p-4 text-sm text-[#6b7280]">
            No profiles match that search.
          </div>
        )}
      </div>
    </section>
  );
}
