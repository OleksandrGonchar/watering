#include "api.h"
#include "config.h"
#include "net.h"

namespace api {

namespace {

constexpr int ACCESS_TTL_GUARD_SECONDS = 60;

bool parseLoginResponse(const String& body, Tokens& out, uint32_t nowEpoch) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[api] login parse error: %s\n", err.c_str());
    return false;
  }
  out.accessToken = doc["accessToken"].as<const char*>() ?: "";
  out.refreshToken = doc["refreshToken"].as<const char*>() ?: "";
  uint32_t ttl = doc["accessExpiresInSec"].as<uint32_t>();
  if (ttl == 0) ttl = 900;
  out.accessExpiresAtEpoch = nowEpoch + ttl;
  return out.accessToken.length() > 0 && out.refreshToken.length() > 0;
}

}  // namespace

bool login(Tokens& out) {
  JsonDocument req;
  req["deviceId"] = DEVICE_ID;
  req["username"] = DEVICE_USER;
  req["password"] = DEVICE_PASS;
  String body;
  serializeJson(req, body);

  String resp;
  int status = net::httpPost("/api/device/login", body, "", resp);
  if (status != 200) {
    Serial.printf("[api] login failed status=%d\n", status);
    return false;
  }
  return parseLoginResponse(resp, out, (uint32_t)(millis() / 1000));
}

bool ensureValidToken(Tokens& tokens, uint32_t nowEpoch) {
  if (tokens.accessToken.length() > 0 &&
      tokens.accessExpiresAtEpoch > nowEpoch + ACCESS_TTL_GUARD_SECONDS) {
    return true;
  }

  if (tokens.refreshToken.length() > 0) {
    JsonDocument req;
    req["refreshToken"] = tokens.refreshToken;
    String body;
    serializeJson(req, body);
    String resp;
    int status = net::httpPost("/api/device/refresh", body, "", resp);
    if (status == 200) {
      Tokens refreshed;
      if (parseLoginResponse(resp, refreshed, nowEpoch)) {
        tokens = refreshed;
        return true;
      }
    }
    Serial.printf("[api] refresh failed status=%d, falling back to login\n", status);
  }

  Tokens fresh;
  if (!login(fresh)) return false;
  tokens = fresh;
  tokens.accessExpiresAtEpoch = nowEpoch + 900;  // refine — login uses millis-relative
  return true;
}

SyncResult sync(const Tokens& tokens,
                int configVersion,
                bool sendClaimCode,
                const std::vector<schedule::PendingEvent>& pending) {
  SyncResult r{};
  r.ok = false;
  r.nextWakeSeconds = 1800;

  JsonDocument req;
  req["configVersion"] = configVersion;
  if (sendClaimCode) {
    req["claimCode"] = CLAIM_CODE;
  }
  if (!pending.empty()) {
    JsonArray arr = req["events"].to<JsonArray>();
    for (const auto& e : pending) {
      JsonObject o = arr.add<JsonObject>();
      o["scheduleId"] = e.scheduleId;
      o["durationSeconds"] = e.durationSeconds;
      o["wateredAt"] = e.wateredAtIso;
    }
  }
  String body;
  serializeJson(req, body);

  String resp;
  int status = net::httpPost("/api/device/sync", body, tokens.accessToken, resp);
  if (status != 200) {
    Serial.printf("[api] sync failed status=%d body=%s\n", status, resp.c_str());
    return r;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    Serial.printf("[api] sync parse error: %s\n", err.c_str());
    return r;
  }

  r.ok = true;
  r.claimed = doc["claimed"].as<bool>();
  r.configChanged = doc["configChanged"].as<bool>();
  r.configVersion = doc["configVersion"].as<int>();
  r.currentLocalTime = doc["currentLocalTime"].as<const char*>() ?: "";
  r.nextWakeSeconds = doc["nextWakeSeconds"].as<uint32_t>();
  if (r.nextWakeSeconds == 0) r.nextWakeSeconds = 1800;

  if (r.configChanged) {
    schedule::parseConfig(doc, r.schedules, r.configVersion);
  }
  return r;
}

}  // namespace api
