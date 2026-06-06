"use client";

import { useState, useSyncExternalStore } from "react";

interface NavigatorWithUsb extends Navigator {
  usb?: {
    requestDevice(options: { filters: Array<Record<string, unknown>> }): Promise<unknown>;
  };
}

interface SmartInstallButtonProps {
  selectedIds: string[];
  selectedCount?: number;
  onDeviceDetected?: () => void;
  fallbackToInstructions?: boolean;
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

export function SmartInstallButton({
  selectedIds,
  selectedCount = 0,
  onDeviceDetected,
  fallbackToInstructions = true,
}: SmartInstallButtonProps) {
  const webUsbAvailable = useSyncExternalStore(
    subscribeWebUsb,
    getWebUsbSnapshot,
    getServerWebUsbSnapshot
  );
  const [status, setStatus] = useState("Ready to download");

  async function connectDevice() {
    const usb = (navigator as NavigatorWithUsb).usb;
    if (!usb) {
      setStatus("WebUSB unavailable here; use the qFlipper ZIP fallback below.");
      return;
    }

    try {
      await usb.requestDevice({ filters: [] });
      setStatus("Flipper detected. Continue with the installer pack and copy it through qFlipper.");
      onDeviceDetected?.();
    } catch {
      setStatus("Connection cancelled");
    }
  }

  const params = new URLSearchParams();
  for (const id of selectedIds) params.append("profile", id);
  const hasSelection = selectedIds.length > 0;
  const downloadHref = hasSelection ? `/api/pack?${params.toString()}` : undefined;

  return (
    <section className="overflow-hidden rounded-xl border border-border bg-gradient-to-br from-card to-card/80 p-4 shadow-lg">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
        <div>
          <h2 className="text-lg font-semibold text-foreground">📦 Install Pack</h2>
          <p className="text-sm text-muted-foreground">
            {hasSelection
              ? `${selectedIds.length} profile${selectedIds.length === 1 ? "" : "s"} and ${selectedCount} command${selectedCount === 1 ? "" : "s"} selected`
              : "Select at least one profile"}
          </p>
          <p className="mt-1 text-xs font-medium uppercase tracking-[0.12em] text-muted-foreground">{status}</p>
        </div>
        <div className="flex flex-col gap-2 sm:flex-row">
          <button
            type="button"
            onClick={connectDevice}
            className="h-11 rounded-lg border border-border bg-background px-4 text-sm font-semibold text-foreground transition hover:bg-accent/10 disabled:cursor-not-allowed disabled:opacity-50"
            disabled={!webUsbAvailable}
          >
            Detect Flipper over USB
          </button>
          {downloadHref ? (
            <a
              href={downloadHref}
              className="inline-flex h-11 items-center justify-center rounded-lg bg-gradient-to-r from-accent to-accent-primary-hover px-5 text-sm font-semibold text-white shadow-lg shadow-accent/20 transition-all hover:shadow-accent/30 hover:scale-105"
            >
              Download Install Pack
            </a>
          ) : (
            <span className="inline-flex h-11 cursor-not-allowed items-center justify-center rounded-lg bg-background/50 px-5 text-sm font-semibold text-muted-foreground">
              Download Install Pack
            </span>
          )}
        </div>
      </div>
      {fallbackToInstructions && (
        <p className="mt-3 text-xs leading-5 text-muted-foreground">
          WebUSB support varies by browser. The ZIP works offline with qFlipper SD card copy.
        </p>
      )}
    </section>
  );
}
