#ifndef WATERING_RTC_H
#define WATERING_RTC_H

#include <Arduino.h>
#include <RTClib.h>

namespace rtc {

// Initialize I2C and DS3231. Returns false if RTC is missing/lost-power.
bool begin();

// Current time as DateTime (local time of the device's timezone — RTC is
// stored in local wall clock, not UTC).
DateTime now();

// Apply server-provided current local time. The server sends a string in the
// form "YYYY-MM-DDTHH:MM:SS" already converted to the device's IANA timezone.
// Returns true if the RTC was actually adjusted (drift > 30 seconds).
bool applyServerLocalTime(const String& iso);

// Format a DateTime as "YYYY-MM-DDTHH:MM:SS" (no timezone suffix).
String toLocalIso(const DateTime& dt);

// Format a DateTime as "YYYY-MM-DD" (calendar day in local time).
String toDateString(const DateTime& dt);

}  // namespace rtc

#endif
