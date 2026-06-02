import { describe, it, expect } from "vitest";
import JSZip from "jszip";
import { GET } from "../app/api/install-bundle/download/route";

describe("GET /api/install-bundle/download", () => {
  it("returns a Flipper-ready ZIP with the expected headers", async () => {
    const res = await GET();

    expect(res.status).toBe(200);
    expect(res.headers.get("Content-Type")).toBe("application/zip");
    expect(res.headers.get("Content-Disposition")).toBe(
      "attachment; filename=flipdeck-flipper-install-pack.zip"
    );
  });

  it("includes the SD card folder structure Flipper users can drag and drop", async () => {
    const res = await GET();
    const zip = await JSZip.loadAsync(await res.arrayBuffer());

    expect(zip.file("README-FIRST.txt")).toBeTruthy();
    expect(zip.file("apps_data/flipdeck/settings.json")).toBeTruthy();
    expect(zip.file("apps_data/flipdeck/profiles/git.json")).toBeTruthy();
    expect(zip.file("apps_data/flipdeck/snippets/react_component.txt")).toBeTruthy();

    const readme = await zip.file("README-FIRST.txt")!.async("string");
    expect(readme).toContain("Open qFlipper");

    const gitProfile = JSON.parse(
      await zip.file("apps_data/flipdeck/profiles/git.json")!.async("string")
    );
    expect(gitProfile.id).toBe("git");
  });
});
