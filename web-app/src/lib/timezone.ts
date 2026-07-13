import { formatInTimeZone } from "date-fns-tz";

/**
 * Format a Date as a wall-clock ISO string in the given IANA timezone.
 * Example: 2026-05-30T12:01:23 (no timezone suffix — the device already
 * keeps its RTC in its own local time).
 */
export function formatLocalIsoForDevice(
  date: Date,
  timezone: string,
): string {
  return formatInTimeZone(date, timezone, "yyyy-MM-dd'T'HH:mm:ss");
}

/** Earliest wateredAt we accept from a device (pre-2024 = almost certainly a bogus RTC). */
const WATERED_AT_MIN_MS = Date.UTC(2024, 0, 1);

/** Allow a small clock skew ahead of the server. */
const WATERED_AT_MAX_SKEW_MS = 24 * 60 * 60 * 1000;

/**
 * Parse a device-reported wateredAt ISO string into a Date safe for Prisma.
 * Unset/corrupt DS1302 clocks often send values like `2000-00-00T00:00:00`
 * (Invalid Date) or absurd years; those would make createMany throw and abort
 * the whole /sync. Fall back to `fallback` (usually server now) so the event
 * is still recorded and the rest of sync can proceed.
 */
export function parseDeviceWateredAt(
  iso: string,
  fallback: Date = new Date(),
): Date {
  const parsed = new Date(iso);
  if (Number.isNaN(parsed.getTime())) return fallback;

  const maxMs = Date.now() + WATERED_AT_MAX_SKEW_MS;
  if (parsed.getTime() < WATERED_AT_MIN_MS || parsed.getTime() > maxMs) {
    return fallback;
  }
  return parsed;
}

/**
 * Common IANA timezones offered in the UI.
 */
export const COMMON_TIMEZONES = [
  "Europe/Kyiv",
  "Europe/Warsaw",
  "Europe/Berlin",
  "Europe/London",
  "Europe/Lisbon",
  "America/New_York",
  "America/Los_Angeles",
  "Asia/Tbilisi",
  "Asia/Tashkent",
  "UTC",
];

/**
 * Validate that the given string is a known IANA timezone supported by Intl.
 */
export function isValidTimezone(tz: string): boolean {
  try {
    new Intl.DateTimeFormat("en-US", { timeZone: tz }).format(new Date());
    return true;
  } catch {
    return false;
  }
}
