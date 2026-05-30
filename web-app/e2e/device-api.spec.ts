import { test, expect } from "./fixtures";

/**
 * End-to-end tests for the device-side API. These exercise the same contract
 * the ESP firmware uses, but from a Playwright `request` fixture so we can
 * verify it without flashing real hardware.
 */

test.describe("POST /api/device/login", () => {
  test("rejects unknown device", async ({ request }) => {
    const r = await request.post("/api/device/login", {
      data: {
        deviceId: "ESP_TEST_DOES_NOT_EXIST",
        username: "nope",
        password: "nope",
      },
    });
    expect(r.status()).toBe(401);
  });

  test("rejects wrong password", async ({ request, testDevice }) => {
    const r = await request.post("/api/device/login", {
      data: {
        deviceId: testDevice.id,
        username: testDevice.username,
        password: "wrong-password",
      },
    });
    expect(r.status()).toBe(401);
  });

  test("returns access + refresh tokens for valid credentials", async ({
    request,
    testDevice,
  }) => {
    const r = await request.post("/api/device/login", {
      data: {
        deviceId: testDevice.id,
        username: testDevice.username,
        password: testDevice.password,
      },
    });
    expect(r.status()).toBe(200);
    const body = await r.json();
    expect(body.accessToken).toBeTruthy();
    expect(body.refreshToken).toBeTruthy();
    expect(body.accessExpiresInSec).toBeGreaterThan(0);
  });
});

test.describe("POST /api/device/sync", () => {
  test("rejects requests without bearer token", async ({ request }) => {
    const r = await request.post("/api/device/sync", {
      data: { configVersion: 0 },
    });
    expect(r.status()).toBe(401);
  });

  test("rejects garbage bearer token", async ({ request }) => {
    const r = await request.post("/api/device/sync", {
      data: { configVersion: 0 },
      headers: { authorization: "Bearer not-a-jwt" },
    });
    expect(r.status()).toBe(401);
  });

  test("happy path: claim flag + lastSeenAt + watering events persist", async ({
    request,
    prisma,
    testDevice,
  }) => {
    const login = await request.post("/api/device/login", {
      data: {
        deviceId: testDevice.id,
        username: testDevice.username,
        password: testDevice.password,
      },
    });
    expect(login.status()).toBe(200);
    const { accessToken } = await login.json();

    const before = await prisma.device.findUniqueOrThrow({
      where: { id: testDevice.id },
    });
    expect(before.lastSeenAt).toBeNull();

    const r = await request.post("/api/device/sync", {
      headers: { authorization: `Bearer ${accessToken}` },
      data: {
        configVersion: 0,
        events: [
          {
            scheduleId: null,
            durationSeconds: 12,
            wateredAt: new Date().toISOString(),
          },
        ],
      },
    });
    expect(r.status()).toBe(200);
    const body = await r.json();
    expect(body).toMatchObject({
      claimed: false,
      configChanged: true,
      configVersion: 1,
      nextWakeSeconds: expect.any(Number),
    });
    expect(typeof body.currentLocalTime).toBe("string");
    expect(Array.isArray(body.schedules)).toBe(true);

    const after = await prisma.device.findUniqueOrThrow({
      where: { id: testDevice.id },
    });
    expect(after.lastSeenAt).not.toBeNull();

    const events = await prisma.wateringEvent.findMany({
      where: { deviceId: testDevice.id },
    });
    expect(events).toHaveLength(1);
    expect(events[0].durationSeconds).toBe(12);
  });

  test("configChanged is false on second sync with same configVersion", async ({
    request,
    testDevice,
    prisma,
  }) => {
    const login = await request.post("/api/device/login", {
      data: {
        deviceId: testDevice.id,
        username: testDevice.username,
        password: testDevice.password,
      },
    });
    const { accessToken } = await login.json();

    const v1 = await request.post("/api/device/sync", {
      headers: { authorization: `Bearer ${accessToken}` },
      data: { configVersion: 0 },
    });
    const v1Body = await v1.json();
    const currentVersion = v1Body.configVersion;

    const v2 = await request.post("/api/device/sync", {
      headers: { authorization: `Bearer ${accessToken}` },
      data: { configVersion: currentVersion },
    });
    const v2Body = await v2.json();
    expect(v2Body.configChanged).toBe(false);
    expect(v2Body.schedules).toBeUndefined();

    // sanity: no schedules in DB for this device.
    const schedules = await prisma.schedule.findMany({
      where: { deviceId: testDevice.id },
    });
    expect(schedules).toHaveLength(0);
  });
});

test.describe("POST /api/device/refresh", () => {
  test("rotates refresh token; old token is then invalid", async ({
    request,
    testDevice,
  }) => {
    const login = await request.post("/api/device/login", {
      data: {
        deviceId: testDevice.id,
        username: testDevice.username,
        password: testDevice.password,
      },
    });
    const { refreshToken: t1 } = await login.json();

    const r1 = await request.post("/api/device/refresh", {
      data: { refreshToken: t1 },
    });
    expect(r1.status()).toBe(200);
    const r1Body = await r1.json();
    expect(r1Body.accessToken).toBeTruthy();
    expect(r1Body.refreshToken).toBeTruthy();
    expect(r1Body.refreshToken).not.toBe(t1);

    // Replay attack: reusing the old refresh token must fail.
    const replay = await request.post("/api/device/refresh", {
      data: { refreshToken: t1 },
    });
    expect(replay.status()).toBe(401);
  });

  test("replaying revoked token nukes ALL active refresh tokens for device", async ({
    request,
    prisma,
    testDevice,
  }) => {
    const login = await request.post("/api/device/login", {
      data: {
        deviceId: testDevice.id,
        username: testDevice.username,
        password: testDevice.password,
      },
    });
    const { refreshToken: t1 } = await login.json();

    // Rotate once, t1 -> t2. t1 is now revoked.
    const r1 = await request.post("/api/device/refresh", {
      data: { refreshToken: t1 },
    });
    const { refreshToken: t2 } = await r1.json();

    // Replay t1 -> 401, should also revoke t2 (defence-in-depth).
    const replay = await request.post("/api/device/refresh", {
      data: { refreshToken: t1 },
    });
    expect(replay.status()).toBe(401);

    // t2 must now also be unusable.
    const t2Use = await request.post("/api/device/refresh", {
      data: { refreshToken: t2 },
    });
    expect(t2Use.status()).toBe(401);

    const live = await prisma.deviceRefreshToken.findMany({
      where: { deviceId: testDevice.id, revokedAt: null },
    });
    expect(live).toHaveLength(0);
  });
});

test.describe("Public marketing pages", () => {
  test("home or dashboard renders Ukrainian copy via SSR", async ({ request }) => {
    const r = await request.get("/");
    expect(r.status()).toBe(200);
    const html = await r.text();
    expect(html).toContain("Smart Watering");
    // Either landing page (Clerk mode, no session) or dashboard (local mode).
    const hasLanding = html.includes("Увійти") && html.includes("Створити акаунт");
    const hasDashboard = html.includes("Мої пристрої");
    expect(hasLanding || hasDashboard).toBe(true);
  });
});
