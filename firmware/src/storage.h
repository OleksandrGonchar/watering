#ifndef WATERING_STORAGE_H
#define WATERING_STORAGE_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Persistent layout in LittleFS:
//   /config.json   { configVersion, schedules: [ {id,timeLocal,durationSeconds,position} ] }
//   /tokens.json   { accessToken, refreshToken, accessExpiresAtEpoch }
//   /state.json    { schedules: [ {id, lastRunDate "YYYY-MM-DD", lastRunEpoch} ] }
//   /pending.json  { events: [ {scheduleId,durationSeconds,wateredAtIso} ] }
//   /alerts.json   { overflow, overflowSensors, lowWater }  (unreported alert snapshot)

namespace storage {

bool begin();

bool loadConfig(JsonDocument& out);
bool saveConfig(const JsonDocument& doc);

bool loadTokens(JsonDocument& out);
bool saveTokens(const JsonDocument& doc);
bool clearTokens();

bool loadState(JsonDocument& out);
bool saveState(const JsonDocument& doc);

bool loadPending(JsonDocument& out);
bool savePending(const JsonDocument& doc);
bool clearPending();

bool loadAlerts(JsonDocument& out);
bool saveAlerts(const JsonDocument& doc);
bool clearAlerts();

}  // namespace storage

#endif
