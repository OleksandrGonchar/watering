import Link from "next/link";
import { redirect } from "next/navigation";
import { prisma } from "@/lib/prisma";
import { DeviceCard } from "@/components/DeviceCard";
import { Button } from "@/components/ui/button";
import { getCurrentUserId } from "@/lib/auth";

export default async function DashboardPage() {
  const userId = await getCurrentUserId();
  if (!userId) {
    redirect("/sign-in");
  }

  await prisma.user.upsert({
    where: { id: userId },
    update: {},
    create: { id: userId },
  });

  const devices = await prisma.device.findMany({
    where: { ownerId: userId },
    include: {
      schedules: { select: { id: true } },
      alerts: {
        where: {
          OR: [
            { type: "overflow", acknowledgedAt: null },
            { type: "low_water", resolvedAt: null },
          ],
        },
        select: { type: true },
      },
    },
    orderBy: { createdAt: "asc" },
  });

  return (
    <div className="flex flex-col gap-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold">Мої пристрої</h1>
          <p className="text-sm text-muted-foreground">
            Усього: {devices.length}
          </p>
        </div>
        <Button asChild>
          <Link href="/dashboard/claim">+ Додати пристрій</Link>
        </Button>
      </div>

      {devices.length === 0 ? (
        <div className="rounded-xl border border-dashed p-10 text-center text-muted-foreground">
          У вас ще немає прив&apos;язаних пристроїв. Натисніть{" "}
          <strong>«Додати пристрій»</strong>, щоб ввести DEVICE_ID та CLAIM_CODE.
        </div>
      ) : (
        <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {devices.map((d) => (
            <DeviceCard
              key={d.id}
              id={d.id}
              name={d.name}
              timezone={d.timezone}
              lastSeenAt={d.lastSeenAt?.toISOString() ?? null}
              batteryPct={d.batteryPct}
              schedulesCount={d.schedules.length}
              hasOverflow={d.alerts.some((a) => a.type === "overflow")}
              hasLowWater={d.alerts.some((a) => a.type === "low_water")}
            />
          ))}
        </div>
      )}
    </div>
  );
}
