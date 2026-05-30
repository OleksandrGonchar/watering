import { test as base } from "@playwright/test";
import { PrismaClient } from "@prisma/client";
import bcrypt from "bcryptjs";
import { randomBytes } from "node:crypto";

export const TEST_DEVICE_PREFIX = "ESP_TEST_";
export const TEST_USERNAME_PREFIX = "test-";

export type TestDevice = {
  id: string;
  username: string;
  password: string;
  claimCode: string;
  credentialId: string;
};

type Fixtures = {
  prisma: PrismaClient;
  testDevice: TestDevice;
};

const sharedPrisma = new PrismaClient();

export const test = base.extend<Fixtures>({
  prisma: async ({}, use) => {
    await use(sharedPrisma);
  },

  testDevice: async ({ prisma }, use) => {
    const id = `${TEST_DEVICE_PREFIX}${randomBytes(4).toString("hex").toUpperCase()}`;
    const username = `${TEST_USERNAME_PREFIX}${randomBytes(4).toString("hex")}`;
    const password = `pw_${randomBytes(8).toString("hex")}`;
    const claimCode = randomBytes(3).toString("hex").toUpperCase();

    const credential = await prisma.deviceCredential.create({
      data: {
        username,
        passwordHash: await bcrypt.hash(password, 4),
        label: "e2e",
      },
    });

    await prisma.device.create({
      data: {
        id,
        credentialId: credential.id,
        claimCodeHash: await bcrypt.hash(claimCode, 4),
        timezone: "Europe/Kyiv",
      },
    });

    try {
      await use({
        id,
        username,
        password,
        claimCode,
        credentialId: credential.id,
      });
    } finally {
      await prisma.device.deleteMany({ where: { id } });
      await prisma.deviceCredential
        .deleteMany({ where: { id: credential.id } })
        .catch(() => {});
    }
  },
});

export { expect } from "@playwright/test";
