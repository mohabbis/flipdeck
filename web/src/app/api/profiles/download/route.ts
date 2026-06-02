import { NextResponse } from "next/server";
import JSZip from "jszip";
import { profileFiles } from "@/lib/profiles";

export async function GET() {
  const zip = new JSZip();

  for (const { fileName, profile } of profileFiles) {
    zip.file(fileName, `${JSON.stringify(profile, null, 2)}\n`);
  }

  const zipContent = await zip.generateAsync({ type: "arraybuffer" });

  return new NextResponse(Buffer.from(zipContent), {
    headers: {
      "Content-Type": "application/zip",
      "Content-Disposition": "attachment; filename=flipdeck-profiles.zip",
    },
  });
}
