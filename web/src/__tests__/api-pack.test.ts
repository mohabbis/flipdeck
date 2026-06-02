import { describe, expect, it } from "vitest";
import JSZip from "jszip";
import { NextRequest } from "next/server";
import { GET, POST } from "../app/api/pack/route";

async function loadZip(response: Response) {
  return JSZip.loadAsync(await response.arrayBuffer());
}

describe("/api/pack", () => {
  it("GET returns a custom install ZIP with selected query profiles", async () => {
    const request = new NextRequest("http://localhost/api/pack?profile=git&profile=node");
    const res = await GET(request);

    expect(res.status).toBe(200);
    expect(res.headers.get("Content-Type")).toBe("application/zip");
    expect(res.headers.get("Content-Disposition")).toBe(
      "attachment; filename=flipdeck-custom-pack.zip"
    );

    const zip = await loadZip(res);
    expect(zip.file("README-FIRST.txt")).toBeTruthy();
    expect(zip.file("apps_data/flipdeck/settings.json")).toBeTruthy();
    expect(zip.file("apps_data/flipdeck/profiles/git.json")).toBeTruthy();
    expect(zip.file("apps_data/flipdeck/profiles/node.json")).toBeTruthy();
    expect(zip.file("apps_data/flipdeck/profiles/python.json")).toBeNull();
  });

  it("POST returns a custom install ZIP from a JSON profile list", async () => {
    const request = new NextRequest("http://localhost/api/pack", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ profiles: ["docker"] }),
    });

    const res = await POST(request);
    const zip = await loadZip(res);

    expect(zip.file("apps_data/flipdeck/profiles/docker.json")).toBeTruthy();
    expect(zip.file("apps_data/flipdeck/profiles/git.json")).toBeNull();

    const dockerProfile = JSON.parse(
      await zip.file("apps_data/flipdeck/profiles/docker.json")!.async("string")
    );
    expect(dockerProfile.id).toBe("docker");
    expect(dockerProfile.commands[0]).toMatchObject({
      delay_ms: 100,
      confirmation_required: true,
    });
  });
});
