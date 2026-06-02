import { CommandAudit } from "@/components/CommandAudit";
import { CommandPreview } from "@/components/CommandPreview";
import { getProfileCommands, normalizeProfile, profileFiles } from "@/lib/profiles";

interface PreviewPageProps {
  params: Promise<{ profile: string }>;
}

export default async function PreviewPage({ params }: PreviewPageProps) {
  const { profile: profileId } = await params;
  const profile = profileFiles
    .map(({ fileName, profile }) => normalizeProfile(profile, fileName))
    .find((candidate) => candidate.id === profileId);

  if (!profile) {
    return (
      <main className="min-h-screen bg-[#0F172A] px-4 py-10 text-white">
        <div className="mx-auto max-w-3xl rounded-lg border border-white/10 bg-white/[0.07] p-6">
          Profile not found.
        </div>
      </main>
    );
  }

  return (
    <main className="min-h-screen bg-[#0F172A] px-4 py-10 text-white">
      <div className="mx-auto grid max-w-5xl gap-6 lg:grid-cols-[1fr_360px]">
        <CommandPreview profile={profile} renderAs="flipper-screen" />
        <CommandAudit commands={getProfileCommands(profile)} />
      </div>
    </main>
  );
}
