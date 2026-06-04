import { NextRequest, NextResponse } from "next/server";
import JSZip from "jszip";
import { normalizeProfile, profileFiles } from "@/lib/profiles";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

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

  if (ids.length && !selectedProfiles.length) {
    throw new Error(`No matching profiles found for: ${ids.join(", ")}`);
  }

  zip.file(
    "README-FIRST.txt",
    [
      "FlipDeck install pack",
      "",
      "Copy the apps_data folder to the root of your Flipper Zero SD card with qFlipper.",
      "Review every command before running it on a host computer.",
      "No firmware flashing is required.",
      "",
    ].join("\n")
  );

  for (const { fileName, profile } of selectedProfiles) {
    zip.file(`${APP_ROOT}/profiles/${fileName}`, `${JSON.stringify(profile, null, 2)}\n`);
  }

  zip.file(`${APP_ROOT}/settings.json`, `${JSON.stringify(settings, null, 2)}\n`);

  return zip.generateAsync({ type: "uint8array" });
}

function zipResponse(zipContent: Uint8Array) {
  return new NextResponse(Buffer.from(zipContent), {
    status: 200,
    headers: {
      "Content-Type": "application/zip",
      "Content-Disposition": "attachment; filename=flipdeck-install-pack.zip",
      "Cache-Control": "no-store",
    },
  });
}

function errorResponse(error: unknown) {
  const message = error instanceof Error ? error.message : "Unable to build install pack.";
  return NextResponse.json({ error: message }, { status: 400 });
}

export async function GET(request: NextRequest) {
  try {
    const ids = request.nextUrl.searchParams.getAll("profile");
    const zipContent = await buildPack(ids);
    return zipResponse(zipContent);
  } catch (error) {
    return errorResponse(error);
  }
}

export async function POST(request: NextRequest) {
  try {
    const body = (await request.json().catch(() => ({}))) as { profiles?: string[] };
    const zipContent = await buildPack(body.profiles ?? []);
    return zipResponse(zipContent);
  } catch (error) {
    return errorResponse(error);
  }
}

