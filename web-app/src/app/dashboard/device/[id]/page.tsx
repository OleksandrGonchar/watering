import Link from "next/link";
import { notFound, redirect } from "next/navigation";
import { ChevronLeft } from "lucide-react";
import { prisma } from "@/lib/prisma";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { ScheduleEditor } from "@/components/ScheduleEditor";
import { AlertBanner } from "@/components/AlertBanner";
import { formatLocalIsoForDevice } from "@/lib/timezone";
import { getCurrentUserId } from "@/lib/auth";

export default async function DevicePage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const userId = await getCurrentUserId();
  if (!userId) {
    redirect("/sign-in");
  }
  const { id } = await params;

  const device = await prisma.device.findFirst({
    where: { id, ownerId: userId },
    include: {
      schedules: { orderBy: [{ type: "asc" }, { position: "asc" }] },
      wateringEvents: {
        orderBy: { wateredAt: "desc" },
        take: 20,
      },
      alerts: {
        orderBy: { createdAt: "desc" },
        take: 50,
      },
    },
  });

  if (!device) {
    notFound();
  }

  const wateringSchedules = device.schedules
    .filter((s) => s.type !== "humidifier")
    .map((s) => ({ timeLocal: s.timeLocal, durationSeconds: s.durationSeconds }));
  const humidifierSchedules = device.schedules
    .filter((s) => s.type === "humidifier")
    .map((s) => ({ timeLocal: s.timeLocal, durationSeconds: s.durationSeconds }));

  const activeOverflowSensors = device.alerts
    .filter((a) => a.type === "overflow" && a.acknowledgedAt === null)
    .map((a) => a.sensorIndex)
    .filter((n): n is number => n != null)
    .sort((a, b) => a - b);
  const hasActiveOverflow = device.alerts.some(
    (a) => a.type === "overflow" && a.acknowledgedAt === null,
  );
  const hasActiveLowWater = device.alerts.some(
    (a) => a.type === "low_water" && a.resolvedAt === null,
  );

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

      {hasActiveOverflow ? (
        <AlertBanner
          variant="overflow"
          title="Перелив води — полив зупинено"
          description={
            activeOverflowSensors.length > 0
              ? `Спрацював датчик переливу №${activeOverflowSensors.join(", №")}. Червоний світлодіод блимає, доки не натиснете кнопку на пристрої.`
              : "Спрацював датчик переливу. Червоний світлодіод блимає, доки не натиснете кнопку на пристрої."
          }
        />
      ) : null}

      {hasActiveLowWater ? (
        <AlertBanner
          variant="lowWater"
          title="Низький рівень води"
          description="Поплавок опустився до дна ємності. Долийте воду — плашка зникне автоматично після наступної синхронізації."
        />
      ) : null}

      <Card>
        <CardHeader>
          <CardTitle>Розклад поливу</CardTitle>
        </CardHeader>
        <CardContent>
          <ScheduleEditor
            deviceId={device.id}
            type="watering"
            initialSchedules={wateringSchedules}
          />
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Розклад зволоження</CardTitle>
        </CardHeader>
        <CardContent>
          <ScheduleEditor
            deviceId={device.id}
            type="humidifier"
            maxDuration={1800}
            initialSchedules={humidifierSchedules}
            emptyHint="Немає розкладів — зволоження не запускатиметься."
          />
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Останні поливи та зволоження</CardTitle>
        </CardHeader>
        <CardContent>
          {device.wateringEvents.length === 0 ? (
            <p className="text-sm text-muted-foreground">
              Поки що немає записів.
            </p>
          ) : (
            <ul className="divide-y text-sm">
              {device.wateringEvents.map((e) => {
                const isHumidifier = e.type === "humidifier";
                return (
                  <li
                    key={e.id}
                    className="flex items-center justify-between py-2"
                  >
                    <span className="flex items-center gap-2">
                      <span
                        className={
                          isHumidifier
                            ? "rounded bg-blue-500/10 px-1.5 py-0.5 text-xs font-medium text-blue-600 dark:text-blue-400"
                            : "rounded bg-green-500/10 px-1.5 py-0.5 text-xs font-medium text-green-700 dark:text-green-400"
                        }
                      >
                        {isHumidifier ? "Зволоження" : "Полив"}
                      </span>
                      <span className="text-muted-foreground">
                        {formatLocalIsoForDevice(e.wateredAt, device.timezone)}
                      </span>
                    </span>
                    <span>{e.durationSeconds} сек</span>
                  </li>
                );
              })}
            </ul>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Історія тривог</CardTitle>
        </CardHeader>
        <CardContent>
          {device.alerts.length === 0 ? (
            <p className="text-sm text-muted-foreground">
              Тривог не було. Все добре.
            </p>
          ) : (
            <ul className="divide-y text-sm">
              {device.alerts.map((a) => {
                const isOverflow = a.type === "overflow";
                const active = isOverflow
                  ? a.acknowledgedAt === null
                  : a.resolvedAt === null;
                const label = isOverflow
                  ? `Перелив води${a.sensorIndex ? ` (датчик №${a.sensorIndex})` : ""}`
                  : "Низький рівень води";
                const closedAt = isOverflow ? a.acknowledgedAt : a.resolvedAt;
                const status = active
                  ? "Активна"
                  : isOverflow
                    ? `Підтверджено ${formatLocalIsoForDevice(closedAt!, device.timezone)}`
                    : `Вирішено ${formatLocalIsoForDevice(closedAt!, device.timezone)}`;
                return (
                  <li key={a.id} className="flex flex-col gap-0.5 py-2">
                    <div className="flex items-center justify-between">
                      <span
                        className={
                          isOverflow
                            ? "font-medium text-red-600 dark:text-red-400"
                            : "font-medium text-blue-600 dark:text-blue-400"
                        }
                      >
                        {label}
                      </span>
                      <span
                        className={
                          active
                            ? "text-xs font-semibold uppercase text-amber-600 dark:text-amber-400"
                            : "text-xs text-muted-foreground"
                        }
                      >
                        {status}
                      </span>
                    </div>
                    <span className="text-xs text-muted-foreground">
                      {formatLocalIsoForDevice(a.createdAt, device.timezone)}
                    </span>
                  </li>
                );
              })}
            </ul>
          )}
        </CardContent>
      </Card>
    </div>
  );
}
