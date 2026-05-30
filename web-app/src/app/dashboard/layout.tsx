import Link from "next/link";
import { isLocalAuthMode } from "@/lib/authMode";

async function HeaderRight() {
  if (isLocalAuthMode()) {
    return (
      <span className="rounded-full border border-dashed px-3 py-1 text-xs text-muted-foreground">
        Local mode
      </span>
    );
  }
  const { UserButton } = await import("@clerk/nextjs");
  return <UserButton afterSignOutUrl="/" />;
}

export default async function DashboardLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <div className="min-h-screen bg-background">
      <header className="border-b">
        <div className="container mx-auto flex h-14 items-center justify-between px-4">
          <Link href="/dashboard" className="font-semibold">
            Smart Watering
          </Link>
          <HeaderRight />
        </div>
      </header>
      <main className="container mx-auto px-4 py-8">{children}</main>
    </div>
  );
}
