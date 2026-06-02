import { NextRequest, NextResponse } from "next/server";
import JSZip from "jszip";
import { normalizeProfile, profileFiles } from "@/lib/profiles";

const APP_ROOT = "apps_data/flipdeck";

const settings = {
  confirm_before_send: true,
  auto_detect_usb: true,
  show_icons: true,
  show_descriptions: true,
  send_delay_ms: 100,
  startup_category: "",
};

function profilesForIds(ids: string[]) {
  const normalized = profileFiles.map(({ fileName, profile }) => ({
    fileName,
    profile: normalizeProfile(profile, fileName),
  }));

  if (!ids.length) return normalized;
  const selected = new Set(ids);
  return normalized.filter(({ profile }) => profile.id && selected.has(profile.id));
}

async function buildPack(ids: string[]) {
  const zip = new JSZip();
  const selectedProfiles = profilesForIds(ids);

  zip.file(
    "README-FIRST.txt",
    [
      "FlipDeck custom install pack",
      "",
      "Copy the apps_data folder to the root of your Flipper Zero SD card with qFlipper.",
      "Review every command before running it on a host computer.",
      "",
    ].join("\n")
  );

  for (const { fileName, profile } of selectedProfiles) {
    zip.file(`${APP_ROOT}/profiles/${fileName}`, `${JSON.stringify(profile, null, 2)}\n`);
  }

  zip.file(`${APP_ROOT}/settings.json`, `${JSON.stringify(settings, null, 2)}\n`);

  return zip.generateAsync({ type: "arraybuffer" });
}

export async function GET(request: NextRequest) {
  const ids = request.nextUrl.searchParams.getAll("profile");
  const zipContent = await buildPack(ids);

  return new NextResponse(Buffer.from(zipContent), {
    headers: {
      "Content-Type": "application/zip",
      "Content-Disposition": "attachment; filename=flipdeck-custom-pack.zip",
    },
  });
}

export async function POST(request: NextRequest) {
  const body = (await request.json().catch(() => ({}))) as { profiles?: string[] };
  const zipContent = await buildPack(body.profiles ?? []);

  return new NextResponse(Buffer.from(zipContent), {
    headers: {
      "Content-Type": "application/zip",
      "Content-Disposition": "attachment; filename=flipdeck-custom-pack.zip",
    },
  });
}
