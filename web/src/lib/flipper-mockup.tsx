import type { Profile } from "@/types/flipdeck";
import { getProfileCommands } from "@/lib/profiles";

export function renderFlipperAscii(profile: Profile): string {
  const commands = getProfileCommands(profile);
  const width = 32;
  const border = `+${"-".repeat(width)}+`;
  const title = (profile.name || "Profile").slice(0, width).padEnd(width);
  const rows = commands.slice(0, 5).map((command, index) => {
    const cursor = index === 0 ? ">" : " ";
    const label = `${cursor} ${command.label}`.slice(0, 24).padEnd(24);
    const type = command.type === "key_combo" ? "combo" : command.type;
    return `|${label}${type.padStart(8).slice(0, 8)}|`;
  });

  while (rows.length < 5) rows.push(`|${" ".repeat(width)}|`);

  return [border, `|${title}|`, `|${"-".repeat(width)}|`, ...rows, border].join("\n");
}
