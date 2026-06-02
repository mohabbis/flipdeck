import { NextResponse } from "next/server";
import { getProfilesById } from "@/lib/profiles";

export function GET() {
  return NextResponse.json(getProfilesById());
}
