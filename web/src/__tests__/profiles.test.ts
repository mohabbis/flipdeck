import { describe, expect, it } from "vitest";
import {
  getDeviceProfile,
  getProfileCommands,
  getProfileId,
  normalizeProfile,
  primaryCategory,
  resolveCategory,
} from "../lib/profiles";
import type { Profile } from "../types/flipdeck";

describe("getProfileId", () => {
  it("prefers the explicit id field", () => {
    expect(getProfileId("git.json", { name: "Git", id: "version-control" })).toBe("version-control");
  });

  it("falls back to the file name without extension", () => {
    expect(getProfileId("python.json", { name: "Python" })).toBe("python");
  });
});

describe("getProfileCommands (v1 → v2 migration)", () => {
  it("reads from the v2 commands key", () => {
    const profile: Profile = {
      name: "P",
      commands: [{ label: "Status", type: "text", value: "git status\n" }],
    };
    expect(getProfileCommands(profile)).toEqual([
      { label: "Status", type: "text", value: "git status\n", delay_ms: 100, confirmation_required: true },
    ]);
  });

  it("migrates v1 actions to commands", () => {
    const profile: Profile = {
      name: "P",
      actions: [{ label: "Status", type: "text", value: "git status\n" }],
    };
    const [command] = getProfileCommands(profile);
    expect(command.label).toBe("Status");
  });

  it("maps the v1 confirm field onto confirmation_required", () => {
    const profile: Profile = {
      name: "P",
      actions: [{ label: "Risky", type: "text", value: "x", confirm: false }],
    };
    expect(getProfileCommands(profile)[0].confirmation_required).toBe(false);
  });

  it("keeps an explicit confirmation_required over a legacy confirm", () => {
    const profile: Profile = {
      name: "P",
      commands: [{ label: "X", type: "text", value: "x", confirm: true, confirmation_required: false }],
    };
    expect(getProfileCommands(profile)[0].confirmation_required).toBe(false);
  });

  it("defaults confirmation_required to true when neither field is set", () => {
    const profile: Profile = { name: "P", commands: [{ label: "X", type: "text", value: "x" }] };
    expect(getProfileCommands(profile)[0].confirmation_required).toBe(true);
  });

  it("defaults delay_ms to 100 but preserves an explicit value", () => {
    const profile: Profile = {
      name: "P",
      commands: [
        { label: "Fast", type: "text", value: "a", delay_ms: 5 },
        { label: "Default", type: "text", value: "b" },
      ],
    };
    const [fast, dflt] = getProfileCommands(profile);
    expect(fast.delay_ms).toBe(5);
    expect(dflt.delay_ms).toBe(100);
  });

  it("returns an empty array when a profile has no commands or actions", () => {
    expect(getProfileCommands({ name: "Empty" })).toEqual([]);
  });
});

describe("primaryCategory / resolveCategory", () => {
  it("derives the category from known ids", () => {
    expect(primaryCategory("wifi-devboard")).toBe("wifi");
    expect(primaryCategory("aws")).toBe("cloud");
    expect(primaryCategory("docker")).toBe("cloud");
    expect(primaryCategory("system")).toBe("system");
    expect(primaryCategory("presentation")).toBe("system");
  });

  it("falls back to dev for unknown ids", () => {
    expect(primaryCategory("git")).toBe("dev");
    expect(primaryCategory("anything-else")).toBe("dev");
  });

  it("prefers an explicit category over the id-derived fallback", () => {
    expect(resolveCategory({ name: "P", category: "custom" }, "aws")).toBe("custom");
    expect(resolveCategory({ name: "P" }, "aws")).toBe("cloud");
  });
});

describe("normalizeProfile", () => {
  const raw: Profile = {
    name: "Git",
    actions: [
      { label: "Status", type: "text", value: "git status\n" },
      { label: "Danger", type: "text", value: "rm -rf /\n" },
    ],
  };

  it("fills in id, defaults, and normalized commands", () => {
    const normalized = normalizeProfile(raw, "git.json");
    expect(normalized.id).toBe("git");
    expect(normalized.description).toBe("");
    expect(normalized.icon).toBe("git");
    expect(normalized.category).toBe("dev");
    expect(normalized.tags).toEqual([]);
    expect(normalized.commands).toHaveLength(2);
  });

  it("computes metadata including a risk_count from the safety audit", () => {
    const normalized = normalizeProfile(raw, "git.json");
    expect(normalized.metadata).toEqual({
      command_count: 2,
      categories: ["dev"],
      risk_count: 1,
    });
  });

  it("preserves caller-supplied description, icon, and tags", () => {
    const normalized = normalizeProfile(
      { name: "Git", description: "desc", icon: "custom", tags: ["a"] },
      "git.json"
    );
    expect(normalized.description).toBe("desc");
    expect(normalized.icon).toBe("custom");
    expect(normalized.tags).toEqual(["a"]);
  });
});

describe("getDeviceProfile (on-device parser contract)", () => {
  const raw: Profile = {
    name: "Git",
    actions: [{ label: "Status", type: "text", value: "git status\n" }],
    metadata: { command_count: 1, categories: ["dev"], risk_count: 0 },
  };

  it("emits only the keys the C parser expects", () => {
    const device = getDeviceProfile(raw, "git.json");
    expect(Object.keys(device).sort()).toEqual(["commands", "description", "icon", "id", "name"]);
  });

  it("drops metadata and the legacy actions key", () => {
    const device = getDeviceProfile(raw, "git.json");
    expect(device.metadata).toBeUndefined();
    expect(device.actions).toBeUndefined();
    expect(device.commands).toHaveLength(1);
  });

  it("keeps commands as the only trailing array so the last ] is the commands end", () => {
    // The C parser locates the commands array via the final ']' in the file.
    // Any other trailing array (e.g. metadata.categories) would break parsing.
    const json = JSON.stringify(getDeviceProfile(raw, "git.json"));
    expect(json.lastIndexOf("]")).toBe(json.length - 2); // closing ] then closing }
  });
});
