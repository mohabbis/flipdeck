// @vitest-environment jsdom
import { afterEach, describe, expect, it } from "vitest";
import { cleanup, render, screen } from "@testing-library/react";
import { CommandAudit } from "../components/CommandAudit";
import type { FlipDeckAction } from "../types/flipdeck";

afterEach(cleanup);

const cmd = (label: string, value: string): FlipDeckAction => ({ label, type: "text", value });

describe("CommandAudit", () => {
  it("shows an all-clear badge when no command is risky", () => {
    render(<CommandAudit commands={[cmd("Status", "git status\n")]} />);
    expect(screen.getByText(/All clear/i)).toBeTruthy();
    expect(screen.getByText(/No high-risk shell patterns detected/i)).toBeTruthy();
  });

  it("shows a blocked badge and guidance for a critical command", () => {
    render(<CommandAudit commands={[cmd("Nuke", "rm -rf /\n")]} />);
    expect(screen.getByText(/1 blocked/i)).toBeTruthy();
    expect(screen.getByText(/never safe to send automatically/i)).toBeTruthy();
    // The offending command label surfaces in the risk row.
    expect(screen.getByText(/Recursive delete/)).toBeTruthy();
  });

  it("shows a warning badge (not blocked) for warning-only commands", () => {
    render(<CommandAudit commands={[cmd("Sudo", "sudo systemctl restart docker")]} />);
    expect(screen.getByText(/1 warning/i)).toBeTruthy();
    expect(screen.queryByText(/blocked/i)).toBeNull();
  });

  it("pluralizes the warning count", () => {
    render(
      <CommandAudit
        commands={[cmd("A", "sudo a"), cmd("B", "chmod 777 ./x")]}
      />
    );
    expect(screen.getByText(/2 warnings/i)).toBeTruthy();
  });

  it("reports the snippet count in the checked label and audits snippet bodies", () => {
    render(
      <CommandAudit
        commands={[cmd("Status", "git status\n")]}
        snippets={{ "danger.txt": "rm -rf /\n" }}
      />
    );
    expect(screen.getByText(/1 commands \+ 1 snippets checked/i)).toBeTruthy();
    expect(screen.getByText(/1 blocked/i)).toBeTruthy();
  });
});
