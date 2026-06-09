import { describe, expect, it } from "vitest";
import { GET } from "../app/api/health/route";

describe("GET /api/health", () => {
  it("returns healthy JSON", async () => {
    const res = GET();

    expect(res.status).toBe(200);
    expect(res.headers.get("Content-Type")).toContain("application/json");

    const data = await res.json();
    expect(data).toMatchObject({
      ok: true,
      service: "flipdeck-web",
    });
    expect(typeof data.timestamp).toBe("string");
    expect(Number.isNaN(Date.parse(data.timestamp))).toBe(false);
  });
});
