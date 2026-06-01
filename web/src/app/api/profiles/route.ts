import { NextRequest, NextResponse } from "next/server";
import { Profile } from "@/types/flipdeck";
import { readFileSync, readdirSync } from "fs";
import { join } from "path";

// Path to SD card profiles
const PROFILES_PATH = join(process.cwd(), "..", "..", "..", "sd_card", "apps_data", "flipdeck", "profiles");

export async function GET(request: NextRequest) {
  try {
    const files = readdirSync(PROFILES_PATH);
    const profiles: Record<string, Profile> = {};
    
    for (const file of files) {
      if (file.endsWith(".json")) {
        const content = readFileSync(join(PROFILES_PATH, file), "utf-8");
        const profile: Profile = JSON.parse(content);
        profiles[profile.id] = profile;
      }
    }
    
    return NextResponse.json(profiles);
  } catch (error) {
    return NextResponse.json(
      { error: "Failed to load profiles" },
      { status: 500 }
    );
  }
}