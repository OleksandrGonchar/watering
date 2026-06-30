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
