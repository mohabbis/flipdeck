import { NextRequest, NextResponse } from "next/server";
import JSZip from "jszip";
import { getDeviceProfile, getProfileCommands, getProfileId, profileFiles } from "@/lib/profiles";
import { auditCommands, hasCriticalRisk, type CommandRisk } from "@/lib/safety-check";

const APP_ROOT = "apps_data/flipdeck";

/**
 * Server-side safety gate: never serialize a profile that contains a critical
 * command, even if the request bypasses the UI. Returns the offending risks
 * (empty when the selection is safe to ship).
 */
function criticalRisksFor(selected: ReturnType<typeof profilesForIds>): CommandRisk[] {
  const risks = selected.flatMap(({ profile }) => auditCommands(getProfileCommands(profile)));
  return hasCriticalRisk(risks) ? risks.filter((risk) => risk.severity === "critical") : [];
}

const settings = {
  confirm_before_send: true,
  auto_detect_usb: true,
  show_icons: true,
  show_descriptions: true,
  send_delay_ms: 100,
  startup_category: "",
};

function profilesForIds(ids: string[]) {
  if (!ids.length) return profileFiles;
  const selected = new Set(ids);
  return profileFiles.filter(({ fileName, profile }) => selected.has(getProfileId(fileName, profile)));
}

async function buildPack(ids: string[]) {
  const zip = new JSZip();
  const selectedProfiles = profilesForIds(ids);

  zip.file(
    "README-FIRST.txt",
    [
      "FlipDeck custom install pack",
      "",
      "Primary install: keep the microSD card inside your Flipper Zero, connect USB, then copy the apps_data folder to the Flipper SD card with qFlipper.",
      "Manual fallback: copy apps_data to a mounted Flipper microSD card yourself, then reinsert it before launching FlipDeck.",
      "Review every command before running it on a host computer.",
      "",
    ].join("\n")
  );

  for (const { fileName, profile } of selectedProfiles) {
    zip.file(
      `${APP_ROOT}/profiles/${fileName}`,
      `${JSON.stringify(getDeviceProfile(profile, fileName), null, 2)}\n`
    );
  }

  zip.file(`${APP_ROOT}/settings.json`, `${JSON.stringify(settings, null, 2)}\n`);

  return zip.generateAsync({ type: "arraybuffer" });
}

function blockedResponse(risks: CommandRisk[]): NextResponse {
  return NextResponse.json(
    {
      error: "Pack rejected: selection contains commands blocked by the FlipDeck safety policy.",
      blocked: risks.map((risk) => ({ rule: risk.label, command: risk.command, value: risk.value })),
    },
    { status: 422 }
  );
}

async function packResponse(ids: string[]): Promise<NextResponse> {
  const critical = criticalRisksFor(profilesForIds(ids));
  if (critical.length) return blockedResponse(critical);

  const zipContent = await buildPack(ids);
  return new NextResponse(Buffer.from(zipContent), {
    headers: {
      "Content-Type": "application/zip",
      "Content-Disposition": "attachment; filename=flipdeck-custom-pack.zip",
    },
  });
}

export async function GET(request: NextRequest) {
  return packResponse(request.nextUrl.searchParams.getAll("profile"));
}

export async function POST(request: NextRequest) {
  const body = (await request.json().catch(() => ({}))) as { profiles?: string[] };
  return packResponse(body.profiles ?? []);
}
