import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "FlipDeck - USB Command Deck for Flipper Zero",
  description: "Turn your Flipper Zero into a programmable USB keyboard and command deck for developers and power users.",
  icons: { icon: "/icon.svg" },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en" className="h-full antialiased">
      <body className="min-h-full flex flex-col">{children}</body>
    </html>
  );
}
