#include "rtc.h"
#include <Wire.h>

namespace {
RTC_DS3231 g_rtc;
bool g_initialized = false;
}  // namespace

namespace rtc {

bool begin() {
  Wire.begin();  // SDA=D2 (GPIO4), SCL=D1 (GPIO5)
  if (!g_rtc.begin()) {
    Serial.println("[rtc] DS3231 not found");
    return false;
  }
  if (g_rtc.lostPower()) {
    Serial.println("[rtc] DS3231 lost power, time may be wrong until first sync");
  }
  g_initialized = true;
  return true;
}

DateTime now() {
  if (!g_initialized) return DateTime((uint32_t)0);
  return g_rtc.now();
}

bool applyServerLocalTime(const String& iso) {
  if (!g_initialized) return false;
  if (iso.length() < 19) return false;

  int year = iso.substring(0, 4).toInt();
  int month = iso.substring(5, 7).toInt();
  int day = iso.substring(8, 10).toInt();
  int hour = iso.substring(11, 13).toInt();
  int minute = iso.substring(14, 16).toInt();
  int second = iso.substring(17, 19).toInt();
  if (year < 2024 || month < 1 || month > 12) return false;

  DateTime serverTime(year, month, day, hour, minute, second);
  DateTime current = g_rtc.now();
  long drift = (long)serverTime.unixtime() - (long)current.unixtime();
  if (drift < 0) drift = -drift;

  if (drift > 30) {
    g_rtc.adjust(serverTime);
    Serial.printf("[rtc] adjusted by %ld seconds\n", drift);
    return true;
  }
  return false;
}

String toLocalIso(const DateTime& dt) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02u",
           dt.year(), dt.month(), dt.day(),
           dt.hour(), dt.minute(), dt.second());
  return String(buf);
}

String toDateString(const DateTime& dt) {
  char buf[11];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u", dt.year(), dt.month(), dt.day());
  return String(buf);
}

}  // namespace rtc
