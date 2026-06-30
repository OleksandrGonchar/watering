import { NextRequest, NextResponse } from "next/server";
import { z } from "zod";
import {
  TOKEN_TTL,
  issueRefreshToken,
  signAccessToken,
  verifyDeviceCredentials,
} from "@/lib/deviceAuth";

const schema = z.object({
  deviceId: z.string().min(1).max(64),
  username: z.string().min(1).max(64),
  password: z.string().min(1).max(128),
});

export async function POST(req: NextRequest) {
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

  try {
    const { deviceId, username, password } = parsed.data;
    const verification = await verifyDeviceCredentials(deviceId, username, password);
    if (!verification.ok) {
      return NextResponse.json({ error: "unauthorized" }, { status: 401 });
    }

    const accessToken = await signAccessToken(deviceId);
    const refreshToken = await issueRefreshToken(deviceId);

    return NextResponse.json({
      accessToken,
      refreshToken,
      accessExpiresInSec: TOKEN_TTL.accessSeconds,
    });
  } catch (err) {
    console.error("[device/login] unexpected error:", err);
    return NextResponse.json({ error: "server_error" }, { status: 500 });
  }
}
