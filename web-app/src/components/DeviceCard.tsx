import Link from "next/link";
import { AlertTriangle, Battery, Clock, Droplets, Wifi, WifiOff } from "lucide-react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";

const ONLINE_THRESHOLD_MS = 3 * 60 * 60 * 1000;

function formatLastSeen(iso: string | null): { label: string; online: boolean } {
  if (!iso) return { label: "Ніколи не виходив на зв'язок", online: false };
  const ms = Date.now() - new Date(iso).getTime();
  const online = ms < ONLINE_THRESHOLD_MS;
  if (ms < 60_000) return { label: "Щойно", online };
  if (ms < 3_600_000) return { label: `${Math.floor(ms / 60_000)} хв тому`, online };
  if (ms < 86_400_000) return { label: `${Math.floor(ms / 3_600_000)} год тому`, online };
  return { label: `${Math.floor(ms / 86_400_000)} дн тому`, online };
}

interface DeviceCardProps {
  id: string;
  name: string | null;
  timezone: string;
  lastSeenAt: string | null;
  batteryPct: number | null;
  schedulesCount: number;
  hasOverflow?: boolean;
  hasLowWater?: boolean;
}

export function DeviceCard({
  id,
  name,
  timezone,
  lastSeenAt,
  batteryPct,
  schedulesCount,
  hasOverflow = false,
  hasLowWater = false,
}: DeviceCardProps) {
  const seen = formatLastSeen(lastSeenAt);

  return (
    <Link href={`/dashboard/device/${id}`} className="block">
      <Card className="h-full transition hover:shadow-md">
        <CardHeader>
          <CardTitle className="flex items-center justify-between gap-2">
            <span className="truncate">{name ?? id}</span>
            <span className="flex items-center gap-1.5">
              {hasOverflow ? (
                <AlertTriangle
                  className="h-4 w-4 text-red-600"
                  aria-label="Перелив води"
                />
              ) : null}
              {hasLowWater ? (
                <Droplets
                  className="h-4 w-4 text-blue-600"
                  aria-label="Низький рівень води"
                />
              ) : null}
              {seen.online ? (
                <Wifi className="h-4 w-4 text-green-600" />
              ) : (
                <WifiOff className="h-4 w-4 text-muted-foreground" />
              )}
            </span>
          </CardTitle>
          <p className="truncate text-xs text-muted-foreground">{id}</p>
        </CardHeader>
        <CardContent className="flex flex-col gap-2 text-sm">
          <div className="flex items-center gap-2 text-muted-foreground">
            <Clock className="h-4 w-4" />
            <span>{seen.label}</span>
          </div>
          <div className="flex items-center gap-2 text-muted-foreground">
            <Battery className="h-4 w-4" />
            <span>{batteryPct != null ? `${batteryPct}%` : "—"}</span>
          </div>
          <div className="text-xs text-muted-foreground">
            Розкладів: {schedulesCount} · TZ: {timezone}
          </div>
        </CardContent>
      </Card>
    </Link>
  );
}
