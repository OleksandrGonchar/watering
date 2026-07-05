#include "storage.h"
#include <LittleFS.h>

namespace {

bool readJson(const char* path, JsonDocument& out) {
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  DeserializationError err = deserializeJson(out, f);
  f.close();
  if (err) {
    Serial.printf("[storage] parse error %s: %s\n", path, err.c_str());
    return false;
  }
  return true;
}

bool writeJson(const char* path, const JsonDocument& doc) {
  File f = LittleFS.open(path, "w");
  if (!f) {
    Serial.printf("[storage] cannot open %s for write\n", path);
    return false;
  }
  size_t written = serializeJson(doc, f);
  f.close();
  return written > 0;
}

bool removeIfExists(const char* path) {
  if (LittleFS.exists(path)) return LittleFS.remove(path);
  return true;
}

}  // namespace

namespace storage {

bool begin() {
  if (!LittleFS.begin()) {
    Serial.println("[storage] mount failed, formatting...");
    LittleFS.format();
    return LittleFS.begin();
  }
  return true;
}

bool loadConfig(JsonDocument& out) { return readJson("/config.json", out); }
bool saveConfig(const JsonDocument& doc) { return writeJson("/config.json", doc); }

bool loadTokens(JsonDocument& out) { return readJson("/tokens.json", out); }
bool saveTokens(const JsonDocument& doc) { return writeJson("/tokens.json", doc); }
bool clearTokens() { return removeIfExists("/tokens.json"); }

bool loadState(JsonDocument& out) { return readJson("/state.json", out); }
bool saveState(const JsonDocument& doc) { return writeJson("/state.json", doc); }

bool loadPending(JsonDocument& out) { return readJson("/pending.json", out); }
bool savePending(const JsonDocument& doc) { return writeJson("/pending.json", doc); }
bool clearPending() { return removeIfExists("/pending.json"); }

bool loadAlerts(JsonDocument& out) { return readJson("/alerts.json", out); }
bool saveAlerts(const JsonDocument& doc) { return writeJson("/alerts.json", doc); }
bool clearAlerts() { return removeIfExists("/alerts.json"); }

}  // namespace storage
