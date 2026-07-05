import { NextRequest, NextResponse } from "next/server";
import { z } from "zod";
import { prisma } from "@/lib/prisma";
import { getCurrentUserId } from "@/lib/auth";

type RouteCtx = { params: Promise<{ id: string }> };

async function getOwnedDeviceOr404(
  userId: string,
  deviceId: string,
) {
  return prisma.device.findFirst({
    where: { id: deviceId, ownerId: userId },
  });
}

export async function GET(_req: NextRequest, ctx: RouteCtx) {
  const userId = await getCurrentUserId();
  if (!userId) {
    return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  }
  const { id } = await ctx.params;

  const device = await getOwnedDeviceOr404(userId, id);
  if (!device) {
    return NextResponse.json({ error: "not_found" }, { status: 404 });
  }

  const schedules = await prisma.schedule.findMany({
    where: { deviceId: device.id },
    orderBy: [{ type: "asc" }, { position: "asc" }],
  });

  return NextResponse.json({
    deviceId: device.id,
    configVersion: device.configVersion,
    schedules: schedules.map((s) => ({
      id: s.id,
      timeLocal: s.timeLocal,
      durationSeconds: s.durationSeconds,
      position: s.position,
      type: s.type,
    })),
  });
}

const HHMM = /^([01]\d|2[0-3]):[0-5]\d$/;

// Max run length differs per actuator: watering pump is short bursts, the
// humidifier may mist for longer (the board stays awake during the run).
const MAX_DURATION_BY_TYPE = { watering: 600, humidifier: 1800 } as const;

const putSchema = z.object({
  type: z.enum(["watering", "humidifier"]).default("watering"),
  schedules: z
    .array(
      z.object({
        timeLocal: z.string().regex(HHMM, "Use HH:MM 24h format"),
        durationSeconds: z.number().int().min(1).max(1800),
      }),
    )
    .max(5),
});

export async function PUT(req: NextRequest, ctx: RouteCtx) {
  const userId = await getCurrentUserId();
  if (!userId) {
    return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  }
  const { id } = await ctx.params;

  const device = await getOwnedDeviceOr404(userId, id);
  if (!device) {
    return NextResponse.json({ error: "not_found" }, { status: 404 });
  }

  let body: unknown;
  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "invalid_json" }, { status: 400 });
  }

  const parsed = putSchema.safeParse(body);
  if (!parsed.success) {
    return NextResponse.json(
      { error: "invalid_request", details: parsed.error.flatten() },
      { status: 400 },
    );
  }

  const { type } = parsed.data;
  const maxDuration = MAX_DURATION_BY_TYPE[type];
  if (parsed.data.schedules.some((s) => s.durationSeconds > maxDuration)) {
    return NextResponse.json(
      { error: "duration_too_long", maxDuration, type },
      { status: 400 },
    );
  }

  // Replace only the rows of this type so the two editors (watering /
  // humidifier) don't clobber each other.
  const result = await prisma.$transaction(async (tx) => {
    await tx.schedule.deleteMany({ where: { deviceId: device.id, type } });
    if (parsed.data.schedules.length > 0) {
      await tx.schedule.createMany({
        data: parsed.data.schedules.map((s, idx) => ({
          deviceId: device.id,
          timeLocal: s.timeLocal,
          durationSeconds: s.durationSeconds,
          position: idx,
          type,
        })),
      });
    }
    return tx.device.update({
      where: { id: device.id },
      data: { configVersion: { increment: 1 } },
      include: {
        schedules: { orderBy: [{ type: "asc" }, { position: "asc" }] },
      },
    });
  });

  return NextResponse.json({
    deviceId: result.id,
    configVersion: result.configVersion,
    schedules: result.schedules.map((s) => ({
      id: s.id,
      timeLocal: s.timeLocal,
      durationSeconds: s.durationSeconds,
      position: s.position,
      type: s.type,
    })),
  });
}
