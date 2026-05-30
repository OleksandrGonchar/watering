import { SignJWT, jwtVerify } from "jose";
import { createHash, randomBytes } from "node:crypto";
import bcrypt from "bcryptjs";
import { prisma } from "@/lib/prisma";

const ACCESS_TOKEN_TTL_SECONDS = 15 * 60;
const REFRESH_TOKEN_TTL_SECONDS = 60 * 24 * 60 * 60;

function getJwtSecret(): Uint8Array {
  const secret = process.env.DEVICE_JWT_SECRET;
  if (!secret || secret.length < 32) {
    throw new Error(
      "DEVICE_JWT_SECRET env var is missing or too short (need >=32 chars)",
    );
  }
  return new TextEncoder().encode(secret);
}

export type DeviceTokenPayload = {
  sub: string;
  type: "device";
  iat?: number;
  exp?: number;
};

export async function signAccessToken(deviceId: string): Promise<string> {
  return new SignJWT({ sub: deviceId, type: "device" })
    .setProtectedHeader({ alg: "HS256" })
    .setIssuedAt()
    .setExpirationTime(`${ACCESS_TOKEN_TTL_SECONDS}s`)
    .setSubject(deviceId)
    .sign(getJwtSecret());
}

export async function verifyAccessToken(
  token: string,
): Promise<DeviceTokenPayload> {
  const { payload } = await jwtVerify(token, getJwtSecret(), {
    algorithms: ["HS256"],
  });
  if (payload.type !== "device" || typeof payload.sub !== "string") {
    throw new Error("Invalid token payload");
  }
  return payload as DeviceTokenPayload;
}

function hashRefreshToken(token: string): string {
  return createHash("sha256").update(token).digest("hex");
}

export async function issueRefreshToken(deviceId: string): Promise<string> {
  const raw = randomBytes(32).toString("hex");
  const tokenHash = hashRefreshToken(raw);
  const expiresAt = new Date(Date.now() + REFRESH_TOKEN_TTL_SECONDS * 1000);

  await prisma.deviceRefreshToken.create({
    data: { deviceId, tokenHash, expiresAt },
  });

  return raw;
}

/**
 * Verify and rotate a refresh token. If the refresh token was already revoked
 * (replay attack), invalidate ALL refresh tokens for the device.
 *
 * Returns the device id and a freshly issued raw refresh token.
 */
export async function rotateRefreshToken(
  rawRefreshToken: string,
): Promise<{ deviceId: string; newRefreshToken: string } | null> {
  const tokenHash = hashRefreshToken(rawRefreshToken);
  const record = await prisma.deviceRefreshToken.findUnique({
    where: { tokenHash },
  });

  if (!record) return null;

  if (record.revokedAt || record.expiresAt < new Date()) {
    // Replay or expired: nuke all refresh tokens for this device.
    await prisma.deviceRefreshToken.updateMany({
      where: { deviceId: record.deviceId, revokedAt: null },
      data: { revokedAt: new Date() },
    });
    return null;
  }

  await prisma.deviceRefreshToken.update({
    where: { id: record.id },
    data: { revokedAt: new Date() },
  });

  const newRefreshToken = await issueRefreshToken(record.deviceId);
  return { deviceId: record.deviceId, newRefreshToken };
}

export async function verifyDeviceCredentials(
  deviceId: string,
  username: string,
  password: string,
): Promise<{ ok: true } | { ok: false; reason: string }> {
  const device = await prisma.device.findUnique({
    where: { id: deviceId },
    include: { credential: true },
  });
  if (!device) return { ok: false, reason: "device_not_found" };
  if (device.credential.username !== username) {
    return { ok: false, reason: "credential_mismatch" };
  }
  const passwordOk = await bcrypt.compare(
    password,
    device.credential.passwordHash,
  );
  if (!passwordOk) return { ok: false, reason: "credential_mismatch" };
  return { ok: true };
}

export const TOKEN_TTL = {
  accessSeconds: ACCESS_TOKEN_TTL_SECONDS,
  refreshSeconds: REFRESH_TOKEN_TTL_SECONDS,
};
