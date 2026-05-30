#include "schedule.h"
#include "pump.h"
#include "rtc.h"

namespace schedule {

namespace {

bool parseHHMM(const char* s, int& h, int& m) {
  if (!s) return false;
  if (strlen(s) < 5) return false;
  if (s[2] != ':') return false;
  h = (s[0] - '0') * 10 + (s[1] - '0');
  m = (s[3] - '0') * 10 + (s[4] - '0');
  return h >= 0 && h <= 23 && m >= 0 && m <= 59;
}

}  // namespace

bool parseConfig(const JsonDocument& doc, std::vector<Schedule>& out, int& out_version) {
  out.clear();
  if (!doc["configVersion"].is<int>()) return false;
  out_version = doc["configVersion"].as<int>();
  JsonArrayConst arr = doc["schedules"].as<JsonArrayConst>();
  if (arr.isNull()) return true;  // empty schedules is fine
  for (JsonObjectConst o : arr) {
    Schedule s;
    s.id = o["id"].as<int>();
    s.durationSeconds = o["durationSeconds"].as<int>();
    int h, m;
    if (!parseHHMM(o["timeLocal"].as<const char*>(), h, m)) continue;
    s.hour = h;
    s.minute = m;
    out.push_back(s);
  }
  return true;
}

void serializeConfig(JsonDocument& doc, int version, const std::vector<Schedule>& schedules) {
  doc.clear();
  doc["configVersion"] = version;
  JsonArray arr = doc["schedules"].to<JsonArray>();
  for (const auto& s : schedules) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = s.id;
    char hhmm[6];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", s.hour, s.minute);
    o["timeLocal"] = hhmm;
    o["durationSeconds"] = s.durationSeconds;
  }
}

void parseState(const JsonDocument& doc, std::map<int, ScheduleStateEntry>& out) {
  out.clear();
  JsonArrayConst arr = doc["schedules"].as<JsonArrayConst>();
  if (arr.isNull()) return;
  for (JsonObjectConst o : arr) {
    int id = o["id"].as<int>();
    ScheduleStateEntry e;
    e.lastRunDate = o["lastRunDate"].as<const char*>() ?: "";
    e.lastRunEpoch = o["lastRunEpoch"].as<uint32_t>();
    out[id] = e;
  }
}

void serializeState(JsonDocument& doc, const std::map<int, ScheduleStateEntry>& state) {
  doc.clear();
  JsonArray arr = doc["schedules"].to<JsonArray>();
  for (const auto& kv : state) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = kv.first;
    o["lastRunDate"] = kv.second.lastRunDate;
    o["lastRunEpoch"] = kv.second.lastRunEpoch;
  }
}

void parsePending(const JsonDocument& doc, std::vector<PendingEvent>& out) {
  out.clear();
  JsonArrayConst arr = doc["events"].as<JsonArrayConst>();
  if (arr.isNull()) return;
  for (JsonObjectConst o : arr) {
    PendingEvent e;
    e.scheduleId = o["scheduleId"].as<int>();
    e.durationSeconds = o["durationSeconds"].as<uint32_t>();
    e.wateredAtIso = o["wateredAtIso"].as<const char*>() ?: "";
    out.push_back(e);
  }
}

void serializePending(JsonDocument& doc, const std::vector<PendingEvent>& events) {
  doc.clear();
  JsonArray arr = doc["events"].to<JsonArray>();
  for (const auto& e : events) {
    JsonObject o = arr.add<JsonObject>();
    o["scheduleId"] = e.scheduleId;
    o["durationSeconds"] = e.durationSeconds;
    o["wateredAtIso"] = e.wateredAtIso;
  }
}

void runDueSchedules(const DateTime& now,
                     const std::vector<Schedule>& schedules,
                     std::map<int, ScheduleStateEntry>& state,
                     std::vector<PendingEvent>& pending) {
  String today = rtc::toDateString(now);
  uint32_t nowEpoch = now.unixtime();

  for (const auto& s : schedules) {
    int nowMinutes = now.hour() * 60 + now.minute();
    int schMinutes = s.hour * 60 + s.minute;
    if (nowMinutes < schMinutes) continue;

    auto it = state.find(s.id);
    if (it != state.end()) {
      if (it->second.lastRunDate == today) continue;
      // Circuit breaker: never fire two waterings within 6 hours, even if
      // calendar day changed (defends 23:59 -> 00:01 cross-midnight wake).
      if (nowEpoch >= it->second.lastRunEpoch &&
          (nowEpoch - it->second.lastRunEpoch) < 6UL * 3600UL) {
        Serial.printf("[schedule] id=%d skipped (6h circuit breaker)\n", s.id);
        continue;
      }
    }

    Serial.printf("[schedule] running id=%d for %d s\n", s.id, s.durationSeconds);
    pump::run(s.durationSeconds);

    PendingEvent ev;
    ev.scheduleId = s.id;
    ev.durationSeconds = s.durationSeconds;
    ev.wateredAtIso = rtc::toLocalIso(now);
    pending.push_back(ev);

    ScheduleStateEntry e;
    e.lastRunDate = today;
    e.lastRunEpoch = nowEpoch;
    state[s.id] = e;
  }
}

}  // namespace schedule
