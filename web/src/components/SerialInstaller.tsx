"use client";

import { useRef, useState } from "react";
import { FlipperSerial, FlipperSerialError, isWebSerialSupported } from "@/lib/flipper-serial";
import { DEFAULT_SETTINGS, SNIPPETS } from "@/lib/install-data";
import { profileFiles, normalizeProfile, getProfileId } from "@/lib/profiles";

type Phase =
  | { kind: "idle" }
  | { kind: "connecting" }
  | { kind: "ready" }
  | { kind: "installing"; message: string; fraction: number }
  | { kind: "success"; count: number }
  | { kind: "error"; message: string };

interface SerialInstallerProps {
  selectedIds: string[];
}

export function SerialInstaller({ selectedIds }: SerialInstallerProps) {
  const [phase, setPhase] = useState<Phase>({ kind: "idle" });
  const flipper = useRef(new FlipperSerial());

  const supported = isWebSerialSupported();

  async function connect() {
    setPhase({ kind: "connecting" });
    try {
      await flipper.current.connect();
      setPhase({ kind: "ready" });
    } catch (err) {
      const msg = err instanceof Error ? err.message : "Connection failed";
      setPhase({ kind: "error", message: msg });
    }
  }

  async function disconnect() {
    await flipper.current.disconnect();
    setPhase({ kind: "idle" });
  }

  async function install() {
    const selected = profileFiles
      .map(({ fileName, profile }) => ({
        id: getProfileId(fileName, profile),
        profile: normalizeProfile(profile, fileName),
        fileName,
      }))
      .filter(({ id }) => selectedIds.includes(id));

    const profiles = selected.map(({ id, profile }) => ({
      id,
      json: JSON.stringify(profile, null, 2) + "\n",
    }));

    const snippets = Object.entries(SNIPPETS).map(([name, content]) => ({
      name,
      content: content + "\n",
    }));

    const settingsJson = JSON.stringify(DEFAULT_SETTINGS, null, 2) + "\n";

    setPhase({ kind: "installing", message: "Starting…", fraction: 0 });

    try {
      await flipper.current.install(profiles, snippets, settingsJson, (message, fraction) => {
        setPhase({ kind: "installing", message, fraction });
      });
      setPhase({ kind: "success", count: profiles.length });
    } catch (err) {
      const msg = err instanceof FlipperSerialError ? err.message : "Installation failed";
      setPhase({ kind: "error", message: msg });
    }
  }

  async function retry() {
    await flipper.current.disconnect();
    setPhase({ kind: "idle" });
  }

  if (!supported) {
    return (
      <div className="rounded-xl border border-border bg-card/50 p-4 text-sm text-muted-foreground">
        <span className="mr-2 font-semibold text-foreground">Direct install unavailable —</span>
        Chrome or Edge required for Web Serial. Use the ZIP download below.
      </div>
    );
  }

  if (phase.kind === "idle") {
    return (
      <button
        onClick={connect}
        className="inline-flex h-11 w-full items-center justify-center gap-2 rounded-lg border border-accent/40 bg-accent/10 px-5 text-sm font-semibold text-accent transition hover:bg-accent/20"
      >
        <span>⚡</span>
        Connect Flipper &amp; Install Directly
      </button>
    );
  }

  if (phase.kind === "connecting") {
    return (
      <div className="flex h-11 items-center gap-3 rounded-lg border border-border bg-card/50 px-4 text-sm text-muted-foreground">
        <span className="inline-block h-4 w-4 animate-spin rounded-full border-2 border-accent border-t-transparent" />
        Select your Flipper in the browser prompt…
      </div>
    );
  }

  if (phase.kind === "ready") {
    return (
      <div className="flex flex-col gap-2 sm:flex-row sm:items-center">
        <div className="flex items-center gap-2 text-sm">
          <span className="font-semibold text-accent-success">✓</span>
          <span className="text-foreground font-semibold">Flipper connected</span>
        </div>
        <div className="flex gap-2 sm:ml-auto">
          <button
            onClick={disconnect}
            className="h-9 rounded-lg border border-border bg-card px-3 text-xs font-medium text-muted-foreground transition hover:text-foreground"
          >
            Disconnect
          </button>
          <button
            onClick={install}
            disabled={selectedIds.length === 0}
            className="h-9 rounded-lg bg-gradient-to-r from-accent to-accent-primary-hover px-4 text-sm font-semibold text-white shadow-lg shadow-accent/20 transition hover:shadow-accent/30 hover:scale-105 disabled:cursor-not-allowed disabled:opacity-50"
          >
            Install {selectedIds.length} profile{selectedIds.length !== 1 ? "s" : ""} to Flipper
          </button>
        </div>
      </div>
    );
  }

  if (phase.kind === "installing") {
    const pct = Math.round(phase.fraction * 100);
    return (
      <div className="space-y-2">
        <div className="flex items-center justify-between text-sm">
          <span className="text-muted-foreground">{phase.message}</span>
          <span className="font-mono text-xs text-accent">{pct}%</span>
        </div>
        <div className="h-2 overflow-hidden rounded-full bg-card">
          <div
            className="h-full rounded-full bg-gradient-to-r from-accent to-accent-primary-hover transition-all duration-300"
            style={{ width: `${pct}%` }}
          />
        </div>
      </div>
    );
  }

  if (phase.kind === "success") {
    return (
      <div className="flex items-center justify-between rounded-lg border border-accent-success/30 bg-accent-success/10 px-4 py-3">
        <div>
          <p className="font-semibold text-accent-success">
            ✓ {phase.count} profile{phase.count !== 1 ? "s" : ""} installed
          </p>
          <p className="text-xs text-muted-foreground">Launch FlipDeck from Apps on your Flipper</p>
        </div>
        <button
          onClick={retry}
          className="text-xs text-muted-foreground underline underline-offset-2 hover:text-foreground"
        >
          Disconnect
        </button>
      </div>
    );
  }

  // error
  return (
    <div className="rounded-lg border border-red-500/30 bg-red-500/10 px-4 py-3">
      <p className="font-semibold text-red-400">Installation failed</p>
      <p className="mt-0.5 text-xs text-muted-foreground">{phase.message}</p>
      <button
        onClick={retry}
        className="mt-2 text-xs font-medium text-accent underline underline-offset-2 hover:no-underline"
      >
        Try again
      </button>
    </div>
  );
}
