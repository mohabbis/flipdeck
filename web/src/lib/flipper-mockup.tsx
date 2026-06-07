import type { Profile } from "@/types/flipdeck";
import { getProfileCommands } from "@/lib/profiles";

const WIDTH = 32;
const VISIBLE_ROWS = 5;

export type PreviewScreen =
  | { kind: "list"; selectedIndex: number }
  | { kind: "confirm"; selectedIndex: number }
  | { kind: "sent"; selectedIndex: number };

function box(title: string, lines: string[]): string {
  const border = `+${"-".repeat(WIDTH)}+`;
  const head = `|${title.slice(0, WIDTH).padEnd(WIDTH)}|`;
  const rule = `|${"-".repeat(WIDTH)}|`;
  const body = lines.slice(0, VISIBLE_ROWS).map((line) => `|${line.slice(0, WIDTH).padEnd(WIDTH)}|`);
  while (body.length < VISIBLE_ROWS) body.push(`|${" ".repeat(WIDTH)}|`);
  return [border, head, rule, ...body, border].join("\n");
}

function renderList(profile: Profile, selectedIndex: number): string {
  const commands = getProfileCommands(profile);
  const start = Math.max(0, Math.min(selectedIndex - 2, commands.length - VISIBLE_ROWS));

  const rows = commands.slice(start, start + VISIBLE_ROWS).map((command, i) => {
    const index = start + i;
    const cursor = index === selectedIndex ? ">" : " ";
    const label = `${cursor} ${command.label}`.slice(0, 24).padEnd(24);
    const type = command.type === "key_combo" ? "combo" : command.type;
    return `${label}${type.padStart(8).slice(0, 8)}`;
  });

  return box(profile.name || "Profile", rows);
}

function renderConfirm(profile: Profile, selectedIndex: number): string {
  const commands = getProfileCommands(profile);
  const command = commands[selectedIndex];
  if (!command) return renderList(profile, selectedIndex);

  return box("Send Command?", [
    command.label,
    command.value.split("\n")[0] ?? command.value,
    "",
    command.confirmation_required ? "Requires OK on device" : "",
    "",
  ]);
}

function renderSent(profile: Profile, selectedIndex: number): string {
  const commands = getProfileCommands(profile);
  const command = commands[selectedIndex];
  if (!command) return renderList(profile, selectedIndex);

  return box("Sent!", ["", `> ${command.label}`.slice(0, WIDTH), "", "", ""]);
}

export function renderFlipperAscii(
  profile: Profile,
  screen: PreviewScreen = { kind: "list", selectedIndex: 0 }
): string {
  switch (screen.kind) {
    case "confirm":
      return renderConfirm(profile, screen.selectedIndex);
    case "sent":
      return renderSent(profile, screen.selectedIndex);
    case "list":
    default:
      return renderList(profile, screen.selectedIndex);
  }
}
