import { NextRequest, NextResponse } from "next/server";
import { auth } from "@clerk/nextjs/server";
import bcrypt from "bcryptjs";
import { z } from "zod";
import { prisma } from "@/lib/prisma";
import { isValidTimezone } from "@/lib/timezone";

const schema = z.object({
  deviceId: z.string().min(1).max(64),
  claimCode: z.string().min(1).max(64),
  name: z.string().min(1).max(80).optional(),
  timezone: z.string().min(1).max(64).optional(),
});

export async function POST(req: NextRequest) {
  const { userId } = await auth();
  if (!userId) {
    return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  }

  let body: unknown;
  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "invalid_json" }, { status: 400 });
  }

  const parsed = schema.safeParse(body);
  if (!parsed.success) {
    return NextResponse.json({ error: "invalid_request" }, { status: 400 });
  }

  if (parsed.data.timezone && !isValidTimezone(parsed.data.timezone)) {
    return NextResponse.json({ error: "invalid_timezone" }, { status: 400 });
  }

  // Ensure the User row exists (lazy create on first claim).
  await prisma.user.upsert({
    where: { id: userId },
    update: {},
    create: { id: userId },
  });

  const device = await prisma.device.findUnique({
    where: { id: parsed.data.deviceId },
  });

  if (!device) {
    return NextResponse.json({ error: "device_not_found" }, { status: 404 });
  }

  if (device.ownerId && device.ownerId !== userId) {
    return NextResponse.json(
      { error: "already_claimed" },
      { status: 409 },
    );
  }

  if (!device.claimCodeHash) {
    return NextResponse.json(
      { error: "device_not_claimable" },
      { status: 409 },
    );
  }

  const claimOk = await bcrypt.compare(
    parsed.data.claimCode,
    device.claimCodeHash,
  );
  if (!claimOk) {
    return NextResponse.json(
      { error: "invalid_claim_code" },
      { status: 401 },
    );
  }

  const updated = await prisma.device.update({
    where: { id: device.id },
    data: {
      ownerId: userId,
      name: parsed.data.name ?? device.name,
      timezone: parsed.data.timezone ?? device.timezone,
      claimCodeHash: null,
    },
  });

  return NextResponse.json({
    device: {
      id: updated.id,
      name: updated.name,
      timezone: updated.timezone,
    },
  });
}
