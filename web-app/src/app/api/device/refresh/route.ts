import { NextRequest, NextResponse } from "next/server";
import { z } from "zod";
import {
  TOKEN_TTL,
  rotateRefreshToken,
  signAccessToken,
} from "@/lib/deviceAuth";

const schema = z.object({
  refreshToken: z.string().min(16).max(256),
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
    const rotated = await rotateRefreshToken(parsed.data.refreshToken);
    if (!rotated) {
      return NextResponse.json({ error: "unauthorized" }, { status: 401 });
    }

    const accessToken = await signAccessToken(rotated.deviceId);

    return NextResponse.json({
      accessToken,
      refreshToken: rotated.newRefreshToken,
      accessExpiresInSec: TOKEN_TTL.accessSeconds,
    });
  } catch (err) {
    console.error("[device/refresh] unexpected error:", err);
    return NextResponse.json({ error: "server_error" }, { status: 500 });
  }
}
