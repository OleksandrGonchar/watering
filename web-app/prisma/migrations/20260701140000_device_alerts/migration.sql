-- CreateTable
CREATE TABLE "DeviceAlert" (
    "id" SERIAL NOT NULL,
    "deviceId" TEXT NOT NULL,
    "type" TEXT NOT NULL,
    "sensorIndex" INTEGER,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "acknowledgedAt" TIMESTAMP(3),
    "resolvedAt" TIMESTAMP(3),

    CONSTRAINT "DeviceAlert_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE INDEX "DeviceAlert_deviceId_createdAt_idx" ON "DeviceAlert"("deviceId", "createdAt");

-- AddForeignKey
ALTER TABLE "DeviceAlert" ADD CONSTRAINT "DeviceAlert_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE CASCADE ON UPDATE CASCADE;
