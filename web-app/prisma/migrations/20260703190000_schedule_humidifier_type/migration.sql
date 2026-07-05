-- AlterTable
ALTER TABLE "Schedule" ADD COLUMN "type" TEXT NOT NULL DEFAULT 'watering';

-- AlterTable
ALTER TABLE "WateringEvent" ADD COLUMN "type" TEXT NOT NULL DEFAULT 'watering';
