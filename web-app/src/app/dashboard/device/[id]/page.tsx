import Link from "next/link";
import { auth } from "@clerk/nextjs/server";
import { notFound, redirect } from "next/navigation";
import { ChevronLeft } from "lucide-react";
import { prisma } from "@/lib/prisma";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { ScheduleEditor } from "@/components/ScheduleEditor";
import { formatLocalIsoForDevice } from "@/lib/timezone";

export default async function DevicePage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { userId } = await auth();
  if (!userId) {
    redirect("/sign-in");
  }
  const { id } = await params;

  const device = await prisma.device.findFirst({
    where: { id, ownerId: userId },
    include: {
      schedules: { orderBy: { position: "asc" } },
      wateringEvents: {
        orderBy: { wateredAt: "desc" },
        take: 20,
      },
    },
  });

  if (!device) {
    notFound();
  }

  return (
    <div className="flex flex-col gap-6">
      <div>
        <Link
          href="/dashboard"
          className="inline-flex items-center text-sm text-muted-foreground hover:underline"
        >
          <ChevronLeft className="h-4 w-4" />
          Усі пристрої
        </Link>
        <h1 className="mt-2 text-2xl font-semibold">
          {device.name ?? device.id}
        </h1>
        <p className="text-xs text-muted-foreground">
          {device.id} · TZ {device.timezone} · config v{device.configVersion}
        </p>
      </div>

      <Card>
        <CardHeader>
          <CardTitle>Розклад поливу</CardTitle>
        </CardHeader>
        <CardContent>
          <ScheduleEditor
            deviceId={device.id}
            initialSchedules={device.schedules.map((s) => ({
              timeLocal: s.timeLocal,
              durationSeconds: s.durationSeconds,
            }))}
          />
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Останні поливи</CardTitle>
        </CardHeader>
        <CardContent>
          {device.wateringEvents.length === 0 ? (
            <p className="text-sm text-muted-foreground">
              Поки що немає записів про поливи.
            </p>
          ) : (
            <ul className="divide-y text-sm">
              {device.wateringEvents.map((e) => (
                <li
                  key={e.id}
                  className="flex items-center justify-between py-2"
                >
                  <span className="text-muted-foreground">
                    {formatLocalIsoForDevice(e.wateredAt, device.timezone)}
                  </span>
                  <span>{e.durationSeconds} сек</span>
                </li>
              ))}
            </ul>
          )}
        </CardContent>
      </Card>
    </div>
  );
}
