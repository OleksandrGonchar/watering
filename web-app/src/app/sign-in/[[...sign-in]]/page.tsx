import { redirect } from "next/navigation";
import { isLocalAuthMode } from "@/lib/authMode";

export default async function Page() {
  if (isLocalAuthMode()) {
    redirect("/dashboard");
  }
  const { SignIn } = await import("@clerk/nextjs");
  return (
    <main className="flex min-h-screen items-center justify-center p-6">
      <SignIn />
    </main>
  );
}
