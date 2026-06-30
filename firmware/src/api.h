#ifndef WATERING_API_H
#define WATERING_API_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "schedule.h"

namespace api {

struct Tokens {
  String accessToken;
  uint32_t accessExpiresAtEpoch;
  String refreshToken;
};

// Returns true on success and writes new tokens into `out`.
bool login(Tokens& out);

// Try refresh first; if refresh fails, fall back to full login. Updates
// `tokens` in place on success.
//
// When forceRefresh is true, the "access token still valid per local clock"
// shortcut is skipped and a refresh/login is always attempted. Use this after
// the server rejected a token with 401 — the device clock (DS1302) may be
// wrong, so we must not trust it to decide token validity.
bool ensureValidToken(Tokens& tokens, uint32_t nowEpoch, bool forceRefresh = false);

struct SyncResult {
  bool ok;
  bool unauthorized;             // true if the server rejected the token (401)
  bool claimed;
  bool configChanged;
  int configVersion;
  std::vector<schedule::Schedule> schedules;
  String currentLocalTime;       // "YYYY-MM-DDTHH:MM:SS"
  uint32_t nextWakeSeconds;
};

// POST /api/device/sync. Body is built from the args. On 401, callers should
// refresh tokens and retry.
SyncResult sync(const Tokens& tokens,
                int configVersion,
                bool sendClaimCode,
                const std::vector<schedule::PendingEvent>& pending);

}  // namespace api

#endif
