import { NextRequest, NextResponse } from "next/server";
import { verifyAccessToken } from "@/lib/deviceAuth";

/**
 * Extract and verify the Bearer device JWT from a request.
 * Returns the deviceId or a NextResponse with the appropriate error.
 */
export async function authenticateDevice(
  req: NextRequest,
): Promise<{ deviceId: string } | NextResponse> {
  const auth = req.headers.get("authorization") ?? "";
  const m = auth.match(/^Bearer (.+)$/i);
  if (!m) {
    return NextResponse.json(
      { error: "missing_token" },
      { status: 401 },
    );
  }

  try {
    const payload = await verifyAccessToken(m[1]);
    return { deviceId: payload.sub };
  } catch {
    return NextResponse.json(
      { error: "invalid_or_expired_token" },
      { status: 401 },
    );
  }
}
