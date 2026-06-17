"use client";

import { useMemo, useState } from "react";
import type { Profile } from "@/types/flipdeck";
import { getProfileCommands, resolveCategory } from "@/lib/profiles";

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
  return resolveCategory(profile, profile.id ?? "");
}

const ICON_GLYPHS: Record<string, string> = {
  git: "\u{1F500}",
  docker: "\u{1F433}",
  node: "\u{2B22}",
  python: "\u{1F40D}",
  aws: "☁️",
  vscode: "\u{1F4BB}",
  system: "⚙️",
  snippets: "\u{1F4CB}",
  presentation: "\u{1F4FA}",
  wifi: "\u{1F4F6}",
};

function profileGlyph(profile: Profile): string {
  return ICON_GLYPHS[profile.icon ?? ""] ?? "\u{1F4E6}";
}

function categoryTone(category: string, selected: boolean) {
  const tones: Record<string, string> = {
    dev: selected
      ? "border-accent-success bg-accent-success/10"
      : "border-border bg-card hover:border-accent-success/45",
    cloud: selected
      ? "border-accent-secondary bg-accent-secondary/10"
      : "border-border bg-card hover:border-accent-secondary/45",
    system: selected
      ? "border-accent bg-accent/10"
      : "border-border bg-card hover:border-accent/45",
    wifi: selected
      ? "border-sky-400 bg-sky-400/10"
      : "border-border bg-card hover:border-sky-400/45",
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
  filters = ["dev", "cloud", "system", "wifi"],
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
    <section className="overflow-hidden rounded-xl border border-border bg-card shadow-lg">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
        <div>
          <h2 className="text-lg font-semibold text-foreground">Profiles</h2>
          <p className="text-sm text-muted-foreground">
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
              className="h-10 w-full rounded-lg border border-border bg-background px-3 text-sm text-foreground outline-none ring-0 placeholder:text-muted-foreground focus:border-accent focus:ring-1 focus:ring-accent/20"
            />
          )}
          <div className="grid grid-cols-2 gap-2 sm:flex sm:justify-end">
            <button
              type="button"
              onClick={onSelectAll}
              className="h-9 rounded-lg border border-border bg-background px-3 text-sm font-semibold text-foreground transition hover:border-accent/50 hover:bg-accent/10"
            >
              Select all
            </button>
            <button
              type="button"
              onClick={onClear}
              className="h-9 rounded-lg border border-border bg-background px-3 text-sm font-semibold text-muted-foreground transition hover:border-accent/50 hover:text-foreground"
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
            className={`h-9 rounded-lg border px-3 text-sm capitalize transition ${
              activeFilter === filter
                ? "border-accent bg-accent text-white shadow-md"
                : "border-border bg-background text-muted-foreground hover:border-foreground/25"
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
              className={`group grid min-h-24 cursor-pointer grid-cols-[auto_1fr_auto] gap-3 rounded-xl border p-4 text-left transition-all ${categoryTone(category, selected)} ${
                active ? "ring-2 ring-accent/30 shadow-lg" : "shadow-sm hover:shadow-md"
              }`}
            >
              <input
                type="checkbox"
                checked={selected}
                onChange={() => onToggle(profileId)}
                className="mt-1 h-4 w-4 rounded border-border accent-accent"
              />
              <span className="min-w-0">
                <span className="block font-semibold text-foreground">
                  <span aria-hidden="true" className="mr-1.5">
                    {profileGlyph(profile)}
                  </span>
                  {profile.name}
                </span>
                <span className="mt-1 block text-sm leading-5 text-muted-foreground">
                  {profile.description}
                </span>
                {!!profile.tags?.length && (
                  <span className="mt-2 flex flex-wrap gap-1.5">
                    {profile.tags.map((tag) => (
                      <span
                        key={tag}
                        className="rounded-full border border-border bg-background px-2 py-0.5 text-xs text-muted-foreground"
                      >
                        {tag}
                      </span>
                    ))}
                  </span>
                )}
              </span>
              <span className="self-start rounded-lg border border-border bg-background/80 px-2 py-1 text-xs font-semibold text-muted-foreground group-hover:text-foreground">
                {getProfileCommands(profile).length}
              </span>
            </label>
          );
        })}
        {!filteredProfiles.length && (
          <div className="rounded-xl border border-dashed border-border bg-background p-4 text-sm text-muted-foreground">
            No profiles match that search.
          </div>
        )}
      </div>
    </section>
  );
}
