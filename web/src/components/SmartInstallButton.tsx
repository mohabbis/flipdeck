"use client";

import { useEffect, useState } from "react";

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
  const [webUsbAvailable, setWebUsbAvailable] = useState(false);
  const [status, setStatus] = useState("Ready");

  useEffect(() => {
    setWebUsbAvailable(Boolean((navigator as NavigatorWithUsb).usb));
  }, []);

  async function connectDevice() {
    const usb = (navigator as NavigatorWithUsb).usb;
    if (!usb) {
      setStatus("WebUSB unavailable in this browser");
      return;
    }

    try {
      await usb.requestDevice({ filters: [] });
      setStatus("Device detected");
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
          <h2 className="text-lg font-semibold text-white">Install</h2>
          <p className="text-sm text-slate-300">{status}</p>
        </div>
        <div className="flex flex-col gap-2 sm:flex-row">
          <button
            type="button"
            onClick={connectDevice}
            className="h-11 rounded-md border border-white/20 bg-slate-950/40 px-4 text-sm font-semibold text-[#C7FFF1] transition hover:bg-[#00D4AA]/20 disabled:cursor-not-allowed disabled:opacity-50"
            disabled={!webUsbAvailable}
          >
            Detect Flipper
          </button>
          <a
            href={`/api/pack?${params.toString()}`}
            className="inline-flex h-11 items-center justify-center rounded-md bg-[#ff7a00] px-4 text-sm font-semibold text-white shadow-lg shadow-orange-950/30 transition hover:bg-[#ff9d2e]"
          >
            Download Pack
          </a>
        </div>
      </div>
      {fallbackToInstructions && (
        <p className="mt-3 text-xs leading-5 text-slate-400">
          WebUSB support varies by browser. The ZIP works offline with qFlipper SD card copy.
        </p>
      )}
    </section>
  );
}
