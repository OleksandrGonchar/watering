#include "rtc.h"

#include <ThreeWire.h>
#include <RtcDS1302.h>

// DS1302 (MH-Real-Time Clock Module-2) — 3-wire serial interface.
//   CLK (SCLK) -> D1 / GPIO5
//   DAT (IO)   -> D2 / GPIO4
//   RST (CE)   -> D5 / GPIO14
// NOTE: the module's "RST" pin is the chip-enable line of the DS1302, NOT the
// ESP8266 reset. Do not confuse it with the D0<->RST deep-sleep jumper.
namespace {

constexpr uint8_t DS1302_CLK = 5;   // D1
constexpr uint8_t DS1302_DAT = 4;   // D2
constexpr uint8_t DS1302_RST = 14;  // D5

ThreeWire g_wire(DS1302_DAT, DS1302_CLK, DS1302_RST);  // (IO, SCLK, CE)
RtcDS1302<ThreeWire> g_rtc(g_wire);
bool g_initialized = false;

// Convert a Makuna RtcDateTime into the RTClib DateTime used by the rest of
// the firmware. RtcDateTime::Year() already returns the full year (e.g. 2026).
DateTime toDateTime(const RtcDateTime& dt) {
  return DateTime(dt.Year(), dt.Month(), dt.Day(),
                  dt.Hour(), dt.Minute(), dt.Second());
}

}  // namespace

namespace rtc {

bool begin() {
  g_rtc.Begin();

  // The DS1302 ships write-protected and possibly halted. Clear both so we can
  // adjust the time and so the oscillator actually runs.
  if (g_rtc.GetIsWriteProtected()) {
    g_rtc.SetIsWriteProtected(false);
  }
  if (!g_rtc.GetIsRunning()) {
    Serial.println("[rtc] DS1302 was halted, starting oscillator");
    g_rtc.SetIsRunning(true);
  }

  if (!g_rtc.IsDateTimeValid()) {
    // Battery/cap empty or first power-on: time is bogus until the first
    // server sync corrects it.
    Serial.println("[rtc] DS1302 time invalid, will rely on first server sync");
  }

  g_initialized = true;
  return true;
}

DateTime now() {
  if (!g_initialized) return DateTime((uint32_t)0);
  return toDateTime(g_rtc.GetDateTime());
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

  RtcDateTime serverTime(year, month, day, hour, minute, second);
  RtcDateTime current = g_rtc.GetDateTime();
  long drift = (long)serverTime.Epoch32Time() - (long)current.Epoch32Time();
  if (drift < 0) drift = -drift;

  if (drift > 30 || !g_rtc.IsDateTimeValid()) {
    if (g_rtc.GetIsWriteProtected()) g_rtc.SetIsWriteProtected(false);
    g_rtc.SetDateTime(serverTime);
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
