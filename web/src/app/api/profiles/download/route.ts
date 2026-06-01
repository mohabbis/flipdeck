import { NextRequest, NextResponse } from "next/server";
import JSZip from "jszip";
import { readFileSync, readdirSync } from "fs";
import { join } from "path";

// Path to profiles in public directory
const PROFILES_PATH = join(process.cwd(), "..", "public", "profiles");

export async function GET(request: NextRequest) {
  try {
    const zip = new JSZip();
    
    // Check if profiles directory exists
    let files: string[] = [];
    try {
      files = readdirSync(PROFILES_PATH);
    } catch {
      // If no profiles in public, use embedded default profiles
    }
    
    if (files.length === 0) {
      // Use embedded profiles as fallback
      const defaultProfiles = [
        { id: "git", file: "git.json" },
        { id: "node", file: "node.json" },
        { id: "python", file: "python.json" },
        { id: "docker", file: "docker.json" },
        { id: "system", file: "system.json" },
        { id: "vscode", file: "vscode.json" },
        { id: "presentation", file: "presentation.json" },
        { id: "aws", file: "aws.json" },
        { id: "snippets", file: "snippets.json" },
      ];
      
      for (const profile of defaultProfiles) {
        // Read from SD card as source of truth
        try {
          const sdPath = join(process.cwd(), "..", "..", "..", "sd_card", "apps_data", "flipdeck", "profiles", profile.file);
          const content = readFileSync(sdPath, "utf-8");
          zip.file(profile.file, content);
        } catch {
          // Skip if file doesn't exist
        }
      }
    } else {
      for (const file of files) {
        if (file.endsWith(".json")) {
          const content = readFileSync(join(PROFILES_PATH, file), "utf-8");
          zip.file(file, content);
        }
      }
    }
    
    const zipContent = await zip.generateAsync({ type: "arraybuffer" });
    
    return new NextResponse(Buffer.from(zipContent), {
      headers: {
        "Content-Type": "application/zip",
        "Content-Disposition": "attachment; filename=flipdeck-profiles.zip",
      },
    });
  } catch (error) {
    return NextResponse.json(
      { error: "Failed to create zip archive" },
      { status: 500 }
    );
  }
}