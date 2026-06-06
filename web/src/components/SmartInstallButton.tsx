"use client";

import { useState, useSyncExternalStore } from "react";
import { SerialInstaller } from "@/components/SerialInstaller";

interface NavigatorWithUsb extends Navigator {
  usb?: {
    requestDevice(options: { filters: Array<Record<string, unknown>> }): Promise<unknown>;
  };
}

interface SmartInstallButtonProps {
  selectedIds: string[];
  selectedCount?: number;
}

function subscribeWebUsb() {
  return () => undefined;
}

function getWebUsbSnapshot() {
  return typeof navigator !== "undefined" && Boolean((navigator as NavigatorWithUsb).usb);
}

function getServerWebUsbSnapshot() {
  return false;
}

export function SmartInstallButton({ selectedIds, selectedCount = 0 }: SmartInstallButtonProps) {
  const webUsbAvailable = useSyncExternalStore(
    subscribeWebUsb,
    getWebUsbSnapshot,
    getServerWebUsbSnapshot
  );
  const [detectStatus, setDetectStatus] = useState<string | null>(null);

  async function detectDevice() {
    const usb = (navigator as NavigatorWithUsb).usb;
    if (!usb) return;
    try {
      await usb.requestDevice({ filters: [] });
      setDetectStatus("Flipper detected over USB.");
    } catch {
      setDetectStatus(null);
    }
  }

  const params = new URLSearchParams();
  for (const id of selectedIds) params.append("profile", id);
  const hasSelection = selectedIds.length > 0;
  const downloadHref = hasSelection ? `/api/pack?${params.toString()}` : undefined;

  return (
    <section className="overflow-hidden rounded-xl border border-border bg-gradient-to-br from-card to-card/80 p-4 shadow-lg">
      {/* Direct serial install — leads the UX */}
      <div className="mb-4">
        <h2 className="mb-1 text-lg font-semibold text-foreground">⚡ Install to Flipper</h2>
        <p className="mb-3 text-sm text-muted-foreground">
          {hasSelection
            ? `${selectedIds.length} profile${selectedIds.length === 1 ? "" : "s"} · ${selectedCount} command${selectedCount === 1 ? "" : "s"} selected`
            : "Select at least one profile to install"}
        </p>
        <SerialInstaller selectedIds={selectedIds} />
      </div>

      {/* ZIP fallback */}
      <div className="border-t border-border pt-4">
        <div className="flex flex-col gap-2 sm:flex-row sm:items-center sm:justify-between">
          <p className="text-xs text-muted-foreground">
            Or download a ZIP and copy via qFlipper SD card browser
          </p>
          <div className="flex gap-2">
            {webUsbAvailable && (
              <button
                type="button"
                onClick={detectDevice}
                title="Detect Flipper over WebUSB"
                className="h-8 rounded-lg border border-border bg-background px-3 text-xs font-medium text-muted-foreground transition hover:text-foreground"
              >
                {detectStatus ?? "Detect via WebUSB"}
              </button>
            )}
            {downloadHref ? (
              <a
                href={downloadHref}
                className="inline-flex h-8 items-center justify-center rounded-lg border border-border bg-card px-4 text-xs font-semibold text-foreground transition hover:border-accent/50 hover:text-accent"
              >
                Download ZIP
              </a>
            ) : (
              <span className="inline-flex h-8 cursor-not-allowed items-center justify-center rounded-lg border border-border bg-background/50 px-4 text-xs font-semibold text-muted-foreground">
                Download ZIP
              </span>
            )}
          </div>
        </div>
      </div>
    </section>
  );
}
