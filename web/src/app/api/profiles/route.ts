import { NextResponse } from "next/server";
import { Profile } from "@/types/flipdeck";
import { readFileSync, readdirSync } from "fs";
import { join } from "path";

// Path to profiles copied into the web public directory
const PROFILES_PATH = join(process.cwd(), "public", "profiles");

export async function GET() {
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
  } catch {
    return NextResponse.json(
      { error: "Failed to load profiles" },
      { status: 500 }
    );
  }
}