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
//   4. Most wakes are local-only (sensors + schedules). Wi-Fi + /sync run
//      every SYNC_EVERY_WAKES wakes (default: 6 × 30s ≈ every 3 minutes).
//   5. On sync wakes: connect Wi-Fi, POST /sync, refresh schedules/RTC.
//   6. Deep-sleep LOCAL_WAKE_SECONDS (default 30s; capped at 70 minutes).
//
// Hardware: GPIO16 (D0) MUST be wired to RST for deep-sleep wake.

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "storage.h"
#include "rtc.h"
#include "pump.h"
#include "humidifier.h"
#include "schedule.h"
#include "sensors.h"
#include "alerts.h"
#include "net.h"
#include "api.h"

namespace {

constexpr uint32_t MAX_DEEP_SLEEP_SECONDS = 70UL * 60UL;

#ifndef LOCAL_WAKE_SECONDS
#define LOCAL_WAKE_SECONDS 30UL
#endif
#ifndef SYNC_EVERY_WAKES
#define SYNC_EVERY_WAKES 6
#endif
constexpr uint32_t WAKE_META_MAGIC = 0x57414B45UL;  // 'WAKE'

struct WakeMeta {
  uint32_t magic;
  uint32_t wakesSinceSync;
};

bool loadWakeMeta(WakeMeta& meta) {
  if (!ESP.rtcUserMemoryRead(0, reinterpret_cast<uint32_t*>(&meta),
                             sizeof(meta))) {
    return false;
  }
  return meta.magic == WAKE_META_MAGIC;
}

void saveWakeMeta(const WakeMeta& meta) {
  WakeMeta copy = meta;
  ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&copy), sizeof(copy));
}

void deepSleepFor(uint32_t seconds) {
  if (seconds == 0) seconds = LOCAL_WAKE_SECONDS;
  if (seconds > MAX_DEEP_SLEEP_SECONDS) seconds = MAX_DEEP_SLEEP_SECONDS;
  sensors::redLed(false);
#ifdef DEBUG_SKIP_DEEP_SLEEP
  // USB debugging: timer wake needs D0↔RST. Without it, deep sleep never
  // returns — use a plain delay + ESP.restart(). The bootloader lines that
  // follow ("ets Jan 8… rst cause:…") are normal soft-restart noise, not a
  // physical RESET button press. Remove this define for real deep sleep.
  Serial.printf(
      "[main] DEBUG: delay %u s then ESP.restart() "
      "(looks like reset in Serial — expected)\n",
      seconds);
  Serial.flush();
  delay((uint32_t)seconds * 1000UL);
  ESP.restart();
#else
  Serial.printf("[main] deep sleeping for %u s\n", seconds);
  Serial.flush();
  ESP.deepSleep((uint64_t)seconds * 1000000ULL);
#endif
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

void loadAlertSnapshot(bool& overflow, uint8_t& overflowSensors) {
  JsonDocument doc;
  if (!storage::loadAlerts(doc)) return;
  if (doc["overflow"].as<bool>()) {
    overflow = true;
    overflowSensors |= doc["overflowSensors"].as<uint8_t>();
  }
  // lowWater in /alerts.json is ignored: the live reed reading is authoritative.
}

void saveAlertSnapshot(bool overflow, uint8_t overflowSensors, bool lowWater) {
  if (!overflow && !lowWater) {
    storage::clearAlerts();
    return;
  }
  JsonDocument doc;
  doc["overflow"] = overflow;
  doc["overflowSensors"] = overflowSensors;
  doc["lowWater"] = lowWater;
  storage::saveAlerts(doc);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("==== Smart Watering boot ====");
  Serial.printf("Device: %s\n", DEVICE_ID);

  // sensors::begin() brings up the PCF8574 first — humidifier motor direction
  // and dead-point reeds live on the expander.
  pump::begin();
  sensors::begin();
  humidifier::begin();
  bool storageOk = storage::begin();
  rtc::begin();  // logs raw time; isValid() gates schedules

  if (!storageOk) {
    Serial.println("[main] storage init failed — sleeping briefly");
    deepSleepFor(LOCAL_WAKE_SECONDS);
    return;
  }

  // === Wake cadence (RTC memory survives deep sleep, cleared on power loss) ===
  WakeMeta wakeMeta{};
  if (!loadWakeMeta(wakeMeta)) {
    wakeMeta.magic = WAKE_META_MAGIC;
    // Power-on / first boot: sync immediately.
    wakeMeta.wakesSinceSync = SYNC_EVERY_WAKES;
  } else {
    wakeMeta.wakesSinceSync++;
  }
  saveWakeMeta(wakeMeta);

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

  // === Water-safety alerts ===
  // Overflow latches from a previous failed-to-report snapshot (and stays
  // until the user presses the ack button). Low water follows the live reed
  // reading so a stale /alerts.json cannot keep the alarm on after refill.
  bool overflowAlert = false;
  uint8_t overflowSensors = 0;
  loadAlertSnapshot(overflowAlert, overflowSensors);

  bool lowWaterAlert = sensors::isLowWater();
  uint8_t liveOverflow = sensors::overflowMask();
  if (liveOverflow != 0) {
    overflowAlert = true;
    overflowSensors |= liveOverflow;
  }
  sensors::logInputs();
  Serial.printf("[main] alerts: overflow=%d (mask=0x%02x) lowWater=%d\n",
                overflowAlert ? 1 : 0, overflowSensors, lowWaterAlert ? 1 : 0);

  // === Run due schedules locally (catch-up) ===
  // Never water while overflowing, tank empty, or RTC still at 2000-00-00 —
  // bogus clock would fire the wrong schedules. Invalid clock is fixed after
  // /sync (see below), then we try catch-up once more.
  bool schedulesRan = false;
  auto tryRunSchedules = [&](const char* phase) {
    if (schedulesRan) return;
    if (!rtc::isValid() || schedules.empty() || overflowAlert || lowWaterAlert) {
      Serial.printf(
          "[main] skipping schedules (%s rtcValid=%d schedules=%u overflow=%d "
          "lowWater=%d)\n",
          phase, rtc::isValid() ? 1 : 0, (unsigned)schedules.size(),
          overflowAlert ? 1 : 0, lowWaterAlert ? 1 : 0);
      return;
    }
    DateTime now = rtc::now();
    Serial.printf("[main] now=%s (%s), schedules=%u\n",
                  rtc::toLocalIso(now).c_str(), phase,
                  (unsigned)schedules.size());
    schedule::RunOutcome outcome;
    schedule::runDueSchedules(now, schedules, state, pending, outcome);
    if (outcome.overflow) {
      overflowAlert = true;
      overflowSensors |= outcome.overflowSensors;
    }
    JsonDocument doc;
    schedule::serializeState(doc, state);
    storage::saveState(doc);
    schedulesRan = true;
  };

  tryRunSchedules("pre-sync");

  // Persist the alert snapshot so it survives a failed sync / reset.
  saveAlertSnapshot(overflowAlert, overflowSensors, lowWaterAlert);

  if (!pending.empty()) {
    JsonDocument doc;
    schedule::serializePending(doc, pending);
    storage::savePending(doc);
  }

  // Sync every N wakes, or sooner when we must talk to the server.
  bool needSync = wakeMeta.wakesSinceSync >= SYNC_EVERY_WAKES ||
                  !rtc::isValid() || !pending.empty() || overflowAlert;
  Serial.printf("[main] wake=%u/%u needSync=%d (rtcValid=%d pending=%u)\n",
                wakeMeta.wakesSinceSync, (unsigned)SYNC_EVERY_WAKES,
                needSync ? 1 : 0, rtc::isValid() ? 1 : 0,
                (unsigned)pending.size());

  bool ackOverflow = false;

  if (!needSync) {
    // Local-only wake: sensors/schedules already handled; skip Wi-Fi.
    if (overflowAlert) {
      // Should be unreachable (overflow forces needSync), kept as safety net.
      alerts::blinkRedUntilButton();
      ackOverflow = true;
      needSync = true;
    } else {
      deepSleepFor(LOCAL_WAKE_SECONDS);
      return;
    }
  }

  // === Sync with server ===
  if (!net::connect()) {
    Serial.println("[main] WiFi failed — will retry next wake");
    // Surface the alert locally even while offline (overflow can't wait).
    if (overflowAlert) {
      alerts::blinkRedUntilButton();
      ackOverflow = true;
    }
    deepSleepFor(LOCAL_WAKE_SECONDS);
    return;
  }

  api::Tokens tokens;
  loadTokensFromStorage(tokens);

  // Don't trust unixtime from a 2000-00-00 RTC for token expiry decisions.
  uint32_t nowEpoch =
      rtc::isValid() ? rtc::now().unixtime() : (millis() / 1000);
  if (!api::ensureValidToken(tokens, nowEpoch)) {
    Serial.println("[main] auth failed — will retry next wake");
    if (overflowAlert) {
      alerts::blinkRedUntilButton();
      ackOverflow = true;
    }
    deepSleepFor(LOCAL_WAKE_SECONDS);
    return;
  }
  saveTokensToStorage(tokens);

  // claim_code is sent until the device is claimed. We don't know the claimed
  // status until after the first /sync, so always send on first run after
  // power-on. Server ignores it once the user has claimed.
  bool sendClaimCode = true;

  api::AlertReport alertReport;
  alertReport.overflowActive = overflowAlert;
  alertReport.overflowSensors = overflowSensors;
  alertReport.lowWaterActive = lowWaterAlert;
  alertReport.ackOverflow = ackOverflow;

  api::SyncResult result =
      api::sync(tokens, configVersion, sendClaimCode, pending, alertReport);

  // The server rejected our access token (likely expired, or our clock is
  // wrong and we wrongly considered it valid). Force a refresh/login that
  // ignores the local clock, then retry once.
  if (!result.ok && result.unauthorized) {
    Serial.println("[main] /sync 401 — forcing token refresh and retrying");
    if (api::ensureValidToken(tokens, nowEpoch, /*forceRefresh=*/true)) {
      saveTokensToStorage(tokens);
      result =
          api::sync(tokens, configVersion, sendClaimCode, pending, alertReport);
    }
  }

  if (!result.ok) {
    Serial.println("[main] /sync failed — keeping pending events for retry");
    // Alert (if any) is kept in /alerts.json for the next wake. Still surface
    // it locally so the user sees the LED even while offline.
    if (overflowAlert && !ackOverflow) {
      alerts::blinkRedUntilButton();
      ackOverflow = true;
    }
    deepSleepFor(LOCAL_WAKE_SECONDS);
    return;
  }

  // Successful sync: reset the cadence counter and clear local queues.
  wakeMeta.wakesSinceSync = 0;
  saveWakeMeta(wakeMeta);
  storage::clearPending();
  storage::clearAlerts();

  // Sync RTC from server local time (force-writes when chip still has 2000-00-00).
  if (result.currentLocalTime.length() > 0) {
    Serial.printf("[main] server time=%s\n", result.currentLocalTime.c_str());
    rtc::applyServerLocalTime(result.currentLocalTime);
  } else {
    Serial.println("[main] sync ok but currentLocalTime empty — cannot set RTC");
  }

  // Persist new schedules if config changed.
  if (result.configChanged) {
    schedules = result.schedules;
    configVersion = result.configVersion;
    JsonDocument doc;
    schedule::serializeConfig(doc, configVersion, schedules);
    storage::saveConfig(doc);

    // Drop state entries that no longer correspond to a known schedule.
    std::map<int, schedule::ScheduleStateEntry> trimmed;
    for (const auto& s : schedules) {
      auto it = state.find(s.id);
      if (it != state.end()) trimmed[s.id] = it->second;
    }
    state.swap(trimmed);
    JsonDocument st;
    schedule::serializeState(st, state);
    storage::saveState(st);

    Serial.printf("[main] config updated: v%d, %u schedules\n", configVersion,
                  (unsigned)schedules.size());
  }

  // Catch-up watering after the clock was corrected (skipped pre-sync).
  pending.clear();
  tryRunSchedules("post-sync");
  if (!pending.empty()) {
    // Report events produced after the clock fix on the next wake (we already
    // cleared pending above after the first sync). Best-effort second sync.
    api::AlertReport quiet;
    quiet.lowWaterActive = lowWaterAlert;
    quiet.overflowActive = overflowAlert;
    quiet.overflowSensors = overflowSensors;
    api::SyncResult catchUp =
        api::sync(tokens, configVersion, /*sendClaimCode=*/false, pending, quiet);
    if (catchUp.ok) {
      storage::clearPending();
    } else {
      JsonDocument doc;
      schedule::serializePending(doc, pending);
      storage::savePending(doc);
      Serial.println("[main] post-sync watering events kept for next wake");
    }
  }

  Serial.printf("[main] claimed=%d configChanged=%d nextLocalWake=%u s rtcValid=%d\n",
                result.claimed ? 1 : 0, result.configChanged ? 1 : 0,
                (unsigned)LOCAL_WAKE_SECONDS, rtc::isValid() ? 1 : 0);

  // === Alert indicators ===
  if (overflowAlert && !ackOverflow) {
    // Stay awake and blink the red LED until the user presses the button.
    alerts::blinkRedUntilButton();
    ackOverflow = true;

    // Report the acknowledgement so the server hides the red banner (the row
    // stays in history). Best-effort — a failure just means the banner clears
    // on a later sync.
    api::AlertReport ackReport;
    ackReport.ackOverflow = true;
    std::vector<schedule::PendingEvent> noPending;
    api::SyncResult ackRes =
        api::sync(tokens, configVersion, /*sendClaimCode=*/false, noPending, ackReport);
    if (!ackRes.ok && ackRes.unauthorized &&
        api::ensureValidToken(tokens, nowEpoch, /*forceRefresh=*/true)) {
      saveTokensToStorage(tokens);
      api::sync(tokens, configVersion, /*sendClaimCode=*/false, noPending, ackReport);
    }
    storage::clearAlerts();
  }

  // Local cadence owns sleep length (server nextWakeSeconds is ignored).
  deepSleepFor(LOCAL_WAKE_SECONDS);
}

void loop() {
  // Empty: setup() handles a single wake cycle and goes back to deep sleep.
}
