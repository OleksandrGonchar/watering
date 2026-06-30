// Smart Watering — ESP8266 firmware entry point.
//
// Lifecycle (everything happens in setup(); loop() is empty because the chip
// goes into deep sleep at the very end):
//
//   1. Init Serial, LittleFS, DS1302 RTC, pump GPIO.
//   2. Load persisted config (schedules), state (last_run_date per schedule),
//      tokens, and pending events.
//   3. For each schedule, run the catch-up algorithm: if it's past the
//      scheduled time today AND we haven't run today AND the 6h circuit
//      breaker is satisfied, water now. Append a PendingEvent.
//   4. Connect Wi-Fi (skipped only on RTC failure or no schedules at all).
//   5. POST /sync with current configVersion + pending events. On 401, refresh
//      tokens and retry. On configChanged, persist new schedules. Sync DS1302
//      from currentLocalTime.
//   6. Save state, clear pending events on success, deep-sleep for
//      nextWakeSeconds (capped at 70 minutes per ESP.deepSleep call).
//
// Hardware: GPIO16 (D0) MUST be wired to RST for deep-sleep wake.

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "storage.h"
#include "rtc.h"
#include "pump.h"
#include "schedule.h"
#include "net.h"
#include "api.h"

namespace {

constexpr uint32_t MAX_DEEP_SLEEP_SECONDS = 70UL * 60UL;
constexpr uint32_t DEFAULT_WAKE_SECONDS = 30UL * 60UL;

void deepSleepFor(uint32_t seconds) {
  if (seconds == 0) seconds = DEFAULT_WAKE_SECONDS;
  if (seconds > MAX_DEEP_SLEEP_SECONDS) seconds = MAX_DEEP_SLEEP_SECONDS;
  Serial.printf("[main] deep sleeping for %u s\n", seconds);
  Serial.flush();
  ESP.deepSleep((uint64_t)seconds * 1000000ULL);
}

bool loadTokensFromStorage(api::Tokens& out) {
  JsonDocument doc;
  if (!storage::loadTokens(doc)) return false;
  out.accessToken = doc["accessToken"].as<const char*>() ?: "";
  out.accessExpiresAtEpoch = doc["accessExpiresAtEpoch"].as<uint32_t>();
  out.refreshToken = doc["refreshToken"].as<const char*>() ?: "";
  return out.refreshToken.length() > 0 || out.accessToken.length() > 0;
}

void saveTokensToStorage(const api::Tokens& t) {
  JsonDocument doc;
  doc["accessToken"] = t.accessToken;
  doc["accessExpiresAtEpoch"] = t.accessExpiresAtEpoch;
  doc["refreshToken"] = t.refreshToken;
  storage::saveTokens(doc);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("==== Smart Watering boot ====");
  Serial.printf("Device: %s\n", DEVICE_ID);

  pump::begin();
  bool storageOk = storage::begin();
  bool rtcOk = rtc::begin();

  if (!storageOk) {
    Serial.println("[main] storage init failed — sleeping briefly");
    deepSleepFor(60);
    return;
  }

  // === Load persistent state ===
  std::vector<schedule::Schedule> schedules;
  int configVersion = 0;
  {
    JsonDocument doc;
    if (storage::loadConfig(doc)) {
      schedule::parseConfig(doc, schedules, configVersion);
    }
  }

  std::map<int, schedule::ScheduleStateEntry> state;
  {
    JsonDocument doc;
    if (storage::loadState(doc)) schedule::parseState(doc, state);
  }

  std::vector<schedule::PendingEvent> pending;
  {
    JsonDocument doc;
    if (storage::loadPending(doc)) schedule::parsePending(doc, pending);
  }

  // === Run due schedules locally (catch-up, no network needed) ===
  if (rtcOk && !schedules.empty()) {
    DateTime now = rtc::now();
    Serial.printf("[main] now=%s, schedules=%u\n",
                  rtc::toLocalIso(now).c_str(), (unsigned)schedules.size());
    schedule::runDueSchedules(now, schedules, state, pending);

    JsonDocument doc;
    schedule::serializeState(doc, state);
    storage::saveState(doc);
  } else {
    Serial.printf("[main] skipping schedules (rtcOk=%d, schedules=%u)\n",
                  rtcOk ? 1 : 0, (unsigned)schedules.size());
  }

  if (!pending.empty()) {
    JsonDocument doc;
    schedule::serializePending(doc, pending);
    storage::savePending(doc);
  }

  // === Sync with server ===
  if (!net::connect()) {
    Serial.println("[main] WiFi failed — will retry next wake");
    deepSleepFor(DEFAULT_WAKE_SECONDS);
    return;
  }

  api::Tokens tokens;
  loadTokensFromStorage(tokens);

  uint32_t nowEpoch = rtcOk ? rtc::now().unixtime() : (millis() / 1000);
  if (!api::ensureValidToken(tokens, nowEpoch)) {
    Serial.println("[main] auth failed — will retry next wake");
    deepSleepFor(DEFAULT_WAKE_SECONDS);
    return;
  }
  saveTokensToStorage(tokens);

  // claim_code is sent until the device is claimed. We don't know the claimed
  // status until after the first /sync, so always send on first run after
  // power-on. Server ignores it once the user has claimed.
  bool sendClaimCode = true;

  api::SyncResult result = api::sync(tokens, configVersion, sendClaimCode, pending);

  // The server rejected our access token (likely expired, or our clock is
  // wrong and we wrongly considered it valid). Force a refresh/login that
  // ignores the local clock, then retry once.
  if (!result.ok && result.unauthorized) {
    Serial.println("[main] /sync 401 — forcing token refresh and retrying");
    if (api::ensureValidToken(tokens, nowEpoch, /*forceRefresh=*/true)) {
      saveTokensToStorage(tokens);
      result = api::sync(tokens, configVersion, sendClaimCode, pending);
    }
  }

  if (!result.ok) {
    Serial.println("[main] /sync failed — keeping pending events for retry");
    deepSleepFor(DEFAULT_WAKE_SECONDS);
    return;
  }

  // Successful sync: clear pending queue.
  storage::clearPending();

  // Sync RTC if drift > 30s.
  if (rtcOk && result.currentLocalTime.length() > 0) {
    rtc::applyServerLocalTime(result.currentLocalTime);
  }

  // Persist new schedules if config changed.
  if (result.configChanged) {
    JsonDocument doc;
    schedule::serializeConfig(doc, result.configVersion, result.schedules);
    storage::saveConfig(doc);

    // Drop state entries that no longer correspond to a known schedule.
    std::map<int, schedule::ScheduleStateEntry> trimmed;
    for (const auto& s : result.schedules) {
      auto it = state.find(s.id);
      if (it != state.end()) trimmed[s.id] = it->second;
    }
    JsonDocument st;
    schedule::serializeState(st, trimmed);
    storage::saveState(st);

    Serial.printf("[main] config updated: v%d, %u schedules\n",
                  result.configVersion, (unsigned)result.schedules.size());
  }

  Serial.printf("[main] claimed=%d configChanged=%d nextWake=%u s\n",
                result.claimed ? 1 : 0,
                result.configChanged ? 1 : 0,
                result.nextWakeSeconds);

  deepSleepFor(result.nextWakeSeconds);
}

void loop() {
  // Empty: setup() handles a single wake cycle and goes back to deep sleep.
}
