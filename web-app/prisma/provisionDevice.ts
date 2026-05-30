/**
 * CLI: provision a new device + credential pair, print a ready-to-paste config.h
 *
 * Usage:
 *   pnpm provision-device \
 *     --id=ESP_BEDROOM_FLOWERS_01 \
 *     --username=batch-2026-05 \
 *     --password='<random>' \
 *     --claim-code=A1B2C3 \
 *     --tz=Europe/Kyiv \
 *     [--name='Фікус'] \
 *     [--server=https://your-app.vercel.app]
 *
 * If --password / --claim-code are omitted, secure random values are generated.
 */
import { PrismaClient } from "@prisma/client";
import bcrypt from "bcryptjs";
import { randomBytes } from "node:crypto";

const prisma = new PrismaClient();

type Args = {
  id: string;
  username: string;
  password: string;
  claimCode: string;
  tz: string;
  name?: string;
  server?: string;
};

function parseArgs(): Args {
  const out: Record<string, string> = {};
  for (const arg of process.argv.slice(2)) {
    const m = arg.match(/^--([^=]+)=(.*)$/);
    if (m) out[m[1]] = m[2];
  }

  const id = out.id;
  const username = out.username;
  const tz = out.tz ?? "Europe/Kyiv";
  if (!id || !username) {
    console.error(
      "Missing required args. Required: --id=<DEVICE_ID> --username=<group>",
    );
    console.error(
      "Optional: --password=<…> --claim-code=<…> --tz=Europe/Kyiv --name='…' --server=https://…",
    );
    process.exit(1);
  }

  const password = out.password ?? randomBytes(18).toString("base64url");
  const claimCode =
    out["claim-code"] ?? randomBytes(3).toString("hex").toUpperCase();

  return {
    id,
    username,
    password,
    claimCode,
    tz,
    name: out.name,
    server: out.server,
  };
}

async function main() {
  const args = parseArgs();

  const passwordHash = await bcrypt.hash(args.password, 10);
  const claimCodeHash = await bcrypt.hash(args.claimCode, 10);

  let credential = await prisma.deviceCredential.findUnique({
    where: { username: args.username },
  });

  if (!credential) {
    credential = await prisma.deviceCredential.create({
      data: {
        username: args.username,
        passwordHash,
        label: args.name ?? null,
      },
    });
    console.error(
      `[provision] created new credential username='${args.username}' (id=${credential.id})`,
    );
  } else {
    console.error(
      `[provision] reusing existing credential username='${args.username}' (id=${credential.id}). Password NOT changed.`,
    );
  }

  const existing = await prisma.device.findUnique({ where: { id: args.id } });
  if (existing) {
    console.error(
      `[provision] device '${args.id}' already exists. Aborting to avoid overwrite.`,
    );
    process.exit(2);
  }

  await prisma.device.create({
    data: {
      id: args.id,
      credentialId: credential.id,
      name: args.name ?? null,
      timezone: args.tz,
      claimCodeHash,
    },
  });

  const serverUrl = args.server ?? "https://your-app.vercel.app";

  const credentialPassword = credential.passwordHash === passwordHash
    ? args.password
    : "<password set when credential was first created — reuse it>";

  console.log("");
  console.log("# === Paste into firmware/include/config.h ===");
  console.log("#ifndef CONFIG_H");
  console.log("#define CONFIG_H");
  console.log("");
  console.log(`#define WIFI_SSID    "<your-wifi-ssid>"`);
  console.log(`#define WIFI_PASS    "<your-wifi-pass>"`);
  console.log("");
  console.log(`#define DEVICE_ID    "${args.id}"`);
  console.log(`#define DEVICE_USER  "${args.username}"`);
  console.log(`#define DEVICE_PASS  "${credentialPassword}"`);
  console.log(`#define CLAIM_CODE   "${args.claimCode}"`);
  console.log(`#define SERVER_URL   "${serverUrl}"`);
  console.log("");
  console.log("#endif");
  console.log("# === End config.h ===");
  console.log("");
  console.log(`Device claim code (give to the user): ${args.claimCode}`);
  console.log(`Timezone: ${args.tz}`);
  if (credential.passwordHash === passwordHash) {
    console.log(`Credential password (save securely): ${args.password}`);
  }
}

main()
  .catch((e) => {
    console.error(e);
    process.exit(1);
  })
  .finally(async () => {
    await prisma.$disconnect();
  });
