import type { Metadata } from "next";
import { ClerkProvider } from "@clerk/nextjs";
import { isLocalAuthMode } from "@/lib/authMode";
import "./globals.css";

export const metadata: Metadata = {
  title: "Smart Watering",
  description: "Розумний полив рослин",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  const html = (
    <html lang="uk">
      <body className="min-h-screen bg-background antialiased">{children}</body>
    </html>
  );
  return isLocalAuthMode() ? html : <ClerkProvider>{html}</ClerkProvider>;
}
