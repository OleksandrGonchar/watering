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
#include "humidifier.h"
#include "schedule.h"
#include "sensors.h"
#include "alerts.h"
#include "net.h"
#include "api.h"

namespace {

constexpr uint32_t MAX_DEEP_SLEEP_SECONDS = 70UL * 60UL;
constexpr uint32_t DEFAULT_WAKE_SECONDS = 60UL;

void deepSleepFor(uint32_t seconds) {
  if (seconds == 0) seconds = DEFAULT_WAKE_SECONDS;
  if (seconds > MAX_DEEP_SLEEP_SECONDS) seconds = MAX_DEEP_SLEEP_SECONDS;
  // Drive LEDs off before sleep so an active-low blue LED cannot stay lit
  // while the pin floats.
  sensors::redLed(false);
  sensors::blueLed(false);
#ifdef DEBUG_SKIP_DEEP_SLEEP
  // USB debugging: timer wake needs D0↔RST. Without it, deep sleep never
  // returns — use a plain delay + restart so Serial keeps working.
  Serial.printf("[main] DEBUG: delay %u s then restart (no deep sleep)\n", seconds);
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
  // TX-only UART: we never read Serial input, and freeing RX/GPIO3 lets the
  // bare-GPIO wiring drive the blue LED there. Logs on TX still work.
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  delay(50);
  Serial.println();
  Serial.println("==== Smart Watering boot ====");
  Serial.printf("Device: %s\n", DEVICE_ID);

  pump::begin();
  humidifier::begin();
  sensors::begin();
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

  // === Run due schedules locally (catch-up, no network needed) ===
  // Never water while overflowing or when the tank is empty.
  if (rtcOk && !schedules.empty() && !overflowAlert && !lowWaterAlert) {
    DateTime now = rtc::now();
    Serial.printf("[main] now=%s, schedules=%u\n",
                  rtc::toLocalIso(now).c_str(), (unsigned)schedules.size());
    schedule::RunOutcome outcome;
    schedule::runDueSchedules(now, schedules, state, pending, outcome);
    if (outcome.overflow) {
      overflowAlert = true;
      overflowSensors |= outcome.overflowSensors;
    }

    JsonDocument doc;
    schedule::serializeState(doc, state);
    storage::saveState(doc);
  } else {
    Serial.printf(
        "[main] skipping schedules (rtcOk=%d, schedules=%u, overflow=%d, "
        "lowWater=%d)\n",
        rtcOk ? 1 : 0, (unsigned)schedules.size(), overflowAlert ? 1 : 0,
        lowWaterAlert ? 1 : 0);
  }

  // Persist the alert snapshot so it survives a failed sync / reset.
  saveAlertSnapshot(overflowAlert, overflowSensors, lowWaterAlert);

  if (!pending.empty()) {
    JsonDocument doc;
    schedule::serializePending(doc, pending);
    storage::savePending(doc);
  }

  // === Sync with server ===
  if (!net::connect()) {
    Serial.println("[main] WiFi failed — will retry next wake");
    // Surface the alert locally even while offline (overflow can't wait).
    if (overflowAlert) {
      alerts::blinkRedUntilButton();
    } else if (lowWaterAlert) {
      alerts::blinkBlueFor(LOW_WATER_MIN_BLINK_MS);
    }
    deepSleepFor(DEFAULT_WAKE_SECONDS);
    return;
  }

  api::Tokens tokens;
  loadTokensFromStorage(tokens);

  uint32_t nowEpoch = rtcOk ? rtc::now().unixtime() : (millis() / 1000);
  if (!api::ensureValidToken(tokens, nowEpoch)) {
    Serial.println("[main] auth failed — will retry next wake");
    if (overflowAlert) {
      alerts::blinkRedUntilButton();
    } else if (lowWaterAlert) {
      alerts::blinkBlueFor(LOW_WATER_MIN_BLINK_MS);
    }
    deepSleepFor(DEFAULT_WAKE_SECONDS);
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
  alertReport.ackOverflow = false;

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
    if (overflowAlert) {
      alerts::blinkRedUntilButton();
    } else if (lowWaterAlert) {
      alerts::blinkBlueFor(LOW_WATER_MIN_BLINK_MS);
    }
    deepSleepFor(DEFAULT_WAKE_SECONDS);
    return;
  }

  // Successful sync: server now knows about any alert, clear the local queues.
  storage::clearPending();
  storage::clearAlerts();

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

  // === Alert indicators ===
  if (overflowAlert) {
    // Stay awake and blink the red LED until the user presses the button.
    alerts::blinkRedUntilButton();

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
  } else if (lowWaterAlert) {
    // Blink the blue LED for at least the configured window, then sleep.
    alerts::blinkBlueFor(LOW_WATER_MIN_BLINK_MS);
  }

  deepSleepFor(result.nextWakeSeconds);
}

void loop() {
  // Empty: setup() handles a single wake cycle and goes back to deep sleep.
}
