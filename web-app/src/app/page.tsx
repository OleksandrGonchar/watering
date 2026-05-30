import Link from "next/link";
import { redirect } from "next/navigation";
import { getCurrentUserId } from "@/lib/auth";

export default async function HomePage() {
  const userId = await getCurrentUserId();
  if (userId) {
    redirect("/dashboard");
  }

  return (
    <main className="container mx-auto flex min-h-screen flex-col items-center justify-center gap-6 p-8 text-center">
      <h1 className="text-4xl font-bold tracking-tight sm:text-5xl">
        Smart Watering
      </h1>
      <p className="max-w-xl text-muted-foreground">
        Розумна система автополиву ваших кімнатних рослин з керуванням
        розкладом через web-кабінет.
      </p>
      <div className="flex gap-3">
        <Link
          href="/sign-in"
          className="inline-flex h-10 items-center justify-center rounded-md bg-primary px-6 text-sm font-medium text-primary-foreground shadow hover:bg-primary/90"
        >
          Увійти
        </Link>
        <Link
          href="/sign-up"
          className="inline-flex h-10 items-center justify-center rounded-md border border-input bg-background px-6 text-sm font-medium shadow-sm hover:bg-accent"
        >
          Створити акаунт
        </Link>
      </div>
    </main>
  );
}
