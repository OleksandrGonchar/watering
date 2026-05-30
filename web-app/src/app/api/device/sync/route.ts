import { NextRequest, NextResponse } from "next/server";
import bcrypt from "bcryptjs";
import { z } from "zod";
import { prisma } from "@/lib/prisma";
import { authenticateDevice } from "@/lib/deviceAuthMiddleware";
import { formatLocalIsoForDevice } from "@/lib/timezone";

const DEFAULT_NEXT_WAKE_SECONDS = 30 * 60;

const wateringEventSchema = z.object({
  scheduleId: z.number().int().nullable().optional(),
  durationSeconds: z.number().int().min(0).max(3600),
  wateredAt: z.string().min(10),
});

const bodySchema = z.object({
  configVersion: z.number().int().min(0),
  claimCode: z.string().min(1).max(64).optional(),
  events: z.array(wateringEventSchema).max(50).optional(),
});

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

  const device = await prisma.device.findUnique({
    where: { id: deviceId },
    include: { schedules: { orderBy: { position: "asc" } } },
  });
  if (!device) {
    return NextResponse.json({ error: "device_not_found" }, { status: 404 });
  }

  // Persist watering events from the device.
  if (parsed.data.events && parsed.data.events.length > 0) {
    await prisma.wateringEvent.createMany({
      data: parsed.data.events.map((e) => ({
        deviceId,
        scheduleId: e.scheduleId ?? null,
        durationSeconds: e.durationSeconds,
        wateredAt: new Date(e.wateredAt),
      })),
    });
  }

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
    }));
  }

  return NextResponse.json(response);
}
