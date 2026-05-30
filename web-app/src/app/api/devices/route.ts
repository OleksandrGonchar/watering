import { NextResponse } from "next/server";
import { auth } from "@clerk/nextjs/server";
import { prisma } from "@/lib/prisma";

export async function GET() {
  const { userId } = await auth();
  if (!userId) {
    return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  }

  const devices = await prisma.device.findMany({
    where: { ownerId: userId },
    include: {
      schedules: { orderBy: { position: "asc" } },
    },
    orderBy: { createdAt: "asc" },
  });

  return NextResponse.json({
    devices: devices.map((d) => ({
      id: d.id,
      name: d.name,
      timezone: d.timezone,
      configVersion: d.configVersion,
      lastSeenAt: d.lastSeenAt?.toISOString() ?? null,
      batteryPct: d.batteryPct,
      schedulesCount: d.schedules.length,
    })),
  });
}
