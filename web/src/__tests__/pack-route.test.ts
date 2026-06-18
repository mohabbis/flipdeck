import { describe, expect, it, vi } from "vitest";

// Inject a malicious profile so the server-side gate has something to reject.
// The real getProfileCommands / getProfileId / getDeviceProfile keep operating.
vi.mock("@/lib/profiles", async (importOriginal) => {
  const actual = await importOriginal<typeof import("@/lib/profiles")>();
  return {
    ...actual,
    profileFiles: [
      {
        fileName: "evil.json",
        profile: {
          name: "Evil",
          id: "evil",
          commands: [{ label: "Wipe", type: "text", value: "rm -rf /" }],
        },
      },
      {
        fileName: "good.json",
        profile: {
          name: "Good",
          id: "good",
          commands: [{ label: "Status", type: "text", value: "git status\n" }],
        },
      },
    ],
  };
});

import { GET } from "@/app/api/pack/route";
import { NextRequest } from "next/server";

function request(query: string) {
  return new NextRequest(`http://localhost/api/pack${query}`);
}

describe("GET /api/pack server-side safety gate", () => {
  it("rejects a selection containing a critical command with 422", async () => {
    const res = await GET(request("?profile=evil"));
    expect(res.status).toBe(422);
    const body = await res.json();
    expect(body.blocked).toEqual([
      { rule: "Recursive delete", command: "Wipe", value: "rm -rf /" },
    ]);
  });

  it("serves a ZIP for a clean selection", async () => {
    const res = await GET(request("?profile=good"));
    expect(res.status).toBe(200);
    expect(res.headers.get("Content-Type")).toBe("application/zip");
  });

  it("rejects the default (all profiles) when any bundled profile is critical", async () => {
    const res = await GET(request(""));
    expect(res.status).toBe(422);
  });
});
