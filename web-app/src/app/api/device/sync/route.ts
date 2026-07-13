import { NextRequest, NextResponse } from "next/server";
import bcrypt from "bcryptjs";
import { z } from "zod";
import { prisma } from "@/lib/prisma";
import { authenticateDevice } from "@/lib/deviceAuthMiddleware";
import {
  formatLocalIsoForDevice,
  parseDeviceWateredAt,
} from "@/lib/timezone";

const DEFAULT_NEXT_WAKE_SECONDS = 30 * 60;

const wateringEventSchema = z.object({
  scheduleId: z.number().int().nullable().optional(),
  durationSeconds: z.number().int().min(0).max(3600),
  wateredAt: z.string().min(10),
  type: z.enum(["watering", "humidifier"]).optional(),
});

const alertsSchema = z
  .object({
    overflow: z
      .object({
        active: z.boolean(),
        sensors: z.array(z.number().int().min(1).max(2)).max(2).optional(),
      })
      .optional(),
    lowWater: z.object({ active: z.boolean() }).optional(),
    ackOverflow: z.boolean().optional(),
  })
  .optional();

const bodySchema = z.object({
  configVersion: z.number().int().min(0),
  claimCode: z.string().min(1).max(64).optional(),
  events: z.array(wateringEventSchema).max(50).optional(),
  alerts: alertsSchema,
});

type AlertsPayload = z.infer<typeof alertsSchema>;

// Idempotently reconcile the device-reported alert state into DeviceAlert rows.
// The device re-reports its live state every sync, so we only create a row when
// there isn't already an open one, and close open rows when the condition
// clears. Rows are never deleted, keeping the full history.
async function reconcileAlerts(deviceId: string, alerts: AlertsPayload) {
  if (!alerts) return;

  // Overflow acknowledgement (button pressed on the device): hide the red
  // banner by stamping acknowledgedAt on every still-open overflow row.
  if (alerts.ackOverflow) {
    await prisma.deviceAlert.updateMany({
      where: { deviceId, type: "overflow", acknowledgedAt: null },
      data: { acknowledgedAt: new Date() },
    });
  }

  if (alerts.overflow?.active) {
    const sensors =
      alerts.overflow.sensors && alerts.overflow.sensors.length > 0
        ? alerts.overflow.sensors
        : [null];
    for (const sensorIndex of sensors) {
      const open = await prisma.deviceAlert.findFirst({
        where: {
          deviceId,
          type: "overflow",
          sensorIndex: sensorIndex ?? null,
          acknowledgedAt: null,
        },
      });
      if (!open) {
        await prisma.deviceAlert.create({
          data: { deviceId, type: "overflow", sensorIndex: sensorIndex ?? null },
        });
      }
    }
  }

  if (alerts.lowWater) {
    if (alerts.lowWater.active) {
      const open = await prisma.deviceAlert.findFirst({
        where: { deviceId, type: "low_water", resolvedAt: null },
      });
      if (!open) {
        await prisma.deviceAlert.create({
          data: { deviceId, type: "low_water" },
        });
      }
    } else {
      await prisma.deviceAlert.updateMany({
        where: { deviceId, type: "low_water", resolvedAt: null },
        data: { resolvedAt: new Date() },
      });
    }
  }
}

export async function POST(req: NextRequest) {
  const authResult = await authenticateDevice(req);
  if (authResult instanceof NextResponse) return authResult;
  const { deviceId } = authResult;

  let body: unknown;
  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "invalid_json" }, { status: 400 });
  }

  const parsed = bodySchema.safeParse(body);
  if (!parsed.success) {
    return NextResponse.json({ error: "invalid_request" }, { status: 400 });
  }

  try {
  const device = await prisma.device.findUnique({
    where: { id: deviceId },
    include: { schedules: { orderBy: { position: "asc" } } },
  });
  if (!device) {
    return NextResponse.json({ error: "device_not_found" }, { status: 404 });
  }

  // Persist watering events from the device. Sanitize wateredAt so a bogus
  // RTC timestamp (Invalid Date / year 2000 / far future) cannot abort sync.
  if (parsed.data.events && parsed.data.events.length > 0) {
    const syncedAt = new Date();
    await prisma.wateringEvent.createMany({
      data: parsed.data.events.map((e) => {
        const wateredAt = parseDeviceWateredAt(e.wateredAt, syncedAt);
        if (wateredAt === syncedAt && e.wateredAt) {
          console.warn(
            `[device/sync] device ${deviceId}: replaced invalid wateredAt "${e.wateredAt}" with server time`,
          );
        }
        return {
          deviceId,
          scheduleId: e.scheduleId ?? null,
          durationSeconds: e.durationSeconds,
          wateredAt,
          type: e.type ?? "watering",
        };
      }),
    });
  }

  // Raise / clear water-safety alerts (overflow, low water).
  await reconcileAlerts(deviceId, parsed.data.alerts);

  // Try to claim the device if a claim code is provided and unclaimed.
  let claimed = device.ownerId !== null;
  if (!claimed && parsed.data.claimCode && device.claimCodeHash) {
    const matches = await bcrypt.compare(
      parsed.data.claimCode,
      device.claimCodeHash,
    );
    if (matches) {
      // The actual user-facing claim happens in /api/devices/claim. The device
      // only learns whether *some* user has claimed it. We do not auto-assign
      // ownership from device-side; the claim_code merely tells the device to
      // keep advertising itself until the user confirms.
    }
  }
  // claimed flag reflects whether ownerId is set.
  claimed = device.ownerId !== null;

  await prisma.device.update({
    where: { id: deviceId },
    data: { lastSeenAt: new Date() },
  });

  const configChanged = parsed.data.configVersion !== device.configVersion;

  const response: {
    claimed: boolean;
    configChanged: boolean;
    configVersion: number;
    schedules?: Array<{
      id: number;
      timeLocal: string;
      durationSeconds: number;
      position: number;
      type: string;
    }>;
    currentLocalTime: string;
    nextWakeSeconds: number;
  } = {
    claimed,
    configChanged,
    configVersion: device.configVersion,
    currentLocalTime: formatLocalIsoForDevice(new Date(), device.timezone),
    nextWakeSeconds: DEFAULT_NEXT_WAKE_SECONDS,
  };

  if (configChanged) {
    response.schedules = device.schedules.map((s) => ({
      id: s.id,
      timeLocal: s.timeLocal,
      durationSeconds: s.durationSeconds,
      position: s.position,
      type: s.type,
    }));
  }

  return NextResponse.json(response);
  } catch (err) {
    console.error("[device/sync] unexpected error:", err);
    return NextResponse.json({ error: "server_error" }, { status: 500 });
  }
}
