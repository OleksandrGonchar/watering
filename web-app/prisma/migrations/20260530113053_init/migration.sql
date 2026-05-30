-- CreateTable
CREATE TABLE "User" (
    "id" TEXT NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "User_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "DeviceCredential" (
    "id" TEXT NOT NULL,
    "username" TEXT NOT NULL,
    "passwordHash" TEXT NOT NULL,
    "label" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "DeviceCredential_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "Device" (
    "id" TEXT NOT NULL,
    "credentialId" TEXT NOT NULL,
    "ownerId" TEXT,
    "name" TEXT,
    "timezone" TEXT NOT NULL DEFAULT 'Europe/Kyiv',
    "claimCodeHash" TEXT,
    "configVersion" INTEGER NOT NULL DEFAULT 1,
    "lastSeenAt" TIMESTAMP(3),
    "batteryPct" INTEGER,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "Device_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "Schedule" (
    "id" SERIAL NOT NULL,
    "deviceId" TEXT NOT NULL,
    "timeLocal" TEXT NOT NULL,
    "durationSeconds" INTEGER NOT NULL,
    "position" INTEGER NOT NULL,

    CONSTRAINT "Schedule_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "DeviceRefreshToken" (
    "id" TEXT NOT NULL,
    "deviceId" TEXT NOT NULL,
    "tokenHash" TEXT NOT NULL,
    "expiresAt" TIMESTAMP(3) NOT NULL,
    "revokedAt" TIMESTAMP(3),
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "DeviceRefreshToken_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "WateringEvent" (
    "id" SERIAL NOT NULL,
    "deviceId" TEXT NOT NULL,
    "scheduleId" INTEGER,
    "durationSeconds" INTEGER NOT NULL,
    "wateredAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "WateringEvent_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "DeviceCredential_username_key" ON "DeviceCredential"("username");

-- CreateIndex
CREATE INDEX "Device_credentialId_idx" ON "Device"("credentialId");

-- CreateIndex
CREATE INDEX "Device_ownerId_idx" ON "Device"("ownerId");

-- CreateIndex
CREATE INDEX "Schedule_deviceId_idx" ON "Schedule"("deviceId");

-- CreateIndex
CREATE UNIQUE INDEX "DeviceRefreshToken_tokenHash_key" ON "DeviceRefreshToken"("tokenHash");

-- CreateIndex
CREATE INDEX "DeviceRefreshToken_deviceId_idx" ON "DeviceRefreshToken"("deviceId");

-- CreateIndex
CREATE INDEX "WateringEvent_deviceId_wateredAt_idx" ON "WateringEvent"("deviceId", "wateredAt");

-- AddForeignKey
ALTER TABLE "Device" ADD CONSTRAINT "Device_credentialId_fkey" FOREIGN KEY ("credentialId") REFERENCES "DeviceCredential"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Device" ADD CONSTRAINT "Device_ownerId_fkey" FOREIGN KEY ("ownerId") REFERENCES "User"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Schedule" ADD CONSTRAINT "Schedule_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "DeviceRefreshToken" ADD CONSTRAINT "DeviceRefreshToken_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "WateringEvent" ADD CONSTRAINT "WateringEvent_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE CASCADE ON UPDATE CASCADE;
