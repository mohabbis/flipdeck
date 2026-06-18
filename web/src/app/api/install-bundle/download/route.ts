import { NextResponse } from "next/server";
import JSZip from "jszip";
import { getProfileCommands, profileFiles } from "@/lib/profiles";
import { DEFAULT_SETTINGS, SNIPPETS } from "@/lib/install-data";
import { auditCommands, auditSnippets, hasCriticalRisk } from "@/lib/safety-check";

const APP_ROOT = "apps_data/flipdeck";

const settings = DEFAULT_SETTINGS;
const snippets = SNIPPETS;

export async function GET() {
  const risks = [
    ...profileFiles.flatMap(({ profile }) => auditCommands(getProfileCommands(profile))),
    ...auditSnippets(snippets),
  ];
  if (hasCriticalRisk(risks)) {
    return NextResponse.json(
      {
        error: "Install bundle rejected: bundled content contains commands blocked by the FlipDeck safety policy.",
        blocked: risks
          .filter((risk) => risk.severity === "critical")
          .map((risk) => ({ rule: risk.label, command: risk.command, value: risk.value })),
      },
      { status: 422 }
    );
  }

  const zip = new JSZip();

  zip.file(
    "README-FIRST.txt",
    [
      "FlipDeck Flipper Zero install pack",
      "",
      "1. Leave the microSD card inside your Flipper Zero.",
      "2. Plug in and unlock your Flipper Zero over USB.",
      "3. Open qFlipper on your computer and open the SD card file browser.",
      "4. Drag the apps_data folder from this ZIP onto the Flipper SD card root.",
      "5. Merge/replace the FlipDeck files when prompted.",
      "6. Launch FlipDeck from Apps on your Flipper.",
      "",
      "Manual fallback: if you are developing or qFlipper is unavailable, copy apps_data to a mounted Flipper microSD card with an external card reader, eject it, then insert it back into the Flipper.",
      "",
      "Profiles are installed to /apps_data/flipdeck/profiles/.",
      "Only use command profiles you trust on computers you own or administer.",
      "",
    ].join("\n")
  );

  for (const { fileName, profile } of profileFiles) {
    zip.file(
      `${APP_ROOT}/profiles/${fileName}`,
      `${JSON.stringify(profile, null, 2)}\n`
    );
  }

  for (const [fileName, snippet] of Object.entries(snippets)) {
    zip.file(`${APP_ROOT}/snippets/${fileName}`, `${snippet}\n`);
  }

  zip.file(`${APP_ROOT}/settings.json`, `${JSON.stringify(settings, null, 2)}\n`);

  const zipContent = await zip.generateAsync({ type: "arraybuffer" });

  return new NextResponse(Buffer.from(zipContent), {
    headers: {
      "Content-Type": "application/zip",
      "Content-Disposition": "attachment; filename=flipdeck-flipper-install-pack.zip",
    },
  });
}
