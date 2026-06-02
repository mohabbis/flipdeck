import type { Profile } from "@/types/flipdeck";

import awsProfile from "../../public/profiles/aws.json";
import dockerProfile from "../../public/profiles/docker.json";
import gitProfile from "../../public/profiles/git.json";
import nodeProfile from "../../public/profiles/node.json";
import presentationProfile from "../../public/profiles/presentation.json";
import pythonProfile from "../../public/profiles/python.json";
import snippetsProfile from "../../public/profiles/snippets.json";
import systemProfile from "../../public/profiles/system.json";
import vscodeProfile from "../../public/profiles/vscode.json";

export interface ProfileFile {
  fileName: string;
  profile: Profile;
}

export const profileFiles: ProfileFile[] = [
  { fileName: "aws.json", profile: awsProfile as Profile },
  { fileName: "docker.json", profile: dockerProfile as Profile },
  { fileName: "git.json", profile: gitProfile as Profile },
  { fileName: "node.json", profile: nodeProfile as Profile },
  { fileName: "presentation.json", profile: presentationProfile as Profile },
  { fileName: "python.json", profile: pythonProfile as Profile },
  { fileName: "snippets.json", profile: snippetsProfile as Profile },
  { fileName: "system.json", profile: systemProfile as Profile },
  { fileName: "vscode.json", profile: vscodeProfile as Profile },
];

export function getProfilesById(): Record<string, Profile> {
  return Object.fromEntries(
    profileFiles.map(({ profile }) => [profile.id, profile])
  );
}
