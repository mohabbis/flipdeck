"use client";

import { useState } from "react";

interface NavigatorWithUsb extends Navigator {
  usb?: {
    requestDevice(options: { filters: Array<Record<string, unknown>> }): Promise<unknown>;
  };
}

interface SmartInstallButtonProps {
  selectedIds: string[];
  onDeviceDetected?: () => void;
  fallbackToInstructions?: boolean;
}

export function SmartInstallButton({
  selectedIds,
  onDeviceDetected,
  fallbackToInstructions = true,
}: SmartInstallButtonProps) {
  const [status, setStatus] = useState("Connect over USB with the microSD card inside the Flipper.");
  const webUsbAvailable =
    typeof navigator !== "undefined" && Boolean((navigator as NavigatorWithUsb).usb);

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

  return (
    <section className="rounded-lg border border-[#00D4AA]/40 bg-[linear-gradient(135deg,rgba(0,212,170,0.22),rgba(255,122,0,0.13))] p-4 shadow-2xl shadow-[#00D4AA]/10 backdrop-blur-md">
      <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
        <div>
          <h2 className="text-lg font-semibold text-white">Install to your Flipper</h2>
          <p className="text-sm text-slate-300">{status}</p>
        </div>
        <div className="flex flex-col gap-2 sm:flex-row">
          <button
            type="button"
            onClick={connectDevice}
            className="h-11 rounded-md border border-white/20 bg-slate-950/40 px-4 text-sm font-semibold text-[#C7FFF1] transition hover:bg-[#00D4AA]/20 disabled:cursor-not-allowed disabled:opacity-50"
            disabled={!webUsbAvailable}
          >
            Detect Flipper over USB
          </button>
          <a
            href={`/api/pack?${params.toString()}`}
            className="inline-flex h-11 items-center justify-center rounded-md bg-[#ff7a00] px-4 text-sm font-semibold text-white shadow-lg shadow-orange-950/30 transition hover:bg-[#ff9d2e]"
          >
            Download installer pack
          </a>
        </div>
      </div>
      {fallbackToInstructions && (
        <p className="mt-3 text-xs leading-5 text-slate-400">
          Primary path: keep the microSD card in the Flipper, connect USB, then copy the ZIP
          contents through qFlipper. Manual fallback: download the .fap or ZIP and copy it to the
          SD card yourself.
        </p>
      )}
    </section>
  );
}
