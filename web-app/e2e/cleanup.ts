/**
 * Manual cleanup of e2e artifacts. Useful if a test crashed and left orphan
 * Device / DeviceCredential rows in the DB.
 *
 * Usage: pnpm test:e2e:clean
 */
import { PrismaClient } from "@prisma/client";
import { TEST_DEVICE_PREFIX, TEST_USERNAME_PREFIX } from "./fixtures";

const prisma = new PrismaClient();

async function main() {
  const devices = await prisma.device.deleteMany({
    where: { id: { startsWith: TEST_DEVICE_PREFIX } },
  });
  const creds = await prisma.deviceCredential.deleteMany({
    where: { username: { startsWith: TEST_USERNAME_PREFIX } },
  });
  console.log(
    `[cleanup] removed ${devices.count} test device(s) and ${creds.count} test credential(s)`,
  );
}

main()
  .catch((e) => {
    console.error(e);
    process.exit(1);
  })
  .finally(() => prisma.$disconnect());
